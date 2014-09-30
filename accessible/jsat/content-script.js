/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Logger',
  'resource://gre/modules/accessibility/Utils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Presentation',
  'resource://gre/modules/accessibility/Presentation.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'TraversalRules',
  'resource://gre/modules/accessibility/TraversalRules.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Utils',
  'resource://gre/modules/accessibility/Utils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'EventManager',
  'resource://gre/modules/accessibility/EventManager.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'ContentControl',
  'resource://gre/modules/accessibility/ContentControl.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Roles',
  'resource://gre/modules/accessibility/Constants.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'States',
  'resource://gre/modules/accessibility/Constants.jsm');

Logger.debug('content-script.js');

let eventManager = null;
let contentControl = null;

function forwardToParent(aMessage) {
  // XXX: This is a silly way to make a deep copy
  let newJSON = JSON.parse(JSON.stringify(aMessage.json));
  newJSON.origin = 'child';
  sendAsyncMessage(aMessage.name, newJSON);
}

function forwardToChild(aMessage, aListener, aVCPosition) {
  let acc = aVCPosition || Utils.getVirtualCursor(content.document).position;

  if (!Utils.isAliveAndVisible(acc) || acc.role != Roles.INTERNAL_FRAME) {
    return false;
  }

  Logger.debug(() => {
    return ['forwardToChild', Logger.accessibleToString(acc),
            aMessage.name, JSON.stringify(aMessage.json, null, '  ')];
  });

  let mm = Utils.getMessageManager(acc.DOMNode);

  if (aListener) {
    mm.addMessageListener(aMessage.name, aListener);
  }

  // XXX: This is a silly way to make a deep copy
  let newJSON = JSON.parse(JSON.stringify(aMessage.json));
  newJSON.origin = 'parent';
  if (Utils.isContentProcess) {
    // XXX: OOP content's screen offset is 0,
    // so we remove the real screen offset here.
    newJSON.x -= content.mozInnerScreenX;
    newJSON.y -= content.mozInnerScreenY;
  }
  mm.sendAsyncMessage(aMessage.name, newJSON);
  return true;
}

function activateContextMenu(aMessage) {
  let position = Utils.getVirtualCursor(content.document).position;
  if (!forwardToChild(aMessage, activateContextMenu, position)) {
    sendAsyncMessage('AccessFu:ActivateContextMenu',
      { bounds: Utils.getBounds(position, true) });
  }
}

function presentCaretChange(aText, aOldOffset, aNewOffset) {
  if (aOldOffset !== aNewOffset) {
    let msg = Presentation.textSelectionChanged(aText, aNewOffset, aNewOffset,
                                                aOldOffset, aOldOffset, true);
    sendAsyncMessage('AccessFu:Present', msg);
  }
}

function scroll(aMessage) {
  let position = Utils.getVirtualCursor(content.document).position;
  if (!forwardToChild(aMessage, scroll, position)) {
    sendAsyncMessage('AccessFu:DoScroll',
                     { bounds: Utils.getBounds(position, true),
                       page: aMessage.json.page,
                       horizontal: aMessage.json.horizontal });
  }
}

function adjustRange(aMessage) {
  function sendUpDownKey(aAccessible) {
    let acc = Utils.getEmbeddedControl(aAccessible) || aAccessible;
    let elem = acc.DOMNode;
    if (elem) {
      if (elem.tagName === 'INPUT' && elem.type === 'range') {
        elem[aMessage.json.direction === 'forward' ? 'stepDown' : 'stepUp']();
        let changeEvent = content.document.createEvent('UIEvent');
        changeEvent.initEvent('change', true, true);
        elem.dispatchEvent(changeEvent);
      } else {
        let evt = content.document.createEvent('KeyboardEvent');
        let keycode = aMessage.json.direction == 'forward' ?
              content.KeyEvent.DOM_VK_DOWN : content.KeyEvent.DOM_VK_UP;
        evt.initKeyEvent(
          "keypress", false, true, null, false, false, false, false, keycode, 0);
        elem.dispatchEvent(evt);
      }
    }
  }

  let position = Utils.getVirtualCursor(content.document).position;
  if (!forwardToChild(aMessage, adjustRange, position)) {
    sendUpDownKey(position);
  }
}
addMessageListener(
  'AccessFu:Start',
  function(m) {
    if (m.json.logLevel) {
      Logger.logLevel = Logger[m.json.logLevel];
    }

    Logger.debug('AccessFu:Start');
    if (m.json.buildApp)
      Utils.MozBuildApp = m.json.buildApp;

    addMessageListener('AccessFu:ContextMenu', activateContextMenu);
    addMessageListener('AccessFu:Scroll', scroll);
    addMessageListener('AccessFu:AdjustRange', adjustRange);

    if (!contentControl) {
      contentControl = new ContentControl(this);
    }
    contentControl.start();

    if (!eventManager) {
      eventManager = new EventManager(this, contentControl);
    }
    eventManager.inTest = m.json.inTest;
    eventManager.start();

    function contentStarted() {
      let accDoc = Utils.AccRetrieval.getAccessibleFor(content.document);
      if (accDoc && !Utils.getState(accDoc).contains(States.BUSY)) {
        sendAsyncMessage('AccessFu:ContentStarted');
      } else {
        content.setTimeout(contentStarted, 0);
      }
    }

    if (m.json.inTest) {
      // During a test we want to wait for the document to finish loading for
      // consistency.
      contentStarted();
    }
  });

addMessageListener(
  'AccessFu:Stop',
  function(m) {
    Logger.debug('AccessFu:Stop');

    removeMessageListener('AccessFu:ContextMenu', activateContextMenu);
    removeMessageListener('AccessFu:Scroll', scroll);

    eventManager.stop();
    contentControl.stop();
  });

sendAsyncMessage('AccessFu:Ready');
