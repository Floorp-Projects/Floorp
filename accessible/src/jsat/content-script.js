/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces;
let Cu = Components.utils;

const MOVEMENT_GRANULARITY_CHARACTER = 1;
const MOVEMENT_GRANULARITY_WORD = 2;
const MOVEMENT_GRANULARITY_PARAGRAPH = 8;

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
XPCOMUtils.defineLazyModuleGetter(this, 'Roles',
  'resource://gre/modules/accessibility/Constants.jsm');

Logger.debug('content-script.js');

let eventManager = null;

function moveCursor(aMessage) {
  if (Logger.logLevel >= Logger.DEBUG) {
    Logger.debug(aMessage.name, JSON.stringify(aMessage.json, null, ' '));
  }

  let vc = Utils.getVirtualCursor(content.document);
  let origin = aMessage.json.origin;
  let action = aMessage.json.action;
  let rule = TraversalRules[aMessage.json.rule];

  function moveCursorInner() {
    try {
      if (origin == 'parent' &&
          !Utils.isAliveAndVisible(vc.position)) {
        // We have a bad position in this frame, move vc to last or first item.
        if (action == 'moveNext') {
          return vc.moveFirst(rule);
        } else if (action == 'movePrevious') {
          return vc.moveLast(rule);
        }
      }

      return vc[action](rule);
    } catch (x) {
      if (action == 'moveNext' || action == 'movePrevious') {
        // If we are trying to move next/prev put the vc on the focused item.
        let acc = Utils.AccRetrieval.
          getAccessibleFor(content.document.activeElement);
        return vc.moveNext(rule, acc, true);
      } else {
        throw x;
      }
    }

    return false;
  }

  try {
    if (origin != 'child' &&
        forwardToChild(aMessage, moveCursor, vc.position)) {
      // We successfully forwarded the move to the child document.
      return;
    }

    if (moveCursorInner()) {
      // If we moved, try forwarding the message to the new position,
      // it may be a frame with a vc of its own.
      forwardToChild(aMessage, moveCursor, vc.position);
    } else {
      // If we did not move, we probably reached the end or start of the
      // document, go back to parent content and move us out of the iframe.
      if (origin == 'parent') {
        vc.position = null;
      }
      forwardToParent(aMessage);
    }
  } catch (x) {
    Logger.logException(x, 'Cursor move failed');
  }
}

function moveToPoint(aMessage) {
  if (Logger.logLevel >= Logger.DEBUG) {
    Logger.debug(aMessage.name, JSON.stringify(aMessage.json, null, ' '));
  }

  let vc = Utils.getVirtualCursor(content.document);
  let details = aMessage.json;
  let rule = TraversalRules[details.rule];

  try {
    let dpr = content.devicePixelRatio;
    vc.moveToPoint(rule, details.x * dpr, details.y * dpr, true);
    forwardToChild(aMessage, moveToPoint, vc.position);
  } catch (x) {
    Logger.logException(x, 'Failed move to point');
  }
}

function showCurrent(aMessage) {
  if (Logger.logLevel >= Logger.DEBUG) {
    Logger.debug(aMessage.name, JSON.stringify(aMessage.json, null, ' '));
  }

  let vc = Utils.getVirtualCursor(content.document);

  if (!forwardToChild(vc, showCurrent, aMessage)) {
    if (!vc.position && aMessage.json.move) {
      vc.moveFirst(TraversalRules.Simple);
    } else {
      sendAsyncMessage('AccessFu:Present', Presentation.pivotChanged(
                         vc.position, null, Ci.nsIAccessiblePivot.REASON_NONE,
                         vc.startOffset, vc.endOffset));
    }
  }
}

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

  if (Logger.logLevel >= Logger.DEBUG) {
    Logger.debug('forwardToChild', Logger.accessibleToString(acc),
                 aMessage.name, JSON.stringify(aMessage.json, null, '  '));
  }

  let mm = Utils.getMessageManager(acc.DOMNode);
  mm.addMessageListener(aMessage.name, aListener);
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

function activateCurrent(aMessage) {
  Logger.debug('activateCurrent');
  function activateAccessible(aAccessible) {
    try {
      if (aMessage.json.activateIfKey &&
          aAccessible.role != Roles.KEY) {
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
      let docAcc = Utils.AccRetrieval.getAccessibleFor(content.document);
      let docX = {}, docY = {}, docW = {}, docH = {};
      docAcc.getBounds(docX, docY, docW, docH);

      let objX = {}, objY = {}, objW = {}, objH = {};
      aAccessible.getBounds(objX, objY, objW, objH);

      let x = Math.round((objX.value - docX.value) + objW.value / 2);
      let y = Math.round((objY.value - docY.value) + objH.value / 2);

      let node = aAccessible.DOMNode || aAccessible.parent.DOMNode;

      function dispatchMouseEvent(aEventType) {
        let evt = content.document.createEvent('MouseEvents');
        evt.initMouseEvent(aEventType, true, true, content,
                           x, y, 0, 0, 0, false, false, false, false, 0, null);
        node.dispatchEvent(evt);
      }

      dispatchMouseEvent('mousedown');
      dispatchMouseEvent('mouseup');
    }

    if (aAccessible.role !== Roles.KEY) {
      // Keys will typically have a sound of their own.
      sendAsyncMessage('AccessFu:Present',
                       Presentation.actionInvoked(aAccessible, 'click'));
    }
  }

  function moveCaretTo(aAccessible, aOffset) {
    let accText = aAccessible.QueryInterface(Ci.nsIAccessibleText);
    let oldOffset = accText.caretOffset;
    let text = accText.getText(0, accText.characterCount);

    if (aOffset >= 0 && aOffset <= accText.characterCount) {
      accText.caretOffset = aOffset;
    }

    presentCaretChange(text, oldOffset, accText.caretOffset);
  }

  let focusedAcc = Utils.AccRetrieval.getAccessibleFor(content.document.activeElement);
  if (focusedAcc && focusedAcc.role === Roles.ENTRY) {
    moveCaretTo(focusedAcc, aMessage.json.offset);
    return;
  }

  let position = Utils.getVirtualCursor(content.document).position;
  if (!forwardToChild(aMessage, activateCurrent, position)) {
    activateAccessible(position);
  }
}

function activateContextMenu(aMessage) {
  function sendContextMenuCoordinates(aAccessible) {
    let bounds = Utils.getBounds(aAccessible);
    sendAsyncMessage('AccessFu:ActivateContextMenu', {bounds: bounds});
  }

  let position = Utils.getVirtualCursor(content.document).position;
  if (!forwardToChild(aMessage, activateContextMenu, position)) {
    sendContextMenuCoordinates(position);
  }
}

function moveByGranularity(aMessage) {
  let direction = aMessage.json.direction;
  let vc = Utils.getVirtualCursor(content.document);
  let granularity;

  switch(aMessage.json.granularity) {
    case MOVEMENT_GRANULARITY_CHARACTER:
      granularity = Ci.nsIAccessiblePivot.CHAR_BOUNDARY;
      break;
    case MOVEMENT_GRANULARITY_WORD:
      granularity = Ci.nsIAccessiblePivot.WORD_BOUNDARY;
      break;
    default:
      return;
  }

  if (direction === 'Previous') {
    vc.movePreviousByText(granularity);
  } else if (direction === 'Next') {
    vc.moveNextByText(granularity);
  }
}

function moveCaret(aMessage) {
  let direction = aMessage.json.direction;
  let granularity = aMessage.json.granularity;
  let accessible = Utils.getVirtualCursor(content.document).position;
  let accText = accessible.QueryInterface(Ci.nsIAccessibleText);
  let oldOffset = accText.caretOffset;
  let text = accText.getText(0, accText.characterCount);

  let start = {}, end = {};
  if (direction === 'Previous' && !aMessage.json.atStart) {
    switch (granularity) {
      case MOVEMENT_GRANULARITY_CHARACTER:
        accText.caretOffset--;
        break;
      case MOVEMENT_GRANULARITY_WORD:
        accText.getTextBeforeOffset(accText.caretOffset,
                                    Ci.nsIAccessibleText.BOUNDARY_WORD_START, start, end);
        accText.caretOffset = end.value === accText.caretOffset ? start.value : end.value;
        break;
      case MOVEMENT_GRANULARITY_PARAGRAPH:
        let startOfParagraph = text.lastIndexOf('\n', accText.caretOffset - 1);
        accText.caretOffset = startOfParagraph !== -1 ? startOfParagraph : 0;
        break;
    }
  } else if (direction === 'Next' && !aMessage.json.atEnd) {
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
        accText.caretOffset = text.indexOf('\n', accText.caretOffset + 1);
        break;
    }
  }

  presentCaretChange(text, oldOffset, accText.caretOffset);
}

function presentCaretChange(aText, aOldOffset, aNewOffset) {
  if (aOldOffset !== aNewOffset) {
    let msg = Presentation.textSelectionChanged(aText, aNewOffset, aNewOffset,
                                                aOldOffset, aOldOffset, true);
    sendAsyncMessage('AccessFu:Present', msg);
  }
}

function scroll(aMessage) {
  function sendScrollCoordinates(aAccessible) {
    let bounds = Utils.getBounds(aAccessible);
    sendAsyncMessage('AccessFu:DoScroll',
                     { bounds: bounds,
                       page: aMessage.json.page,
                       horizontal: aMessage.json.horizontal });
  }

  let position = Utils.getVirtualCursor(content.document).position;
  if (!forwardToChild(aMessage, scroll, position)) {
    sendScrollCoordinates(position);
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
    Logger.debug('AccessFu:Start');
    if (m.json.buildApp)
      Utils.MozBuildApp = m.json.buildApp;

    addMessageListener('AccessFu:MoveToPoint', moveToPoint);
    addMessageListener('AccessFu:MoveCursor', moveCursor);
    addMessageListener('AccessFu:ShowCurrent', showCurrent);
    addMessageListener('AccessFu:Activate', activateCurrent);
    addMessageListener('AccessFu:ContextMenu', activateContextMenu);
    addMessageListener('AccessFu:Scroll', scroll);
    addMessageListener('AccessFu:AdjustRange', adjustRange);
    addMessageListener('AccessFu:MoveCaret', moveCaret);
    addMessageListener('AccessFu:MoveByGranularity', moveByGranularity);

    if (!eventManager) {
      eventManager = new EventManager(this);
    }
    eventManager.start();
  });

addMessageListener(
  'AccessFu:Stop',
  function(m) {
    Logger.debug('AccessFu:Stop');

    removeMessageListener('AccessFu:MoveToPoint', moveToPoint);
    removeMessageListener('AccessFu:MoveCursor', moveCursor);
    removeMessageListener('AccessFu:ShowCurrent', showCurrent);
    removeMessageListener('AccessFu:Activate', activateCurrent);
    removeMessageListener('AccessFu:ContextMenu', activateContextMenu);
    removeMessageListener('AccessFu:Scroll', scroll);
    removeMessageListener('AccessFu:MoveCaret', moveCaret);
    removeMessageListener('AccessFu:MoveByGranularity', moveByGranularity);

    eventManager.stop();
  });

sendAsyncMessage('AccessFu:Ready');
