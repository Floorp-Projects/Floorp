/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
} from '/common/common.js';
import * as ApiTabs from '/common/api-tabs.js';

import Tab from '/common/Tab.js';

function log(...args) {
  internalLogger('background/handle-tab-multiselect', ...args);
}

Tab.onUpdated.addListener((tab, info, options = {}) => {
  if (!('highlighted' in info) ||
      !tab.$TST.subtreeCollapsed ||
      tab.$TST.collapsed ||
      !tab.$TST.multiselected ||
      options.inheritHighlighted === false)
    return;

  const collapsedDescendants = tab.$TST.descendants;
  log('inherit highlighted state from root visible tab: ', {
    highlighted: info.highlighted,
    collapsedDescendants
  });
  for (const descendant of collapsedDescendants) {
    browser.tabs.update(descendant.id, {
      highlighted: info.highlighted,
      active:      descendant.active
    }).catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
  }
});
