/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

import {
  log,
  configs
} from '/common/common.js';
import * as MetricsData from '/common/metrics-data.js';
import * as SidebarConnection from '/common/sidebar-connection.js';
import * as TabsStore from '/common/tabs-store.js';

import Tab from '/common/Tab.js';

import * as Background from './background.js';
import './handle-misc.js';
import './handle-moved-tabs.js';
import './handle-new-tabs.js';
import './handle-removed-tabs.js';
import './handle-tab-bunches.js';
import './handle-tab-focus.js';
import './handle-tab-multiselect.js';
import './handle-tree-changes.js';

log.context = 'BG';

MetricsData.add('index: Loaded');

window.addEventListener('DOMContentLoaded', Background.init, { once: true });

window.dumpMetricsData = () => {
  return MetricsData.toString();
};
window.dumpLogs = () => {
  return log.logs.join('\n');
};

// for old debugging method
window.log = log;
window.gMetricsData = MetricsData;
window.Tab = Tab;
window.TabsStore = TabsStore;
window.SidebarConnection = SidebarConnection;
window.configs = configs;
