/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");
const Services = require("Services");
const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { accessibleWalkerSpec } = require("devtools/shared/specs/accessibility");

loader.lazyRequireGetter(this, "AccessibleActor", "devtools/server/actors/accessibility/accessible", true);
loader.lazyRequireGetter(this, "CustomHighlighterActor", "devtools/server/actors/highlighters", true);
loader.lazyRequireGetter(this, "DevToolsUtils", "devtools/shared/DevToolsUtils");
loader.lazyRequireGetter(this, "events", "devtools/shared/event-emitter");
loader.lazyRequireGetter(this, "getCurrentZoom", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "InspectorUtils", "InspectorUtils");
loader.lazyRequireGetter(this, "isDefunct", "devtools/server/actors/utils/accessibility", true);
loader.lazyRequireGetter(this, "isTypeRegistered", "devtools/server/actors/highlighters", true);
loader.lazyRequireGetter(this, "isWindowIncluded", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "isXUL", "devtools/server/actors/highlighters/utils/markup", true);
loader.lazyRequireGetter(this, "loadSheet", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "register", "devtools/server/actors/highlighters", true);
loader.lazyRequireGetter(this, "removeSheet", "devtools/shared/layout/utils", true);

const kStateHover = 0x00000004; // NS_EVENT_STATE_HOVER

const HIGHLIGHTER_STYLES_SHEET = `data:text/css;charset=utf-8,
* {
  transition: none !important;
}

:-moz-devtools-highlighted {
  color: transparent !important;
  text-shadow: none !important;
}`;

const nsIAccessibleEvent = Ci.nsIAccessibleEvent;
const nsIAccessibleStateChangeEvent = Ci.nsIAccessibleStateChangeEvent;
const nsIAccessibleRole = Ci.nsIAccessibleRole;

const {
  EVENT_TEXT_CHANGED,
  EVENT_TEXT_INSERTED,
  EVENT_TEXT_REMOVED,
  EVENT_ACCELERATOR_CHANGE,
  EVENT_ACTION_CHANGE,
  EVENT_DEFACTION_CHANGE,
  EVENT_DESCRIPTION_CHANGE,
  EVENT_DOCUMENT_ATTRIBUTES_CHANGED,
  EVENT_HIDE,
  EVENT_NAME_CHANGE,
  EVENT_OBJECT_ATTRIBUTE_CHANGED,
  EVENT_REORDER,
  EVENT_STATE_CHANGE,
  EVENT_TEXT_ATTRIBUTE_CHANGED,
  EVENT_VALUE_CHANGE,
} = nsIAccessibleEvent;

// TODO: We do not need this once bug 1422913 is fixed. We also would not need
// to fire a name change event for an accessible that has an updated subtree and
// that has its name calculated from the said subtree.
const NAME_FROM_SUBTREE_RULE_ROLES = new Set([
  nsIAccessibleRole.ROLE_BUTTONDROPDOWN,
  nsIAccessibleRole.ROLE_BUTTONDROPDOWNGRID,
  nsIAccessibleRole.ROLE_BUTTONMENU,
  nsIAccessibleRole.ROLE_CELL,
  nsIAccessibleRole.ROLE_CHECKBUTTON,
  nsIAccessibleRole.ROLE_CHECK_MENU_ITEM,
  nsIAccessibleRole.ROLE_CHECK_RICH_OPTION,
  nsIAccessibleRole.ROLE_COLUMN,
  nsIAccessibleRole.ROLE_COLUMNHEADER,
  nsIAccessibleRole.ROLE_COMBOBOX_OPTION,
  nsIAccessibleRole.ROLE_DEFINITION,
  nsIAccessibleRole.ROLE_GRID_CELL,
  nsIAccessibleRole.ROLE_HEADING,
  nsIAccessibleRole.ROLE_HELPBALLOON,
  nsIAccessibleRole.ROLE_HTML_CONTAINER,
  nsIAccessibleRole.ROLE_KEY,
  nsIAccessibleRole.ROLE_LABEL,
  nsIAccessibleRole.ROLE_LINK,
  nsIAccessibleRole.ROLE_LISTITEM,
  nsIAccessibleRole.ROLE_MATHML_IDENTIFIER,
  nsIAccessibleRole.ROLE_MATHML_NUMBER,
  nsIAccessibleRole.ROLE_MATHML_OPERATOR,
  nsIAccessibleRole.ROLE_MATHML_TEXT,
  nsIAccessibleRole.ROLE_MATHML_STRING_LITERAL,
  nsIAccessibleRole.ROLE_MATHML_GLYPH,
  nsIAccessibleRole.ROLE_MENUITEM,
  nsIAccessibleRole.ROLE_OPTION,
  nsIAccessibleRole.ROLE_OUTLINEITEM,
  nsIAccessibleRole.ROLE_PAGETAB,
  nsIAccessibleRole.ROLE_PARENT_MENUITEM,
  nsIAccessibleRole.ROLE_PUSHBUTTON,
  nsIAccessibleRole.ROLE_RADIOBUTTON,
  nsIAccessibleRole.ROLE_RADIO_MENU_ITEM,
  nsIAccessibleRole.ROLE_RICH_OPTION,
  nsIAccessibleRole.ROLE_ROW,
  nsIAccessibleRole.ROLE_ROWHEADER,
  nsIAccessibleRole.ROLE_SUMMARY,
  nsIAccessibleRole.ROLE_SWITCH,
  nsIAccessibleRole.ROLE_TABLE_COLUMN_HEADER,
  nsIAccessibleRole.ROLE_TABLE_ROW_HEADER,
  nsIAccessibleRole.ROLE_TEAR_OFF_MENU_ITEM,
  nsIAccessibleRole.ROLE_TERM,
  nsIAccessibleRole.ROLE_TOGGLE_BUTTON,
  nsIAccessibleRole.ROLE_TOOLTIP,
]);

const IS_OSX = Services.appinfo.OS === "Darwin";

/**
 * Helper function that determines if nsIAccessible object is in stale state. When an
 * object is stale it means its subtree is not up to date.
 *
 * @param  {nsIAccessible}  accessible
 *         object to be tested.
 * @return {Boolean}
 *         True if accessible object is stale, false otherwise.
 */
function isStale(accessible) {
  const extraState = {};
  accessible.getState({}, extraState);
  // extraState.value is a bitmask. We are applying bitwise AND to mask out
  // irrelevant states.
  return !!(extraState.value & Ci.nsIAccessibleStates.EXT_STATE_STALE);
}

/**
 * The AccessibleWalkerActor stores a cache of AccessibleActors that represent
 * accessible objects in a given document.
 *
 * It is also responsible for implicitely initializing and shutting down
 * accessibility engine by storing a reference to the XPCOM accessibility
 * service.
 */
const AccessibleWalkerActor = ActorClassWithSpec(accessibleWalkerSpec, {
  initialize(conn, targetActor) {
    Actor.prototype.initialize.call(this, conn);
    this.targetActor = targetActor;
    this.refMap = new Map();
    this.setA11yServiceGetter();
    this.onPick = this.onPick.bind(this);
    this.onHovered = this.onHovered.bind(this);
    this._preventContentEvent = this._preventContentEvent.bind(this);
    this.onKey = this.onKey.bind(this);
    this.onHighlighterEvent = this.onHighlighterEvent.bind(this);
  },

  get highlighter() {
    if (!this._highlighter) {
      if (isXUL(this.rootWin)) {
        if (!isTypeRegistered("XULWindowAccessibleHighlighter")) {
          register("XULWindowAccessibleHighlighter", "xul-accessible");
        }

        this._highlighter = CustomHighlighterActor(this,
                                                   "XULWindowAccessibleHighlighter");
      } else {
        if (!isTypeRegistered("AccessibleHighlighter")) {
          register("AccessibleHighlighter", "accessible");
        }

        this._highlighter = CustomHighlighterActor(this, "AccessibleHighlighter");
      }

      this.manage(this._highlighter);
      this._highlighter.on("highlighter-event", this.onHighlighterEvent);
    }

    return this._highlighter;
  },

  setA11yServiceGetter() {
    DevToolsUtils.defineLazyGetter(this, "a11yService", () => {
      Services.obs.addObserver(this, "accessible-event");
      return Cc["@mozilla.org/accessibilityService;1"].getService(
        Ci.nsIAccessibilityService);
    });
  },

  get rootWin() {
    return this.targetActor && this.targetActor.window;
  },

  get rootDoc() {
    return this.targetActor && this.targetActor.window.document;
  },

  reset() {
    try {
      Services.obs.removeObserver(this, "accessible-event");
    } catch (e) {
      // Accessible event observer might not have been initialized if a11y
      // service was never used.
    }

    this.cancelPick();

    // Clean up accessible actors cache.
    if (this.refMap.size > 0) {
      try {
        if (this.rootDoc) {
          this.purgeSubtree(this.getRawAccessibleFor(this.rootDoc),
                            this.rootDoc);
        }
      } catch (e) {
        // Accessibility service might be already destroyed.
      } finally {
        this.refMap.clear();
      }
    }

    this._childrenPromise = null;
    delete this.a11yService;
    this.setA11yServiceGetter();
  },

  destroy() {
    Actor.prototype.destroy.call(this);

    this.reset();

    if (this._highlighter) {
      this._highlighter.off("highlighter-event", this.onHighlighterEvent);
      this._highlighter = null;
    }

    this.targetActor = null;
    this.refMap = null;
  },

  getRef(rawAccessible) {
    return this.refMap.get(rawAccessible);
  },

  addRef(rawAccessible) {
    let actor = this.refMap.get(rawAccessible);
    if (actor) {
      return actor;
    }

    actor = new AccessibleActor(this, rawAccessible);
    this.manage(actor);
    this.refMap.set(rawAccessible, actor);

    return actor;
  },

  /**
   * Clean up accessible actors cache for a given accessible's subtree.
   *
   * @param  {null|nsIAccessible} rawAccessible
   * @param  {null|Object}   rawNode
   */
  purgeSubtree(rawAccessible, rawNode) {
    if (!rawAccessible) {
      return;
    }

    const actor = this.getRef(rawAccessible);
    if (actor && rawAccessible && !actor.isDefunct) {
      for (let child = rawAccessible.firstChild; child; child = child.nextSibling) {
        this.purgeSubtree(child);
      }
    }

    this.refMap.delete(rawAccessible);

    if (actor) {
      events.emit(this, "accessible-destroy", actor);
      actor.destroy();
    }

    // If corresponding DOMNode is a top level document, clear entire cache.
    if (rawNode && rawNode === this.rootDoc) {
      this.refMap.clear();
    }
  },

  /**
   * A helper method. Accessibility walker is assumed to have only 1 child which
   * is the top level document.
   */
  async children() {
    if (this._childrenPromise) {
      return this._childrenPromise;
    }

    this._childrenPromise = Promise.all([this.getDocument()]);
    const children = await this._childrenPromise;
    this._childrenPromise = null;
    return children;
  },

  /**
   * A promise for a root document accessible actor that only resolves when its
   * corresponding document accessible object is fully loaded.
   *
   * @return {Promise}
   */
  getDocument() {
    if (!this.rootDoc || !this.rootDoc.documentElement) {
      return this.once("document-ready").then(docAcc => this.addRef(docAcc));
    }

    if (isXUL(this.rootWin)) {
      const doc = this.addRef(this.getRawAccessibleFor(this.rootDoc));
      return Promise.resolve(doc);
    }

    const doc = this.getRawAccessibleFor(this.rootDoc);
    if (isStale(doc)) {
      return this.once("document-ready").then(docAcc => this.addRef(docAcc));
    }

    return Promise.resolve(this.addRef(doc));
  },

  /**
   * Get an accessible actor for a domnode actor.
   * @param  {Object} domNode
   *         domnode actor for which accessible actor is being created.
   * @return {Promse}
   *         A promise that resolves when accessible actor is created for a
   *         domnode actor.
   */
  getAccessibleFor(domNode) {
    // We need to make sure that the document is loaded processed by a11y first.
    return this.getDocument().then(() =>
      this.addRef(this.getRawAccessibleFor(domNode.rawNode)));
  },

  /**
   * Get a raw accessible object for a raw node.
   * @param  {DOMNode} rawNode
   *         Raw node for which accessible object is being retrieved.
   * @return {nsIAccessible}
   *         Accessible object for a given DOMNode.
   */
  getRawAccessibleFor(rawNode) {
    // Accessible can only be retrieved iff accessibility service is enabled.
    if (!Services.appinfo.accessibilityEnabled) {
      return null;
    }

    return this.a11yService.getAccessibleFor(rawNode);
  },

  async getAncestry(accessible) {
    if (!accessible || accessible.indexInParent === -1) {
      return [];
    }
    const doc = await this.getDocument();
    const ancestry = [];
    if (accessible === doc) {
      return ancestry;
    }

    try {
      let parent = accessible;
      while (parent && (parent = parent.parentAcc) && parent != doc) {
        ancestry.push(parent);
      }
      ancestry.push(doc);
    } catch (error) {
      throw new Error(`Failed to get ancestor for ${accessible}: ${error}`);
    }

    return ancestry.map(parent => (
      { accessible: parent, children: parent.children() }));
  },

  onHighlighterEvent: function(data) {
    this.emit("highlighter-event", data);
  },

  /**
   * Accessible event observer function.
   *
   * @param {nsIAccessibleEvent} subject
   *                                      accessible event object.
   */
  observe(subject) {
    const event = subject.QueryInterface(nsIAccessibleEvent);
    const rawAccessible = event.accessible;
    const accessible = this.getRef(rawAccessible);

    if ((rawAccessible instanceof Ci.nsIAccessibleDocument) && !accessible) {
      const rootDocAcc = this.getRawAccessibleFor(this.rootDoc);
      if (rawAccessible === rootDocAcc && !isStale(rawAccessible)) {
        this.purgeSubtree(rawAccessible, event.DOMNode);
        // If it's a top level document notify listeners about the document
        // being ready.
        events.emit(this, "document-ready", rawAccessible);
      }
    }

    switch (event.eventType) {
      case EVENT_STATE_CHANGE:
        const { state, isEnabled } = event.QueryInterface(nsIAccessibleStateChangeEvent);
        const isBusy = state & Ci.nsIAccessibleStates.STATE_BUSY;
        if (accessible) {
          // Only propagate state change events for active accessibles.
          if (isBusy && isEnabled) {
            if (rawAccessible instanceof Ci.nsIAccessibleDocument) {
              // Remove its existing cache from tree.
              this.purgeSubtree(rawAccessible, event.DOMNode);
            }
            return;
          }
          events.emit(accessible, "states-change", accessible.states);
        }

        break;
      case EVENT_NAME_CHANGE:
        if (accessible) {
          events.emit(accessible, "name-change", rawAccessible.name,
            event.DOMNode == this.rootDoc ?
              undefined : this.getRef(rawAccessible.parent), this);
        }
        break;
      case EVENT_VALUE_CHANGE:
        if (accessible) {
          events.emit(accessible, "value-change", rawAccessible.value);
        }
        break;
      case EVENT_DESCRIPTION_CHANGE:
        if (accessible) {
          events.emit(accessible, "description-change", rawAccessible.description);
        }
        break;
      case EVENT_REORDER:
        if (accessible) {
          accessible.children().forEach(child =>
            events.emit(child, "index-in-parent-change", child.indexInParent));
          events.emit(accessible, "reorder", rawAccessible.childCount, this);
        }
        break;
      case EVENT_HIDE:
        this.purgeSubtree(rawAccessible);
        break;
      case EVENT_DEFACTION_CHANGE:
      case EVENT_ACTION_CHANGE:
        if (accessible) {
          events.emit(accessible, "actions-change", accessible.actions);
        }
        break;
      case EVENT_TEXT_CHANGED:
      case EVENT_TEXT_INSERTED:
      case EVENT_TEXT_REMOVED:
        if (accessible) {
          events.emit(accessible, "text-change", this);
          if (NAME_FROM_SUBTREE_RULE_ROLES.has(rawAccessible.role)) {
            events.emit(accessible, "name-change", rawAccessible.name,
              event.DOMNode == this.rootDoc ?
                undefined : this.getRef(rawAccessible.parent), this);
          }
        }
        break;
      case EVENT_DOCUMENT_ATTRIBUTES_CHANGED:
      case EVENT_OBJECT_ATTRIBUTE_CHANGED:
      case EVENT_TEXT_ATTRIBUTE_CHANGED:
        if (accessible) {
          events.emit(accessible, "attributes-change", accessible.attributes);
        }
        break;
      // EVENT_ACCELERATOR_CHANGE is currently not fired by gecko accessibility.
      case EVENT_ACCELERATOR_CHANGE:
        if (accessible) {
          events.emit(accessible, "shortcut-change", accessible.keyboardShortcut);
        }
        break;
      default:
        break;
    }
  },

  /**
   * Public method used to show an accessible object highlighter on the client
   * side.
   *
   * @param  {Object} accessible
   *         AccessibleActor to be highlighted.
   * @param  {Object} options
   *         Object used for passing options. Available options:
   *         - duration {Number}
   *                    Duration of time that the highlighter should be shown.
   * @return {Boolean}
   *         True if highlighter shows the accessible object.
   */
  highlightAccessible(accessible, options = {}) {
    this.unhighlight();
    const { bounds } = accessible;
    if (!bounds) {
      return false;
    }

    // Disable potential mouse driven transitions (This is important because accessibility
    // highlighter temporarily modifies text color related CSS properties. In case where
    // there are transitions that affect them, there might be unexpected side effects when
    // taking a snapshot for contrast measurement)
    const { DOMNode: rawNode } = accessible.rawAccessible;
    const win = rawNode.ownerGlobal;
    loadSheet(win, HIGHLIGHTER_STYLES_SHEET);
    const { audit, name, role } = accessible;
    const shown = this.highlighter.show({ rawNode },
                                        { ...options, ...bounds, name, role, audit });
    // Re-enable transitions.
    removeSheet(win, HIGHLIGHTER_STYLES_SHEET);
    return shown;
  },

  /**
   * Public method used to hide an accessible object highlighter on the client
   * side.
   */
  unhighlight() {
    if (!this._highlighter) {
      return;
    }

    this.highlighter.hide();
  },

  /**
   * Picking state that indicates if picking is currently enabled and, if so,
   * what the current and hovered accessible objects are.
   */
  _isPicking: false,
  _currentAccessible: null,

  /**
   * Check is event handling is allowed.
   */
  _isEventAllowed: function({ view }) {
    return this.rootWin instanceof Ci.nsIDOMChromeWindow ||
           isWindowIncluded(this.rootWin, view);
  },

  _preventContentEvent(event) {
    event.stopPropagation();
    event.preventDefault();

    const target = event.originalTarget || event.target;
    if (target !== this._currentTarget) {
      this._resetStateAndReleaseTarget();
      this._currentTarget = target;
      // We use InspectorUtils to save the original hover content state of the target
      // element (that includes its hover state). In order to not trigger any visual
      // changes to the element that depend on its hover state we remove the state while
      // the element is the most current target of the highlighter.
      //
      // TODO: This logic can be removed if/when we can use elementsAtPoint API for
      // determining topmost DOMNode that corresponds to specific coordinates. We would
      // then be able to use a highlighter overlay that would prevent all pointer events
      // to content but still render highlighter for the node/element correctly.
      this._currentTargetHoverState =
        InspectorUtils.getContentState(target) & kStateHover;
      InspectorUtils.removeContentState(target, kStateHover);
    }
  },

  /**
   * Click event handler for when picking is enabled.
   *
   * @param  {Object} event
   *         Current click event.
   */
  onPick(event) {
    if (!this._isPicking) {
      return;
    }

    this._preventContentEvent(event);
    if (!this._isEventAllowed(event)) {
      return;
    }

    // If shift is pressed, this is only a preview click, send the event to
    // the client, but don't stop picking.
    if (event.shiftKey) {
      if (!this._currentAccessible) {
        this._currentAccessible = this._findAndAttachAccessible(event);
      }
      events.emit(this, "picker-accessible-previewed", this._currentAccessible);
      return;
    }

    this._unsetPickerEnvironment();
    this._isPicking = false;
    if (!this._currentAccessible) {
      this._currentAccessible = this._findAndAttachAccessible(event);
    }
    events.emit(this, "picker-accessible-picked", this._currentAccessible);
  },

  /**
   * Hover event handler for when picking is enabled.
   *
   * @param  {Object} event
   *         Current hover event.
   */
  onHovered(event) {
    if (!this._isPicking) {
      return;
    }

    this._preventContentEvent(event);
    if (!this._isEventAllowed(event)) {
      return;
    }

    const accessible = this._findAndAttachAccessible(event);
    if (!accessible) {
      return;
    }

    if (this._currentAccessible !== accessible) {
      this.highlightAccessible(accessible);
      events.emit(this, "picker-accessible-hovered", accessible);
      this._currentAccessible = accessible;
    }
  },

  /**
   * Keyboard event handler for when picking is enabled.
   *
   * @param  {Object} event
   *         Current keyboard event.
   */
  onKey(event) {
    if (!this._currentAccessible || !this._isPicking) {
      return;
    }

    this._preventContentEvent(event);
    if (!this._isEventAllowed(event)) {
      return;
    }

    /**
     * KEY: Action/scope
     * ENTER/CARRIAGE_RETURN: Picks current accessible
     * ESC/CTRL+SHIFT+C: Cancels picker
     */
    switch (event.keyCode) {
      // Select the element.
      case event.DOM_VK_RETURN:
        this._onPick(event);
        break;
      // Cancel pick mode.
      case event.DOM_VK_ESCAPE:
        this.cancelPick();
        events.emit(this, "picker-accessible-canceled");
        break;
      case event.DOM_VK_C:
        if ((IS_OSX && event.metaKey && event.altKey) ||
          (!IS_OSX && event.ctrlKey && event.shiftKey)) {
          this.cancelPick();
          events.emit(this, "picker-accessible-canceled");
        }
        break;
      default:
        break;
    }
  },

  /**
   * Picker method that starts picker content listeners.
   */
  pick: function() {
    if (!this._isPicking) {
      this._isPicking = true;
      this._setPickerEnvironment();
    }
  },

  /**
   * This pick method also focuses the highlighter's target window.
   */
  pickAndFocus: function() {
    this.pick();
    this.rootWin.focus();
  },

  attachAccessible(rawAccessible, accessibleDocument) {
    // If raw accessible object is defunct or detached, no need to cache it and
    // its ancestry.
    if (!rawAccessible || isDefunct(rawAccessible) || rawAccessible.indexInParent < 0) {
      return null;
    }

    const accessible = this.addRef(rawAccessible);
    // There is a chance that ancestry lookup can fail if the accessible is in
    // the detached subtree. At that point the root accessible object would be
    // defunct and accessing it via parent property will throw.
    try {
      let parent = accessible;
      while (parent && parent.rawAccessible != accessibleDocument) {
        parent = parent.parentAcc;
      }
    } catch (error) {
      throw new Error(`Failed to get ancestor for ${accessible}: ${error}`);
    }

    return accessible;
  },

  /**
   * When RDM is used, users can set custom DPR values that are different from the device
   * they are using. Store true screenPixelsPerCSSPixel value to be able to use accessible
   * highlighter features correctly.
   */
  get pixelRatio() {
    const { contentViewer } = this.targetActor.docShell;
    const { windowUtils } = this.rootWin;
    const overrideDPPX = contentViewer.overrideDPPX;
    let ratio;
    if (overrideDPPX) {
      contentViewer.overrideDPPX = 0;
      ratio = windowUtils.screenPixelsPerCSSPixel;
      contentViewer.overrideDPPX = overrideDPPX;
    } else {
      ratio = windowUtils.screenPixelsPerCSSPixel;
    }

    return ratio;
  },

  /**
   * Find deepest accessible object that corresponds to the screen coordinates of the
   * mouse pointer and attach it to the AccessibilityWalker tree.
   *
   * @param  {Object} event
   *         Correspoinding content event.
   * @return {null|Object}
   *         Accessible object, if available, that corresponds to a DOM node.
   */
  _findAndAttachAccessible(event) {
    const target = event.originalTarget || event.target;
    const docAcc = this.getRawAccessibleFor(this.rootDoc);
    const win = target.ownerGlobal;
    const scale = this.pixelRatio / getCurrentZoom(win);
    const rawAccessible = docAcc.getDeepestChildAtPoint(
      event.screenX * scale,
      event.screenY * scale);
    return this.attachAccessible(rawAccessible, docAcc);
  },

  /**
   * Start picker content listeners.
   */
  _setPickerEnvironment: function() {
    const target = this.targetActor.chromeEventHandler;
    target.addEventListener("mousemove", this.onHovered, true);
    target.addEventListener("click", this.onPick, true);
    target.addEventListener("mousedown", this._preventContentEvent, true);
    target.addEventListener("mouseup", this._preventContentEvent, true);
    target.addEventListener("mouseover", this._preventContentEvent, true);
    target.addEventListener("mouseout", this._preventContentEvent, true);
    target.addEventListener("mouseleave", this._preventContentEvent, true);
    target.addEventListener("mouseenter", this._preventContentEvent, true);
    target.addEventListener("dblclick", this._preventContentEvent, true);
    target.addEventListener("keydown", this.onKey, true);
    target.addEventListener("keyup", this._preventContentEvent, true);
  },

  /**
   * If content is still alive, stop picker content listeners, reset the hover state for
   * last target element.
   */
  _unsetPickerEnvironment: function() {
    const target = this.targetActor.chromeEventHandler;

    if (!target) {
      return;
    }

    target.removeEventListener("mousemove", this.onHovered, true);
    target.removeEventListener("click", this.onPick, true);
    target.removeEventListener("mousedown", this._preventContentEvent, true);
    target.removeEventListener("mouseup", this._preventContentEvent, true);
    target.removeEventListener("mouseover", this._preventContentEvent, true);
    target.removeEventListener("mouseout", this._preventContentEvent, true);
    target.removeEventListener("mouseleave", this._preventContentEvent, true);
    target.removeEventListener("mouseenter", this._preventContentEvent, true);
    target.removeEventListener("dblclick", this._preventContentEvent, true);
    target.removeEventListener("keydown", this.onKey, true);
    target.removeEventListener("keyup", this._preventContentEvent, true);

    this._resetStateAndReleaseTarget();
  },

  /**
   * When using accessibility highlighter, we keep track of the most current event pointer
   * event target. In order to update or release the target, we need to make sure we set
   * the content state (using InspectorUtils) to its original value.
   *
   * TODO: This logic can be removed if/when we can use elementsAtPoint API for
   * determining topmost DOMNode that corresponds to specific coordinates. We would then
   * be able to use a highlighter overlay that would prevent all pointer events to content
   * but still render highlighter for the node/element correctly.
   */
  _resetStateAndReleaseTarget() {
    if (!this._currentTarget) {
      return;
    }

    try {
      if (this._currentTargetHoverState) {
        InspectorUtils.setContentState(this._currentTarget, kStateHover);
      }
    } catch (e) {
      // DOMNode is already dead.
    }

    this._currentTarget = null;
    this._currentTargetState = null;
  },

  /**
   * Cacncel picker pick. Remvoe all content listeners and hide the highlighter.
   */
  cancelPick: function() {
    this.unhighlight();

    if (this._isPicking) {
      this._unsetPickerEnvironment();
      this._isPicking = false;
      this._currentAccessible = null;
    }
  },
});

exports.AccessibleWalkerActor = AccessibleWalkerActor;
