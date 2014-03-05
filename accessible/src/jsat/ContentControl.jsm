/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, 'Services',
  'resource://gre/modules/Services.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Utils',
  'resource://gre/modules/accessibility/Utils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Logger',
  'resource://gre/modules/accessibility/Utils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Roles',
  'resource://gre/modules/accessibility/Constants.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'TraversalRules',
  'resource://gre/modules/accessibility/TraversalRules.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Presentation',
  'resource://gre/modules/accessibility/Presentation.jsm');

this.EXPORTED_SYMBOLS = ['ContentControl'];

this.ContentControl = function ContentControl(aContentScope) {
  this._contentScope = Cu.getWeakReference(aContentScope);
  this._vcCache = new WeakMap();
  this._childMessageSenders = new WeakMap();
};

this.ContentControl.prototype = {
  messagesOfInterest: ['AccessFu:MoveCursor',
                       'AccessFu:ClearCursor',
                       'AccessFu:MoveToPoint',
                       'AccessFu:AutoMove'],

  start: function ContentControl_start() {
    let cs = this._contentScope.get();
    for (let message of this.messagesOfInterest) {
      cs.addMessageListener(message, this);
    }
  },

  stop: function ContentControl_stop() {
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

  receiveMessage: function ContentControl_receiveMessage(aMessage) {
    Logger.debug(() => {
      return ['ContentControl.receiveMessage',
        this.document.location.toString(),
        JSON.stringify(aMessage.json)];
    });

    try {
      switch (aMessage.name) {
        case 'AccessFu:MoveCursor':
          this.handleMove(aMessage);
          break;
        case 'AccessFu:ClearCursor':
          this.handleClear(aMessage);
          break;
        case 'AccessFu:MoveToPoint':
          this.handleMoveToPoint(aMessage);
          break;
        case 'AccessFu:AutoMove':
          this.handleAutoMove(aMessage);
          break;
        default:
          break;
      }
    } catch (x) {
      Logger.logException(
        x, 'Error handling message: ' + JSON.stringify(aMessage.json));
    }
  },

  handleMove: function ContentControl_handleMove(aMessage) {
    let origin = aMessage.json.origin;
    let action = aMessage.json.action;
    let vc = this.vc;

    if (origin != 'child' && this.sendToChild(vc, aMessage)) {
      // Forwarded succesfully to child cursor.
      return;
    }

    let moved = vc[action](TraversalRules[aMessage.json.rule]);

    if (moved) {
      if (origin === 'child') {
        // We just stepped out of a child, clear child cursor.
        Utils.getMessageManager(aMessage.target).sendAsyncMessage(
          'AccessFu:ClearCursor', {});
      } else {
        // We potentially landed on a new child cursor. If so, we want to
        // either be on the first or last item in the child doc.
        let childAction = action;
        if (action === 'moveNext') {
          childAction = 'moveFirst';
        } else if (action === 'movePrevious') {
          childAction = 'moveLast';
        }

        // Attempt to forward move to a potential child cursor in our
        // new position.
        this.sendToChild(vc, aMessage, { action: childAction});
      }
    } else if (!this._childMessageSenders.has(aMessage.target)) {
      // We failed to move, and the message is not from a child, so forward
      // to parent.
      this.sendToParent(aMessage);
    }
  },

  handleMoveToPoint: function ContentControl_handleMoveToPoint(aMessage) {
    let [x, y] = [aMessage.json.x, aMessage.json.y];
    let rule = TraversalRules[aMessage.json.rule];
    let vc = this.vc;
    let win = this.window;

    let dpr = win.devicePixelRatio;
    this.vc.moveToPoint(rule, x * dpr, y * dpr, true);

    let delta = Utils.isContentProcess ?
      { x: x - win.mozInnerScreenX, y: y - win.mozInnerScreenY } : {};
    this.sendToChild(vc, aMessage, delta);
  },

  handleClear: function ContentControl_handleClear(aMessage) {
    this.sendToChild(this.vc, aMessage);
    this.vc.position = null;
  },

  handleAutoMove: function ContentControl_handleAutoMove(aMessage) {
    this.autoMove(null, aMessage.json);
  },

  getChildCursor: function ContentControl_getChildCursor(aAccessible) {
    let acc = aAccessible || this.vc.position;
    if (Utils.isAliveAndVisible(acc) && acc.role === Roles.INTERNAL_FRAME) {
      let domNode = acc.DOMNode;
      let mm = this._childMessageSenders.get(domNode, null);
      if (!mm) {
        mm = Utils.getMessageManager(domNode);
        mm.addWeakMessageListener('AccessFu:MoveCursor', this);
        this._childMessageSenders.set(domNode, mm);
      }

      return mm;
    }

    return null;
  },

  sendToChild: function ContentControl_sendToChild(aVirtualCursor,
                                                   aMessage,
                                                   aReplacer) {
    let mm = this.getChildCursor(aVirtualCursor.position);
    if (!mm) {
      return false;
    }

    // XXX: This is a silly way to make a deep copy
    let newJSON = JSON.parse(JSON.stringify(aMessage.json));
    newJSON.origin = 'parent';
    for (let attr in aReplacer) {
      newJSON[attr] = aReplacer[attr];
    }

    mm.sendAsyncMessage(aMessage.name, newJSON);
    return true;
  },

  sendToParent: function ContentControl_sendToParent(aMessage) {
    // XXX: This is a silly way to make a deep copy
    let newJSON = JSON.parse(JSON.stringify(aMessage.json));
    newJSON.origin = 'child';
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
  autoMove: function ContentControl_autoMove(aAnchor, aOptions = {}) {
    let win = this.window;
    win.clearTimeout(this._autoMove);

    let moveFunc = () => {
      let vc = this.vc;
      let acc = aAnchor;
      let rule = aOptions.onScreenOnly ?
        TraversalRules.SimpleOnScreen : TraversalRules.Simple;
      let forcePresentFunc = () => {
        if (aOptions.forcePresent) {
          this._contentScope.get().sendAsyncMessage(
            'AccessFu:Present', Presentation.pivotChanged(
              vc.position, null, Ci.nsIAccessiblePivot.REASON_NONE,
              vc.startOffset, vc.endOffset));
        }
      };

      if (aOptions.noOpIfOnScreen &&
        Utils.isAliveAndVisible(vc.position, true)) {
        forcePresentFunc();
        return;
      }

      if (aOptions.moveToFocused) {
        acc = Utils.AccRetrieval.getAccessibleFor(
          this.document.activeElement) || acc;
      }

      let moved = false;
      let moveMethod = aOptions.moveMethod || 'moveNext'; // default is moveNext
      let moveFirstOrLast = moveMethod in ['moveFirst', 'moveLast'];
      if (!moveFirstOrLast || acc) {
        // We either need next/previous or there is an anchor we need to use.
        moved = vc[moveFirstOrLast ? 'moveNext' : moveMethod](rule, acc, true);
      }
      if (moveFirstOrLast && !moved) {
        // We move to first/last after no anchor move happened or succeeded.
        moved = vc[moveMethod](rule);
      }

      let sentToChild = this.sendToChild(vc, {
        name: 'AccessFu:AutoMove',
        json: aOptions
      });

      if (!moved && !sentToChild) {
        forcePresentFunc();
      }
    };

    if (aOptions.delay) {
      this._autoMove = win.setTimeout(moveFunc, aOptions.delay);
    } else {
      moveFunc();
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference,
    Ci.nsIMessageListener
  ])
};