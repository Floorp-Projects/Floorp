# -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef RELEASE_OR_BETA
pref("devtools.debugger.new-debugger-frontend", false);
#else
pref("devtools.debugger.new-debugger-frontend", true);
#endif

// Enable the Debugger
pref("devtools.debugger.enabled", true);
pref("devtools.debugger.chrome-debugging-host", "localhost");
pref("devtools.debugger.chrome-debugging-port", 6080);
pref("devtools.debugger.chrome-debugging-websocket", false);
pref("devtools.debugger.remote-host", "localhost");
pref("devtools.debugger.remote-timeout", 20000);
pref("devtools.debugger.pause-on-exceptions", false);
pref("devtools.debugger.ignore-caught-exceptions", true);
pref("devtools.debugger.source-maps-enabled", true);
pref("devtools.debugger.client-source-maps-enabled", true);
pref("devtools.debugger.pretty-print-enabled", true);
pref("devtools.debugger.auto-pretty-print", false);
pref("devtools.debugger.auto-black-box", true);
pref("devtools.debugger.workers", false);

// The default Debugger UI settings
pref("devtools.debugger.ui.panes-workers-and-sources-width", 200);
pref("devtools.debugger.ui.panes-instruments-width", 300);
pref("devtools.debugger.ui.panes-visible-on-startup", false);
pref("devtools.debugger.ui.variables-sorting-enabled", true);
pref("devtools.debugger.ui.variables-only-enum-visible", false);
pref("devtools.debugger.ui.variables-searchbox-visible", false);
pref("devtools.debugger.call-stack-visible", false);
pref("devtools.debugger.scopes-visible", false);
pref("devtools.debugger.start-panel-collapsed", false);
pref("devtools.debugger.end-panel-collapsed", false);
pref("devtools.debugger.tabs", "[]");
pref("devtools.debugger.pending-selected-location", "{}");

