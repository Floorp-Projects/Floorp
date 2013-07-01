/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#filter substitution

#ifdef DEBUG
// disable content and content script caching
pref("nglayout.debug.disable_xul_cache", true);
pref("nglayout.debug.disable_xul_fastload", true);
pref("devtools.errorconsole.enabled", true);
#endif

// Automatically submit crash reports
pref("app.crashreporter.autosubmit", false);
// Has the user been prompted about crash reporting?
pref("app.crashreporter.prompted", false);

// Debug prefs, see input.js
pref("metro.debug.treatmouseastouch", false);
pref("metro.debug.colorizeInputOverlay", false);
pref("metro.debug.selection.displayRanges", false);
pref("metro.debug.selection.dumpRanges", false);
pref("metro.debug.selection.dumpEvents", false);

// Enable off main thread compositing
pref("layers.offmainthreadcomposition.enabled", true);

// Enable Microsoft TSF support by default for imes.
pref("intl.enable_tsf_support", true);

pref("general.autoScroll", true);
pref("general.smoothScroll", true);
pref("general.smoothScroll.durationToIntervalRatio", 200);
pref("mousewheel.enable_pixel_scrolling", true);

// For browser.xml binding
//
// cacheRatio* is a ratio that determines the amount of pixels to cache. The
// ratio is multiplied by the viewport width or height to get the displayport's
// width or height, respectively.
//
// (divide integer value by 1000 to get the ratio)
//
// For instance: cachePercentageWidth is 1500
//               viewport height is 500
//               => display port height will be 500 * 1.5 = 750
//
pref("toolkit.browser.cacheRatioWidth", 2000);
pref("toolkit.browser.cacheRatioHeight", 3000);

// How long before a content view (a handle to a remote scrollable object)
// expires.
pref("toolkit.browser.contentViewExpire", 3000);


pref("toolkit.defaultChromeURI", "chrome://browser/content/browser.xul");
pref("browser.chromeURL", "chrome://browser/content/");

pref("browser.tabs.remote", false);

// Telemetry
pref("toolkit.telemetry.enabled", true);
pref("toolkit.telemetry.prompted", 2);

pref("toolkit.screen.lock", false);

// From libpref/src/init/all.js, extended to allow a slightly wider zoom range.
pref("zoom.minPercent", 20);
pref("zoom.maxPercent", 400);
pref("toolkit.zoomManager.zoomValues", ".2,.3,.5,.67,.8,.9,1,1.1,1.2,1.33,1.5,1.7,2,2.4,3,4");

// Device pixel to CSS px ratio, in percent. Set to -1 to calculate based on display density.
pref("browser.viewport.scaleRatio", -1);

/* use long press to display a context menu */
pref("ui.click_hold_context_menus", false);

/* offline cache prefs */
pref("browser.offline-apps.notify", true);

/* protocol warning prefs */
pref("network.protocol-handler.warn-external.tel", false);
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.vnd.youtube", false);
pref("network.protocol-handler.warn-external.ms-windows-store", false);
pref("network.protocol-handler.external.ms-windows-store", true);

/* history max results display */
pref("browser.display.history.maxresults", 100);

/* max items per section of the startui */
pref("browser.display.startUI.maxresults", 16);

// Backspace and Shift+Backspace behavior
// 0 goes Back/Forward
// 1 act like PgUp/PgDown
// 2 and other values, nothing
pref("browser.backspace_action", 0);

/* session history */
pref("browser.sessionhistory.max_entries", 50);

// On startup, automatically restore tabs from last time?
pref("browser.startup.sessionRestore", false);

/* session store */
pref("browser.sessionstore.resume_from_crash", true);
pref("browser.sessionstore.resume_session_once", false);
pref("browser.sessionstore.resume_from_crash_timeout", 60); // minutes
// minimal interval between two save operations in milliseconds
pref("browser.sessionstore.interval", 15000); // milliseconds
// maximum amount of POSTDATA to be saved in bytes per history entry (-1 = all of it)
// (NB: POSTDATA will be saved either entirely or not at all)
pref("browser.sessionstore.postdata", 0);
// on which sites to save text data, POSTDATA and cookies
// 0 = everywhere, 1 = unencrypted sites, 2 = nowhere
pref("browser.sessionstore.privacy_level", 0);
// the same as browser.sessionstore.privacy_level, but for saving deferred session data
pref("browser.sessionstore.privacy_level_deferred", 1);
// how many tabs can be reopened (per window)
pref("browser.sessionstore.max_tabs_undo", 10);
// number of crashes that can occur before the about:sessionrestore page is displayed
// (this pref has no effect if more than 6 hours have passed since the last crash)
pref("browser.sessionstore.max_resumed_crashes", 1);
// restore_on_demand overrides MAX_CONCURRENT_TAB_RESTORES (sessionstore constant)
// and restore_hidden_tabs. When true, tabs will not be restored until they are
// focused (also applies to tabs that aren't visible). When false, the values
// for MAX_CONCURRENT_TAB_RESTORES and restore_hidden_tabs are respected.
// Selected tabs are always restored regardless of this pref.
pref("browser.sessionstore.restore_on_demand", true);

/* these should help performance */
pref("mozilla.widget.force-24bpp", true);
pref("mozilla.widget.use-buffer-pixmap", true);
pref("mozilla.widget.disable-native-theme", false);
pref("layout.reflow.synthMouseMove", false);

/* "Preview" of framerate increase for animations, discussed in 710563. */
pref("layout.frame_rate.precise", true);

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
pref("browser.download.manager.addToRecentDocs", true);
pref("browser.download.manager.displayedHistoryDays", 7);
pref("browser.download.manager.resumeOnWakeDelay", 10000);
pref("browser.download.manager.quitBehavior", 0);

/* download alerts (disabled above) */
pref("alerts.totalOpenTime", 6000);

/* download helper */
pref("browser.helperApps.deleteTempFileOnExit", false);

/* password manager */
pref("signon.rememberSignons", true);
pref("signon.SignonFileName", "signons.txt");

/* find helper */
pref("findhelper.autozoom", true);

// this will automatically enable inline spellchecking (if it is available) for
// editable elements in HTML
// 0 = spellcheck nothing
// 1 = check multi-line controls [default]
// 2 = check multi/single line controls
pref("layout.spellcheckDefault", 1);

/* extension manager and xpinstall */
// Disable all add-on locations other than the profile
pref("extensions.enabledScopes", 1);
// Auto-disable any add-ons that are "dropped in" to the profile
pref("extensions.autoDisableScopes", 1);
// Disable add-on installation via the web-exposed APIs
pref("xpinstall.enabled", false);
pref("xpinstall.whitelist.add", "addons.mozilla.org");
pref("extensions.autoupdate.enabled", false);
pref("extensions.update.enabled", false);

/* blocklist preferences */
pref("extensions.blocklist.enabled", true);
pref("extensions.blocklist.interval", 86400);
pref("extensions.blocklist.url", "https://addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/%PRODUCT%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/%PING_COUNT%/%TOTAL_PING_COUNT%/%DAYS_SINCE_LAST_PING%/");
pref("extensions.blocklist.detailsURL", "https://www.mozilla.org/%LOCALE%/blocklist/");

/* block popups by default, and notify the user about blocked popups */
pref("dom.disable_open_during_load", true);
pref("privacy.popups.showBrowserMessage", true);

/* disable opening windows with the dialog feature */
pref("dom.disable_window_open_dialog_feature", true);

pref("keyword.enabled", true);

pref("accessibility.typeaheadfind", false);
pref("accessibility.typeaheadfind.timeout", 5000);
pref("accessibility.typeaheadfind.flashBar", 1);
pref("accessibility.typeaheadfind.linksonly", false);
pref("accessibility.typeaheadfind.casesensitive", 0);

// Trun on F7 caret browsing hot key
pref("accessibility.browsewithcaret_shortcut.enabled", true);
pref("accessibility.browsewithcaret", false);

// Whether or not we show a dialog box informing the user that the update was
// successfully applied.
pref("app.update.showInstalledUI", false);

// pointer to the default engine name
pref("browser.search.defaultenginename", "chrome://browser/locale/browser.properties");

// SSL error page behaviour
pref("browser.ssl_override_behavior", 2);
pref("browser.xul.error_pages.expert_bad_cert", false);

// disable logging for the search service by default
pref("browser.search.log", false);

// ordering of search engines in the engine list.
pref("browser.search.order.1", "chrome://browser/locale/browser.properties");
pref("browser.search.order.2", "chrome://browser/locale/browser.properties");
pref("browser.search.order.3", "chrome://browser/locale/browser.properties");

// send ping to the server to update
pref("browser.search.update", true);

// disable logging for the search service update system by default
pref("browser.search.update.log", false);

// Check whether we need to perform engine updates every 6 hours
pref("browser.search.update.interval", 21600);

// enable search suggestions by default
pref("browser.search.suggest.enabled", true);

// tell the search service that we don't really expose the "current engine"
pref("browser.search.noCurrentEngine", true);

#ifdef MOZ_OFFICIAL_BRANDING
// {moz:official} expands to "official"
pref("browser.search.official", true);
#endif

// enable xul error pages
pref("browser.xul.error_pages.enabled", true);

// Specify emptyRestriction = 0 so that bookmarks appear in the list by default
pref("browser.urlbar.default.behavior", 0);
pref("browser.urlbar.default.behavior.emptyRestriction", 0);

// Let the faviconservice know that we display favicons as 25x25px so that it
// uses the right size when optimizing favicons
pref("places.favicons.optimizeToDimension", 25);

// various and sundry awesomebar prefs (should remove/re-evaluate
// these once bug 447900 is fixed)
pref("browser.urlbar.trimURLs", true);
pref("browser.urlbar.formatting.enabled", true);
pref("browser.urlbar.clickSelectsAll", true);
pref("browser.urlbar.doubleClickSelectsAll", true);
pref("browser.urlbar.autoFill", false);
pref("browser.urlbar.matchOnlyTyped", false);
pref("browser.urlbar.matchBehavior", 1);
pref("browser.urlbar.filter.javascript", true);
pref("browser.urlbar.maxRichResults", 8);
pref("browser.urlbar.search.chunkSize", 1000);
pref("browser.urlbar.search.timeout", 100);
pref("browser.urlbar.restrict.history", "^");
pref("browser.urlbar.restrict.bookmark", "*");
pref("browser.urlbar.restrict.tag", "+");
pref("browser.urlbar.match.title", "#");
pref("browser.urlbar.match.url", "@");
pref("browser.history.grouping", "day");
pref("browser.history.showSessions", false);
pref("browser.sessionhistory.max_entries", 50);
pref("browser.history_expire_days", 180);
pref("browser.history_expire_days_min", 90);
pref("browser.history_expire_sites", 40000);
pref("browser.places.migratePostDataAnnotations", true);
pref("browser.places.updateRecentTagsUri", true);
pref("places.frecency.numVisits", 10);
pref("places.frecency.numCalcOnIdle", 50);
pref("places.frecency.numCalcOnMigrate", 50);
pref("places.frecency.updateIdleTime", 60000);
pref("places.frecency.firstBucketCutoff", 4);
pref("places.frecency.secondBucketCutoff", 14);
pref("places.frecency.thirdBucketCutoff", 31);
pref("places.frecency.fourthBucketCutoff", 90);
pref("places.frecency.firstBucketWeight", 100);
pref("places.frecency.secondBucketWeight", 70);
pref("places.frecency.thirdBucketWeight", 50);
pref("places.frecency.fourthBucketWeight", 30);
pref("places.frecency.defaultBucketWeight", 10);
pref("places.frecency.embedVisitBonus", 0);
pref("places.frecency.linkVisitBonus", 100);
pref("places.frecency.typedVisitBonus", 2000);
pref("places.frecency.bookmarkVisitBonus", 150);
pref("places.frecency.downloadVisitBonus", 0);
pref("places.frecency.permRedirectVisitBonus", 0);
pref("places.frecency.tempRedirectVisitBonus", 0);
pref("places.frecency.defaultVisitBonus", 0);
pref("places.frecency.unvisitedBookmarkBonus", 140);
pref("places.frecency.unvisitedTypedBonus", 200);

// disable color management
pref("gfx.color_management.mode", 0);

// don't allow JS to move and resize existing windows
pref("dom.disable_window_move_resize", true);

// prevent click image resizing for nsImageDocument
pref("browser.enable_click_image_resizing", false);

// open in tab preferences
// 0=default window, 1=current window/tab, 2=new window, 3=new tab in most window
pref("browser.link.open_external", 3);
pref("browser.link.open_newwindow", 3);
// 0=force all new windows to tabs, 1=don't force, 2=only force those with no features set
pref("browser.link.open_newwindow.restriction", 0);

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

pref("plugins.force.wmode", "opaque");

// What default should we use for the time span in the sanitizer:
// 0 - Clear everything
// 1 - Last Hour
// 2 - Last 2 Hours
// 3 - Last 4 Hours
// 4 - Today
pref("privacy.sanitize.timeSpan", 1);
pref("privacy.sanitize.sanitizeOnShutdown", false);
pref("privacy.sanitize.migrateFx3Prefs",    false);

// enable geo
pref("geo.enabled", true);

// JS error console
pref("devtools.errorconsole.enabled", false);

// kinetic tweakables
pref("browser.ui.kinetic.updateInterval", 16);
pref("browser.ui.kinetic.exponentialC", 1400);
pref("browser.ui.kinetic.polynomialC", 100);
pref("browser.ui.kinetic.swipeLength", 160);
pref("browser.ui.zoom.animationDuration", 200); // ms duration of double-tap zoom animation

// pinch gesture
pref("browser.ui.pinch.maxGrowth", 150);     // max pinch distance growth
pref("browser.ui.pinch.maxShrink", 200);     // max pinch distance shrinkage
pref("browser.ui.pinch.scalingFactor", 500); // scaling factor for above pinch limits

pref("ui.mouse.radius.enabled", true);
pref("ui.touch.radius.enabled", true);

// plugins
pref("plugin.disable", true);
pref("dom.ipc.plugins.enabled", true);

// process priority
// higher values give content process less CPU time
pref("dom.ipc.content.nice", 1);

// product URLs
// The breakpad report server to link to in about:crashes
pref("breakpad.reportURL", "https://crash-stats.mozilla.com/report/index/");
// TODO: This is not the correct article for metro!!!
pref("app.sync.tutorialURL", "https://support.mozilla.org/kb/sync-firefox-between-desktop-and-mobile");
pref("app.support.baseURL", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/");
pref("app.privacyURL", "http://www.mozilla.org/%LOCALE%/legal/privacy/firefox.html");
pref("app.creditsURL", "http://www.mozilla.org/credits/");
pref("app.channelURL", "http://www.mozilla.org/%LOCALE%/firefox/channel/");

// Name of alternate about: page for certificate errors (when undefined, defaults to about:neterror)
pref("security.alternate_certificate_error_page", "certerror");

pref("security.warn_viewing_mixed", false); // Warning is disabled.  See Bug 616712.

// Override some named colors to avoid inverse OS themes

/* app update prefs */

#ifdef MOZ_UPDATER

// Whether or not app updates are enabled
pref("app.update.enabled", true);

// This preference turns on app.update.mode and allows automatic download and
// install to take place. We use a separate boolean toggle for this to make
// the UI easier to construct.
pref("app.update.auto", true);

// Defines how the Application Update Service notifies the user about updates:
//
// AUM Set to:        Minor Releases:     Major Releases:
// 0                  download no prompt  download no prompt
// 1                  download no prompt  download no prompt if no incompatibilities
// 2                  download no prompt  prompt
//
// See chart in nsUpdateService.js source for more details
pref("app.update.mode", 0);

// Enables update checking in the Metro environment.
// add-on incompatibilities are ignored by updates in Metro.
pref("app.update.metro.enabled", true);

// If set to true, the Update Service will present no UI for any event.
pref("app.update.silent", true);

// If set to true, the Update Service will apply updates in the background
// when it finishes downloading them.
pref("app.update.staging.enabled", true);

// Update service URL:
pref("app.update.url", "https://aus3.mozilla.org/update/3/%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml");

// Show the Update Checking/Ready UI when the user was idle for x seconds
pref("app.update.idletime", 60);

// Whether or not we show a dialog box informing the user that the update was
// successfully applied. This is off in Firefox by default since we show a
// upgrade start page instead! Other apps may wish to show this UI, and supply
// a whatsNewURL field in their brand.properties that contains a link to a page
// which tells users what's new in this new update.
pref("app.update.showInstalledUI", false);

// 0 = suppress prompting for incompatibilities if there are updates available
//     to newer versions of installed addons that resolve them.
// 1 = suppress prompting for incompatibilities only if there are VersionInfo
//     updates available to installed addons that resolve them, not newer
//     versions.
pref("app.update.incompatible.mode", 0);

// Whether or not to attempt using the service for updates.
#ifdef MOZ_MAINTENANCE_SERVICE
pref("app.update.service.enabled", true);
#endif

// The minimum delay in seconds for the timer to fire.
// default=2 minutes
pref("app.update.timerMinimumDelay", 120);

// Enables some extra Application Update Logging (can reduce performance)
pref("app.update.log", false);

// The number of general background check failures to allow before notifying the
// user of the failure. User initiated update checks always notify the user of
// the failure.
pref("app.update.backgroundMaxErrors", 10);

// When |app.update.cert.requireBuiltIn| is true or not specified the
// final certificate and all certificates the connection is redirected to before
// the final certificate for the url specified in the |app.update.url|
// preference must be built-in.
pref("app.update.cert.requireBuiltIn", true);

// When |app.update.cert.checkAttributes| is true or not specified the
// certificate attributes specified in the |app.update.certs.| preference branch
// are checked against the certificate for the url specified by the
// |app.update.url| preference.
pref("app.update.cert.checkAttributes", true);

// The number of certificate attribute check failures to allow for background
// update checks before notifying the user of the failure. User initiated update
// checks always notify the user of the certificate attribute check failure.
pref("app.update.cert.maxErrors", 5);

// The |app.update.certs.| preference branch contains branches that are
// sequentially numbered starting at 1 that contain attribute name / value
// pairs for the certificate used by the server that hosts the update xml file
// as specified in the |app.update.url| preference. When these preferences are
// present the following conditions apply for a successful update check:
// 1. the uri scheme must be https
// 2. the preference name must exist as an attribute name on the certificate and
//    the value for the name must be the same as the value for the attribute name
//    on the certificate.
// If these conditions aren't met it will be treated the same as when there is
// no update available. This validation will not be performed when the
// |app.update.url.override| user preference has been set for testing updates or
// when the |app.update.cert.checkAttributes| preference is set to false. Also,
// the |app.update.url.override| preference should ONLY be used for testing.
// IMPORTANT! firefox.js should also be updated for updates to certs.X.issuerName
pref("app.update.certs.1.issuerName", "OU=Equifax Secure Certificate Authority,O=Equifax,C=US");
pref("app.update.certs.1.commonName", "aus3.mozilla.org");
pref("app.update.certs.2.issuerName", "CN=Thawte SSL CA,O=\"Thawte, Inc.\",C=US");
pref("app.update.certs.2.commonName", "aus3.mozilla.org");

// User-settable override to app.update.url for testing purposes.
//pref("app.update.url.override", "");

// replace newlines with spaces on paste into single-line text boxes
pref("editor.singleLine.pasteNewlines", 2);

#ifdef MOZ_SERVICES_SYNC
// sync service
pref("services.sync.registerEngines", "Tab,Bookmarks,Form,History,Password,Prefs");

// prefs to sync by default
pref("services.sync.prefs.sync.browser.startup.sessionRestore", true);
pref("services.sync.prefs.sync.browser.tabs.warnOnClose", true);
pref("services.sync.prefs.sync.devtools.errorconsole.enabled", true);
pref("services.sync.prefs.sync.lightweightThemes.isThemeSelected", true);
pref("services.sync.prefs.sync.lightweightThemes.usedThemes", true);
pref("services.sync.prefs.sync.privacy.donottrackheader.enabled", true);
pref("services.sync.prefs.sync.privacy.donottrackheader.value", true);
pref("services.sync.prefs.sync.signon.rememberSignons", true);
#endif

// threshold where a tap becomes a drag, in 1/240" reference pixels
// The names of the preferences are to be in sync with nsEventStateManager.cpp
pref("ui.dragThresholdX", 50);
pref("ui.dragThresholdY", 50);

// prevent tooltips from showing up
pref("browser.chrome.toolbar_tips", false);

// Completely disable pdf.js as an option to preview pdfs within firefox.
// Note: if this is not disabled it does not necessarily mean pdf.js is the pdf
// handler just that it is an option.
pref("pdfjs.disabled", false);
// Used by pdf.js to know the first time firefox is run with it installed so it
// can become the default pdf viewer.
pref("pdfjs.firstRun", true);
// The values of preferredAction and alwaysAskBeforeHandling before pdf.js
// became the default.
pref("pdfjs.previousHandler.preferredAction", 0);
pref("pdfjs.previousHandler.alwaysAskBeforeHandling", false);

// The maximum amount of decoded image data we'll willingly keep around (we
// might keep around more than this, but we'll try to get down to this value).
// (This is intentionally on the high side; see bug 746055.)
pref("image.mem.max_decoded_image_kb", 256000);

// enable touch events interfaces
pref("dom.w3c_touch_events.enabled", 1);
pref("dom.w3c_touch_events.safetyX", 5); // escape borders in units of 1/240"
pref("dom.w3c_touch_events.safetyY", 20); // escape borders in units of 1/240"

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
pref("browser.safebrowsing.warning.infoURL", "https://www.mozilla.org/%LOCALE%/firefox/phishing-protection/");
pref("browser.geolocation.warning.infoURL", "https://www.mozilla.org/%LOCALE%/firefox/geolocation/");

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
pref("urlclassifier.max-complete-age", 2700);

// Maximum size of the sqlite3 cache during an update, in bytes
pref("urlclassifier.updatecachemax", 41943040);

// URL for checking the reason for a malware warning.
pref("browser.safebrowsing.malware.reportURL", "http://safebrowsing.clients.google.com/safebrowsing/diagnostic?client=%NAME%&hl=%LOCALE%&site=");
#endif

// True if this is the first time we are showing about:firstrun
pref("browser.firstrun.show.localepicker", false);

// True if you always want dump() to work
//
// On Android, you also need to do the following for the output
// to show up in logcat:
//
// $ adb shell stop
// $ adb shell setprop log.redirect-stdio true
// $ adb shell start
pref("javascript.options.showInConsole", true);
pref("browser.dom.window.dump.enabled", true);

// controls if we want camera support
pref("device.camera.enabled", true);
pref("media.realtime_decoder.enabled", true);

// Metro manages state by autodetection
pref("network.manage-offline-status", true);

// Enable HTML fullscreen API in content.
pref("full-screen-api.enabled", true);
// But don't require approval when content enters fullscreen; we'll keep our
// UI/chrome visible still, so there's no need to approve entering fullscreen.
pref("full-screen-api.approval-required", false);
// Don't allow fullscreen requests to percolate across content/chrome boundary,
// so that our chrome/UI remains visible after content enters fullscreen.
pref("full-screen-api.content-only", true);
// Don't make top-level widgets fullscreen. This only applies when running in
// "metrodesktop" mode, not when running in full metro mode. This prevents the
// window from changing size when we go fullscreen; the content expands to fill
// the window, the window size doesn't change. This pref has no effect when
// running in actual Metro mode, as the widget will already be fullscreen then.
pref("full-screen-api.ignore-widgets", true);
