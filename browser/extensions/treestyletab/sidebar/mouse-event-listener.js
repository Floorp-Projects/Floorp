/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Tree Style Tab.
 *
 * The Initial Developer of the Original Code is YUKI "Piro" Hiroshi.
 * Portions created by the Initial Developer are Copyright (C) 2011-2021
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): YUKI "Piro" Hiroshi <piro.outsider.reflex@gmail.com>
 *                 wanabe <https://github.com/wanabe>
 *                 Tetsuharu OHZEKI <https://github.com/saneyuki>
 *                 Xidorn Quan <https://github.com/upsuper> (Firefox 40+ support)
 *                 lv7777 (https://github.com/lv7777)
 *
 * ***** END LICENSE BLOCK ******/
'use strict';

import MenuUI from '/extlib/MenuUI.js';

import {
  log as internalLogger,
  wait,
  dumpTab,
  mapAndFilter,
  countMatched,
  configs,
  shouldApplyAnimation,
  isMacOS,
} from '/common/common.js';
import * as Constants from '/common/constants.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TreeBehavior from '/common/tree-behavior.js';
import * as TSTAPI from '/common/tst-api.js';
import * as MetricsData from '/common/metrics-data.js';

import Tab from '/common/Tab.js';

import * as BackgroundConnection from './background-connection.js';
import * as Sidebar from './sidebar.js';
import * as EventUtils from './event-utils.js';
import * as DragAndDrop from './drag-and-drop.js';
import * as TabContextMenu from './tab-context-menu.js';
import * as Scroll from './scroll.js';

function log(...args) {
  internalLogger('sidebar/mouse-event-listener', ...args);
}

let mTargetWindow;

const mTabBar = document.querySelector('#tabbar');
const mContextualIdentitySelector = document.getElementById(Constants.kCONTEXTUAL_IDENTITY_SELECTOR);
const mNewTabActionSelector       = document.getElementById(Constants.kNEWTAB_ACTION_SELECTOR);

let mHasMouseOverListeners = false;

Sidebar.onInit.addListener(() => {
  mTargetWindow = TabsStore.getCurrentWindowId();
});

Sidebar.onBuilt.addListener(async () => {
  document.addEventListener('mousedown', onMouseDown);
  document.addEventListener('mouseup', onMouseUp);
  document.addEventListener('click', onClick);
  document.addEventListener('auxclick', onAuxClick);
  document.addEventListener('dragstart', onDragStart);
  mTabBar.addEventListener('dblclick', onDblClick);
  mTabBar.addEventListener('mouseover', onMouseOver);

  MetricsData.add('mouse-event-listener: Sidebar.onBuilt: apply configs');

  browser.runtime.onMessage.addListener(onMessage);
  BackgroundConnection.onMessage.addListener(onBackgroundMessage);

  if (!document.documentElement.classList.contains('incognito'))
    mContextualIdentitySelector.ui = new MenuUI({
      root:       mContextualIdentitySelector,
      appearance: 'panel',
      onCommand:  onContextualIdentitySelect,
      animationDuration: shouldApplyAnimation() ? configs.collapseDuration : 0.001
    });

  mNewTabActionSelector.ui = new MenuUI({
    root:       mNewTabActionSelector,
    appearance: 'panel',
    onCommand:  onNewTabActionSelect,
    animationDuration: shouldApplyAnimation() ? configs.collapseDuration : 0.001
  });
});

Sidebar.onReady.addListener(() => {
  updateSpecialEventListenersForAPIListeners();
});

function updateSpecialEventListenersForAPIListeners() {
  const shouldListenMouseMove = TSTAPI.hasListenerForMessageType(TSTAPI.kNOTIFY_TAB_MOUSEMOVE);
  if (shouldListenMouseMove != onMouseMove.listening) {
    if (!onMouseMove.listening) {
      window.addEventListener('mousemove', onMouseMove, { capture: true, passive: true });
      onMouseMove.listening = true;
    }
    else {
      window.removeEventListener('mousemove', onMouseMove, { capture: true, passive: true });
      onMouseMove.listening = false;
    }
  }

  const shouldListenMouseOut = TSTAPI.hasListenerForMessageType(TSTAPI.kNOTIFY_TAB_MOUSEOUT);
  if (shouldListenMouseOut != onMouseOut.listening) {
    if (!onMouseOut.listening) {
      window.addEventListener('mouseout', onMouseOut, { capture: true, passive: true });
      onMouseOut.listening = true;
    }
    else {
      window.removeEventListener('mouseout', onMouseOut, { capture: true, passive: true });
      onMouseOut.listening = false;
    }
  }

  mHasMouseOverListeners = shouldListenMouseOut || TSTAPI.hasListenerForMessageType(TSTAPI.kNOTIFY_TAB_MOUSEOVER);
}


/* handlers for DOM events */

function onMouseMove(event) {
  const tab = EventUtils.getTabFromEvent(event);
  if (tab) {
    TSTAPI.sendMessage({
      type:     TSTAPI.kNOTIFY_TAB_MOUSEMOVE,
      tab:      new TSTAPI.TreeItem(tab),
      window:   mTargetWindow,
      windowId: mTargetWindow,
      ctrlKey:  event.ctrlKey,
      shiftKey: event.shiftKey,
      altKey:   event.altKey,
      metaKey:  event.metaKey,
      dragging: DragAndDrop.isCapturingForDragging()
    }, { tabProperties: ['tab'] }).catch(_error => {});
  }
}
onMouseMove = EventUtils.wrapWithErrorHandler(onMouseMove);

let mLastWarmUpTab = -1;

function onMouseOver(event) {
  const tab = EventUtils.getTabFromEvent(event);

  if (tab &&
      mLastWarmUpTab != tab.id &&
      typeof browser.tabs.warmup == 'function') {
    browser.tabs.warmup(tab.id);
    mLastWarmUpTab = tab.id;
  }

  if (!mHasMouseOverListeners)
    return;

  // We enter the tab or one of its children, but not from any of the tabs
  // (other) children, so we are now starting to hover this tab (relatedTarget
  // contains the target of the mouseout event or null if there is none). This
  // also includes the case where we enter the tab directly without going
  // through another tab or the sidebar, which causes relatedTarget to be null
  const enterTabFromAncestor = tab && !tab.$TST.element.contains(event.relatedTarget);

  if (enterTabFromAncestor) {
    TSTAPI.sendMessage({
      type:     TSTAPI.kNOTIFY_TAB_MOUSEOVER,
      tab:      new TSTAPI.TreeItem(tab),
      window:   mTargetWindow,
      windowId: mTargetWindow,
      ctrlKey:  event.ctrlKey,
      shiftKey: event.shiftKey,
      altKey:   event.altKey,
      metaKey:  event.metaKey,
      dragging: DragAndDrop.isCapturingForDragging()
    }, { tabProperties: ['tab'] }).catch(_error => {});
  }
}
onMouseOver = EventUtils.wrapWithErrorHandler(onMouseOver);

function onMouseOut(event) {
  const tab = EventUtils.getTabFromEvent(event);

  // We leave the tab or any of its children, but not for one of the tabs
  // (other) children, so we are no longer hovering this tab (relatedTarget
  // contains the target of the mouseover event or null if there is none). This
  // also includes the case where we leave the tab directly without going
  // through another tab or the sidebar, which causes relatedTarget to be null
  const leaveTabToAncestor = tab && !tab.$TST.element.contains(event.relatedTarget);

  if (leaveTabToAncestor) {
    TSTAPI.sendMessage({
      type:     TSTAPI.kNOTIFY_TAB_MOUSEOUT,
      tab:      new TSTAPI.TreeItem(tab),
      window:   mTargetWindow,
      windowId: mTargetWindow,
      ctrlKey:  event.ctrlKey,
      shiftKey: event.shiftKey,
      altKey:   event.altKey,
      metaKey:  event.metaKey,
      dragging: DragAndDrop.isCapturingForDragging()
    }, { tabProperties: ['tab'] }).catch(_error => {});
  }
}
onMouseOut = EventUtils.wrapWithErrorHandler(onMouseOut);

let mLastDragStartTimestamp = -1;

function onMouseDown(event) {
  EventUtils.cancelHandleMousedown(event.button);
  TabContextMenu.close();
  DragAndDrop.clearDropPosition();
  DragAndDrop.clearDraggingState();

  if (EventUtils.isEventFiredOnAnchor(event) &&
      !EventUtils.isAccelAction(event) &&
      event.button != 2) {
    log('onMouseDown: canceled / mouse down on a selector anchor');
    event.stopPropagation();
    event.preventDefault();
    const selector = document.getElementById(EventUtils.getElementTarget(event).closest('[data-menu-ui]').dataset.menuUi);
    selector.ui.open({
      anchor: event.target
    });
    return;
  }

  const target = event.target;
  const tab = EventUtils.getTabFromEvent(event) || EventUtils.getTabFromTabbarEvent(event);
  log('onMouseDown: found target tab: ', tab, event);

  const extraContentsInfo = getOriginalExtraContentsTarget(event);
  const mousedownDetail   = getMouseEventDetail(event, tab);
  mousedownDetail.$extraContentsInfo = extraContentsInfo;
  log('onMouseDown ', mousedownDetail);

  if (mousedownDetail.targetType == 'selector')
    return;

  if (mousedownDetail.isMiddleClick) {
    log('onMouseDown: canceled / middle click');
    event.stopPropagation();
    event.preventDefault();
  }

  const mousedown = {
    detail: mousedownDetail,
    treeItem: new TSTAPI.TreeItem(tab),
    promisedMousedownNotified: Promise.resolve(),
    timestamp: Date.now(),
  };

  const apiEventType = tab ?
    TSTAPI.kNOTIFY_TAB_MOUSEDOWN :
    mousedownDetail.targetType == 'newtabbutton' ?
      TSTAPI.kNOTIFY_NEW_TAB_BUTTON_MOUSEDOWN :
      TSTAPI.kNOTIFY_TABBAR_MOUSEDOWN;

  mousedown.promisedMousedownNotified = Promise.all([
    browser.runtime.sendMessage({type: apiEventType })
      .catch(ApiTabs.createErrorHandler()),
    (async () => {
      log('Sending message to mousedown listeners ', { extraContentsInfo });
      const allowed = await tryMouseOperationAllowedWithExtraContents(
        apiEventType,
        mousedown,
        extraContentsInfo
      );
      if (!allowed) {
        log(' => canceled');
        return true;
      }

      log(' => allowed');
      return false;
    })()
  ]).then(results => results[1]);

  // Firefox switches tab focus on mousedown, and keeps
  // tab multiselection for draging of them together.
  // We simulate the behavior here.
  mousedown.promisedMousedownNotified.then(canceled => {
    if (canceled)
      EventUtils.cancelHandleMousedown(event.button);

    if (!EventUtils.getLastMousedown(event.button) ||
        mousedown.expired ||
        canceled)
      return;

    const onRegularArea = (
      !mousedown.detail.twisty &&
      !mousedown.detail.soundButton &&
      !mousedown.detail.closebox
    );
    const wasMultiselectionAction = (
      mousedown.detail.ctrlKey ||
      mousedown.detail.shiftKey
    );
    if (mousedown.detail.button == 0 &&
        onRegularArea &&
        !wasMultiselectionAction &&
        tab)
      BackgroundConnection.sendMessage({
        type:  Constants.kCOMMAND_ACTIVATE_TAB,
        tabId: tab.id,
        byMouseOperation:   true,
        keepMultiselection: true
      });
  });

  EventUtils.setLastMousedown(event.button, mousedown);
  mousedown.timeout = setTimeout(async () => {
    if (!EventUtils.getLastMousedown(event.button))
      return;

    if (event.button == 0 &&
        mousedownDetail.targetType == 'newtabbutton' &&
        configs.longPressOnNewTabButton &&
        mLastDragStartTimestamp < mousedown.timestamp) {
      mousedown.expired = true;
      const selector = document.getElementById(configs.longPressOnNewTabButton);
      if (selector) {
        const anchor = target.parentNode.querySelector(`[data-menu-ui="${selector.id}"]`);
        const anchorVisible = anchor && window.getComputedStyle(anchor, null).display != 'none';
        selector.ui.open({
          anchor: anchorVisible && anchor || target
        });
      }
      return;
    }

    if (TSTAPI.getListenersForMessageType(TSTAPI.kNOTIFY_TAB_DRAGREADY).length == 0)
      return;

    if (event.button == 0 &&
        tab) {
      log('onMouseDown expired');
      mousedown.expired = true;
    }
  }, configs.longPressDuration);
}
onMouseDown = EventUtils.wrapWithErrorHandler(onMouseDown);

function getMouseEventDetail(event, tab) {
  return {
    targetType:    getMouseEventTargetType(event),
    tab:           tab && tab.id,
    tabId:         tab && tab.id,
    window:        mTargetWindow,
    windowId:      mTargetWindow,
    twisty:        EventUtils.isEventFiredOnTwisty(event),
    soundButton:   EventUtils.isEventFiredOnSoundButton(event),
    closebox:      EventUtils.isEventFiredOnClosebox(event),
    button:        event.button,
    ctrlKey:       event.ctrlKey,
    shiftKey:      event.shiftKey,
    altKey:        event.altKey,
    metaKey:       event.metaKey,
    isMiddleClick: EventUtils.isMiddleClick(event),
    isAccelClick:  EventUtils.isAccelAction(event),
    lastInnerScreenY: window.mozInnerScreenY
  };
}

function getOriginalExtraContentsTarget(event) {
  try {
    let target = event.originalTarget;
    if (target && target.nodeType != Node.ELEMENT_NODE)
      target = target.parentNode;

    const extraContents = target.closest(`.extra-item`);
    if (extraContents)
      return {
        owners: new Set([extraContents.dataset.owner]),
        target: target.outerHTML
      };
  }
  catch(_error) {
    // this may happen by mousedown on scrollbar
  }

  return {
    owners: new Set(),
    target: null
  };
}

async function tryMouseOperationAllowedWithExtraContents(type, mousedown, extraContentsInfo) {
  if (extraContentsInfo &&
      extraContentsInfo.owners &&
      extraContentsInfo.owners.size > 0) {
    const allowed = await TSTAPI.tryOperationAllowed(
      type,
      {
        ...mousedown.detail,
        tab:                mousedown.treeItem,
        originalTarget:     extraContentsInfo.target,
        $extraContentsInfo: null
      },
      { tabProperties: ['tab'],
        targets:       extraContentsInfo.owners }
    );
    if (!allowed)
      return false;
  }
  const allowed = await TSTAPI.tryOperationAllowed(
    type,
    {
      ...mousedown.detail,
      tab:                mousedown.treeItem,
      $extraContentsInfo: null
    },
    { tabProperties: ['tab'],
      except:        extraContentsInfo && extraContentsInfo.owners }
  );
  if (!allowed)
    return false;
  return true;
}

function getMouseEventTargetType(event) {
  if (event.target.closest('.rich-confirm, #blocking-screen'))
    return 'outside';

  if (EventUtils.getTabFromEvent(event))
    return 'tab';

  if (EventUtils.isEventFiredOnNewTabButton(event))
    return 'newtabbutton';

  if (EventUtils.isEventFiredOnMenuOrPanel(event) ||
      EventUtils.isEventFiredOnAnchor(event))
    return 'selector';

  const allRange = document.createRange();
  allRange.selectNodeContents(document.body);
  const containerRect = allRange.getBoundingClientRect();
  allRange.detach();
  if (event.clientX < containerRect.left ||
      event.clientX > containerRect.right ||
      event.clientY < containerRect.top ||
      event.clientY > containerRect.bottom)
    return 'outside';

  return 'blank';
}

let mLastMouseUpX = -1;
let mLastMouseUpY = -1;
let mLastMouseUpOnTab = -1;

async function onMouseUp(event) {
  const unsafeTab = EventUtils.getTabFromEvent(event, { force: true }) || EventUtils.getTabFromTabbarEvent(event, { force: true });
  const tab       = EventUtils.getTabFromEvent(event) || EventUtils.getTabFromTabbarEvent(event);
  log('onMouseUp: ', unsafeTab, { living: !!tab });

  DragAndDrop.endMultiDrag(unsafeTab, event);

  if (EventUtils.isEventFiredOnMenuOrPanel(event) ||
      EventUtils.isEventFiredOnAnchor(event)) {
    log(' => on menu or anchor');
    return;
  }

  const lastMousedown = EventUtils.getLastMousedown(event.button);
  EventUtils.cancelHandleMousedown(event.button);
  const extraContentsInfo = lastMousedown && lastMousedown.detail && lastMousedown.detail.$extraContentsInfo;
  if (!lastMousedown) {
    log(' => no lastMousedown');
    return;
  }
  if (lastMousedown.detail.targetType == 'outside') {
    log(' => out of contents');
    return;
  }

  if (tab) {
    const mouseupInfo = {
      ...lastMousedown,
      detail:   getMouseEventDetail(event, tab),
      treeItem: new TSTAPI.TreeItem(tab)
    };

    const mouseupAllowed = await tryMouseOperationAllowedWithExtraContents(
      TSTAPI.kNOTIFY_TAB_MOUSEUP,
      mouseupInfo,
      extraContentsInfo
    );
    if (!mouseupAllowed) {
      log(' => not allowed (mouseup)');
      return true;
    }

    const clickAllowed = await tryMouseOperationAllowedWithExtraContents(
      TSTAPI.kNOTIFY_TAB_CLICKED,
      mouseupInfo,
      extraContentsInfo
    );
    if (!clickAllowed) {
      log(' => not allowed (clicked');
      return true;
    }
  }

  let promisedCanceled = null;
  if (lastMousedown.treeItem && lastMousedown.detail.targetType == 'tab')
    promisedCanceled = lastMousedown.promisedMousedownNotified;

  if (lastMousedown.expired ||
      lastMousedown.detail.targetType != getMouseEventTargetType(event) || // when the cursor was moved before mouseup
      (tab && tab != Tab.get(lastMousedown.detail.tab))) { // when the tab was already removed
    log(' => expired, different type, or different tab');
    return;
  }

  if (promisedCanceled && await promisedCanceled) {
    log('onMouseUp: canceled / by other addons');
    return;
  }

  // not canceled, then fallback to default behavior
  return handleDefaultMouseUp({ lastMousedown, tab, event });
}
onMouseUp = EventUtils.wrapWithErrorHandler(onMouseUp);

async function handleDefaultMouseUp({ lastMousedown, tab, event }) {
  log('handleDefaultMouseUp ', lastMousedown.detail);

  if (tab &&
      lastMousedown.detail.button != 2 &&
      await handleDefaultMouseUpOnTab({ lastMousedown, tab, event }))
    return;

  if (tab) {
    mLastMouseUpX = event.clientX;
    mLastMouseUpY = event.clientY;
    mLastMouseUpOnTab = Date.now();
  }

  // following codes are for handlig of click event on the tab bar itself.
  const actionForNewTabCommand = lastMousedown.detail.isMiddleClick ?
    configs.autoAttachOnNewTabButtonMiddleClick :
    lastMousedown.detail.isAccelClick ?
      configs.autoAttachOnNewTabButtonAccelClick :
      configs.autoAttachOnNewTabCommand;
  if (EventUtils.isEventFiredOnNewTabButton(event)) {
    if (lastMousedown.detail.button != 2) {
      log('onMouseUp: click on the new tab button');
      const mouseupInfo = {
        ...lastMousedown,
        detail:   getMouseEventDetail(event),
        treeItem: null,
      };

      const mouseUpAllowed = await tryMouseOperationAllowedWithExtraContents(
        TSTAPI.kNOTIFY_NEW_TAB_BUTTON_MOUSEUP,
        mouseupInfo,
        lastMousedown.detail.$extraContentsInfo
      );
      if (!mouseUpAllowed)
        return;

      const clickAllowed = await tryMouseOperationAllowedWithExtraContents(
        TSTAPI.kNOTIFY_NEW_TAB_BUTTON_CLICKED,
        mouseupInfo,
        lastMousedown.detail.$extraContentsInfo
      );
      if (!clickAllowed)
        return;

      // Simulation of Firefox's built-in behavior.
      // See also: https://github.com/piroor/treestyletab/issues/2593
      if (event.shiftKey && !lastMousedown.detail.isAccelClick) {
        browser.windows.create({});
      }
      else {
        const activeTab = Tab.getActiveTab(mTargetWindow);
        const cookieStoreId = (actionForNewTabCommand == Constants.kNEWTAB_OPEN_AS_NEXT_SIBLING_WITH_INHERITED_CONTAINER) ? activeTab.cookieStoreId : null
        handleNewTabAction(event, {
          action: actionForNewTabCommand,
          cookieStoreId
        });
      }
    }
    return;
  }

  // Multiple middle clicks to close tabs can be detected as a middle click on the tab bar.
  // We should ignore if the cursor is not moved and the closing tab is still in animation.
  // See also: https://github.com/piroor/treestyletab/issues/1968
  if (shouldApplyAnimation() &&
      Date.now() - mLastMouseUpOnTab <= configs.collapseDuration &&
      Math.abs(mLastMouseUpX - event.clientX) < configs.acceptableFlickerToIgnoreClickOnTabAndTabbar / 2 &&
      Math.abs(mLastMouseUpY - event.clientY) < configs.acceptableFlickerToIgnoreClickOnTabAndTabbar / 2)
    return;

  log('onMouseUp: notify as a blank area click to other addons');
  const mouseUpAllowed = await TSTAPI.tryOperationAllowed(
    TSTAPI.kNOTIFY_TABBAR_MOUSEUP,
    {
      ...lastMousedown.detail,
      window:             mTargetWindow,
      windowId:           mTargetWindow,
      tab:                lastMousedown.treeItem,
      $extraContentsInfo: null
    },
    { tabProperties: ['tab'] }
  );
  if (!mouseUpAllowed)
    return;

  const clickAllowed = await TSTAPI.tryOperationAllowed(
    TSTAPI.kNOTIFY_TABBAR_CLICKED,
    {
      ...lastMousedown.detail,
      window:             mTargetWindow,
      windowId:           mTargetWindow,
      tab:                lastMousedown.treeItem,
      $extraContentsInfo: null
    },
    { tabProperties: ['tab'] }
  );
  if (!clickAllowed)
    return;

  if (lastMousedown.detail.isMiddleClick) { // Ctrl-click does nothing on Firefox's tab bar!
    log('onMouseUp: default action for middle click on the blank area');
    handleNewTabAction(event, {
      action: configs.autoAttachOnNewTabCommand
    });
  }
}
handleDefaultMouseUp = EventUtils.wrapWithErrorHandler(handleDefaultMouseUp);

async function handleDefaultMouseUpOnTab({ lastMousedown, tab, event } = {}) {
  log('Ready to handle click action on the tab');

  const onRegularArea = (
    !lastMousedown.detail.twisty &&
    !lastMousedown.detail.soundButton &&
    !lastMousedown.detail.closebox
  );
  const wasMultiselectionAction = updateMultiselectionByTabClick(tab, lastMousedown.detail);
  log(' => ', { onRegularArea, wasMultiselectionAction });

  // Firefox clears tab multiselection after the mouseup, so
  // we simulate the behavior.
  if (lastMousedown.detail.button == 0 &&
      onRegularArea &&
      !wasMultiselectionAction)
    BackgroundConnection.sendMessage({
      type:  Constants.kCOMMAND_ACTIVATE_TAB,
      tabId: tab.id,
      byMouseOperation:   true,
      keepMultiselection: false // tab.highlighted
    });

  if (lastMousedown.detail.isMiddleClick) { // Ctrl-click doesn't close tab on Firefox's tab bar!
    log('onMouseUp: middle click on a tab');
    if (lastMousedown.detail.targetType != 'tab') // ignore middle click on blank area
      return false;
    const tabs = TreeBehavior.getClosingTabsFromParent(tab, {
      byInternalOperation: true
    });
    Sidebar.confirmToCloseTabs(tabs.map(tab => tab.$TST.sanitized))
      .then(confirmed => {
        if (!confirmed)
          return;
        const tabIds = tabs.map(tab => tab.id);
        Scroll.tryLockPosition(tabIds);
        BackgroundConnection.sendMessage({
          type:   Constants.kCOMMAND_REMOVE_TABS_BY_MOUSE_OPERATION,
          tabIds
        });
      });
  }
  else if (wasMultiselectionAction) {
    // On Firefox's native tabs, Ctrl-Click or Shift-Click always
    // ignore actions on closeboxes and sound playing icons.
    // Thus we should simulate the behavior.
    return true;
  }
  else if (lastMousedown.detail.twisty &&
           EventUtils.isEventFiredOnTwisty(event)) {
    log('clicked on twisty');
    if (tab.$TST.hasChild)
      BackgroundConnection.sendMessage({
        type:            Constants.kCOMMAND_SET_SUBTREE_COLLAPSED_STATE,
        tabId:           tab.id,
        collapsed:       !tab.$TST.subtreeCollapsed,
        manualOperation: true,
        stack:           configs.debug && new Error().stack
      });
  }
  else if (lastMousedown.detail.soundButton &&
           EventUtils.isEventFiredOnSoundButton(event)) {
    log('clicked on sound button');
    BackgroundConnection.sendMessage({
      type:  Constants.kCOMMAND_TOGGLE_MUTED,
      tabId: tab.id
    });
  }
  else if (lastMousedown.detail.closebox &&
           EventUtils.isEventFiredOnClosebox(event)) {
    log('clicked on closebox');
    //if (!warnAboutClosingTabSubtreeOf(tab)) {
    //  event.stopPropagation();
    //  event.preventDefault();
    //  return;
    //}
    const multiselected  = tab.$TST.multiselected;
    const tabsToBeClosed = multiselected ?
      Tab.getSelectedTabs(tab.windowId) :
      TreeBehavior.getClosingTabsFromParent(tab, {
        byInternalOperation: true
      }) ;
    Sidebar.confirmToCloseTabs(tabsToBeClosed.map(tab => tab.$TST.sanitized), {
      configKey: 'warnOnCloseTabsByClosebox'
    })
      .then(confirmed => {
        if (!confirmed)
          return;
        const tabIds = tabsToBeClosed.map(tab => tab.id);
        Scroll.tryLockPosition(tabIds);
        BackgroundConnection.sendMessage({
          type:   Constants.kCOMMAND_REMOVE_TABS_BY_MOUSE_OPERATION,
          tabIds
        });
      });
  }

  return true;
}
handleDefaultMouseUpOnTab = EventUtils.wrapWithErrorHandler(handleDefaultMouseUpOnTab);

let mLastClickedTab       = null;
let mIsInSelectionSession = false;

function updateMultiselectionByTabClick(tab, event) {
  const ctrlKeyPressed     = event.ctrlKey || (event.metaKey && isMacOS());
  const activeTab          = Tab.getActiveTab(tab.windowId);
  const highlightedTabIds  = new Set(Tab.getHighlightedTabs(tab.windowId).map(tab => tab.id));
  log('updateMultiselectionByTabClick ', { ctrlKeyPressed, activeTab, highlightedTabIds, mIsInSelectionSession });
  if (event.shiftKey) {
    // select the clicked tab and tabs between last activated tab
    const lastClickedTab   = mLastClickedTab || activeTab;
    const betweenTabs      = Tab.getTabsBetween(lastClickedTab, tab);
    const targetTabs       = new Set([lastClickedTab].concat(betweenTabs));
    targetTabs.add(tab);

    log(' => ', { lastClickedTab, betweenTabs, targetTabs });

    try {
      if (!ctrlKeyPressed) {
        const alreadySelectedTabs = Tab.getHighlightedTabs(tab.windowId, { iterator: true });
        log('clear old selection by shift-click');
        for (const alreadySelectedTab of alreadySelectedTabs) {
          if (!targetTabs.has(alreadySelectedTab))
            highlightedTabIds.delete(alreadySelectedTab.id);
        }
      }

      log('set selection by shift-click: ', configs.debug && Array.from(targetTabs, dumpTab));
      for (const toBeSelectedTab of targetTabs) {
        highlightedTabIds.add(toBeSelectedTab.id);
      }

      const rootTabs = [tab];
      if (tab != activeTab &&
          !mIsInSelectionSession)
        rootTabs.push(activeTab);
      for (const root of rootTabs) {
        if (!root.$TST.subtreeCollapsed)
          continue;
        for (const descendant of root.$TST.descendants) {
          highlightedTabIds.add(descendant.id);
        }
      }
      log(' => highlightedTabIds: ', highlightedTabIds);

      // for better performance, we should not call browser.tabs.update() for each tab.
      const indices = mapAndFilter(highlightedTabIds,
                                   id => id == activeTab.id ? undefined : Tab.get(id).index);
      if (highlightedTabIds.has(activeTab.id))
        indices.unshift(activeTab.index);
      browser.tabs.highlight({
        windowId: tab.windowId,
        populate: false,
        tabs:     indices
      }).catch(ApiTabs.createErrorSuppressor());
    }
    catch(_e) { // not implemented on old Firefox
      return false;
    }
    mIsInSelectionSession = true;
    return true;
  }
  else if (ctrlKeyPressed) {
    try {
      log('change selection by ctrl-click: ', dumpTab(tab));
      /* Special operation to toggle selection of collapsed descendants for the active tab.
         - When there is no other multiselected foreign tab
           => toggle multiselection only descendants.
         - When there is one or more multiselected foreign tab
           => toggle multiselection of the active tab and descendants.
              => one of multiselected foreign tabs will be activated.
         - When a foreign tab is highlighted and there is one or more unhighlighted descendants 
           => highlight all descendants (to prevent only the root tab is dragged).
       */
      const activeTabDescendants = activeTab.$TST.descendants;
      let toBeHighlighted = !tab.highlighted;
      log('toBeHighlighted: ', toBeHighlighted);
      if (tab == activeTab &&
          tab.$TST.subtreeCollapsed &&
          activeTabDescendants.length > 0) {
        const highlightedCount  = countMatched(activeTabDescendants, tab => tab.highlighted);
        const partiallySelected = highlightedCount != 0 && highlightedCount != activeTabDescendants.length;
        toBeHighlighted = partiallySelected || !activeTabDescendants[0].highlighted;
        log(' => ', toBeHighlighted, { partiallySelected });
      }
      if (toBeHighlighted)
        highlightedTabIds.add(tab.id);
      else
        highlightedTabIds.delete(tab.id);

      if (tab.$TST.subtreeCollapsed) {
        const descendants = tab == activeTab ? activeTabDescendants : tab.$TST.descendants;
        for (const descendant of descendants) {
          if (toBeHighlighted)
            highlightedTabIds.add(descendant.id);
          else
            highlightedTabIds.delete(descendant.id);
        }
      }

      if (tab == activeTab) {
        if (highlightedTabIds.size == 0) {
          log('Don\'t unhighlight only one highlighted active tab!');
          highlightedTabIds.add(tab.id);
        }
      }
      else if (!mIsInSelectionSession) {
        log('Select active tab and its descendants, for new selection session');
        highlightedTabIds.add(activeTab.id);
        if (activeTab.$TST.subtreeCollapsed) {
          for (const descendant of activeTabDescendants) {
            highlightedTabIds.add(descendant.id);
          }
        }
      }

      // for better performance, we should not call browser.tabs.update() for each tab.
      const indices = mapAndFilter(highlightedTabIds,
                                   id => id == activeTab.id ? undefined : Tab.get(id).index);
      if (highlightedTabIds.has(activeTab.id))
        indices.unshift(activeTab.index);
      browser.tabs.highlight({
        windowId: tab.windowId,
        populate: false,
        tabs:     indices
      }).catch(ApiTabs.createErrorSuppressor());
    }
    catch(_e) { // not implemented on old Firefox
      return false;
    }
    mLastClickedTab       = tab;
    mIsInSelectionSession = true;
    return true;
  }
  else {
    mLastClickedTab       = null;
    mIsInSelectionSession = false;
    return false;
  }
}

Tab.onActivated.addListener((tab, _info = {}) => {
  if (tab.windowId != mTargetWindow)
    return;

  if (mLastClickedTab &&
      tab.id != mLastClickedTab.id &&
      Tab.getHighlightedTabs(mTargetWindow).length == 1) {
    mLastClickedTab       = null;
    mIsInSelectionSession = false;
  }
});

function onClick(_event) {
  // clear unexpectedly left "dragging" state
  // (see also https://github.com/piroor/treestyletab/issues/1921 )
  DragAndDrop.clearDraggingTabsState();
}
onClick = EventUtils.wrapWithErrorHandler(onClick);

function onAuxClick(event) {
  // This is required to prevent new tab from middle-click on a UI link.
  event.stopPropagation();
  event.preventDefault();
}
onAuxClick = EventUtils.wrapWithErrorHandler(onAuxClick);

function onDragStart(event) {
  log('onDragStart ', event);
  mLastDragStartTimestamp = Date.now();

  if (!event.target.closest('.newtab-button')) {
    log('not a draggable item in the tab bar');
    return;
  }

  const modifiers = String(configs.newTabButtonDragGestureModifiers).toLowerCase();
  if (!configs.allowDragNewTabButton ||
      modifiers.includes('alt') != event.altKey ||
      modifiers.includes('ctrl') != event.ctrlKey ||
      modifiers.includes('meta') != event.metaKey ||
      modifiers.includes('shift') != event.shiftKey) {
    log('not allowed drag action on the new tab button');
    event.stopPropagation();
    event.preventDefault();
    return;
  }

  log('new tab button is going to be dragged');

  const selector = document.getElementById(configs.longPressOnNewTabButton);
  if (selector &&
      selector.ui.opened) {
    log('menu is shown: don\'t start dragging');
    event.stopPropagation();
    event.preventDefault();
    return;
  }

  const dt = event.dataTransfer;
  dt.effectAllowed = 'copy';
  dt.setData('text/uri-list', 'about:newtab');
}
onDragStart = EventUtils.wrapWithErrorHandler(onDragStart);

function handleNewTabAction(event, options = {}) {
  log('handleNewTabAction ', { event, options });

  if (!configs.autoAttach && !('action' in options))
    options.action = Constants.kNEWTAB_DO_NOTHING;

  BackgroundConnection.sendMessage({
    type:          Constants.kCOMMAND_NEW_TAB_AS,
    baseTabId:     Tab.getActiveTab(mTargetWindow).id,
    as:            options.action,
    cookieStoreId: options.cookieStoreId,
    inBackground:  event.shiftKey
  });
}

async function onDblClick(event) {
  if (EventUtils.isEventFiredOnNewTabButton(event))
    return;

  const tab = EventUtils.getTabFromEvent(event, { force: true }) || EventUtils.getTabFromTabbarEvent(event, { force: true });
  const livingTab = EventUtils.getTabFromEvent(event);
  log('dblclick tab: ', tab, { living: !!livingTab });

  if (livingTab &&
      !EventUtils.isEventFiredOnTwisty(event) &&
      !EventUtils.isEventFiredOnSoundButton(event)) {
    const detail   = getMouseEventDetail(event, livingTab);
    const treeItem = new TSTAPI.TreeItem(livingTab);
    const extraContentsInfo = getOriginalExtraContentsTarget(event);
    if (extraContentsInfo.owners) {
      const allowed = await TSTAPI.tryOperationAllowed(
        TSTAPI.kNOTIFY_TAB_DBLCLICKED,
        {
          ...detail,
          tab:            treeItem,
          originalTarget: extraContentsInfo.target
        },
        { tabProperties: ['tab'],
          targets:       extraContentsInfo.owners }
      );
      if (!allowed)
        return;
    }
    const allowed = await TSTAPI.tryOperationAllowed(
      TSTAPI.kNOTIFY_TAB_DBLCLICKED,
      {
        ...detail,
        tab: treeItem
      },
      { tabProperties: ['tab'],
        except:        extraContentsInfo.owners }
    );
    if (!allowed)
      return;

    if (!EventUtils.isEventFiredOnClosebox(event) && // closebox action is already processed by onclick listener, so we should not handle it here!
        configs.treeDoubleClickBehavior != Constants.kTREE_DOUBLE_CLICK_BEHAVIOR_NONE) {
      switch (configs.treeDoubleClickBehavior) {
        case Constants.kTREE_DOUBLE_CLICK_BEHAVIOR_TOGGLE_COLLAPSED:
          event.stopPropagation();
          event.preventDefault();
          BackgroundConnection.sendMessage({
            type:            Constants.kCOMMAND_SET_SUBTREE_COLLAPSED_STATE,
            tabId:           livingTab.id,
            collapsed:       !livingTab.$TST.subtreeCollapsed,
            manualOperation: true,
            stack:           configs.debug && new Error().stack
          });
          break;

        case Constants.kTREE_DOUBLE_CLICK_BEHAVIOR_CLOSE:
          event.stopPropagation();
          event.preventDefault();
          const tabIds = [livingTab.id];
          Scroll.tryLockPosition(tabIds);
          BackgroundConnection.sendMessage({
            type:   Constants.kCOMMAND_REMOVE_TABS_BY_MOUSE_OPERATION,
            tabIds
          });
          break;
      }
    }
    return;
  }

  if (tab) // ignore dblclick on closing tab or something
    return;

  event.stopPropagation();
  event.preventDefault();
  handleNewTabAction(event, {
    action: configs.autoAttachOnNewTabCommand
  });
}


function onNewTabActionSelect(item, event) {
  if (item.dataset.value) {
    let action;
    switch (item.dataset.value) {
      default:
        action = Constants.kNEWTAB_OPEN_AS_ORPHAN;
        break;
      case 'child':
        action = Constants.kNEWTAB_OPEN_AS_CHILD;
        break;
      case 'sibling':
        action = Constants.kNEWTAB_OPEN_AS_SIBLING;
        break;
      case 'next-sibling':
        action = Constants.kNEWTAB_OPEN_AS_NEXT_SIBLING;
        break;
    }
    handleNewTabAction(event, { action });
  }
  mNewTabActionSelector.ui.close();
}

function onContextualIdentitySelect(item, event) {
  if (item.dataset.value) {
    const action = EventUtils.isAccelAction(event) ?
      configs.autoAttachOnNewTabButtonMiddleClick :
      configs.autoAttachOnNewTabCommand;
    handleNewTabAction(event, {
      action,
      cookieStoreId: item.dataset.value
    });
  }
  if (mContextualIdentitySelector.ui)
    mContextualIdentitySelector.ui.close();
}


function onMessage(message, _sender, _respond) {
  if (!message ||
      typeof message.type != 'string' ||
      message.type.indexOf('treestyletab:') != 0)
    return;

  //log('onMessage: ', message, sender);
  switch (message.type) {
    case TSTAPI.kCOMMAND_BROADCAST_API_REGISTERED:
      wait(0).then(() => { // wait until addons are updated
        updateSpecialEventListenersForAPIListeners();
      });
      break;

    case TSTAPI.kCOMMAND_BROADCAST_API_UNREGISTERED:
      wait(0).then(() => { // wait until addons are updated
        updateSpecialEventListenersForAPIListeners();
      });
      break;
  }
}

function onBackgroundMessage(message) {
  switch (message.type) {
    case Constants.kNOTIFY_TAB_MOUSEDOWN_EXPIRED:
      if (message.windowId == mTargetWindow) {
        const lastMousedown = EventUtils.getLastMousedown(message.button || 0);
        if (lastMousedown)
          lastMousedown.expired = true;
      }
      break;

    case Constants.kCOMMAND_SHOW_CONTAINER_SELECTOR: {
      if (!mContextualIdentitySelector.ui)
        return;
      const anchor = document.querySelector(`
        :root.contextual-identity-selectable .contextual-identities-selector-anchor,
        .newtab-button
      `);
      mContextualIdentitySelector.ui.open({ anchor });
    }; break;
  }
}
