/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#filter substitution

pref("toolkit.defaultChromeURI", "chrome://b2g/content/shell.html");
pref("browser.chromeURL", "chrome://b2g/content/");

// Bug 945235: Prevent all bars to be considered visible:
pref("toolkit.defaultChromeFeatures", "chrome,dialog=no,close,resizable,scrollbars,extrachrome");

// Device pixel to CSS px ratio, in percent. Set to -1 to calculate based on display density.
pref("browser.viewport.scaleRatio", -1);

/* disable text selection */
pref("browser.ignoreNativeFrameTextSelection", true);

/* cache prefs */
#ifdef MOZ_WIDGET_GONK
pref("browser.cache.disk.enable", true);
pref("browser.cache.disk.capacity", 55000); // kilobytes
pref("browser.cache.disk.parent_directory", "/cache");
#endif
pref("browser.cache.disk.smart_size.enabled", false);
pref("browser.cache.disk.smart_size.first_run", false);

pref("browser.cache.memory.enable", true);
pref("browser.cache.memory.capacity", 1024); // kilobytes

pref("browser.cache.memory_limit", 2048); // 2 MB

/* image cache prefs */
pref("image.cache.size", 1048576); // bytes
pref("image.high_quality_downscaling.enabled", false);
pref("canvas.image.cache.limit", 10485760); // 10 MB

/* offline cache prefs */
pref("browser.offline-apps.notify", false);
pref("browser.cache.offline.enable", true);
pref("offline-apps.allow_by_default", true);

/* protocol warning prefs */
pref("network.protocol-handler.warn-external.tel", false);
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.vnd.youtube", false);

/* protocol expose prefs */
// By default, all protocol handlers are exposed. This means that the browser
// will response to openURL commands for all URL types. It will also try to open
// link clicks inside the browser before failing over to the system handlers.
pref("network.protocol-handler.expose.rtsp", false);

/* http prefs */
pref("network.http.pipelining", true);
pref("network.http.pipelining.ssl", true);
pref("network.http.proxy.pipelining", true);
pref("network.http.pipelining.maxrequests" , 6);
pref("network.http.keep-alive.timeout", 600);
pref("network.http.max-connections", 20);
pref("network.http.max-persistent-connections-per-server", 6);
pref("network.http.max-persistent-connections-per-proxy", 20);

// spdy
pref("network.http.spdy.push-allowance", 32768);

// See bug 545869 for details on why these are set the way they are
pref("network.buffer.cache.count", 24);
pref("network.buffer.cache.size",  16384);

// predictive actions
pref("network.seer.enable", false); // disabled on b2g
pref("network.seer.max-db-size", 2097152); // bytes
pref("network.seer.preserve", 50); // percentage of seer data to keep when cleaning up

/* session history */
pref("browser.sessionhistory.max_total_viewers", 1);
pref("browser.sessionhistory.max_entries", 50);

/* session store */
pref("browser.sessionstore.resume_session_once", false);
pref("browser.sessionstore.resume_from_crash", true);
pref("browser.sessionstore.resume_from_crash_timeout", 60); // minutes
pref("browser.sessionstore.interval", 10000); // milliseconds
pref("browser.sessionstore.max_tabs_undo", 1);

/* these should help performance */
pref("mozilla.widget.force-24bpp", true);
pref("mozilla.widget.use-buffer-pixmap", true);
pref("mozilla.widget.disable-native-theme", true);
pref("layout.reflow.synthMouseMove", false);
pref("layers.enable-tiles", true);
/*
   Cross Process Mutex is not supported on Mac OS X so progressive
   paint can not be enabled for B2G on Mac OS X desktop
*/
#ifdef MOZ_WIDGET_COCOA
pref("layers.progressive-paint", false);
#else
pref("layers.progressive-paint", false);
#endif

/* download manager (don't show the window or alert) */
pref("browser.download.useDownloadDir", true);
pref("browser.download.folderList", 1); // Default to ~/Downloads
pref("browser.download.manager.showAlertOnComplete", false);
pref("browser.download.manager.showAlertInterval", 2000);
pref("browser.download.manager.retention", 2);
pref("browser.download.manager.showWhenStarting", false);
pref("browser.download.manager.closeWhenDone", true);
pref("browser.download.manager.openDelay", 0);
pref("browser.download.manager.focusWhenStarting", false);
pref("browser.download.manager.flashCount", 2);
pref("browser.download.manager.displayedHistoryDays", 7);

/* download helper */
pref("browser.helperApps.deleteTempFileOnExit", false);

/* password manager */
pref("signon.rememberSignons", true);
pref("signon.expireMasterPassword", false);

/* autocomplete */
pref("browser.formfill.enable", true);

/* spellcheck */
pref("layout.spellcheckDefault", 0);

/* block popups by default, and notify the user about blocked popups */
pref("dom.disable_open_during_load", true);
pref("privacy.popups.showBrowserMessage", true);

pref("keyword.enabled", true);

pref("accessibility.typeaheadfind", false);
pref("accessibility.typeaheadfind.timeout", 5000);
pref("accessibility.typeaheadfind.flashBar", 1);
pref("accessibility.typeaheadfind.linksonly", false);
pref("accessibility.typeaheadfind.casesensitive", 0);

// SSL error page behaviour
pref("browser.ssl_override_behavior", 2);
pref("browser.xul.error_pages.expert_bad_cert", false);

// disable logging for the search service by default
pref("browser.search.log", false);

// disable updating
pref("browser.search.update", false);
pref("browser.search.update.log", false);
pref("browser.search.updateinterval", 6);

// enable search suggestions by default
pref("browser.search.suggest.enabled", true);

// tell the search service that we don't really expose the "current engine"
pref("browser.search.noCurrentEngine", true);

// Enable sparse localization by setting a few package locale overrides
pref("chrome.override_package.global", "b2g-l10n");
pref("chrome.override_package.mozapps", "b2g-l10n");
pref("chrome.override_package.passwordmgr", "b2g-l10n");

// enable xul error pages
pref("browser.xul.error_pages.enabled", true);

// disable color management
pref("gfx.color_management.mode", 0);

// don't allow JS to move and resize existing windows
pref("dom.disable_window_move_resize", true);

// prevent click image resizing for nsImageDocument
pref("browser.enable_click_image_resizing", false);

// controls which bits of private data to clear. by default we clear them all.
pref("privacy.item.cache", true);
pref("privacy.item.cookies", true);
pref("privacy.item.offlineApps", true);
pref("privacy.item.history", true);
pref("privacy.item.formdata", true);
pref("privacy.item.downloads", true);
pref("privacy.item.passwords", true);
pref("privacy.item.sessions", true);
pref("privacy.item.geolocation", true);
pref("privacy.item.siteSettings", true);
pref("privacy.item.syncAccount", true);

// base url for the wifi geolocation network provider
pref("geo.provider.use_mls", false);
pref("geo.cell.scan", true);
pref("geo.wifi.uri", "https://location.services.mozilla.com/v1/geolocate?key=%MOZILLA_API_KEY%");

// enable geo
pref("geo.enabled", true);

// content sink control -- controls responsiveness during page load
// see https://bugzilla.mozilla.org/show_bug.cgi?id=481566#c9
pref("content.sink.enable_perf_mode",  2); // 0 - switch, 1 - interactive, 2 - perf
pref("content.sink.pending_event_mode", 0);
pref("content.sink.perf_deflect_count", 1000000);
pref("content.sink.perf_parse_time", 50000000);

// Maximum scripts runtime before showing an alert
// Disable the watchdog thread for B2G. See bug 870043 comment 31.
pref("dom.use_watchdog", false);

// The slow script dialog can be triggered from inside the JS engine as well,
// ensure that those calls don't accidentally trigger the dialog.
pref("dom.max_script_run_time", 0);
pref("dom.max_chrome_script_run_time", 0);

// plugins
pref("plugin.disable", true);
pref("dom.ipc.plugins.enabled", true);

// product URLs
// The breakpad report server to link to in about:crashes
pref("breakpad.reportURL", "https://crash-stats.mozilla.com/report/index/");
pref("app.releaseNotesURL", "http://www.mozilla.com/%LOCALE%/b2g/%VERSION%/releasenotes/");
pref("app.support.baseURL", "http://support.mozilla.com/b2g");
pref("app.privacyURL", "http://www.mozilla.com/%LOCALE%/m/privacy.html");
pref("app.creditsURL", "http://www.mozilla.org/credits/");
pref("app.featuresURL", "http://www.mozilla.com/%LOCALE%/b2g/features/");
pref("app.faqURL", "http://www.mozilla.com/%LOCALE%/b2g/faq/");

// Name of alternate about: page for certificate errors (when undefined, defaults to about:neterror)
pref("security.alternate_certificate_error_page", "certerror");

pref("security.warn_viewing_mixed", false); // Warning is disabled.  See Bug 616712.

// Override some named colors to avoid inverse OS themes
pref("ui.-moz-dialog", "#efebe7");
pref("ui.-moz-dialogtext", "#101010");
pref("ui.-moz-field", "#fff");
pref("ui.-moz-fieldtext", "#1a1a1a");
pref("ui.-moz-buttonhoverface", "#f3f0ed");
pref("ui.-moz-buttonhovertext", "#101010");
pref("ui.-moz-combobox", "#fff");
pref("ui.-moz-comboboxtext", "#101010");
pref("ui.buttonface", "#ece7e2");
pref("ui.buttonhighlight", "#fff");
pref("ui.buttonshadow", "#aea194");
pref("ui.buttontext", "#101010");
pref("ui.captiontext", "#101010");
pref("ui.graytext", "#b1a598");
pref("ui.highlighttext", "#1a1a1a");
pref("ui.threeddarkshadow", "#000");
pref("ui.threedface", "#ece7e2");
pref("ui.threedhighlight", "#fff");
pref("ui.threedlightshadow", "#ece7e2");
pref("ui.threedshadow", "#aea194");
pref("ui.windowframe", "#efebe7");

// Themable via mozSettings
pref("ui.menu", "#f97c17");
pref("ui.menutext", "#ffffff");
pref("ui.infobackground", "#343e40");
pref("ui.infotext", "#686868");
pref("ui.window", "#ffffff");
pref("ui.windowtext", "#000000");
pref("ui.highlight", "#b2f2ff");

// replace newlines with spaces on paste into single-line text boxes
pref("editor.singleLine.pasteNewlines", 2);

// threshold where a tap becomes a drag, in 1/240" reference pixels
// The names of the preferences are to be in sync with EventStateManager.cpp
pref("ui.dragThresholdX", 25);
pref("ui.dragThresholdY", 25);

// Layers Acceleration.  We can only have nice things on gonk, because
// they're not maintained anywhere else.
pref("layers.offmainthreadcomposition.enabled", true);
#ifndef MOZ_WIDGET_GONK
pref("dom.ipc.tabs.disabled", true);
pref("layers.offmainthreadcomposition.async-animations", false);
pref("layers.async-video.enabled", false);
#else
pref("dom.ipc.tabs.disabled", false);
pref("layers.acceleration.disabled", false);
pref("layers.offmainthreadcomposition.async-animations", true);
pref("layers.async-video.enabled", true);
pref("layers.async-pan-zoom.enabled", true);
pref("gfx.content.azure.backends", "cairo");
#endif

// Web Notifications
pref("notification.feature.enabled", true);

// IndexedDB
pref("dom.indexedDB.warningQuota", 5);

// prevent video elements from preloading too much data
pref("media.preload.default", 1); // default to preload none
pref("media.preload.auto", 2);    // preload metadata if preload=auto
pref("media.cache_size", 4096);    // 4MB media cache

// The default number of decoded video frames that are enqueued in
// MediaDecoderReader's mVideoQueue.
pref("media.video-queue.default-size", 3);

// optimize images' memory usage
pref("image.mem.decodeondraw", true);
pref("image.mem.allow_locking_in_content_processes", false); /* don't allow image locking */
pref("image.mem.min_discard_timeout_ms", 86400000); /* 24h, we rely on the out of memory hook */
pref("image.mem.max_decoded_image_kb", 30000); /* 30MB seems reasonable */
pref("image.onload.decode.limit", 24); /* don't decode more than 24 images eagerly */

// XXX this isn't a good check for "are touch events supported", but
// we don't really have a better one at the moment.
// enable touch events interfaces
pref("dom.w3c_touch_events.enabled", 1);
pref("dom.w3c_touch_events.safetyX", 0); // escape borders in units of 1/240"
pref("dom.w3c_touch_events.safetyY", 120); // escape borders in units of 1/240"

#ifdef MOZ_SAFE_BROWSING
// Safe browsing does nothing unless this pref is set
pref("browser.safebrowsing.enabled", true);

// Prevent loading of pages identified as malware
pref("browser.safebrowsing.malware.enabled", true);

// Non-enhanced mode (local url lists) URL list to check for updates
pref("browser.safebrowsing.provider.0.updateURL", "https://safebrowsing.google.com/safebrowsing/downloads?client={moz:client}&appver={moz:version}&pver=2.2&key=%GOOGLE_API_KEY%");

pref("browser.safebrowsing.dataProvider", 0);

// Does the provider name need to be localizable?
pref("browser.safebrowsing.provider.0.name", "Google");
pref("browser.safebrowsing.provider.0.reportURL", "https://safebrowsing.google.com/safebrowsing/report?");
pref("browser.safebrowsing.provider.0.gethashURL", "https://safebrowsing.google.com/safebrowsing/gethash?client={moz:client}&appver={moz:version}&pver=2.2");

// HTML report pages
pref("browser.safebrowsing.provider.0.reportGenericURL", "http://{moz:locale}.phish-generic.mozilla.com/?hl={moz:locale}");
pref("browser.safebrowsing.provider.0.reportErrorURL", "http://{moz:locale}.phish-error.mozilla.com/?hl={moz:locale}");
pref("browser.safebrowsing.provider.0.reportPhishURL", "http://{moz:locale}.phish-report.mozilla.com/?hl={moz:locale}");
pref("browser.safebrowsing.provider.0.reportMalwareURL", "http://{moz:locale}.malware-report.mozilla.com/?hl={moz:locale}");
pref("browser.safebrowsing.provider.0.reportMalwareErrorURL", "http://{moz:locale}.malware-error.mozilla.com/?hl={moz:locale}");

// FAQ URLs

// Name of the about: page contributed by safebrowsing to handle display of error
// pages on phishing/malware hits.  (bug 399233)
pref("urlclassifier.alternate_error_page", "blocked");

// The number of random entries to send with a gethash request.
pref("urlclassifier.gethashnoise", 4);

// If an urlclassifier table has not been updated in this number of seconds,
// a gethash request will be forced to check that the result is still in
// the database.
pref("urlclassifier.max-complete-age", 2700);

// URL for checking the reason for a malware warning.
pref("browser.safebrowsing.malware.reportURL", "https://safebrowsing.google.com/safebrowsing/diagnostic?client=%NAME%&hl=%LOCALE%&site=");
#endif

// True if this is the first time we are showing about:firstrun
pref("browser.firstrun.show.uidiscovery", true);
pref("browser.firstrun.show.localepicker", true);

// initiated by a user
pref("content.ime.strict_policy", true);

// True if you always want dump() to work
//
// On Android, you also need to do the following for the output
// to show up in logcat:
//
// $ adb shell stop
// $ adb shell setprop log.redirect-stdio true
// $ adb shell start
pref("browser.dom.window.dump.enabled", false);

// Default Content Security Policy to apply to privileged and certified apps
pref("security.apps.privileged.CSP.default", "default-src *; script-src 'self'; object-src 'none'; style-src 'self' 'unsafe-inline'");
// If you change this CSP, make sure to update the fast path in nsCSPService.cpp
pref("security.apps.certified.CSP.default", "default-src *; script-src 'self'; object-src 'none'; style-src 'self'");

// Temporarily force-enable GL compositing.  This is default-disabled
// deep within the bowels of the widgetry system.  Remove me when GL
// compositing isn't default disabled in widget/android.
pref("layers.acceleration.force-enabled", true);

// handle links targeting new windows
// 1=current window/tab, 2=new window, 3=new tab in most recent window
pref("browser.link.open_newwindow", 3);

// 0: no restrictions - divert everything
// 1: don't divert window.open at all
// 2: don't divert window.open with features
pref("browser.link.open_newwindow.restriction", 0);

// Enable browser frames (including OOP, except on Windows, where it doesn't
// work), but make in-process browser frames the default.
pref("dom.mozBrowserFramesEnabled", true);

// Enable a (virtually) unlimited number of mozbrowser processes.
// We'll run out of PIDs on UNIX-y systems before we hit this limit.
pref("dom.ipc.processCount", 100000);

pref("dom.ipc.browser_frames.oop_by_default", false);
pref("dom.browser_frames.useAsyncPanZoom", true);

// SMS/MMS
pref("dom.sms.enabled", true);

//The waiting time in network manager.
pref("network.gonk.ms-release-mms-connection", 30000);

// WebContacts
pref("dom.mozContacts.enabled", true);
pref("dom.navigator-property.disable.mozContacts", false);
pref("dom.global-constructor.disable.mozContact", false);

// Shortnumber matching needed for e.g. Brazil:
// 03187654321 can be found with 87654321
pref("dom.phonenumber.substringmatching.BR", 8);
pref("dom.phonenumber.substringmatching.CO", 10);
pref("dom.phonenumber.substringmatching.VE", 7);
pref("dom.phonenumber.substringmatching.CL", 8);

// WebAlarms
pref("dom.mozAlarms.enabled", true);

// SimplePush
pref("services.push.enabled", true);
// Debugging enabled.
pref("services.push.debug", false);
// Is the network connection allowed to be up?
// This preference should be used in UX to enable/disable push.
pref("services.push.connection.enabled", true);
// serverURL to be assigned by services team
pref("services.push.serverURL", "wss://push.services.mozilla.com/");
pref("services.push.userAgentID", "");
// Exponential back-off start is 5 seconds like in HTTP/1.1.
// Maximum back-off is pingInterval.
pref("services.push.retryBaseInterval", 5000);
// Interval at which to ping PushServer to check connection status. In
// milliseconds. If no reply is received within requestTimeout, the connection
// is considered closed.
pref("services.push.pingInterval", 1800000); // 30 minutes
// How long before a DOMRequest errors as timeout
pref("services.push.requestTimeout", 10000);
// enable udp wakeup support
pref("services.push.udp.wakeupEnabled", true);

// NetworkStats
#ifdef MOZ_WIDGET_GONK
pref("dom.mozNetworkStats.enabled", true);
pref("dom.webapps.firstRunWithSIM", true);
#endif

#ifdef MOZ_B2G_RIL
// SingleVariant
pref("dom.mozApps.single_variant_sourcedir", "/persist/svoperapps");
#endif

// WebSettings
pref("dom.mozSettings.enabled", true);
pref("dom.navigator-property.disable.mozSettings", false);
pref("dom.mozPermissionSettings.enabled", true);

// controls if we want camera support
pref("device.camera.enabled", true);
pref("media.realtime_decoder.enabled", true);

// TCPSocket
pref("dom.mozTCPSocket.enabled", true);

// WebPayment
pref("dom.mozPay.enabled", true);

// "Preview" landing of bug 710563, which is bogged down in analysis
// of talos regression.  This is a needed change for higher-framerate
// CSS animations, and incidentally works around an apparent bug in
// our handling of requestAnimationFrame() listeners, which are
// supposed to enable this REPEATING_PRECISE_CAN_SKIP behavior.  The
// secondary bug isn't really worth investigating since it's obseleted
// by bug 710563.
pref("layout.frame_rate.precise", true);

// Handle hardware buttons in the b2g chrome package
pref("b2g.keys.menu.enabled", true);

// Screen timeout in seconds
pref("power.screen.timeout", 60);

pref("full-screen-api.enabled", true);

#ifndef MOZ_WIDGET_GONK
// If we're not actually on physical hardware, don't make the top level widget
// fullscreen when transitioning to fullscreen. This means in emulated
// environments (like the b2g desktop client) we won't make the client window
// fill the whole screen, we'll just make the content fill the client window,
// i.e. it won't give the impression to content that the number of device
// screen pixels changes!
pref("full-screen-api.ignore-widgets", true);
#endif

pref("media.volume.steps", 10);

#ifdef ENABLE_MARIONETTE
//Enable/disable marionette server, set listening port
pref("marionette.defaultPrefs.enabled", true);
pref("marionette.defaultPrefs.port", 2828);
#ifndef MOZ_WIDGET_GONK
// On desktop builds, we need to force the socket to listen on localhost only
pref("marionette.force-local", true);
#endif
#endif

#ifdef MOZ_UPDATER
// When we're applying updates, we can't let anything hang us on
// quit+restart.  The user has no recourse.
pref("shutdown.watchdog.timeoutSecs", 5);
// Timeout before the update prompt automatically installs the update
pref("b2g.update.apply-prompt-timeout", 60000); // milliseconds
// Amount of time to wait after the user is idle before prompting to apply an update
pref("b2g.update.apply-idle-timeout", 600000); // milliseconds
// Amount of time after which connection will be restarted if no progress
pref("b2g.update.download-watchdog-timeout", 120000); // milliseconds
pref("b2g.update.download-watchdog-max-retries", 5);

pref("app.update.enabled", true);
pref("app.update.auto", false);
pref("app.update.silent", false);
pref("app.update.mode", 0);
pref("app.update.incompatible.mode", 0);
pref("app.update.staging.enabled", true);
pref("app.update.service.enabled", true);

// The URL hosting the update manifest.
pref("app.update.url", "http://update.boot2gecko.org/%CHANNEL%/update.xml");
pref("app.update.channel", "@MOZ_UPDATE_CHANNEL@");

// Interval at which update manifest is fetched.  In units of seconds.
pref("app.update.interval", 86400); // 1 day
// Don't throttle background updates.
pref("app.update.download.backgroundInterval", 0);

// Retry update socket connections every 30 seconds in the cases of certain kinds of errors
pref("app.update.socket.retryTimeout", 30000);

// Max of 20 consecutive retries (total 10 minutes) before giving up and marking
// the update download as failed.
// Note: Offline errors will always retry when the network comes online.
pref("app.update.socket.maxErrors", 20);

// Enable update logging for now, to diagnose growing pains in the
// field.
pref("app.update.log", true);
#else
// Explicitly disable the shutdown watchdog.  It's enabled by default.
// When the updater is disabled, we want to know about shutdown hangs.
pref("shutdown.watchdog.timeoutSecs", -1);
#endif

// Check daily for apps updates.
pref("webapps.update.interval", 86400);

// Extensions preferences
pref("extensions.update.enabled", false);
pref("extensions.getAddons.cache.enabled", false);

// Context Menu
pref("ui.click_hold_context_menus", true);
pref("ui.click_hold_context_menus.delay", 750);

// Enable device storage
pref("device.storage.enabled", true);

// Enable pre-installed applications
pref("dom.webapps.useCurrentProfile", true);

// Enable system message
pref("dom.sysmsg.enabled", true);
pref("media.plugins.enabled", false);
pref("media.omx.enabled", true);
pref("media.rtsp.enabled", true);
pref("media.rtsp.video.enabled", false);

// Disable printing (particularly, window.print())
pref("dom.disable_window_print", true);

// Disable window.showModalDialog
pref("dom.disable_window_showModalDialog", true);

// Enable new experimental html forms
pref("dom.experimental_forms", true);
pref("dom.forms.number", true);

// Don't enable <input type=color> yet as we don't have a color picker
// implemented for b2g (bug 875751)
pref("dom.forms.color", false);

// Turns on gralloc-based direct texturing for Gonk
pref("gfx.gralloc.enabled", false);

// XXXX REMOVE FOR PRODUCTION. Turns on GC and CC logging
pref("javascript.options.mem.log", false);

// Increase mark slice time from 10ms to 30ms
pref("javascript.options.mem.gc_incremental_slice_ms", 30);

// Increase time to get more high frequency GC on benchmarks from 1000ms to 1500ms
pref("javascript.options.mem.gc_high_frequency_time_limit_ms", 1500);

pref("javascript.options.mem.gc_high_frequency_heap_growth_max", 300);
pref("javascript.options.mem.gc_high_frequency_heap_growth_min", 120);
pref("javascript.options.mem.gc_high_frequency_high_limit_mb", 40);
pref("javascript.options.mem.gc_high_frequency_low_limit_mb", 0);
pref("javascript.options.mem.gc_low_frequency_heap_growth", 120);
pref("javascript.options.mem.high_water_mark", 6);
pref("javascript.options.mem.gc_allocation_threshold_mb", 1);
pref("javascript.options.mem.gc_decommit_threshold_mb", 1);

// Show/Hide scrollbars when active/inactive
pref("ui.showHideScrollbars", 1);
pref("ui.useOverlayScrollbars", 1);

// Enable the ProcessPriorityManager, and give processes with no visible
// documents a 1s grace period before they're eligible to be marked as
// background. Background processes that are perceivable due to playing
// media are given a longer grace period to accomodate changing tracks, etc.
pref("dom.ipc.processPriorityManager.enabled", true);
pref("dom.ipc.processPriorityManager.backgroundGracePeriodMS", 1000);
pref("dom.ipc.processPriorityManager.backgroundPerceivableGracePeriodMS", 5000);
pref("dom.ipc.processPriorityManager.temporaryPriorityLockMS", 5000);

// Number of different background levels for background processes.  We use
// these different levels to force the low-memory killer to kill processes in
// a LRU order.
pref("dom.ipc.processPriorityManager.backgroundLRUPoolLevels", 5);

// Kernel parameters for process priorities.  These affect how processes are
// killed on low-memory and their relative CPU priorities.
//
// Note: The maximum nice value on Linux is 19, but the max value you should
// use here is 18.  NSPR adds 1 to some threads' nice values, to mark
// low-priority threads.  If the process priority manager were to renice a
// process (and all its threads) to 19, all threads would have the same
// niceness.  Then when we reniced the process to (say) 10, all threads would
// /still/ have the same niceness; we'd effectively have erased NSPR's thread
// priorities.

// The kernel can only accept 6 (OomScoreAdjust, KillUnderKB) pairs. But it is
// okay, kernel will still kill processes with larger OomScoreAdjust first even
// its OomScoreAdjust don't have a corresponding KillUnderKB.

pref("hal.processPriorityManager.gonk.MASTER.OomScoreAdjust", 0);
pref("hal.processPriorityManager.gonk.MASTER.KillUnderKB", 4096);
pref("hal.processPriorityManager.gonk.MASTER.Nice", 0);

pref("hal.processPriorityManager.gonk.PREALLOC.OomScoreAdjust", 67);
pref("hal.processPriorityManager.gonk.PREALLOC.Nice", 18);

pref("hal.processPriorityManager.gonk.FOREGROUND_HIGH.OomScoreAdjust", 67);
pref("hal.processPriorityManager.gonk.FOREGROUND_HIGH.KillUnderKB", 5120);
pref("hal.processPriorityManager.gonk.FOREGROUND_HIGH.Nice", 0);

pref("hal.processPriorityManager.gonk.FOREGROUND.OomScoreAdjust", 134);
pref("hal.processPriorityManager.gonk.FOREGROUND.KillUnderKB", 6144);
pref("hal.processPriorityManager.gonk.FOREGROUND.Nice", 1);

pref("hal.processPriorityManager.gonk.FOREGROUND_KEYBOARD.OomScoreAdjust", 200);
pref("hal.processPriorityManager.gonk.FOREGROUND_KEYBOARD.Nice", 1);

pref("hal.processPriorityManager.gonk.BACKGROUND_PERCEIVABLE.OomScoreAdjust", 400);
pref("hal.processPriorityManager.gonk.BACKGROUND_PERCEIVABLE.KillUnderKB", 7168);
pref("hal.processPriorityManager.gonk.BACKGROUND_PERCEIVABLE.Nice", 7);

pref("hal.processPriorityManager.gonk.BACKGROUND_HOMESCREEN.OomScoreAdjust", 534);
pref("hal.processPriorityManager.gonk.BACKGROUND_HOMESCREEN.KillUnderKB", 8192);
pref("hal.processPriorityManager.gonk.BACKGROUND_HOMESCREEN.Nice", 18);

pref("hal.processPriorityManager.gonk.BACKGROUND.OomScoreAdjust", 667);
pref("hal.processPriorityManager.gonk.BACKGROUND.KillUnderKB", 20480);
pref("hal.processPriorityManager.gonk.BACKGROUND.Nice", 18);

// Processes get this niceness when they have low CPU priority.
pref("hal.processPriorityManager.gonk.LowCPUNice", 18);

// Fire a memory pressure event when the system has less than Xmb of memory
// remaining.  You should probably set this just above Y.KillUnderKB for
// the highest priority class Y that you want to make an effort to keep alive.
// (For example, we want BACKGROUND_PERCEIVABLE to stay alive.)  If you set
// this too high, then we'll send out a memory pressure event every Z seconds
// (see below), even while we have processes that we would happily kill in
// order to free up memory.
pref("hal.processPriorityManager.gonk.notifyLowMemUnderKB", 14336);

// We wait this long before polling the memory-pressure fd after seeing one
// memory pressure event.  (When we're not under memory pressure, we sit
// blocked on a poll(), and this pref has no effect.)
pref("gonk.systemMemoryPressureRecoveryPollMS", 5000);

#ifndef DEBUG
// Enable pre-launching content processes for improved startup time
// (hiding latency).
pref("dom.ipc.processPrelaunch.enabled", true);
// Wait this long before pre-launching a new subprocess.
pref("dom.ipc.processPrelaunch.delayMs", 5000);
#endif

pref("dom.ipc.reuse_parent_app", false);

// When a process receives a system message, we hold a CPU wake lock on its
// behalf for this many seconds, or until it handles the system message,
// whichever comes first.
pref("dom.ipc.systemMessageCPULockTimeoutSec", 30);

// Ignore the "dialog=1" feature in window.open.
pref("dom.disable_window_open_dialog_feature", true);

// Screen reader support
pref("accessibility.accessfu.activate", 2);
pref("accessibility.accessfu.quicknav_modes", "Link,Heading,FormElement,Landmark,ListItem");
// Setting for an utterance order (0 - description first, 1 - description last).
pref("accessibility.accessfu.utterance", 1);
// Whether to skip images with empty alt text
pref("accessibility.accessfu.skip_empty_images", true);

// Enable hit-target fluffing
pref("ui.touch.radius.enabled", true);
pref("ui.touch.radius.leftmm", 3);
pref("ui.touch.radius.topmm", 5);
pref("ui.touch.radius.rightmm", 3);
pref("ui.touch.radius.bottommm", 2);

pref("ui.mouse.radius.enabled", true);
pref("ui.mouse.radius.leftmm", 3);
pref("ui.mouse.radius.topmm", 5);
pref("ui.mouse.radius.rightmm", 3);
pref("ui.mouse.radius.bottommm", 2);

// Disable native prompt
pref("browser.prompt.allowNative", false);

// Minimum delay in milliseconds between network activity notifications (0 means
// no notifications). The delay is the same for both download and upload, though
// they are handled separately. This pref is only read once at startup:
// a restart is required to enable a new value.
pref("network.activity.blipIntervalMilliseconds", 250);

// By default we want the NetworkManager service to manage Gecko's offline
// status for us according to the state of Wifi/cellular data connections.
// In some environments, such as the emulator or hardware with other network
// connectivity, this is not desireable, however, in which case this pref
// can be flipped to false.
pref("network.gonk.manage-offline-status", true);

pref("jsloader.reuseGlobal", true);

// Enable font inflation for browser tab content.
pref("font.size.inflation.minTwips", 120);
// And disable it for lingering master-process UI.
pref("font.size.inflation.disabledInMasterProcess", true);

// Enable freeing dirty pages when minimizing memory; this reduces memory
// consumption when applications are sent to the background.
pref("memory.free_dirty_pages", true);

// Enable the Linux-specific, system-wide memory reporter.
pref("memory.system_memory_reporter", true);

// Don't dump memory reports on OOM, by default.
pref("memory.dump_reports_on_oom", false);

pref("layout.imagevisibility.enabled", true);
pref("layout.imagevisibility.numscrollportwidths", 1);
pref("layout.imagevisibility.numscrollportheights", 1);

// Enable native identity (persona/browserid)
pref("dom.identity.enabled", true);

// Wait up to this much milliseconds when orientation changed
pref("layers.orientation.sync.timeout", 1000);

// Don't discard WebGL contexts for foreground apps on memory
// pressure.
pref("webgl.can-lose-context-in-foreground", false);

// Allow nsMemoryInfoDumper to create a fifo in the temp directory.  We use
// this fifo to trigger about:memory dumps, among other things.
pref("memory_info_dumper.watch_fifo.enabled", true);
pref("memory_info_dumper.watch_fifo.directory", "/data/local");

// See ua-update.json.in for the packaged UA override list
pref("general.useragent.updates.enabled", true);
pref("general.useragent.updates.url", "https://dynamicua.cdn.mozilla.net/0/%APP_ID%");
pref("general.useragent.updates.interval", 604800); // 1 week
pref("general.useragent.updates.retry", 86400); // 1 day

// Make <audio> and <video> talk to the AudioChannelService.
pref("media.useAudioChannelService", true);

pref("b2g.version", @MOZ_B2G_VERSION@);

// Disable console buffering to save memory.
pref("consoleservice.buffered", false);

#ifdef MOZ_WIDGET_GONK
// Performance testing suggests 2k is a better page size for SQLite.
pref("toolkit.storage.pageSize", 2048);
#endif

// Enable captive portal detection.
pref("captivedetect.canonicalURL", "http://detectportal.firefox.com/success.txt");
pref("captivedetect.canonicalContent", "success\n");

// The url of the manifest we use for ADU pings.
pref("ping.manifestURL", "https://marketplace.firefox.com/packaged.webapp");

// Enable the disk space watcher
pref("disk_space_watcher.enabled", true);

// SNTP preferences.
pref("network.sntp.maxRetryCount", 10);
pref("network.sntp.refreshPeriod", 86400); // In seconds.
pref("network.sntp.pools", // Servers separated by ';'.
     "0.pool.ntp.org;1.pool.ntp.org;2.pool.ntp.org;3.pool.ntp.org");
pref("network.sntp.port", 123);
pref("network.sntp.timeout", 30); // In seconds.

// Enable dataStore
pref("dom.datastore.enabled", true);

// DOM Inter-App Communication API.
pref("dom.inter-app-communication-api.enabled", true);

// Allow ADB to run for this many hours before disabling
// (only applies when marionette is disabled)
// 0 disables the timer.
pref("b2g.adb.timeout-hours", 12);

// InputMethod so we can do soft keyboards
pref("dom.mozInputMethod.enabled", true);

// Absolute path to the devtool unix domain socket file used
// to communicate with a usb cable via adb forward
pref("devtools.debugger.unix-domain-socket", "/data/local/debugger-socket");

// enable Skia/GL (OpenGL-accelerated 2D drawing) for large enough 2d canvases,
// falling back to Skia/software for smaller canvases
#ifdef MOZ_WIDGET_GONK
pref("gfx.canvas.azure.backends", "skia");
pref("gfx.canvas.azure.accelerated", true);
#endif

// Turn on dynamic cache size for Skia
pref("gfx.canvas.skiagl.dynamic-cache", true);

// Limit skia to canvases the size of the device screen or smaller
pref("gfx.canvas.max-size-for-skia-gl", -1);

// enable fence with readpixels for SurfaceStream
pref("gfx.gralloc.fence-with-readpixels", true);

// Cell Broadcast API
pref("ril.cellbroadcast.disabled", false);

// The url of the page used to display network error details.
pref("b2g.neterror.url", "app://system.gaiamobile.org/net_error.html");

// Enable Web Speech synthesis API
pref("media.webspeech.synth.enabled", true);

// Downloads API
pref("dom.mozDownloads.enabled", true);
pref("dom.downloads.max_retention_days", 7);

// External Helper Application Handling
//
// All external helper application handling can require the docshell to be
// active before allowing the external helper app service to handle content.
//
// To prevent SD card DoS attacks via downloads we disable background handling.
//
pref("security.exthelperapp.disable_background_handling", true);

// Inactivity time in milliseconds after which we shut down the OS.File worker.
pref("osfile.reset_worker_delay", 5000);

// APZC preferences.
//
// Gaia relies heavily on scroll events for now, so lets fire them
// more often than the default value (100).
pref("apz.asyncscroll.throttle", 40);
pref("apz.pan_repaint_interval", 16);

// Maximum fling velocity in inches/ms.  Slower devices may need to reduce this
// to avoid checkerboarding.  Note, float value must be set as a string.
pref("apz.max_velocity_inches_per_ms", "0.0375");

// Tweak default displayport values to reduce the risk of running out of
// memory when zooming in
pref("apz.x_skate_size_multiplier", "1.25");
pref("apz.y_skate_size_multiplier", "1.5");
pref("apz.x_stationary_size_multiplier", "1.5");
pref("apz.y_stationary_size_multiplier", "1.8");
pref("apz.enlarge_displayport_when_clipped", true);
// Use "sticky" axis locking
pref("apz.axis_lock_mode", 2);
pref("apz.subframe.enabled", true);

// This preference allows FirefoxOS apps (and content, I think) to force
// the use of software (instead of hardware accelerated) 2D canvases by
// creating a context like this:
//
//   canvas.getContext('2d', { willReadFrequently: true })
//
// Using a software canvas can save memory when JS calls getImageData()
// on the canvas frequently. See bug 884226.
pref("gfx.canvas.willReadFrequently.enable", true);

// Disable autofocus until we can have it not bring up the keyboard.
// https://bugzilla.mozilla.org/show_bug.cgi?id=965763
pref("browser.autofocus", false);

// Enable wakelock
pref("dom.wakelock.enabled", true);

// Enable sync and mozId with Firefox Accounts.
#ifdef MOZ_SERVICES_FXACCOUNTS
pref("services.sync.fxaccounts.enabled", true);
pref("identity.fxaccounts.enabled", true);
#endif
