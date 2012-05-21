/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#filter substitution

pref("toolkit.defaultChromeURI", "chrome://browser/content/shell.xul");
pref("browser.chromeURL", "chrome://browser/content/");
#ifdef MOZ_OFFICIAL_BRANDING
pref("browser.homescreenURL", "http://homescreen.gaiamobile.org/");
#else
pref("browser.homescreenURL", "http://homescreen.gaiamobile.org/");
#endif

// All the privileged domains
// XXX TODO : we should read them from a file somewhere
pref("b2g.privileged.domains", "http://browser.gaiamobile.org,
	                            http://calculator.gaiamobile.org,
	                            http://contacts.gaiamobile.org,
	                            http://camera.gaiamobile.org,
	                            http://clock.gaiamobile.org,
	                            http://crystalskull.gaiamobile.org,
	                            http://cubevid.gaiamobile.org,
	                            http://dialer.gaiamobile.org,
	                            http://gallery.gaiamobile.org,
	                            http://homescreen.gaiamobile.org,
	                            http://maps.gaiamobile.org,
	                            http://market.gaiamobile.org,
	                            http://music.gaiamobile.org,
	                            http://penguinpop.gaiamobile.org,
	                            http://settings.gaiamobile.org,
	                            http://sms.gaiamobile.org,
	                            http://towerjelly.gaiamobile.org,
	                            http://video.gaiamobile.org");

// URL for the dialer application.
pref("dom.telephony.app.phone.url", "http://dialer.gaiamobile.org,http://homescreen.gaiamobile.org");

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

/* image cache prefs */
pref("image.cache.size", 1048576); // bytes

/* offline cache prefs */
pref("browser.offline-apps.notify", false);
pref("browser.cache.offline.enable", true);
pref("offline-apps.allow_by_default", true);

/* protocol warning prefs */
pref("network.protocol-handler.warn-external.tel", false);
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.vnd.youtube", false);

/* http prefs */
pref("network.http.pipelining", true);
pref("network.http.pipelining.ssl", true);
pref("network.http.proxy.pipelining", true);
pref("network.http.pipelining.maxrequests" , 6);
pref("network.http.keep-alive.timeout", 600);
pref("network.http.max-connections", 6);
pref("network.http.max-connections-per-server", 4);
pref("network.http.max-persistent-connections-per-server", 4);
pref("network.http.max-persistent-connections-per-proxy", 4);

// See bug 545869 for details on why these are set the way they are
pref("network.buffer.cache.count", 24);
pref("network.buffer.cache.size",  16384);

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
pref("dom.send_after_paint_to_content", true);

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

/* download alerts (disabled above) */
pref("alerts.slideIncrement", 1);
pref("alerts.slideIncrementTime", 10);
pref("alerts.totalOpenTime", 6000);
pref("alerts.height", 50);

/* download helper */
pref("browser.helperApps.deleteTempFileOnExit", false);

/* password manager */
pref("signon.rememberSignons", true);
pref("signon.expireMasterPassword", false);
pref("signon.SignonFileName", "signons.txt");

/* autocomplete */
pref("browser.formfill.enable", true);

/* spellcheck */
pref("layout.spellcheckDefault", 0);

/* block popups by default, and notify the user about blocked popups */
pref("dom.disable_open_during_load", true);
pref("privacy.popups.showBrowserMessage", true);

pref("keyword.enabled", true);
pref("keyword.URL", "https://www.google.com/m?ie=UTF-8&oe=UTF-8&sourceid=navclient&gfns=1&q=");

pref("accessibility.typeaheadfind", false);
pref("accessibility.typeaheadfind.timeout", 5000);
pref("accessibility.typeaheadfind.flashBar", 1);
pref("accessibility.typeaheadfind.linksonly", false);
pref("accessibility.typeaheadfind.casesensitive", 0);

// pointer to the default engine name
pref("browser.search.defaultenginename", "chrome://browser/locale/region.properties");

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

// URL to the Learn More link XXX this is the firefox one.  Bug 495578 fixes this.
pref("browser.geolocation.warning.infoURL", "http://www.mozilla.com/%LOCALE%/firefox/geolocation/");

// base url for the wifi geolocation network provider
pref("geo.wifi.uri", "https://maps.googleapis.com/maps/api/browserlocation/json");

// enable geo
pref("geo.enabled", true);

// content sink control -- controls responsiveness during page load
// see https://bugzilla.mozilla.org/show_bug.cgi?id=481566#c9
pref("content.sink.enable_perf_mode",  2); // 0 - switch, 1 - interactive, 2 - perf
pref("content.sink.pending_event_mode", 0);
pref("content.sink.perf_deflect_count", 1000000);
pref("content.sink.perf_parse_time", 50000000);

pref("javascript.options.mem.high_water_mark", 32);

// Maximum scripts runtime before showing an alert
pref("dom.max_chrome_script_run_time", 0); // disable slow script dialog for chrome
pref("dom.max_script_run_time", 20);

// plugins
pref("plugin.disable", true);
pref("dom.ipc.plugins.enabled", true);

// product URLs
// The breakpad report server to link to in about:crashes
pref("breakpad.reportURL", "http://crash-stats.mozilla.com/report/index/");
pref("app.releaseNotesURL", "http://www.mozilla.com/%LOCALE%/b2g/%VERSION%/releasenotes/");
pref("app.support.baseURL", "http://support.mozilla.com/b2g");
pref("app.feedbackURL", "http://input.mozilla.com/feedback/");
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
pref("ui.highlight", "#fad184");
pref("ui.highlighttext", "#1a1a1a");
pref("ui.infobackground", "#f5f5b5");
pref("ui.infotext", "#000");
pref("ui.menu", "#f7f5f3");
pref("ui.menutext", "#101010");
pref("ui.threeddarkshadow", "#000");
pref("ui.threedface", "#ece7e2");
pref("ui.threedhighlight", "#fff");
pref("ui.threedlightshadow", "#ece7e2");
pref("ui.threedshadow", "#aea194");
pref("ui.window", "#efebe7");
pref("ui.windowtext", "#101010");
pref("ui.windowframe", "#efebe7");

// replace newlines with spaces on paste into single-line text boxes
pref("editor.singleLine.pasteNewlines", 2);

// threshold where a tap becomes a drag, in 1/240" reference pixels
// The names of the preferences are to be in sync with nsEventStateManager.cpp
pref("ui.dragThresholdX", 25);
pref("ui.dragThresholdY", 25);

// Layers Acceleration
pref("layers.acceleration.disabled", false);
pref("layers.offmainthreadcomposition.enabled", false);

// Web Notifications
pref("notification.feature.enabled", true);

// IndexedDB
pref("indexedDB.feature.enabled", true);
pref("dom.indexedDB.warningQuota", 5);

// prevent video elements from preloading too much data
pref("media.preload.default", 1); // default to preload none
pref("media.preload.auto", 2);    // preload metadata if preload=auto

//  0: don't show fullscreen keyboard
//  1: always show fullscreen keyboard
// -1: show fullscreen keyboard based on threshold pref
pref("widget.ime.android.landscape_fullscreen", -1);
pref("widget.ime.android.fullscreen_threshold", 250); // in hundreths of inches

// optimize images memory usage
pref("image.mem.decodeondraw", true);
pref("content.image.allow_locking", false);
pref("image.mem.min_discard_timeout_ms", 10000);

// enable touch events interfaces
pref("dom.w3c_touch_events.enabled", true);
pref("dom.w3c_touch_events.safetyX", 0); // escape borders in units of 1/240"
pref("dom.w3c_touch_events.safetyY", 120); // escape borders in units of 1/240"

#ifdef MOZ_SAFE_BROWSING
// Safe browsing does nothing unless this pref is set
pref("browser.safebrowsing.enabled", true);

// Prevent loading of pages identified as malware
pref("browser.safebrowsing.malware.enabled", true);

// Non-enhanced mode (local url lists) URL list to check for updates
pref("browser.safebrowsing.provider.0.updateURL", "http://safebrowsing.clients.google.com/safebrowsing/downloads?client={moz:client}&appver={moz:version}&pver=2.2");

pref("browser.safebrowsing.dataProvider", 0);

// Does the provider name need to be localizable?
pref("browser.safebrowsing.provider.0.name", "Google");
pref("browser.safebrowsing.provider.0.keyURL", "https://sb-ssl.google.com/safebrowsing/newkey?client={moz:client}&appver={moz:version}&pver=2.2");
pref("browser.safebrowsing.provider.0.reportURL", "http://safebrowsing.clients.google.com/safebrowsing/report?");
pref("browser.safebrowsing.provider.0.gethashURL", "http://safebrowsing.clients.google.com/safebrowsing/gethash?client={moz:client}&appver={moz:version}&pver=2.2");

// HTML report pages
pref("browser.safebrowsing.provider.0.reportGenericURL", "http://{moz:locale}.phish-generic.mozilla.com/?hl={moz:locale}");
pref("browser.safebrowsing.provider.0.reportErrorURL", "http://{moz:locale}.phish-error.mozilla.com/?hl={moz:locale}");
pref("browser.safebrowsing.provider.0.reportPhishURL", "http://{moz:locale}.phish-report.mozilla.com/?hl={moz:locale}");
pref("browser.safebrowsing.provider.0.reportMalwareURL", "http://{moz:locale}.malware-report.mozilla.com/?hl={moz:locale}");
pref("browser.safebrowsing.provider.0.reportMalwareErrorURL", "http://{moz:locale}.malware-error.mozilla.com/?hl={moz:locale}");

// FAQ URLs
pref("browser.safebrowsing.warning.infoURL", "http://www.mozilla.com/%LOCALE%/%APP%/phishing-protection/");
pref("browser.geolocation.warning.infoURL", "http://www.mozilla.com/%LOCALE%/%APP%/geolocation/");

// Name of the about: page contributed by safebrowsing to handle display of error
// pages on phishing/malware hits.  (bug 399233)
pref("urlclassifier.alternate_error_page", "blocked");

// The number of random entries to send with a gethash request.
pref("urlclassifier.gethashnoise", 4);

// The list of tables that use the gethash request to confirm partial results.
pref("urlclassifier.gethashtables", "goog-phish-shavar,goog-malware-shavar");

// If an urlclassifier table has not been updated in this number of seconds,
// a gethash request will be forced to check that the result is still in
// the database.
pref("urlclassifier.confirm-age", 2700);

// Maximum size of the sqlite3 cache during an update, in bytes
pref("urlclassifier.updatecachemax", 4194304);

// URL for checking the reason for a malware warning.
pref("browser.safebrowsing.malware.reportURL", "http://safebrowsing.clients.google.com/safebrowsing/diagnostic?client=%NAME%&hl=%LOCALE%&site=");
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



// Temporarily relax file:// origin checks so that we can use <img>s
// from other dirs as webgl textures and more.  Remove me when we have
// installable apps or wifi support.
pref("security.fileuri.strict_origin_policy", false);

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
pref("dom.mozBrowserFramesWhitelist", "http://homescreen.gaiamobile.org,http://browser.gaiamobile.org");

#ifdef XP_WIN
pref("dom.ipc.tabs.disabled", true);
#else
pref("dom.ipc.tabs.disabled", false);
#endif

pref("dom.ipc.browser_frames.oop_by_default", false);

// Temporary permission hack for WebSMS
pref("dom.sms.enabled", true);
pref("dom.sms.whitelist", "file://,http://homescreen.gaiamobile.org,http://sms.gaiamobile.org");

// Temporary permission hack for WebMobileConnection
pref("dom.mobileconnection.whitelist", "http://system.gaiamobile.org,http://homescreen.gaiamobile.org,http://dialer.gaiamobile.org");

// Temporary permission hack for WebContacts
pref("dom.mozContacts.enabled", true);
pref("dom.mozContacts.whitelist", "http://dialer.gaiamobile.org,http://sms.gaiamobile.org");

// WebSettings
pref("dom.mozSettings.enabled", true);

// Ignore X-Frame-Options headers.
pref("b2g.ignoreXFrameOptions", true);

// controls if we want camera support
pref("device.camera.enabled", true);
pref("media.realtime_decoder.enabled", true);

// "Preview" landing of bug 710563, which is bogged down in analysis
// of talos regression.  This is a needed change for higher-framerate
// CSS animations, and incidentally works around an apparent bug in
// our handling of requestAnimationFrame() listeners, which are
// supposed to enable this REPEATING_PRECISE_CAN_SKIP behavior.  The
// secondary bug isn't really worth investigating since it's obseleted
// by bug 710563.
pref("layout.frame_rate.precise", true);

// Temporary remote js console hack
pref("b2g.remote-js.enabled", true);
pref("b2g.remote-js.port", 9999);

// Handle hardware buttons in the b2g chrome package
pref("b2g.keys.menu.enabled", true);
pref("b2g.keys.search.enabled", false);

// Screen timeout in minutes
pref("power.screen.timeout", 60);
pref("dom.power.whitelist", "http://homescreen.gaiamobile.org,http://settings.gaiamobile.org");

pref("full-screen-api.enabled", true);

pref("media.volume.steps", 10);

//Enable/disable marionette server, set listening port
pref("marionette.defaultPrefs.enabled", true);
pref("marionette.defaultPrefs.port", 2828);

#ifdef MOZ_UPDATER
pref("app.update.enabled", true);
pref("app.update.auto", true);
pref("app.update.silent", true);
pref("app.update.mode", 0);
pref("app.update.incompatible.mode", 0);
pref("app.update.service.enabled", true);

// The URL hosting the update manifest.
pref("app.update.url", "http://update.boot2gecko.org/m2.5/updates.xml");
// Interval at which update manifest is fetched.  In units of seconds.
pref("app.update.interval", 3600); // 1 hour
// First interval to elapse before checking for update.  In units of
// milliseconds.  Capped at 10 seconds.
pref("app.update.timerFirstInterval", 30000);
pref("app.update.timerMinimumDelay", 30); // seconds
// Don't throttle background updates.
pref("app.update.download.backgroundInterval", 0);

// Enable update logging for now, to diagnose growing pains in the
// field.
pref("app.update.log", true);
#endif

// Extensions preferences
pref("extensions.update.enabled", false);
pref("extensions.getAddons.cache.enabled", false);

// Context Menu
pref("ui.click_hold_context_menus", true);
pref("ui.click_hold_context_menus.delay", 1000);

// Enable device storage
pref("device.storage.enabled", true);
