/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
  configs,
  shouldApplyAnimation
} from '/common/common.js';

import * as Constants from '/common/constants.js';
import * as TabsStore from '/common/tabs-store.js';

import Tab from '/common/Tab.js';

import * as BackgroundConnection from './background-connection.js';
import * as CollapseExpand from './collapse-expand.js';

function log(...args) {
  internalLogger('sidebar/indent', ...args);
}

let mPromisedInitializedResolver;
let mPromisedInitialized = new Promise((resolve, _reject) => {
  mPromisedInitializedResolver = resolve;
});
let mIndentDefinition;
let mLastMaxLevel  = -1;
let mLastMaxIndent = -1;
let mTargetWindow;
let mTabBar;

export function init() {
  mTargetWindow = TabsStore.getCurrentWindowId();
  mTabBar       = document.querySelector('#tabbar');

  window.addEventListener('resize', reserveToUpdateIndent);

  mPromisedInitializedResolver();
  mPromisedInitialized = mPromisedInitializedResolver = null;
}

export function updateRestoredTree(cachedIndent) {
  updateVisualMaxTreeLevel();
  update({
    force: true,
    cache: cachedIndent
  });
}

export function update(options = {}) {
  if (!options.cache) {
    const maxLevel  = getMaxTreeLevel(mTargetWindow);
    const maxIndent = mTabBar.getBoundingClientRect().width * (0.33);
    if (maxLevel <= mLastMaxLevel &&
        maxIndent == mLastMaxIndent &&
        !options.force)
      return;

    mLastMaxLevel  = maxLevel + 5;
    mLastMaxIndent = maxIndent;
  }
  else {
    mLastMaxLevel  = options.cache.lastMaxLevel;
    mLastMaxIndent = options.cache.lastMaxIndent;
  }

  if (!mIndentDefinition) {
    mIndentDefinition = document.createElement('style');
    mIndentDefinition.setAttribute('type', 'text/css');
    document.head.appendChild(mIndentDefinition);
  }

  if (options.cache &&
      options.cache.definition) {
    mIndentDefinition.textContent = options.cache.definition;
  }
  else {
    const indentToSelectors = {};
    const defaultIndentToSelectors = {};
    const indentUnitDefinitions = [];
    for (let i = 0; i <= mLastMaxLevel; i++) {
      generateIndentAndSelectorsForMaxLevel(i, indentToSelectors, defaultIndentToSelectors, indentUnitDefinitions);
    }

    const definitions = [];
    for (const indentSet of [defaultIndentToSelectors, indentToSelectors]) {
      const indents = Object.keys(indentSet);
      indents.sort((aA, aB) => parseInt(aA) - parseInt(aB));
      for (const indent of indents) {
        definitions.push(`${indentSet[indent].join(',\n')} { --tab-indent: ${indent}; }`);
      }
    }
    mIndentDefinition.textContent = indentUnitDefinitions.concat(definitions).join('\n');
  }
}
function generateIndentAndSelectorsForMaxLevel(maxLevel, indentToSelectors, defaultIndentToSelectors, indentUnitDefinitions) {
  const indentUnit = calculateIndentUnit(maxLevel);

  let configuredMaxLevel = configs.maxTreeLevel;
  if (configuredMaxLevel < 0)
    configuredMaxLevel = Number.MAX_SAFE_INTEGER;

  const base = `:root[${Constants.kMAX_TREE_LEVEL}="${maxLevel}"]:not(.initializing) .tab:not(.${Constants.kTAB_STATE_PINNED}):not(.${Constants.kTAB_STATE_COLLAPSED_DONE})[${Constants.kLEVEL}]`;

  // default indent for unhandled (deep) level tabs
  const defaultIndent = `${Math.min(maxLevel + 1, configuredMaxLevel) * indentUnit}px`;
  if (!defaultIndentToSelectors[defaultIndent])
    defaultIndentToSelectors[defaultIndent] = [];
  defaultIndentToSelectors[defaultIndent].push(`${base}:not([${Constants.kLEVEL}="0"])`);

  indentUnitDefinitions.push(`:root[${Constants.kMAX_TREE_LEVEL}="${maxLevel}"]:not(.initializing)  {
    --indent-size: ${indentUnit}px;
  }`);

  for (let level = 1; level <= maxLevel; level++) {
    const indent = `${Math.min(level, configuredMaxLevel) * indentUnit}px`;
    if (!indentToSelectors[indent])
      indentToSelectors[indent] = [];
    indentToSelectors[indent].push(`${base}[${Constants.kLEVEL}="${level}"]`);
  }
}
function calculateIndentUnit(maxLevel) {
  const minIndent  = Math.max(Constants.kDEFAULT_MIN_INDENT, configs.minIndent);
  return Math.min(configs.baseIndent, Math.max(Math.floor(mLastMaxIndent / maxLevel), minIndent));
}

export function getCacheInfo() {
  return {
    lastMaxLevel:  mLastMaxLevel,
    lastMaxIndent: mLastMaxIndent,
    definition:    mIndentDefinition.textContent
  };
}


export async function reserveToUpdateVisualMaxTreeLevel() {
  if (mPromisedInitialized)
    await mPromisedInitialized;
  log('reserveToUpdateVisualMaxTreeLevel');
  if (updateVisualMaxTreeLevel.waiting) {
    clearTimeout(updateVisualMaxTreeLevel.waiting);
    delete updateVisualMaxTreeLevel.waiting;
  }

  if (!shouldApplyAnimation()) {
    updateVisualMaxTreeLevel();
    return;
  }

  updateVisualMaxTreeLevel.waiting = setTimeout(() => {
    delete updateVisualMaxTreeLevel.waiting;
    updateVisualMaxTreeLevel();
  }, configs.collapseDuration * 1.5);
}

function updateVisualMaxTreeLevel() {
  const maxLevel = getMaxTreeLevel(mTargetWindow, {
    onlyVisible: configs.indentAutoShrinkOnlyForVisible
  });
  log('updateVisualMaxTreeLevel ', { maxLevel });
  document.documentElement.setAttribute(Constants.kMAX_TREE_LEVEL, Math.max(1, maxLevel));
}

function getMaxTreeLevel(windowId, options = {}) {
  if (typeof options != 'object')
    options = {};
  const tabs = options.onlyVisible ?
    Tab.getVisibleTabs(windowId, { ordered: false }) :
    Tab.getTabs(windowId, { ordered: false }) ;
  let maxLevel = Math.max(...tabs.map(tab => parseInt(tab.$TST.attributes[Constants.kLEVEL] || 0)));
  if (configs.maxTreeLevel > -1)
    maxLevel = Math.min(maxLevel, configs.maxTreeLevel);
  return maxLevel;
}

async function reserveToUpdateIndent() {
  if (mPromisedInitialized)
    await mPromisedInitialized;
  log('reserveToUpdateIndent');
  if (reserveToUpdateIndent.waiting)
    clearTimeout(reserveToUpdateIndent.waiting);
  const delay = shouldApplyAnimation() ? Math.max(configs.indentDuration, configs.collapseDuration) * 1.5 : 100;
  reserveToUpdateIndent.waiting = setTimeout(() => {
    delete reserveToUpdateIndent.waiting;
    update();
  }, delay);
}


CollapseExpand.onUpdated.addListener((_tab, _options) => {
  if (configs.indentAutoShrink &&
      configs.indentAutoShrinkOnlyForVisible)
    reserveToUpdateVisualMaxTreeLevel();
});

const BUFFER_KEY_PREFIX = 'indent-';

BackgroundConnection.onMessage.addListener(async message => {
  switch (message.type) {
    case Constants.kCOMMAND_NOTIFY_TAB_CREATED:
    case Constants.kCOMMAND_NOTIFY_TAB_REMOVING:
      log('listen: ', message.type);
      reserveToUpdateVisualMaxTreeLevel();
      break;

    case Constants.kCOMMAND_NOTIFY_TAB_SHOWN:
    case Constants.kCOMMAND_NOTIFY_TAB_HIDDEN:
    case Constants.kCOMMAND_NOTIFY_CHILDREN_CHANGED:
      log('listen: ', message.type);
      reserveToUpdateIndent();
      reserveToUpdateVisualMaxTreeLevel();
      break;

    case Constants.kCOMMAND_NOTIFY_TAB_LEVEL_CHANGED: {
      if (BackgroundConnection.handleBufferedMessage(message, `${BUFFER_KEY_PREFIX}${message.tabId}`))
        return;
      await Tab.waitUntilTracked(message.tabId, { element: true });
      const tab = Tab.get(message.tabId);
      const lastMessage = BackgroundConnection.fetchBufferedMessage(message.type, `${BUFFER_KEY_PREFIX}${message.tabId}`);
      log('listen: ', message.type, tab, lastMessage);
      if (!tab ||
          !lastMessage)
        return;
      if (tab.$TST.getAttribute(Constants.kLEVEL) != lastMessage.level)
        tab.$TST.setAttribute(Constants.kLEVEL, lastMessage.level);
      reserveToUpdateIndent();
    }; break;

    case Constants.kCOMMAND_NOTIFY_TAB_COLLAPSED_STATE_CHANGED:
      if (!shouldApplyAnimation())
        updateVisualMaxTreeLevel();
      break;
  }
});
