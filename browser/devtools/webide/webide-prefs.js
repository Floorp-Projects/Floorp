# -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

pref("devtools.webide.showProjectEditor", true);
pref("devtools.webide.templatesURL", "https://code.cdn.mozilla.net/templates/list.json");
pref("devtools.webide.autoinstallADBHelper", true);
pref("devtools.webide.autoinstallFxdtAdapters", true);
pref("devtools.webide.autoConnectRuntime", true);
pref("devtools.webide.restoreLastProject", true);
pref("devtools.webide.enableLocalRuntime", false);
pref("devtools.webide.enableRuntimeConfiguration", false);
pref("devtools.webide.addonsURL", "https://ftp.mozilla.org/pub/mozilla.org/labs/fxos-simulator/index.json");
pref("devtools.webide.simulatorAddonsURL", "https://ftp.mozilla.org/pub/mozilla.org/labs/fxos-simulator/#VERSION#/#OS#/fxos_#SLASHED_VERSION#_simulator-#OS#-latest.xpi");
pref("devtools.webide.simulatorAddonID", "fxos_#SLASHED_VERSION#_simulator@mozilla.org");
pref("devtools.webide.simulatorAddonRegExp", "fxos_(.*)_simulator@mozilla\\.org$");
pref("devtools.webide.adbAddonURL", "https://ftp.mozilla.org/pub/mozilla.org/labs/fxos-simulator/adb-helper/#OS#/adbhelper-#OS#-latest.xpi");
pref("devtools.webide.adbAddonID", "adbhelper@mozilla.org");
pref("devtools.webide.adaptersAddonURL", "https://ftp.mozilla.org/pub/mozilla.org/labs/valence/#OS#/valence-#OS#-latest.xpi");
pref("devtools.webide.adaptersAddonID", "fxdevtools-adapters@mozilla.org");
pref("devtools.webide.monitorWebSocketURL", "ws://localhost:9000");
pref("devtools.webide.lastConnectedRuntime", "");
pref("devtools.webide.lastSelectedProject", "");
pref("devtools.webide.logSimulatorOutput", false);
pref("devtools.webide.widget.autoinstall", true);
#ifdef MOZ_DEV_EDITION
pref("devtools.webide.widget.enabled", true);
pref("devtools.webide.widget.inNavbarByDefault", true);
#else
pref("devtools.webide.widget.enabled", false);
pref("devtools.webide.widget.inNavbarByDefault", false);
#endif
pref("devtools.webide.zoom", "1");
pref("devtools.webide.busyTimeout", 10000);
pref("devtools.webide.sidebars", false);
