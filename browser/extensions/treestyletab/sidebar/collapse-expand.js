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
 * Portions created by the Initial Developer are Copyright (C) 2011-2019
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

import {
  log as internalLogger,
  nextFrame,
  configs,
  shouldApplyAnimation
} from '/common/common.js';
import * as Constants from '/common/constants.js';
import * as TabsStore from '/common/tabs-store.js';

import Tab from '/common/Tab.js';

import * as BackgroundConnection from './background-connection.js';

import EventListenerManager from '/extlib/EventListenerManager.js';

import { TabInvalidationTarget } from './components/TabElement.js';

function log(...args) {
  internalLogger('sidebar/collapse-expand', ...args);
}

export const onUpdating = new EventListenerManager();
export const onUpdated = new EventListenerManager();

export function setCollapsed(tab, info = {}) {
  log('setCollapsed ', tab.id, info);
  if (!TabsStore.ensureLivingTab(tab)) // do nothing for closed tab!
    return;

  tab.$TST.shouldExpandLater = false; // clear flag

  if (info.collapsed) {
    tab.$TST.addState(Constants.kTAB_STATE_COLLAPSED);
    TabsStore.removeVisibleTab(tab);
    TabsStore.removeExpandedTab(tab);
  }
  else {
    tab.$TST.removeState(Constants.kTAB_STATE_COLLAPSED);
    TabsStore.addVisibleTab(tab);
    TabsStore.addExpandedTab(tab);
  }

  if (tab.$TST.onEndCollapseExpandAnimation) {
    clearTimeout(tab.$TST.onEndCollapseExpandAnimation.timeout);
    delete tab.$TST.onEndCollapseExpandAnimation;
  }

  if (tab.status == 'loading')
    tab.$TST.addState(Constants.kTAB_STATE_THROBBER_UNSYNCHRONIZED);

  const manager = tab.$TST.collapsedStateChangedManager || new EventListenerManager();

  if (tab.$TST.updatingCollapsedStateCanceller) {
    tab.$TST.updatingCollapsedStateCanceller(tab.$TST.collapsed);
    delete tab.$TST.updatingCollapsedStateCanceller;
  }

  let cancelled = false;
  const canceller = (aNewToBeCollapsed) => {
    cancelled = true;
    if (aNewToBeCollapsed != tab.$TST.collapsed) {
      tab.$TST.removeState(Constants.kTAB_STATE_COLLAPSING);
      tab.$TST.removeState(Constants.kTAB_STATE_EXPANDING);
      TabsStore.removeCollapsingTab(tab);
      TabsStore.removeExpandingTab(tab);
    }
  };
  const onCompleted = (tab, info = {}) => {
    manager.removeListener(onCompleted);
    if (cancelled ||
        !TabsStore.ensureLivingTab(tab)) // do nothing for closed tab!
      return;

    if (shouldApplyAnimation() &&
        !info.justNow &&
        configs.collapseDuration > 0)
      return; // force completion is required only for non-animation case

    //log('=> skip animation');
    if (tab.$TST.collapsed)
      tab.$TST.addState(Constants.kTAB_STATE_COLLAPSED_DONE);
    else
      tab.$TST.removeState(Constants.kTAB_STATE_COLLAPSED_DONE);

    onUpdated.dispatch(tab, {
      collapsed: tab.$TST.collapsed,
      anchor:    info.anchor,
      last:      info.last
    });
  };
  manager.addListener(onCompleted);

  if (!shouldApplyAnimation() ||
      info.justNow ||
      configs.collapseDuration < 1) {
    //log('=> skip animation');
    onCompleted(tab, info);
    return;
  }

  tab.$TST.updatingCollapsedStateCanceller = canceller;

  if (tab.$TST.collapsed) {
    tab.$TST.addState(Constants.kTAB_STATE_COLLAPSING);
    TabsStore.addCollapsingTab(tab);
  }
  else {
    tab.$TST.addState(Constants.kTAB_STATE_EXPANDING);
    tab.$TST.removeState(Constants.kTAB_STATE_COLLAPSED_DONE);
    TabsStore.addExpandingTab(tab);
  }

  onUpdated.dispatch(tab, { collapsed: info.cpllapsed });

  const onCanceled = () => {
    manager.removeListener(onCompleted);
  };

  nextFrame().then(() => {
    if (cancelled ||
        !TabsStore.ensureLivingTab(tab)) { // it was removed while waiting
      onCanceled();
      return;
    }

    //log('start animation for ', dumpTab(tab));
    onUpdating.dispatch(tab, {
      collapsed: tab.$TST.collapsed,
      anchor:    info.anchor,
      last:      info.last
    });

    tab.$TST.onEndCollapseExpandAnimation = (() => {
      if (cancelled) {
        onCanceled();
        return;
      }

      //log('=> finish animation for ', dumpTab(tab));
      tab.$TST.removeState(Constants.kTAB_STATE_COLLAPSING);
      tab.$TST.removeState(Constants.kTAB_STATE_EXPANDING);
      TabsStore.removeCollapsingTab(tab);
      TabsStore.removeExpandingTab(tab);

      // The collapsed state of the tab can be changed by different trigger,
      // so we must respect the actual status of the tab, instead of the
      // "expected status" given via arguments.
      if (tab.$TST.collapsed)
        tab.$TST.addState(Constants.kTAB_STATE_COLLAPSED_DONE);
      else
        tab.$TST.removeState(Constants.kTAB_STATE_COLLAPSED_DONE);

      onUpdated.dispatch(tab, {
        collapsed: tab.$TST.collapsed
      });
    });
    tab.$TST.onEndCollapseExpandAnimation.timeout = setTimeout(() => {
      if (cancelled ||
          !TabsStore.ensureLivingTab(tab) ||
          !tab.$TST.onEndCollapseExpandAnimation) {
        onCanceled();
        return;
      }
      delete tab.$TST.onEndCollapseExpandAnimation.timeout;
      tab.$TST.onEndCollapseExpandAnimation();
      delete tab.$TST.onEndCollapseExpandAnimation;
    }, configs.collapseDuration);
  });
}

const BUFFER_KEY_PREFIX = 'collapse-expand-';

BackgroundConnection.onMessage.addListener(async message => {
  switch (message.type) {
    case Constants.kCOMMAND_NOTIFY_SUBTREE_COLLAPSED_STATE_CHANGED: {
      if (BackgroundConnection.handleBufferedMessage(message, `${BUFFER_KEY_PREFIX}${message.tabId}`))
        return;
      await Tab.waitUntilTracked(message.tabId, { element: true });
      const tab = Tab.get(message.tabId);
      const lastMessage = BackgroundConnection.fetchBufferedMessage(message.type, `${BUFFER_KEY_PREFIX}${message.tabId}`);
      if (!tab ||
          !lastMessage)
        return;
      if (lastMessage.collapsed)
        tab.$TST.addState(Constants.kTAB_STATE_SUBTREE_COLLAPSED);
      else
        tab.$TST.removeState(Constants.kTAB_STATE_SUBTREE_COLLAPSED);
      tab.$TST.invalidateElement(TabInvalidationTarget.Twisty | TabInvalidationTarget.Tooltip);
    }; break;

    case Constants.kCOMMAND_NOTIFY_TAB_COLLAPSED_STATE_CHANGED: {
      if (BackgroundConnection.handleBufferedMessage(message, `${BUFFER_KEY_PREFIX}${message.tabId}`))
        return;
      await Tab.waitUntilTracked(message.tabId, { element: true });
      const tab = Tab.get(message.tabId);
      const lastMessage = BackgroundConnection.fetchBufferedMessage(message.type, `${BUFFER_KEY_PREFIX}${message.tabId}`);
      if (!tab ||
          !lastMessage)
        return;
      setCollapsed(tab, {
        collapsed: lastMessage.collapsed,
        justNow:   lastMessage.justNow,
        anchor:    Tab.get(lastMessage.anchorId),
        last:      lastMessage.last
      });
    }; break;
  }
});
