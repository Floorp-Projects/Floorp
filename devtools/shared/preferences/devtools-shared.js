/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tells if DevTools have been explicitely enabled by the user.
// This pref allows to disable all features related to DevTools
// for users that never use them.
// Until bug 1361080 lands, we always consider them enabled.
pref("devtools.enabled", true);

// Enable deprecation warnings.
pref("devtools.errorconsole.deprecation_warnings", true);

#ifdef NIGHTLY_BUILD
// Don't show the Browser Toolbox prompt on local builds / nightly
pref("devtools.debugger.prompt-connection", false, sticky);
#else
pref("devtools.debugger.prompt-connection", true, sticky);
#endif

#ifdef MOZILLA_OFFICIAL
// Disable debugging chrome
pref("devtools.chrome.enabled", false, sticky);
// Disable remote debugging connections
pref("devtools.debugger.remote-enabled", false, sticky);
#else
// In local builds, enable the browser toolbox by default
pref("devtools.chrome.enabled", true, sticky);
pref("devtools.debugger.remote-enabled", true, sticky);
#endif

// Disable remote debugging protocol logging
pref("devtools.debugger.log", false);
pref("devtools.debugger.log.verbose", false);

pref("devtools.debugger.remote-port", 6000);
pref("devtools.debugger.remote-websocket", false);
// Force debugger server binding on the loopback interface
pref("devtools.debugger.force-local", true);

// Limit for intercepted request and response bodies (1 MB)
// Possible values:
// 0 => the response body has no limit
// n => represents max number of bytes stored
pref("devtools.netmonitor.responseBodyLimit", 1048576);
pref("devtools.netmonitor.requestBodyLimit", 1048576);

// DevTools default color unit
pref("devtools.defaultColorUnit", "authored");

// Used for devtools debugging
pref("devtools.dump.emit", false);

// Disable device discovery logging
pref("devtools.discovery.log", false);
// Whether to scan for DevTools devices via WiFi
pref("devtools.remote.wifi.scan", true);
// Client must complete TLS handshake within this window (ms)
pref("devtools.remote.tls-handshake-timeout", 10000);

// The extension ID for devtools-adb-extension
pref("devtools.remote.adb.extensionID", "adb@mozilla.org");
// The URL for for devtools-adb-extension (overridden in tests to a local path)
pref("devtools.remote.adb.extensionURL", "https://ftp.mozilla.org/pub/mozilla.org/labs/devtools/adb-extension/#OS#/adb-extension-latest-#OS#.xpi");

// URL of the remote JSON catalog used for device simulation
pref("devtools.devices.url", "https://code.cdn.mozilla.net/devices/devices.json");

// Enable Inactive CSS detection. This preference is used both by the client and the
// server.
// TODO: clarify the feature detection strategy for this feature. Bug 1552116.
#if defined(NIGHTLY_BUILD)
pref("devtools.inspector.inactive.css.enabled", true);
#else
pref("devtools.inspector.inactive.css.enabled", false);
#endif
