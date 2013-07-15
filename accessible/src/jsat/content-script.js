/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces;
let Cu = Components.utils;

const ROLE_ENTRY = Ci.nsIAccessibleRole.ROLE_ENTRY;
const ROLE_INTERNAL_FRAME = Ci.nsIAccessibleRole.ROLE_INTERNAL_FRAME;

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
XPCOMUtils.defineLazyModuleGetter(this, 'ObjectWrapper',
  'resource://gre/modules/ObjectWrapper.jsm');

Logger.debug('content-script.js');

let eventManager = null;

function virtualCursorControl(aMessage) {
  if (Logger.logLevel >= Logger.DEBUG)
    Logger.debug(aMessage.name, JSON.stringify(aMessage.json));

  try {
    let vc = Utils.getVirtualCursor(content.document);
    let origin = aMessage.json.origin;
    if (origin != 'child') {
      if (forwardMessage(vc, aMessage))
        return;
    }

    let details = aMessage.json;
    let rule = TraversalRules[details.rule];
    let moved = 0;
    switch (details.action) {
    case 'moveFirst':
    case 'moveLast':
      moved = vc[details.action](rule);
      break;
    case 'moveNext':
    case 'movePrevious':
      try {
        if (origin == 'parent' && vc.position == null) {
          if (details.action == 'moveNext')
            moved = vc.moveFirst(rule);
          else
            moved = vc.moveLast(rule);
        } else {
          moved = vc[details.action](rule);
        }
      } catch (x) {
        let acc = Utils.AccRetrieval.
          getAccessibleFor(content.document.activeElement);
        moved = vc.moveNext(rule, acc, true);
      }
      break;
    case 'moveToPoint':
      if (!this._ppcp) {
        this._ppcp = Utils.getPixelsPerCSSPixel(content);
      }
      moved = vc.moveToPoint(rule,
                             details.x * this._ppcp, details.y * this._ppcp,
                             true);
      break;
    case 'whereIsIt':
      if (!forwardMessage(vc, aMessage)) {
        if (!vc.position && aMessage.json.move)
          vc.moveFirst(TraversalRules.Simple);
        else {
          sendAsyncMessage('AccessFu:Present', Presentation.pivotChanged(
            vc.position, null, Ci.nsIAccessiblePivot.REASON_NONE));
        }
      }

      break;
    default:
      break;
    }

    if (moved == true) {
      forwardMessage(vc, aMessage);
    } else if (moved == false && details.action != 'moveToPoint') {
      if (origin == 'parent') {
        vc.position = null;
      }
      aMessage.json.origin = 'child';
      sendAsyncMessage('AccessFu:VirtualCursor', aMessage.json);
    }
  } catch (x) {
    Logger.logException(x, 'Failed to move virtual cursor');
  }
}

function forwardMessage(aVirtualCursor, aMessage) {
  try {
    let acc = aVirtualCursor.position;
    if (acc && acc.role == ROLE_INTERNAL_FRAME) {
      let mm = Utils.getMessageManager(acc.DOMNode);
      mm.addMessageListener(aMessage.name, virtualCursorControl);
      aMessage.json.origin = 'parent';
      if (Utils.isContentProcess) {
        // XXX: OOP content's screen offset is 0,
        // so we remove the real screen offset here.
        aMessage.json.x -= content.mozInnerScreenX;
        aMessage.json.y -= content.mozInnerScreenY;
      }
      mm.sendAsyncMessage(aMessage.name, aMessage.json);
      return true;
    }
  } catch (x) {
    // Frame may be hidden, we regard this case as false.
  }
  return false;
}

function activateCurrent(aMessage) {
  Logger.debug('activateCurrent');
  function activateAccessible(aAccessible) {
    if (aAccessible.actionCount > 0) {
      aAccessible.doAction(0);
    } else {
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
  if (focusedAcc && focusedAcc.role === ROLE_ENTRY) {
    moveCaretTo(focusedAcc, aMessage.json.offset);
    return;
  }

  let vc = Utils.getVirtualCursor(content.document);
  if (!forwardMessage(vc, aMessage))
    activateAccessible(vc.position);
}

function activateContextMenu(aMessage) {
  function sendContextMenuCoordinates(aAccessible) {
    let objX = {}, objY = {}, objW = {}, objH = {};
    aAccessible.getBounds(objX, objY, objW, objH);
    let x = objX.value + objW.value / 2;
    let y = objY.value + objH.value / 2;
    sendAsyncMessage('AccessFu:ActivateContextMenu', {x: x, y: y});
  }

  let vc = Utils.getVirtualCursor(content.document);
  if (!forwardMessage(vc, aMessage))
    sendContextMenuCoordinates(vc.position);
}

function moveCaret(aMessage) {
  const MOVEMENT_GRANULARITY_CHARACTER = 1;
  const MOVEMENT_GRANULARITY_WORD = 2;
  const MOVEMENT_GRANULARITY_PARAGRAPH = 8;

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
  let vc = Utils.getVirtualCursor(content.document);

  function tryToScroll() {
    let horiz = aMessage.json.horizontal;
    let page = aMessage.json.page;

    // Search up heirarchy for scrollable element.
    let acc = vc.position;
    while (acc) {
      let elem = acc.DOMNode;

      // This is inspired by IndieUI events. Once they are
      // implemented, it should be easy to transition to them.
      // https://dvcs.w3.org/hg/IndieUI/raw-file/tip/src/indie-ui-events.html#scrollrequest
      let uiactions = elem.getAttribute ? elem.getAttribute('uiactions') : '';
      if (uiactions && uiactions.split(' ').indexOf('scroll') >= 0) {
        let evt = elem.ownerDocument.createEvent('CustomEvent');
        let details = horiz ? { deltaX: page * elem.clientWidth } :
          { deltaY: page * elem.clientHeight };
        evt.initCustomEvent(
          'scrollrequest', true, true,
          ObjectWrapper.wrap(details, elem.ownerDocument.defaultView));
        if (!elem.dispatchEvent(evt))
          return;
      }

      // We will do window scrolling next.
      if (elem == content.document)
        break;

      if (!horiz && elem.clientHeight < elem.scrollHeight) {
        let s = content.getComputedStyle(elem);
        if (s.overflowY == 'scroll' || s.overflowY == 'auto') {
          elem.scrollTop += page * elem.clientHeight;
          return true;
        }
      }

      if (horiz) {
        if (elem.clientWidth < elem.scrollWidth) {
          let s = content.getComputedStyle(elem);
          if (s.overflowX == 'scroll' || s.overflowX == 'auto') {
            elem.scrollLeft += page * elem.clientWidth;
            return true;
          }
        }
      }
      acc = acc.parent;
    }

    // Scroll window.
    if (!horiz && content.scrollMaxY &&
        ((page > 0 && content.scrollY < content.scrollMaxY) ||
         (page < 0 && content.scrollY > 0))) {
      content.scroll(0, content.innerHeight * page + content.scrollY);
      return true;
    } else if (horiz && content.scrollMaxX &&
               ((page > 0 && content.scrollX < content.scrollMaxX) ||
                (page < 0 && content.scrollX > 0))) {
      content.scroll(content.innerWidth * page + content.scrollX);
      return true;
    }

    return false;
  }

  if (aMessage.json.origin != 'child') {
    if (forwardMessage(vc, aMessage))
      return;
  }

  if (!tryToScroll()) {
    // Failed to scroll anything in this document. Try in parent document.
    aMessage.json.origin = 'child';
    sendAsyncMessage('AccessFu:Scroll', aMessage.json);
  }
}

addMessageListener(
  'AccessFu:Start',
  function(m) {
    Logger.debug('AccessFu:Start');
    if (m.json.buildApp)
      Utils.MozBuildApp = m.json.buildApp;

    addMessageListener('AccessFu:VirtualCursor', virtualCursorControl);
    addMessageListener('AccessFu:Activate', activateCurrent);
    addMessageListener('AccessFu:ContextMenu', activateContextMenu);
    addMessageListener('AccessFu:Scroll', scroll);
    addMessageListener('AccessFu:MoveCaret', moveCaret);

    if (!eventManager) {
      eventManager = new EventManager(this);
    }
    eventManager.start();
  });

addMessageListener(
  'AccessFu:Stop',
  function(m) {
    Logger.debug('AccessFu:Stop');

    removeMessageListener('AccessFu:VirtualCursor', virtualCursorControl);
    removeMessageListener('AccessFu:Activate', activateCurrent);
    removeMessageListener('AccessFu:ContextMenu', activateContextMenu);
    removeMessageListener('AccessFu:Scroll', scroll);
    removeMessageListener('AccessFu:MoveCaret', moveCaret);

    eventManager.stop();
  });

sendAsyncMessage('AccessFu:Ready');
