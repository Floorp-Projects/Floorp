/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.defineModuleGetter(this, "Utils",
  "resource://gre/modules/accessibility/Utils.jsm");
ChromeUtils.defineModuleGetter(this, "Logger",
  "resource://gre/modules/accessibility/Utils.jsm");
ChromeUtils.defineModuleGetter(this, "Roles",
  "resource://gre/modules/accessibility/Constants.jsm");
ChromeUtils.defineModuleGetter(this, "States",
  "resource://gre/modules/accessibility/Constants.jsm");
ChromeUtils.defineModuleGetter(this, "TraversalRules",
  "resource://gre/modules/accessibility/Traversal.jsm");
ChromeUtils.defineModuleGetter(this, "TraversalHelper",
  "resource://gre/modules/accessibility/Traversal.jsm");
ChromeUtils.defineModuleGetter(this, "Presentation",
  "resource://gre/modules/accessibility/Presentation.jsm");

var EXPORTED_SYMBOLS = ["ContentControl"];

const MOVEMENT_GRANULARITY_CHARACTER = 1;
const MOVEMENT_GRANULARITY_WORD = 2;
const MOVEMENT_GRANULARITY_PARAGRAPH = 8;

const CLIPBOARD_COPY = 0x4000;
const CLIPBOARD_PASTE = 0x8000;
const CLIPBOARD_CUT = 0x10000;

function ContentControl(aContentScope) {
  this._contentScope = Cu.getWeakReference(aContentScope);
  this._childMessageSenders = new WeakMap();
}

this.ContentControl.prototype = {
  messagesOfInterest: ["AccessFu:MoveCursor",
                       "AccessFu:ClearCursor",
                       "AccessFu:MoveToPoint",
                       "AccessFu:AutoMove",
                       "AccessFu:Activate",
                       "AccessFu:MoveByGranularity",
                       "AccessFu:AndroidScroll",
                       "AccessFu:SetSelection",
                       "AccessFu:Clipboard"],

  start: function cc_start() {
    let cs = this._contentScope.get();
    for (let message of this.messagesOfInterest) {
      cs.addMessageListener(message, this);
    }
  },

  stop: function cc_stop() {
    let cs = this._contentScope.get();
    for (let message of this.messagesOfInterest) {
      cs.removeMessageListener(message, this);
    }
  },

  get document() {
    return this._contentScope.get().content.document;
  },

  get window() {
    return this._contentScope.get().content;
  },

  get vc() {
    return Utils.getVirtualCursor(this.document);
  },

  receiveMessage: function cc_receiveMessage(aMessage) {
    Logger.debug(() => {
      return ["ContentControl.receiveMessage",
        aMessage.name,
        JSON.stringify(aMessage.json)];
    });

    // If we get an explicit message, we should immediately cancel any autoMove
    this.cancelAutoMove();

    try {
      let func = this["handle" + aMessage.name.slice(9)]; // 'AccessFu:'.length
      if (func) {
        func.bind(this)(aMessage);
      } else {
        Logger.warning("ContentControl: Unhandled message:", aMessage.name);
      }
    } catch (x) {
      Logger.logException(
        x, "Error handling message: " + JSON.stringify(aMessage.json));
    }
  },

  handleAndroidScroll: function cc_handleAndroidScroll(aMessage) {
    let vc = this.vc;
    let position = vc.position;

    if (aMessage.json.origin != "child" && this.sendToChild(vc, aMessage)) {
      // Forwarded succesfully to child cursor.
      return;
    }

    // Counter-intuitive, but scrolling backward (ie. up), actually should
    // increase range values.
    if (this.adjustRange(position, aMessage.json.direction === "backward")) {
      return;
    }

    this._contentScope.get().sendAsyncMessage("AccessFu:DoScroll",
      { bounds: Utils.getBounds(position),
        page: aMessage.json.direction === "forward" ? 1 : -1,
        horizontal: false });
  },

  handleMoveCursor: function cc_handleMoveCursor(aMessage) {
    let origin = aMessage.json.origin;
    let action = aMessage.json.action;
    let adjustRange = aMessage.json.adjustRange;
    let vc = this.vc;

    if (origin != "child" && this.sendToChild(vc, aMessage)) {
      // Forwarded succesfully to child cursor.
      return;
    }

    if (adjustRange && this.adjustRange(vc.position, action === "moveNext")) {
      return;
    }

    let moved = TraversalHelper.move(vc, action, aMessage.json.rule);

    if (moved) {
      if (origin === "child") {
        // We just stepped out of a child, clear child cursor.
        Utils.getMessageManager(aMessage.target).sendAsyncMessage(
          "AccessFu:ClearCursor", {});
      } else {
        // We potentially landed on a new child cursor. If so, we want to
        // either be on the first or last item in the child doc.
        let childAction = action;
        if (action === "moveNext") {
          childAction = "moveFirst";
        } else if (action === "movePrevious") {
          childAction = "moveLast";
        }

        // Attempt to forward move to a potential child cursor in our
        // new position.
        this.sendToChild(vc, aMessage, { action: childAction }, true);
      }
    } else if (!this._childMessageSenders.has(aMessage.target) &&
               origin !== "top") {
      // We failed to move, and the message is not from a parent, so forward
      // to it.
      this.sendToParent(aMessage);
    } else {
      this._contentScope.get().sendAsyncMessage("AccessFu:Present",
        Presentation.noMove(action));
    }
  },

  handleMoveToPoint: function cc_handleMoveToPoint(aMessage) {
    let [x, y] = [aMessage.json.x, aMessage.json.y];
    let rule = TraversalRules[aMessage.json.rule];

    this.vc.moveToPoint(rule, x, y, true);
  },

  handleClearCursor: function cc_handleClearCursor(aMessage) {
    let forwarded = this.sendToChild(this.vc, aMessage);
    this.vc.position = null;
    if (!forwarded) {
      this._contentScope.get().sendAsyncMessage("AccessFu:CursorCleared");
    }
    this.document.activeElement.blur();
  },

  handleAutoMove: function cc_handleAutoMove(aMessage) {
    this.autoMove(null, aMessage.json);
  },

  handleActivate: function cc_handleActivate(aMessage) {
    let activateAccessible = (aAccessible) => {
      Logger.debug(() => {
        return ["activateAccessible", Logger.accessibleToString(aAccessible)];
      });
      try {
        if (aMessage.json.activateIfKey &&
          !Utils.isActivatableOnFingerUp(aAccessible)) {
          // Only activate keys, don't do anything on other objects.
          return;
        }
      } catch (e) {
        // accessible is invalid. Silently fail.
        return;
      }

      if (aAccessible.actionCount > 0) {
        aAccessible.doAction(0);
      } else {
        let control = Utils.getEmbeddedControl(aAccessible);
        if (control && control.actionCount > 0) {
          control.doAction(0);
        }

        // XXX Some mobile widget sets do not expose actions properly
        // (via ARIA roles, etc.), so we need to generate a click.
        // Could possibly be made simpler in the future. Maybe core
        // engine could expose nsCoreUtiles::DispatchMouseEvent()?
        let docAcc = Utils.AccService.getAccessibleFor(this.document);
        let docX = {}, docY = {}, docW = {}, docH = {};
        docAcc.getBounds(docX, docY, docW, docH);

        let objX = {}, objY = {}, objW = {}, objH = {};
        aAccessible.getBounds(objX, objY, objW, objH);

        let x = Math.round((objX.value - docX.value) + objW.value / 2);
        let y = Math.round((objY.value - docY.value) + objH.value / 2);

        let node = aAccessible.DOMNode || aAccessible.parent.DOMNode;

        for (let eventType of ["mousedown", "mouseup"]) {
          let evt = this.document.createEvent("MouseEvents");
          evt.initMouseEvent(eventType, true, true, this.window,
            x, y, 0, 0, 0, false, false, false, false, 0, null);
          node.dispatchEvent(evt);
        }
      }

      if (!Utils.isActivatableOnFingerUp(aAccessible)) {
        // Keys will typically have a sound of their own.
        this._contentScope.get().sendAsyncMessage("AccessFu:Present",
          Presentation.actionInvoked(aAccessible, "click"));
      }
    };

    let focusedAcc = Utils.AccService.getAccessibleFor(
      this.document.activeElement);
    if (focusedAcc && this.vc.position === focusedAcc
        && focusedAcc.role === Roles.ENTRY) {
      let accText = focusedAcc.QueryInterface(Ci.nsIAccessibleText);
      let oldOffset = accText.caretOffset;
      let newOffset = aMessage.json.offset;
      let text = accText.getText(0, accText.characterCount);

      if (newOffset >= 0 && newOffset <= accText.characterCount) {
        accText.caretOffset = newOffset;
      }

      this.presentCaretChange(text, oldOffset, accText.caretOffset);
      return;
    }

    // recursively find a descendant that is activatable.
    let getActivatableDescendant = (aAccessible) => {
      if (aAccessible.actionCount > 0) {
        return aAccessible;
      }

      for (let acc = aAccessible.firstChild; acc; acc = acc.nextSibling) {
        let activatable = getActivatableDescendant(acc);
        if (activatable) {
          return activatable;
        }
      }

      return null;
    };

    let vc = this.vc;
    if (!this.sendToChild(vc, aMessage, null, true)) {
      let position = vc.position;
      activateAccessible(getActivatableDescendant(position) || position);
    }
  },

  adjustRange: function cc_adjustRange(aAccessible, aStepUp) {
    let acc = Utils.getEmbeddedControl(aAccessible) || aAccessible;
    try {
      acc.QueryInterface(Ci.nsIAccessibleValue);
    } catch (x) {
      // This is not an adjustable, return false.
      return false;
    }

    let elem = acc.DOMNode;
    if (!elem) {
      return false;
    }

    if (elem.tagName === "INPUT" && elem.type === "range") {
      elem[aStepUp ? "stepDown" : "stepUp"]();
      let evt = this.document.createEvent("UIEvent");
      evt.initEvent("change", true, true);
      elem.dispatchEvent(evt);
    } else {
      let evt = this.document.createEvent("KeyboardEvent");
      let keycode = aStepUp ? evt.DOM_VK_DOWN : evt.DOM_VK_UP;
      evt.initKeyEvent(
        "keypress", false, true, null, false, false, false, false, keycode, 0);
      elem.dispatchEvent(evt);
    }

    return true;
  },

  handleMoveByGranularity: function cc_handleMoveByGranularity(aMessage) {
    let { direction, granularity } = aMessage.json;
    let focusedAcc = Utils.AccService.getAccessibleFor(this.document.activeElement);
    if (focusedAcc && Utils.getState(focusedAcc).contains(States.EDITABLE)) {
      this.moveCaret(focusedAcc, direction, granularity);
      return;
    }

    let pivotGranularity;
    switch (granularity) {
      case MOVEMENT_GRANULARITY_CHARACTER:
        pivotGranularity = Ci.nsIAccessiblePivot.CHAR_BOUNDARY;
        break;
      case MOVEMENT_GRANULARITY_WORD:
        pivotGranularity = Ci.nsIAccessiblePivot.WORD_BOUNDARY;
        break;
      default:
        return;
    }

    if (direction === "Previous") {
      this.vc.movePreviousByText(pivotGranularity);
    } else if (direction === "Next") {
      this.vc.moveNextByText(pivotGranularity);
    }
  },

  handleSetSelection: function cc_handleSetSelection(aMessage) {
    const { start, end } = aMessage.json;
    const focusedAcc =
      Utils.AccService.getAccessibleFor(this.document.activeElement);
    if (focusedAcc) {
      const accText = focusedAcc.QueryInterface(Ci.nsIAccessibleText);
      if (start == end) {
        accText.caretOffset = start;
      } else {
        accText.setSelectionBounds(0, start, end);
      }
    }
  },

  handleClipboard: function cc_handleClipboard(aMessage) {
    const { action } = aMessage.json;
    const focusedAcc =
      Utils.AccService.getAccessibleFor(this.document.activeElement);
    if (focusedAcc) {
      const [startSel, endSel] = Utils.getTextSelection(focusedAcc);
      const editText = focusedAcc.QueryInterface(Ci.nsIAccessibleEditableText);
      switch (action) {
        case CLIPBOARD_COPY:
          if (startSel != endSel) {
            editText.copyText(startSel, endSel);
          }
          break;
        case CLIPBOARD_PASTE:
          if (startSel != endSel) {
            editText.deleteText(startSel, endSel);
          }
          editText.pasteText(startSel);
          break;
        case CLIPBOARD_CUT:
          if (startSel != endSel) {
            editText.cutText(startSel, endSel);
          }
          break;
      }
    }
  },

  presentCaretChange: function cc_presentCaretChange(
    aText, aOldOffset, aNewOffset) {
    if (aOldOffset !== aNewOffset) {
      let msg = Presentation.textSelectionChanged(aText, aNewOffset, aNewOffset,
        aOldOffset, aOldOffset, true);
      this._contentScope.get().sendAsyncMessage("AccessFu:Present", msg);
    }
  },

  moveCaret: function cc_moveCaret(accessible, direction, granularity) {
    let accText = accessible.QueryInterface(Ci.nsIAccessibleText);
    let oldOffset = accText.caretOffset;
    let text = accText.getText(0, accText.characterCount);

    let start = {}, end = {};
    if (direction === "Previous" && oldOffset > 0) {
      switch (granularity) {
        case MOVEMENT_GRANULARITY_CHARACTER:
          accText.caretOffset--;
          break;
        case MOVEMENT_GRANULARITY_WORD:
          accText.getTextBeforeOffset(accText.caretOffset,
            Ci.nsIAccessibleText.BOUNDARY_WORD_START, start, end);
          accText.caretOffset = end.value === accText.caretOffset ?
            start.value : end.value;
          break;
        case MOVEMENT_GRANULARITY_PARAGRAPH:
          let startOfParagraph = text.lastIndexOf("\n", accText.caretOffset - 1);
          accText.caretOffset = startOfParagraph !== -1 ? startOfParagraph : 0;
          break;
      }
    } else if (direction === "Next" && oldOffset < accText.characterCount) {
      switch (granularity) {
        case MOVEMENT_GRANULARITY_CHARACTER:
          accText.caretOffset++;
          break;
        case MOVEMENT_GRANULARITY_WORD:
          accText.getTextAtOffset(accText.caretOffset,
                                  Ci.nsIAccessibleText.BOUNDARY_WORD_END, start, end);
          accText.caretOffset = end.value;
          break;
        case MOVEMENT_GRANULARITY_PARAGRAPH:
          accText.caretOffset = text.indexOf("\n", accText.caretOffset + 1);
          break;
      }
    }

    this.presentCaretChange(text, oldOffset, accText.caretOffset);
  },

  getChildCursor: function cc_getChildCursor(aAccessible) {
    let acc = aAccessible || this.vc.position;
    if (Utils.isAliveAndVisible(acc) && acc.role === Roles.INTERNAL_FRAME) {
      let domNode = acc.DOMNode;
      let mm = this._childMessageSenders.get(domNode, null);
      if (!mm) {
        mm = Utils.getMessageManager(domNode);
        mm.addWeakMessageListener("AccessFu:MoveCursor", this);
        this._childMessageSenders.set(domNode, mm);
      }

      return mm;
    }

    return null;
  },

  sendToChild: function cc_sendToChild(aVirtualCursor, aMessage, aReplacer,
                                       aFocus) {
    let position = aVirtualCursor.position;
    let mm = this.getChildCursor(position);
    if (!mm) {
      return false;
    }

    if (aFocus) {
      position.takeFocus();
    }

    // XXX: This is a silly way to make a deep copy
    let newJSON = JSON.parse(JSON.stringify(aMessage.json));
    newJSON.origin = "parent";
    for (let attr in aReplacer) {
      newJSON[attr] = aReplacer[attr];
    }

    mm.sendAsyncMessage(aMessage.name, newJSON);
    return true;
  },

  sendToParent: function cc_sendToParent(aMessage) {
    // XXX: This is a silly way to make a deep copy
    let newJSON = JSON.parse(JSON.stringify(aMessage.json));
    newJSON.origin = "child";
    aMessage.target.sendAsyncMessage(aMessage.name, newJSON);
  },

  /**
   * Move cursor and/or present its location.
   * aOptions could have any of these fields:
   * - delay: in ms, before actual move is performed. Another autoMove call
   *    would cancel it. Useful if we want to wait for a possible trailing
   *    focus move. Default 0.
   * - noOpIfOnScreen: if accessible is alive and visible, don't do anything.
   * - forcePresent: present cursor location, whether we move or don't.
   * - moveToFocused: if there is a focused accessible move to that. This takes
   *    precedence over given anchor.
   * - moveMethod: pivot move method to use, default is 'moveNext',
   */
  autoMove: function cc_autoMove(aAnchor, aOptions = {}) {
    this.cancelAutoMove();

    let moveFunc = () => {
      let vc = this.vc;
      let acc = aAnchor;
      let rule = aOptions.onScreenOnly ?
        TraversalRules.SimpleOnScreen : TraversalRules.Simple;
      let forcePresentFunc = () => {
        if (aOptions.forcePresent) {
          this._contentScope.get().sendAsyncMessage(
            "AccessFu:Present", Presentation.pivotChanged(
              vc.position, null, Ci.nsIAccessiblePivot.REASON_NONE,
              vc.startOffset, vc.endOffset, false));
        }
      };

      if (aOptions.noOpIfOnScreen &&
        Utils.isAliveAndVisible(vc.position, true)) {
        forcePresentFunc();
        return;
      }

      if (aOptions.moveToFocused) {
        acc = Utils.AccService.getAccessibleFor(
          this.document.activeElement) || acc;
      }

      let moved = false;
      let moveMethod = aOptions.moveMethod || "moveNext"; // default is moveNext
      let moveFirstOrLast = moveMethod in ["moveFirst", "moveLast"];
      if (!moveFirstOrLast || acc) {
        // We either need next/previous or there is an anchor we need to use.
        moved = vc[moveFirstOrLast ? "moveNext" : moveMethod](rule, acc, true,
                                                              false);
      }
      if (moveFirstOrLast && !moved) {
        // We move to first/last after no anchor move happened or succeeded.
        moved = vc[moveMethod](rule, false);
      }

      let sentToChild = this.sendToChild(vc, {
        name: "AccessFu:AutoMove",
        json: {
          moveMethod: aOptions.moveMethod,
          moveToFocused: aOptions.moveToFocused,
          noOpIfOnScreen: true,
          forcePresent: true
        }
      }, null, true);

      if (!moved && !sentToChild) {
        forcePresentFunc();
      }
    };

    if (aOptions.delay) {
      this._autoMove = this.window.setTimeout(moveFunc, aOptions.delay);
    } else {
      moveFunc();
    }
  },

  cancelAutoMove: function cc_cancelAutoMove() {
    this.window.clearTimeout(this._autoMove);
    this._autoMove = 0;
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsISupportsWeakReference,
    Ci.nsIMessageListener
  ])
};
