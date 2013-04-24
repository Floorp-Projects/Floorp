/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import('resource://gre/modules/accessibility/Utils.jsm');
Cu.import('resource://gre/modules/accessibility/EventManager.jsm');
Cu.import('resource://gre/modules/accessibility/TraversalRules.jsm');
Cu.import('resource://gre/modules/Services.jsm');

Logger.debug('content-script.js');

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
      moved = vc.moveToPoint(rule, details.x, details.y, true);
      break;
    case 'whereIsIt':
      if (!forwardMessage(vc, aMessage)) {
        if (!vc.position && aMessage.json.move)
          vc.moveFirst(TraversalRules.Simple);
        else
          EventManager.presentVirtualCursorPosition(vc);
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
    Logger.error(x);
  }
}

function forwardMessage(aVirtualCursor, aMessage) {
  try {
    let acc = aVirtualCursor.position;
    if (acc && acc.role == Ci.nsIAccessibleRole.ROLE_INTERNAL_FRAME) {
      let mm = Utils.getMessageManager(acc.DOMNode);
      mm.addMessageListener(aMessage.name, virtualCursorControl);
      aMessage.json.origin = 'parent';
      // XXX: OOP content's screen offset is 0,
      // so we remove the real screen offset here.
      aMessage.json.x -= content.mozInnerScreenX;
      aMessage.json.y -= content.mozInnerScreenY;
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

      let cwu = content.QueryInterface(Ci.nsIInterfaceRequestor).
        getInterface(Ci.nsIDOMWindowUtils);
      cwu.sendMouseEventToWindow('mousedown', x, y, 0, 1, 0, false);
      cwu.sendMouseEventToWindow('mouseup', x, y, 0, 1, 0, false);
    }
  }

  let vc = Utils.getVirtualCursor(content.document);
  if (!forwardMessage(vc, aMessage))
    activateAccessible(vc.position);
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

        let controllers = acc.
          getRelationByType(
            Ci.nsIAccessibleRelation.RELATION_CONTROLLED_BY);
        for (let i = 0; controllers.targetsCount > i; i++) {
          let controller = controllers.getTarget(i);
          // If the section has a controlling slider, it should be considered
          // the page-turner.
          if (controller.role == Ci.nsIAccessibleRole.ROLE_SLIDER) {
            // Sliders are controlled with ctrl+right/left. I just decided :)
            let evt = content.document.createEvent('KeyboardEvent');
            evt.initKeyEvent(
              'keypress', true, true, null,
              true, false, false, false,
              (page > 0) ? evt.DOM_VK_RIGHT : evt.DOM_VK_LEFT, 0);
            controller.DOMNode.dispatchEvent(evt);
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
    addMessageListener('AccessFu:Scroll', scroll);

    EventManager.start(
      function sendMessage(aName, aDetails) {
        sendAsyncMessage(aName, aDetails);
      });

    docShell.QueryInterface(Ci.nsIInterfaceRequestor).
      getInterface(Ci.nsIWebProgress).
      addProgressListener(EventManager,
                          (Ci.nsIWebProgress.NOTIFY_STATE_ALL |
                           Ci.nsIWebProgress.NOTIFY_LOCATION));
    addEventListener('scroll', EventManager, true);
    addEventListener('resize', EventManager, true);
    // XXX: Ideally this would be an a11y event. Bug #742280.
    addEventListener('DOMActivate', EventManager, true);
  });

addMessageListener(
  'AccessFu:Stop',
  function(m) {
    Logger.debug('AccessFu:Stop');

    removeMessageListener('AccessFu:VirtualCursor', virtualCursorControl);
    removeMessageListener('AccessFu:Activate', activateCurrent);
    removeMessageListener('AccessFu:Scroll', scroll);

    EventManager.stop();

    docShell.QueryInterface(Ci.nsIInterfaceRequestor).
      getInterface(Ci.nsIWebProgress).
      removeProgressListener(EventManager);
    removeEventListener('scroll', EventManager, true);
    removeEventListener('resize', EventManager, true);
    // XXX: Ideally this would be an a11y event. Bug #742280.
    removeEventListener('DOMActivate', EventManager, true);
  });

sendAsyncMessage('AccessFu:Ready');
