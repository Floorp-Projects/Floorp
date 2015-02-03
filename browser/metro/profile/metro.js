/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#filter substitution

#ifdef DEBUG
// disable content and content script caching
pref("nglayout.debug.disable_xul_cache", true);
pref("nglayout.debug.disable_xul_fastload", true);
pref("devtools.errorconsole.enabled", true);
pref("devtools.chrome.enabled", true);
#else
pref("devtools.errorconsole.enabled", false);
pref("devtools.chrome.enabled", false);
#endif

// Automatically submit crash reports
#ifdef RELEASE_BUILD
pref("app.crashreporter.autosubmit", false);
pref("app.crashreporter.submitURLs", false);
#else
// For Nightly and Aurora we turn this on by default
pref("app.crashreporter.autosubmit", true);
pref("app.crashreporter.submitURLs", false);
#endif
// Has the user been prompted about crash reporting?
pref("app.crashreporter.prompted", false);

// Debug prefs, see input.js
pref("metro.debug.colorizeInputOverlay", false);
pref("metro.debug.selection.displayRanges", false);
pref("metro.debug.selection.dumpRanges", false);
pref("metro.debug.selection.dumpEvents", false);

// Private browsing is disabled by default until implementation and testing are complete
pref("metro.private_browsing.enabled", false);

// Enable tab-modal prompts
pref("prompts.tab_modal.enabled", true);

// NewTabUtils pref related to top site thumbnail updating.
pref("browser.newtabpage.enabled", true);

// Enable off main thread compositing
pref("layers.offmainthreadcomposition.enabled", true);
pref("layers.async-pan-zoom.enabled", true);
pref("layers.componentalpha.enabled", false);

// Prefs to control the async pan/zoom behaviour
pref("apz.touch_start_tolerance", "0.1"); // dpi * tolerance = pixel threshold
pref("apz.pan_repaint_interval", 50);   // prefer 20 fps
pref("apz.fling_repaint_interval", 50); // prefer 20 fps
pref("apz.smooth_scroll_repaint_interval", 50); // prefer 20 fps
pref("apz.fling_stopped_threshold", "0.2");
pref("apz.x_skate_size_multiplier", "2.5");
pref("apz.y_skate_size_multiplier", "2.5");
pref("apz.min_skate_speed", "10.0");
// 0 = free, 1 = standard, 2 = sticky
pref("apz.axis_lock.mode", 2);
pref("apz.cross_slide.enabled", true);

// Enable Microsoft TSF support by default for imes.
pref("intl.tsf.enable", true);
pref("intl.tsf.support_imm", false);
pref("intl.tsf.hack.atok.create_native_caret", false);

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

// Telemetry
#ifdef MOZ_TELEMETRY_ON_BY_DEFAULT
pref("toolkit.telemetry.enabledPreRelease", true);
#else
pref("toolkit.telemetry.enabled", true);
#endif
pref("toolkit.telemetry.prompted", 2);

pref("toolkit.screen.lock", false);

// From libpref/src/init/all.js. Disabling text zoom in favor of APZ zoom. See bug 936940.
pref("zoom.minPercent", 100);
pref("zoom.maxPercent", 100);
pref("toolkit.zoomManager.zoomValues", "1");

// Device pixel to CSS px ratio, in percent. Set to -1 to calculate based on display density.
pref("browser.viewport.scaleRatio", -1);

// use long press to display a context menu
pref("ui.click_hold_context_menus", false);

// offline cache prefs
pref("browser.offline-apps.notify", true);

// protocol warning prefs
pref("network.protocol-handler.warn-external.tel", false);
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.vnd.youtube", false);
pref("network.protocol-handler.warn-external.ms-windows-store", false);
pref("network.protocol-handler.external.ms-windows-store", true);

/* startui prefs */
// display the overlay nav buttons
pref("browser.display.overlaynavbuttons", true);
// max number of top site tiles to display in the startui
pref("browser.display.startUI.topsites.maxresults", 8);
// max items for the bookmarks compartment in the startui
pref("browser.display.startUI.bookmarks.maxresults", 16);
// max items for the history compartment in the startui
pref("browser.display.startUI.history.maxresults", 16);
// Number of times to display firstrun instructions on new tab page
pref("browser.firstrun.count", 3);
// Has the content first run been dismissed
pref("browser.firstrun-content.dismissed", false);

// Backspace and Shift+Backspace behavior
// 0 goes Back/Forward
// 1 act like PgUp/PgDown
// 2 and other values, nothing
pref("browser.backspace_action", 0);

// session history
pref("browser.sessionhistory.max_entries", 50);

// On startup, don't automatically restore tabs
pref("browser.startup.page", 1);

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

// this will automatically enable inline spellchecking (if it is available) for
// editable elements in HTML
// 0 = spellcheck nothing
// 1 = check multi-line controls [default]
// 2 = check multi/single line controls
pref("layout.spellcheckDefault", 1);

/* extension manager and xpinstall */
// Completely disable extensions
pref("extensions.defaultProviders.enabled", false);
// Disable version checks making addons compatible by default
pref("extensions.strictCompatibility", false);
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
pref("extensions.blocklist.url", "https://blocklist.addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/%PRODUCT%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/%PING_COUNT%/%TOTAL_PING_COUNT%/%DAYS_SINCE_LAST_PING%/");
pref("extensions.blocklist.detailsURL", "https://www.mozilla.org/%LOCALE%/blocklist/");
pref("extensions.showMismatchUI", false);

/* block popups by default, and notify the user about blocked popups */
pref("dom.disable_open_during_load", true);
pref("privacy.popups.showBrowserMessage", true);

// Metro Firefox keeps this set to -1 when donottrackheader.enabled is false.
pref("privacy.donottrackheader.value", -1);

/* disable opening windows with the dialog feature */
pref("dom.disable_window_open_dialog_feature", true);

pref("keyword.enabled", true);
pref("browser.fixup.domainwhitelist.localhost", true);

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
pref("browser.search.defaultenginename", "chrome://browser/locale/region.properties");

// SSL error page behaviour
pref("browser.ssl_override_behavior", 2);
pref("browser.xul.error_pages.expert_bad_cert", false);

// disable logging for the search service by default
pref("browser.search.log", false);

// ordering of search engines in the engine list.
pref("browser.search.order.1", "chrome://browser/locale/region.properties");
pref("browser.search.order.2", "chrome://browser/locale/region.properties");
pref("browser.search.order.3", "chrome://browser/locale/region.properties");

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

// Let the faviconservice know that we display favicons as 25x25px so that it
// uses the right size when optimizing favicons
pref("places.favicons.optimizeToDimension", 25);

// The default behavior for the urlbar can be configured to use any combination
// of the match filters with each additional filter adding more results (union).
pref("browser.urlbar.suggest.history",              true);
pref("browser.urlbar.suggest.bookmark",             true);
pref("browser.urlbar.suggest.openpage",             true);
pref("browser.urlbar.suggest.search",               true);

// Restrictions to current suggestions can also be applied (intersection).
// Typed suggestion works only if history is set to true.
pref("browser.urlbar.suggest.history.onlyTyped",    false);

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
pref("geo.wifi.uri", "https://location.services.mozilla.com/v1/geolocate?key=%MOZILLA_API_KEY%");

// snapped view
pref("browser.ui.snapped.maxWidth", 600);

// kinetic tweakables
pref("browser.ui.kinetic.updateInterval", 16);
pref("browser.ui.kinetic.exponentialC", 1400);
pref("browser.ui.kinetic.polynomialC", 100);
pref("browser.ui.kinetic.swipeLength", 160);
pref("browser.ui.zoom.animationDuration", 200); // ms duration of double-tap zoom animation

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
pref("app.support.baseURL", "https://support.mozilla.org/1/touch/%VERSION%/%OS%/%LOCALE%/");
pref("app.support.inputURL", "https://input.mozilla.org/feedback/metrofirefox");
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
pref("app.update.url", "https://aus4.mozilla.org/update/3/%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml");

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

// The aus update xml certificate checks for application update are disabled on
// Windows since the mar signature check which is currently only implemented on
// Windows is sufficient for preventing us from applying a mar that is not
// valid.

// When |app.update.cert.requireBuiltIn| is true or not specified the
// final certificate and all certificates the connection is redirected to before
// the final certificate for the url specified in the |app.update.url|
// preference must be built-in.
pref("app.update.cert.requireBuiltIn", false);

// When |app.update.cert.checkAttributes| is true or not specified the
// certificate attributes specified in the |app.update.certs.| preference branch
// are checked against the certificate for the url specified by the
// |app.update.url| preference.
pref("app.update.cert.checkAttributes", false);

// User-settable override to app.update.url for testing purposes.
//pref("app.update.url.override", "");

// replace newlines with spaces on paste into single-line text boxes
pref("editor.singleLine.pasteNewlines", 2);

#ifdef MOZ_SERVICES_SYNC
// sync service
pref("services.sync.registerEngines", "Tab,Bookmarks,Form,History,Password,Prefs");

// prefs to sync by default
pref("services.sync.prefs.sync.browser.tabs.warnOnClose", true);
pref("services.sync.prefs.sync.devtools.errorconsole.enabled", true);
pref("services.sync.prefs.sync.lightweightThemes.isThemeSelected", true);
pref("services.sync.prefs.sync.lightweightThemes.usedThemes", true);
pref("services.sync.prefs.sync.privacy.donottrackheader.enabled", true);
pref("services.sync.prefs.sync.privacy.donottrackheader.value", true);
pref("services.sync.prefs.sync.signon.rememberSignons", true);
#endif

// threshold where a tap becomes a drag, in 1/240" reference pixels
// The names of the preferences are to be in sync with EventStateManager.cpp
pref("ui.dragThresholdX", 50);
pref("ui.dragThresholdY", 50);

// prevent tooltips from showing up
pref("browser.chrome.toolbar_tips", false);

#ifdef NIGHTLY_BUILD
// Completely disable pdf.js as an option to preview pdfs within firefox.
// Note: if this is not disabled it does not necessarily mean pdf.js is the pdf
// handler just that it is an option.
pref("pdfjs.disabled", true);
// Used by pdf.js to know the first time firefox is run with it installed so it
// can become the default pdf viewer.
pref("pdfjs.firstRun", true);
// The values of preferredAction and alwaysAskBeforeHandling before pdf.js
// became the default.
pref("pdfjs.previousHandler.preferredAction", 0);
pref("pdfjs.previousHandler.alwaysAskBeforeHandling", false);
#endif

#ifdef NIGHTLY_BUILD
// Shumay is currently experimental.  Toggle this pref to enable Shumway for
// testing and development.
pref("shumway.disabled", true);
// When Shumway is enabled, use it all the time, not only when Flash is set to
// click-to-play (because Metro doesn't even load the native Flash plugin).
pref("shumway.ignoreCTP", true);
#endif

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
pref("browser.geolocation.warning.infoURL", "https://www.mozilla.org/%LOCALE%/firefox/geolocation/");

// Name of the about: page contributed by safebrowsing to handle display of error
// pages on phishing/malware hits.  (bug 399233)
pref("urlclassifier.alternate_error_page", "blocked");

// The number of random entries to send with a gethash request.
pref("urlclassifier.gethashnoise", 4);

// Gethash timeout for Safebrowsing.
pref("urlclassifier.gethash.timeout_ms", 5000);

// If an urlclassifier table has not been updated in this number of seconds,
// a gethash request will be forced to check that the result is still in
// the database.
pref("urlclassifier.max-complete-age", 2700);

// Maximum size of the sqlite3 cache during an update, in bytes
pref("urlclassifier.updatecachemax", 41943040);

// URL for checking the reason for a malware warning.
pref("browser.safebrowsing.malware.reportURL", "https://safebrowsing.google.com/safebrowsing/diagnostic?client=%NAME%&hl=%LOCALE%&site=");
#endif

// True if this is the first time we are showing about:firstrun
pref("browser.firstrun.show.localepicker", false);

// True if you always want dump() to work
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

// image visibility prefs.
// image visibility tries to only keep images near the viewport decoded instead
// of keeping all images decoded.
pref("layout.imagevisibility.numscrollportwidths", 1);
pref("layout.imagevisibility.numscrollportheights", 1);

// Don't enable <input type=color> yet as we don't have a color picker
// implemented for Windows Metro (bug 895464)
pref("dom.forms.color", false);
