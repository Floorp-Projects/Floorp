/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import Configs from '/extlib/Configs.js';
import EventListenerManager from '/extlib/EventListenerManager.js';

import * as Constants from './constants.js';

export const DEVICE_SPECIFIC_CONFIG_KEYS = mapAndFilter(`
  chunkedSyncDataLocal0
  chunkedSyncDataLocal1
  chunkedSyncDataLocal2
  chunkedSyncDataLocal3
  chunkedSyncDataLocal4
  chunkedSyncDataLocal5
  chunkedSyncDataLocal6
  chunkedSyncDataLocal7
  lastConfirmedToCloseTabs
  lastDragOverSidebarOwnerWindowId
  lastDraggedTabs
  loggingConnectionMessages
  loggingQueries
  migratedBookmarkUrls
  requestingPermissions
  requestingPermissionsNatively
  syncAvailableNotified
  syncDeviceInfo
  syncDevicesLocalCache
  syncEnabled
  syncLastMessageTimestamp
  syncOtherDevicesDetected
`.trim().split('\n'), key => {
  key = key.trim();
  return key && key.indexOf('//') != 0 && key;
});

const localKeys = DEVICE_SPECIFIC_CONFIG_KEYS.concat(mapAndFilter(`
  APIEnabled
  accelKey
  baseIndent
  cachedExternalAddons
  colorScheme
  debug
  enableLinuxBehaviors
  enableMacOSBehaviors
  enableWindowsBehaviors
  faviconizedTabScale
  grantedExternalAddonPermissions
  grantedRemovingTabIds
  incognitoAllowedExternalAddons
  logFor
  logTimestamp
  maximumDelayForBug1561879
  minimumIntervalToProcessDragoverEvent
  minIndent
  notifiedFeaturesVersion
  optionsExpandedGroups
  optionsExpandedSections
  sidebarPosition
  sidebarVirtuallyClosedWindows
  sidebarVirtuallyOpenedWindows
  startDragTimeout
  style
  subMenuCloseDelay
  subMenuOpenDelay
  testKey
  userStyleRulesFieldHeight
  userStyleRulesFieldTheme
`.trim().split('\n'), key => {
  key = key.trim();
  return key && key.indexOf('//') != 0 && key;
}));

export const configs = new Configs({
  optionsExpandedSections: ['section-appearance'],
  optionsExpandedGroups: [],

  // appearance
  sidebarPosition: Constants.kTABBAR_POSITION_AUTO,
  sidebarPositionRighsideNotificationShown: false,
  sidebarPositionOptionNotificationTimeout: 20 * 1000,

  style:
    /^Mac/i.test(navigator.platform) ? 'sidebar' :
      (() => {
        const matched = navigator.userAgent.match(/Firefox\/(\d+)\.\d+/);
        return (matched && parseInt(matched[1]) >= 89) ? 'proton' : 'photon'
      })(),
  colorScheme: /^Linux/i.test(navigator.platform) ? 'system-color' : 'photon' ,
  iconColor: 'auto',
  indentLine: 'auto',

  unrepeatableBGImageAspectRatio: 4,

  faviconizePinnedTabs: true,
  maxFaviconizedPinnedTabsInOneRow: 0, // auto
  faviconizedTabScale: 1.75,
  maxPinnedTabsRowsAreaPercentage: 50,

  counterRole: Constants.kCOUNTER_ROLE_CONTAINED_TABS,

  baseIndent: 12,
  minIndent: Constants.kDEFAULT_MIN_INDENT,
  maxTreeLevel: -1,
  indentAutoShrink: true,
  indentAutoShrinkOnlyForVisible: true,
  labelOverflowStyle: 'fade',

  showContextualIdentitiesSelector: false,
  showNewTabActionSelector: true,
  longPressOnNewTabButton: Constants.kCONTEXTUAL_IDENTITY_SELECTOR,
  zoomable: false,
  showCollapsedDescendantsByTooltip: true,

  showDialogInSidebar: false,

  suppressGapFromShownOrHiddenToolbarOnlyOnMouseOperation: true,
  suppressGapFromShownOrHiddenToolbarOnFullScreen: false,
  suppressGapFromShownOrHiddenToolbarOnNewTab: true,
  suppressGapFromShownOrHiddenToolbarInterval: 50,
  suppressGapFromShownOrHiddenToolbarTiemout: 500,
  cancelGapSuppresserHoverDelay: 1000, // msec


  // context menu
  emulateDefaultContextMenu: true,

  context_reloadTree: true,
  context_reloadDescendants: false,
  context_closeTree: true,
  context_closeDescendants: false,
  context_closeOthers: false,
  context_collapseTree: false,
  context_collapseTreeRecursively: true,
  context_collapseAll: true,
  context_expandTree: false,
  context_expandTreeRecursively: true,
  context_expandAll: true,
  context_bookmarkTree: true,
  context_sendTreeToDevice: false,

  context_topLevel_reloadTree: false,
  context_topLevel_reloadDescendants: false,
  context_topLevel_closeTree: false,
  context_topLevel_closeDescendants: false,
  context_topLevel_closeOthers: false,
  context_topLevel_collapseTree: false,
  context_topLevel_collapseTreeRecursively: false,
  context_topLevel_collapseAll: false,
  context_topLevel_expandTree: false,
  context_topLevel_expandTreeRecursively: false,
  context_topLevel_expandAll: false,
  context_topLevel_bookmarkTree: false,
  context_topLevel_sendTreeToDevice: true,

  context_collapsed: false,
  context_pinnedTab: false,
  context_unpinnedTab: false,

  context_openAllBookmarksWithStructure: true,
  context_openAllBookmarksWithStructureRecursively: false,

  openAllBookmarksWithStructureDiscarded: true,


  // tree behavior
  shouldDetectClickOnIndentSpaces: true,

  autoCollapseExpandSubtreeOnAttach: true,
  autoCollapseExpandSubtreeOnSelect: true,
  autoCollapseExpandSubtreeOnSelectExceptActiveTabRemove: true,

  treeDoubleClickBehavior: Constants.kTREE_DOUBLE_CLICK_BEHAVIOR_NONE,

  autoExpandIntelligently: true,
  unfocusableCollapsedTab: true,
  autoExpandOnTabSwitchingShortcuts: true,
  autoExpandOnTabSwitchingShortcutsDelay: 800,
  autoExpandOnLongHover: true,
  autoExpandOnLongHoverDelay: 500,
  autoExpandOnLongHoverRestoreIniitalState: true,

  accelKey: '',

  skipCollapsedTabsForTabSwitchingShortcuts: false,

  syncParentTabAndOpenerTab: true,

  dropLinksOnTabBehavior: Constants.kDROPLINK_ASK,

  tabDragBehavior:      Constants.kDRAG_BEHAVIOR_MOVE | Constants.kDRAG_BEHAVIOR_TEAR_OFF | Constants.kDRAG_BEHAVIOR_ENTIRE_TREE,
  tabDragBehaviorShift: Constants.kDRAG_BEHAVIOR_MOVE | Constants.kDRAG_BEHAVIOR_ENTIRE_TREE | Constants.kDRAG_BEHAVIOR_ALLOW_BOOKMARK,
  showTabDragBehaviorNotification: true,
  guessDraggedNativeTabs: true,
  ignoreTabDropNearSidebarArea: true,

  fixupTreeOnTabVisibilityChanged: false,
  fixupOrderOfTabsFromOtherDevice: true,

  scrollToExpandedTree: true,

  spreadMutedStateOnlyToSoundPlayingTabs: true,


  // tab bunches
  tabBunchesDetectionTimeout: 100,
  tabBunchesDetectionDelayOnNewWindow: 500,
  autoGroupNewTabsFromBookmarks: true,
  tabsFromSameFolderMinThresholdPercentage: 50,
  autoGroupNewTabsFromOthers: false,
  autoGroupNewTabsFromPinned: true,
  groupTabTemporaryStateForNewTabsFromBookmarks: Constants.kGROUP_TAB_TEMPORARY_STATE_PASSIVE,
  groupTabTemporaryStateForNewTabsFromOthers: Constants.kGROUP_TAB_TEMPORARY_STATE_PASSIVE,
  groupTabTemporaryStateForChildrenOfPinned: Constants.kGROUP_TAB_TEMPORARY_STATE_PASSIVE,
  groupTabTemporaryStateForOrphanedTabs: Constants.kGROUP_TAB_TEMPORARY_STATE_AGGRESSIVE,
  renderTreeInGroupTabs: true,
  warnOnAutoGroupNewTabs: true,
  warnOnAutoGroupNewTabsWithListing: true,
  warnOnAutoGroupNewTabsWithListingMaxRows: 5,
  showAutoGroupOptionHint: true,


  // behavior around newly opened tabs
  insertNewChildAt: Constants.kINSERT_END,
  insertNewTabFromPinnedTabAt: Constants.kINSERT_NEXT_TO_LAST_RELATED_TAB,
  insertDroppedTabsAt: Constants.kINSERT_END,

  scrollToNewTabMode: Constants.kSCROLL_TO_NEW_TAB_IF_POSSIBLE,
  scrollLines: 3,

  autoAttach: true,
  autoAttachOnOpenedWithOwner: Constants.kNEWTAB_OPEN_AS_CHILD,
  autoAttachOnNewTabCommand: Constants.kNEWTAB_DO_NOTHING,
  autoAttachOnContextNewTabCommand: Constants.kNEWTAB_OPEN_AS_NEXT_SIBLING_WITH_INHERITED_CONTAINER,
  autoAttachOnNewTabButtonMiddleClick: Constants.kNEWTAB_OPEN_AS_CHILD,
  autoAttachOnNewTabButtonAccelClick: Constants.kNEWTAB_OPEN_AS_NEXT_SIBLING_WITH_INHERITED_CONTAINER,
  autoAttachOnDuplicated: Constants.kNEWTAB_OPEN_AS_NEXT_SIBLING,
  autoAttachSameSiteOrphan: Constants.kNEWTAB_OPEN_AS_CHILD,
  autoAttachOnOpenedFromExternal: Constants.kNEWTAB_DO_NOTHING,
  autoAttachOnAnyOtherTrigger: Constants.kNEWTAB_DO_NOTHING,
  guessNewOrphanTabAsOpenedByNewTabCommand: true,
  guessNewOrphanTabAsOpenedByNewTabCommandUrl: 'about:newtab',
  inheritContextualIdentityToChildTabMode: Constants.kCONTEXTUAL_IDENTITY_DEFAULT,
  inheritContextualIdentityToSameSiteOrphanMode: Constants.kCONTEXTUAL_IDENTITY_FROM_LAST_ACTIVE,
  inheritContextualIdentityToTabsFromExternalMode: Constants.kCONTEXTUAL_IDENTITY_DEFAULT,
  inheritContextualIdentityToTabsFromAnyOtherTriggerMode: Constants.kCONTEXTUAL_IDENTITY_DEFAULT,


  // behavior around closed tab
  parentTabOperationBehaviorMode:     Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_PARALLEL,
  //closeParentBehavior_insideSidebar_collapsed:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE, // permanently consistent
  closeParentBehavior_insideSidebar_expanded:   Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
  closeParentBehavior_outsideSidebar_collapsed: Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
  closeParentBehavior_outsideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
  closeParentBehavior_noSidebar_collapsed:      Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
  closeParentBehavior_noSidebar_expanded:       Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
  //moveParentBehavior_insideSidebar_collapsed:   Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE, // permanently consistent
  //moveParentBehavior_insideSidebar_expanded:    Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE, // permanently consistent
  moveParentBehavior_outsideSidebar_collapsed:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE,
  moveParentBehavior_outsideSidebar_expanded:   Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
  moveParentBehavior_noSidebar_collapsed:       Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
  moveParentBehavior_noSidebar_expanded:        Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
  moveTabsToBottomWhenDetachedFromClosedParent: false,
  promoteAllChildrenWhenClosedParentIsLastChild: true,
  successorTabControlLevel: Constants.kSUCCESSOR_TAB_CONTROL_IN_TREE,
  simulateSelectOwnerOnClose: true,
  simulateLockTabSizing: true,
  supportTabsMultiselect: typeof browser.menus.overrideContext == 'function',
  warnOnCloseTabs: true,
  warnOnCloseTabsNotificationTimeout: 20 * 1000,
  warnOnCloseTabsByClosebox: true,
  warnOnCloseTabsWithListing: true,
  warnOnCloseTabsWithListingMaxRows: 5,
  lastConfirmedToCloseTabs: 0,
  grantedRemovingTabIds: [],
  sidebarVirtuallyOpenedWindows: [], // for automated tests
  sidebarVirtuallyClosedWindows: [], // for automated tests


  // animation
  animation: true,
  animationForce: false,
  smoothScrollEnabled:  true,
  smoothScrollDuration: 150,
  burstDuration:    375,
  indentDuration:   200,
  collapseDuration: 150,
  outOfViewTabNotifyDuration: 750,
  subMenuOpenDelay: 300,
  subMenuCloseDelay: 300,


  // subpanel
  lastSelectedSubPanelProviderId: null,
  lastSubPanelHeight: 0,
  maxSubPanelSizeRatio: 0.66,


  // misc.
  showExpertOptions: false,
  bookmarkTreeFolderName: browser.i18n.getMessage('bookmarkFolder_label_default', ['%TITLE%', '%YEAR%', '%MONTH%', '%DATE%']),
  defaultBookmarkParentId: 'toolbar_____', // 'unfiled_____' for Firefox 83 and olders,
  defaultSearchEngine: 'https://www.google.com/search?q=%s',
  acceleratedTabOperations: true,
  acceleratedTabCreation: false,
  enableWorkaroundForBug1409262: false,
  enableWorkaroundForBug1548949: true,
  maximumDelayForBug1561879: 500,
  workaroundForBug1548949DroppedTabs: null,
  heartbeatInterval: 1000,
  connectionTimeoutDelay: 500,
  maximumAcceptableDelayForTabDuplication: 10 * 1000,
  maximumDelayUntilTabIsTracked: 10 * 60 * 1000,
  delayToBlockUserOperationForTabsRestoration: 1000,
  intervalToUpdateProgressForBlockedUserOperation: 50,
  delayToShowProgressForBlockedUserOperation: 1000,
  acceptableDelayForInternalFocusMoving: 150,
  delayForDuplicatedTabDetection: 0, // https://github.com/piroor/treestyletab/issues/2845
  delayToRetrySyncTabsOrder: 100,
  notificationTimeout: 10 * 1000,
  longPressDuration: 400,
  minimumIntervalToProcessDragoverEvent: 50,
  delayToApplyHighlightedState: 50,
  acceptableFlickerToIgnoreClickOnTabAndTabbar: 10,
  autoDiscardTabForUnexpectedFocus: true,
  autoDiscardTabForUnexpectedFocusDelay: 500,
  avoidDiscardedTabToBeActivatedIfPossible: false,
  undoMultipleTabsClose: true,
  allowDragNewTabButton: true,
  newTabButtonDragGestureModifiers: 'shift',
  migratedBookmarkUrls: [],
  lastDragOverSidebarOwnerWindowId: null,
  notifiedFeaturesVersion: 0,

  useCachedTree: true,

  // This should be removed after https://bugzilla.mozilla.org/show_bug.cgi?id=1388193
  // or https://bugzilla.mozilla.org/show_bug.cgi?id=1421329 become fixed.
  // Otherwise you need to set "svg.context-properties.content.enabled"="true" via "about:config".
  simulateSVGContextFill: true,

  requestingPermissions: null,
  requestingPermissionsNatively: null,
  lastDraggedTabs: null,

  // https://dxr.mozilla.org/mozilla-central/rev/2535bad09d720e71a982f3f70dd6925f66ab8ec7/browser/base/content/browser.css#137
  newTabAnimationDuration: 100,

  chunkedUserStyleRules0: '',
  chunkedUserStyleRules1: '',
  chunkedUserStyleRules2: '',
  chunkedUserStyleRules3: '',
  chunkedUserStyleRules4: '',
  chunkedUserStyleRules5: '',
  chunkedUserStyleRules6: '',
  chunkedUserStyleRules7: '',
  // obsolete, migrated to chunkedUserStyleRules0-5
  userStyleRules: `
/* Show title of unread tabs with red and italic font */
/*
:root.sidebar tab-item.unread .label-content {
  color: red !important;
  font-style: italic !important;
}
*/

/* Add private browsing indicator per tab */
/*
:root.sidebar tab-item.private-browsing tab-label:before {
  content: "🕶";
}
*/
`.trim(),
  userStyleRulesFieldHeight: '10em',
  userStyleRulesFieldTheme: 'auto',

  syncOtherDevicesDetected: false,
  syncAvailableNotified: false,
  syncAvailableNotificationTimeout: 20 * 1000,
  syncDeviceInfo: null,
  syncDevices:    {},
  syncDevicesLocalCache: {},
  syncDeviceExpirationDays: 14,
  // Must be same to "services.sync.engine.tabs.filteredUrls"
  syncUnsendableUrlPattern: '^(about:.*|resource:.*|chrome:.*|wyciwyg:.*|file:.*|blob:.*|moz-extension:.*)$',
  syncLastMessageTimestamp: 0,
  syncReceivedTabsNotificationTimeout: 20 * 1000,
  syncSentTabsNotificationTimeout: 5 * 1000,
  chunkedSyncData0: '',
  chunkedSyncData1: '',
  chunkedSyncData2: '',
  chunkedSyncData3: '',
  chunkedSyncData4: '',
  chunkedSyncData5: '',
  chunkedSyncData6: '',
  chunkedSyncData7: '',
  chunkedSyncDataLocal0: '',
  chunkedSyncDataLocal1: '',
  chunkedSyncDataLocal2: '',
  chunkedSyncDataLocal3: '',
  chunkedSyncDataLocal4: '',
  chunkedSyncDataLocal5: '',
  chunkedSyncDataLocal6: '',
  chunkedSyncDataLocal7: '',


  // Compatibility with other addons
  knownExternalAddons: [
    'multipletab@piro.sakura.ne.jp'
  ],
  cachedExternalAddons: [],
  grantedExternalAddonPermissions: {},
  incognitoAllowedExternalAddons: [],

  // This must be same to the redirect key of Container Bookmarks.
  // https://addons.mozilla.org/firefox/addon/container-bookmarks/
  containerRedirectKey: 'container',


  debug:     false,
  syncEnabled: true,
  APIEnabled: true,
  logTimestamp: true,
  loggingQueries: false,
  logFor: { // git grep configs.logFor | grep -v common.js | cut -d "'" -f 2 | sed -e "s/^/    '/" -e "s/$/': false,/"
    'background/api-tabs-listener': false,
    'background/background-cache': false,
    'background/background': false,
    'background/browser-action-menu': false,
    'background/commands': false,
    'background/context-menu': false,
    'background/handle-tab-bunches': false,
    'background/handle-misc': false,
    'background/handle-moved-tabs': false,
    'background/handle-new-tabs': false,
    'background/handle-removed-tabs': false,
    'background/handle-tab-focus': false,
    'background/handle-tab-multiselect': false,
    'background/handle-tree-changes': false,
    'background/migration': false,
    'background/successor-tab': false,
    'background/tab-context-menu': false,
    'background/tabs-group': false,
    'background/tabs-move': false,
    'background/tabs-open': false,
    'background/tree': false,
    'background/tree-structure': false,
    'common/Tab': false,
    'common/Window': false,
    'common/api-tabs': false,
    'common/bookmark': false,
    'common/contextual-identities': false,
    'common/dialog': false,
    'common/permissions': false,
    'common/sidebar-connection': false,
    'common/sync': false,
    'common/tabs-internal-operation': false,
    'common/tabs-update': false,
    'common/tree-behavior': false,
    'common/tst-api': false,
    'common/unique-id': false,
    'common/user-operation-blocker': false,
    'sidebar/background-connection': false,
    'sidebar/collapse-expand': false,
    'sidebar/drag-and-drop': false,
    'sidebar/event-utils': false,
    'sidebar/gap-canceller': false,
    'sidebar/indent': false,
    'sidebar/mouse-event-listener': false,
    'sidebar/pinned-tabs': false,
    'sidebar/scroll': false,
    'sidebar/sidebar-cache': false,
    'sidebar/sidebar-tabs': false,
    'sidebar/sidebar': false,
    'sidebar/size': false,
    'sidebar/subpanel': false,
    'sidebar/tab-context-menu': false
  },
  loggingConnectionMessages: false,
  enableLinuxBehaviors: false,
  enableMacOSBehaviors: false,
  enableWindowsBehaviors: false,


  // obsolete configs
  sidebarScrollbarPosition: null, // migrated to user stylesheet
  scrollbarMode: null, // migrated to user stylesheet
  suppressGapFromShownOrHiddenToolbar: null, // migrated to suppressGapFromShownOrHiddenToolbarOnFullScreen/NewTab
  fakeContextMenu: null, // migrated to emulateDefaultContextMenu
  context_closeTabOptions_closeTree: null, // migrated to context_topLevel_closeTree
  context_closeTabOptions_closeDescendants: null, // migrated to context_topLevel_closeDescendants
  context_closeTabOptions_closeOthers: null, // migrated to context_topLevel_closeOthers
  collapseExpandSubtreeByDblClick: null, // migrated to treeDoubleClickBehavior
  autoExpandOnCollapsedChildActive: null, // migrate to unfocusableCollapsedTab
  inheritContextualIdentityToNewChildTab: null, // migrated to inheritContextualIdentityToChildTabMode
  inheritContextualIdentityToSameSiteOrphan: null, // migrated to inheritContextualIdentityToSameSiteOrphanMode
  inheritContextualIdentityToTabsFromExternal: null, // migrated to inheritContextualIdentityToTabsFromExternalMode
  promoteFirstChildForClosedRoot:     null, // migrated to Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_INTELLIGENTLY of closeParentBehavior
  parentTabBehaviorForChanges:        null, // migrated to parentTabOperationBehaviorMode
  closeParentBehaviorMode: null, // migrated to parentTabOperationBehaviorMode
  closeParentBehavior:                null, // migrated to closeParentBehavior_insideSidebar_expanded
  closeParentBehavior_outsideSidebar: null, // migrated to closeParentBehavior_outsideSidebar_expanded
  closeParentBehavior_noSidebar:      null, // migrated to closeParentBehavior_noSidebar_expanded
  treatTreeAsExpandedOnClosedWithNoSidebar: null, // migrated to treatTreeAsExpandedOnClosed_noSidebar
  treatTreeAsExpandedOnClosed_outsideSidebar: null, // migrated to closeParentBehavior_noSidebar_expanded and closeParentBehavior_noSidebar_expanded
  treatTreeAsExpandedOnClosed_noSidebar: null, // migrated to closeParentBehavior_noSidebar_collapsed and moveParentBehavior_noSidebar_expanded
  moveFocusInTreeForClosedActiveTab: null, // migrated to "successorTabControlLevel"
  startDragTimeout: null, // migrated to longPressDuration
  simulateCloseTabByDblclick: null, // migrated to "treeDoubleClickBehavior=kTREE_DOUBLE_CLICK_BEHAVIOR_CLOSE"
  moveDroppedTabToNewWindowForUnhandledDragEvent: null, // see also: https://github.com/piroor/treestyletab/issues/1646 , migrated to tabDragBehavior
  // migrated to chunkedUserStyleRules0-5
  userStyleRules0: '',
  userStyleRules1: '',
  userStyleRules2: '',
  userStyleRules3: '',
  userStyleRules4: '',
  userStyleRules5: '',
  userStyleRules6: '',
  userStyleRules7: '',
  autoGroupNewTabsTimeout: null, // migrated to tabBunchesDetectionTimeout
  autoGroupNewTabsDelayOnNewWindow: null, // migrated to tabBunchesDetectionDelayOnNewWindow


  configsVersion: 0,

  testKey: 0 // for tests/utils.js
}, {
  localKeys
});

configs.$addLocalLoadedObserver((key, value) => {
  switch (key) {
    case 'syncEnabled':
      configs.sync = !!value;
      return;

    default:
      return;
  }
});

// cleanup old data
browser.storage.sync.remove(localKeys);

configs.$loaded.then(() => {
  EventListenerManager.debug = configs.debug;
  log.forceStore = false;
  if (!configs.debug)
    log.logs = [];
});


export function loadUserStyleRules() {
  return getChunkedConfig('chunkedUserStyleRules');
}

export function saveUserStyleRules(style) {
  return setChunkedConfig('chunkedUserStyleRules', style);
}

export function getChunkedConfig(key) {
  const chunks = [];
  let count = 0;
  while (true) {
    const slotKey = `${key}${count}`;
    if (!(slotKey in configs))
      break;
    chunks.push(configs[slotKey]);
    count++;
  }
  return joinChunkedStrings(chunks);
}

export function setChunkedConfig(key, value) {
  let slotsSize = 0;
  while (`${key}${slotsSize}` in configs.$default) {
    slotsSize++;
  }

  const chunks = chunkString(value, Constants.kSYNC_STORAGE_SAFE_QUOTA);
  if (chunks.length > slotsSize)
    throw new Error('too large data');

  [...chunks,
    ...Array.from(new Uint8Array(slotsSize), _ => '')]
    .slice(0, slotsSize)
    .forEach((chunk, index) => {
      const slotKey = `${key}${index}`;
      if (slotKey in configs)
        configs[slotKey] = chunk || '';
    });
}

function chunkString(input, maxBytes) {
  let binaryString = btoa(Array.from(new TextEncoder().encode(input), c => String.fromCharCode(c)).join(''));
  const chunks = [];
  while (binaryString.length > 0) {
    chunks.push(binaryString.slice(0, maxBytes));
    binaryString = binaryString.slice(maxBytes);
  }
  return chunks;
}

function joinChunkedStrings(chunks) {
  try {
    const buffer = Uint8Array.from(atob(chunks.join('')).split('').map(bytes => bytes.charCodeAt(0)));
    return new TextDecoder().decode(buffer);
  }
  catch(_error) {
    return '';
  }
}


shouldApplyAnimation.onChanged = new EventListenerManager();
shouldApplyAnimation.prefersReducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
shouldApplyAnimation.prefersReducedMotion.addListener(_event => {
  shouldApplyAnimation.onChanged.dispatch(shouldApplyAnimation());
});
configs.$addObserver(key => {
  switch(key) {
    case 'animation':
    case 'animationForce':
      shouldApplyAnimation.onChanged.dispatch(shouldApplyAnimation());
      break;

    case 'debug':
      EventListenerManager.debug = configs[key];
      break;
  }
});

// Some animation effects like smooth scrolling are still active even if it matches to "prefers-reduced-motion: reduce".
// So this function provides ability to ignore the media query result.
export function shouldApplyAnimation(configOnly = false) {
  if (!configs.animation)
    return false;
  return configOnly || configs.animationForce || !shouldApplyAnimation.prefersReducedMotion.matches;
}


export function log(module, ...args)
{
  const isModuleLog = module in configs.$default.logFor;
  const message    = isModuleLog ? args.shift() : module ;
  const useConsole = configs && configs.debug && (!isModuleLog || configs.logFor[module]);
  const logging    = useConsole || log.forceStore;
  if (!logging)
    return;

  args = args.map(arg => typeof arg == 'function' ? arg() : arg);

  const nest = (new Error()).stack.split('\n').length;
  let indent = '';
  for (let i = 0; i < nest; i++) {
    indent += ' ';
  }
  if (isModuleLog)
    module = `${module}: `
  else
    module = '';

  const timestamp = configs.logTimestamp ? `${getTimeStamp()} ` : '';
  const line = `tst<${log.context}>: ${timestamp}${module}${indent}${message}`;
  if (useConsole)
    console.log(line, ...args);

  log.logs.push(`${line} ${args.reduce((output, arg, index) => {
    output += `${index == 0 ? '' : ', '}${uneval(arg)}`;
    return output;
  }, '')}`);
  log.logs = log.logs.slice(-log.max);
}
log.context = '?';
log.max  = 2000;
log.logs = [];
log.forceStore = true;

// uneval() is no more available after https://bugzilla.mozilla.org/show_bug.cgi?id=1565170
function uneval(value) {
  switch (typeof value) {
    case 'undefined':
      return 'undefined';

    case 'function':
      return value.toString();

    case 'object':
      if (!value)
        return 'null';
    default:
      try {
        return JSON.stringify(value);
      }
      catch(e) {
        return `${String(value)} (couldn't be stringified due to an error: ${String(e)})`;
      }
  }
}

function getTimeStamp() {
  const time = new Date();
  const hours = `0${time.getHours()}`.substr(-2);
  const minutes = `0${time.getMinutes()}`.substr(-2);
  const seconds = `0${time.getSeconds()}`.substr(-2);
  const milliseconds = `00${time.getMilliseconds()}`.substr(-3);
  return `${hours}:${minutes}:${seconds}.${milliseconds}`;
}

configs.$logger = log;

export function dumpTab(tab) {
  if (!configs || !configs.debug)
    return '';
  if (!tab)
    return '<NULL>';
  return `#${tab.id}(${!!tab.$TST ? 'tracked' : '!tracked'})`;
}

export async function wait(task = 0, timeout = 0) {
  if (typeof task != 'function') {
    timeout = task;
    task    = null;
  }
  return new Promise((resolve, _reject) => {
    setTimeout(async () => {
      if (task)
        await task();
      resolve();
    }, timeout);
  });
}

export function nextFrame() {
  return new Promise((resolve, _reject) => {
    window.requestAnimationFrame(resolve);
  });
}

export async function notify({ icon, title, message, timeout, url } = {}) {
  const id = await browser.notifications.create({
    type:    'basic',
    iconUrl: icon || Constants.kNOTIFICATION_DEFAULT_ICON,
    title,
    message
  });

  let onClicked;
  let onClosed;
  return new Promise(async (resolve, _reject) => {
    let resolved = false;

    onClicked = notificationId => {
      if (notificationId != id)
        return;
      if (url) {
        browser.tabs.create({
          url
        });
      }
      resolved = true;
      resolve(true);
    };
    browser.notifications.onClicked.addListener(onClicked);

    onClosed = notificationId => {
      if (notificationId != id)
        return;
      if (!resolved) {
        resolved = true;
        resolve(false);
      }
    };
    browser.notifications.onClosed.addListener(onClosed);

    if (typeof timeout != 'number')
      timeout = configs.notificationTimeout;
    if (timeout >= 0) {
      await wait(timeout);
    }
    await browser.notifications.clear(id);
    if (!resolved)
      resolve(false);
  }).then(clicked => {
    browser.notifications.onClicked.removeListener(onClicked);
    onClicked = null;
    browser.notifications.onClosed.removeListener(onClosed);
    onClosed = null;
    return clicked;
  });
}

export function compareAsNumber(a, b) {
  return a - b;
}


// Helper functions for optimization
// Originally implemented by @bb010g at
// https://github.com/piroor/treestyletab/pull/2368/commits/9d184c4ac6c9977d2557cd17cec8c2a0f21dd527

// For better performance the callback function must return "undefined"
// when the item should not be included. "null", "false", and other false
// values will be included to the mapped result.
export function mapAndFilter(values, mapper) {
  /* This function logically equals to:
  return values.reduce((mappedValues, value) => {
    value = mapper(value);
    if (value !== undefined)
      mappedValues.push(value);
    return mappedValues;
  }, []);
  */
  const maxi = ('length' in values ? values.length : values.size) >>> 0; // define as unsigned int
  const mappedValues = new Array(maxi); // prepare with enough size at first, to avoid needless re-allocation
  let count = 0,
      value, // this must be defined outside of the loop, to avoid needless re-allocation
      mappedValue; // this must be defined outside of the loop, to avoid needless re-allocation
  for (value of values) {
    mappedValue = mapper(value);
    if (mappedValue !== undefined)
      mappedValues[count++] = mappedValue;
  }
  mappedValues.length = count; // shrink the array at last
  return mappedValues;
}

export function mapAndFilterUniq(values, mapper, options = {}) {
  const mappedValues = new Set();
  let value, // this must be defined outside of the loop, to avoid needless re-allocation
      mappedValue; // this must be defined outside of the loop, to avoid needless re-allocation
  for (value of values) {
    mappedValue = mapper(value);
    if (mappedValue !== undefined)
      mappedValues.add(mappedValue);
  }
  return options.set ? mappedValues : Array.from(mappedValues);
}

export function countMatched(values, matcher) {
  /* This function logically equals to:
  return values.reduce((count, value) => {
    if (matcher(value))
      count++;
    return count;
  }, 0);
  */
  let count = 0,
      value; // this must be defined outside of the loop, to avoid needless re-allocation
  for (value of values) {
    if (matcher(value))
      count++;
  }
  return count;
}

export function toLines(values, mapper, separator = '\n') {
  /* This function logically equals to:
  return values.reduce((output, value, index) => {
    output += `${index == 0 ? '' : '\n'}${mapper(value)}`;
    return output;
  }, '');
  */
  const maxi = values.length >>> 0; // define as unsigned int
  let i = 0,
      lines = '';
  while (i < maxi) { // use "while" loop instead "for" loop, for better performance
    lines += `${i == 0 ? '' : separator}${mapper(values[i])}`;
    i++;
  }
  return lines;
}

export async function sha1sum(string) {
  const encoder = new TextEncoder();
  const data = encoder.encode(string);
  const hashBuffer = await crypto.subtle.digest('SHA-1', data);
  const hashArray = Array.from(new Uint8Array(hashBuffer));
  const hashHex = hashArray.map(byte => byte.toString(16).padStart(2, '0')).join('');
  return hashHex;
}

export function sanitizeForHTMLText(text) {
  return text
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

export function sanitizeAccesskeyMark(label) {
  return String(label || '').replace(/\(&[a-z]\)|&([a-z])/gi, '$1');
}


export function isLinux() {
  return configs.enableLinuxBehaviors || /^Linux/i.test(navigator.platform);
}

export function isMacOS() {
  return configs.enableMacOSBehaviors || /^Mac/i.test(navigator.platform);
}

export function isWindows() {
  return configs.enableWindowsBehaviors || /^Win/i.test(navigator.platform);
}
