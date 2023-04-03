/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
import '/extlib/l10n.js';

import {
  log,
  configs
} from '/common/common.js';

import * as MetricsData from '/common/metrics-data.js';
import * as TabsStore from '/common/tabs-store.js';

import Tab from '/common/Tab.js';

import * as BackgroundConnection from './background-connection.js';
import * as Sidebar from './sidebar.js';
import './collapse-expand.js';
import './mouse-event-listener.js';
import './tab-context-menu.js';
import './tst-api-frontend.js';

log.context = 'Sidebar-?';

MetricsData.add('Loaded');

window.addEventListener('load', Sidebar.init, { once: true });

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
window.BackgroundConnection = BackgroundConnection;
window.configs = configs;
