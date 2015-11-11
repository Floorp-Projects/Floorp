# -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

// XXX Toolkit-specific preferences should be moved into toolkit.js

#filter substitution

#
# SYNTAX HINTS:
#
#  - Dashes are delimiters; use underscores instead.
#  - The first character after a period must be alphabetic.
#  - Computed values (e.g. 50 * 1024) don't work.
#

#ifdef XP_UNIX
#ifndef XP_MACOSX
#define UNIX_BUT_NOT_MAC
#endif
#endif

pref("browser.chromeURL","chrome://browser/content/");
pref("browser.hiddenWindowChromeURL", "chrome://browser/content/hiddenWindow.xul");

// Enables some extra Extension System Logging (can reduce performance)
pref("extensions.logging.enabled", false);

// Disables strict compatibility, making addons compatible-by-default.
pref("extensions.strictCompatibility", false);

// Specifies a minimum maxVersion an addon needs to say it's compatible with
// for it to be compatible by default.
pref("extensions.minCompatibleAppVersion", "4.0");
// Temporary preference to forcibly make themes more safe with Australis even if
// extensions.checkCompatibility=false has been set.
pref("extensions.checkCompatibility.temporaryThemeOverride_minAppVersion", "29.0a1");

pref("xpinstall.customConfirmationUI", true);

// Preferences for AMO integration
pref("extensions.getAddons.cache.enabled", true);
pref("extensions.getAddons.maxResults", 15);
pref("extensions.getAddons.get.url", "https://services.addons.mozilla.org/%LOCALE%/firefox/api/%API_VERSION%/search/guid:%IDS%?src=firefox&appOS=%OS%&appVersion=%VERSION%");
pref("extensions.getAddons.getWithPerformance.url", "https://services.addons.mozilla.org/%LOCALE%/firefox/api/%API_VERSION%/search/guid:%IDS%?src=firefox&appOS=%OS%&appVersion=%VERSION%&tMain=%TIME_MAIN%&tFirstPaint=%TIME_FIRST_PAINT%&tSessionRestored=%TIME_SESSION_RESTORED%");
pref("extensions.getAddons.search.browseURL", "https://addons.mozilla.org/%LOCALE%/firefox/search?q=%TERMS%&platform=%OS%&appver=%VERSION%");
pref("extensions.getAddons.search.url", "https://services.addons.mozilla.org/%LOCALE%/firefox/api/%API_VERSION%/search/%TERMS%/all/%MAX_RESULTS%/%OS%/%VERSION%/%COMPATIBILITY_MODE%?src=firefox");
pref("extensions.webservice.discoverURL", "https://services.addons.mozilla.org/%LOCALE%/firefox/discovery/pane/%VERSION%/%OS%/%COMPATIBILITY_MODE%");
pref("extensions.getAddons.recommended.url", "https://services.addons.mozilla.org/%LOCALE%/%APP%/api/%API_VERSION%/list/recommended/all/%MAX_RESULTS%/%OS%/%VERSION%?src=firefox");
pref("extensions.getAddons.link.url", "https://addons.mozilla.org/%LOCALE%/firefox/");

// Blocklist preferences
pref("extensions.blocklist.enabled", true);
// OneCRL freshness checking depends on this value, so if you change it,
// please also update security.onecrl.maximum_staleness_in_seconds.
pref("extensions.blocklist.interval", 86400);
// Controls what level the blocklist switches from warning about items to forcibly
// blocking them.
pref("extensions.blocklist.level", 2);
pref("extensions.blocklist.url", "https://blocklist.addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/%PRODUCT%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/%PING_COUNT%/%TOTAL_PING_COUNT%/%DAYS_SINCE_LAST_PING%/");
pref("extensions.blocklist.detailsURL", "https://www.mozilla.org/%LOCALE%/blocklist/");
pref("extensions.blocklist.itemURL", "https://blocklist.addons.mozilla.org/%LOCALE%/%APP%/blocked/%blockID%");

pref("extensions.update.autoUpdateDefault", true);

pref("extensions.hotfix.id", "firefox-hotfix@mozilla.org");
pref("extensions.hotfix.cert.checkAttributes", true);
pref("extensions.hotfix.certs.1.sha1Fingerprint", "91:53:98:0C:C1:86:DF:47:8F:35:22:9E:11:C9:A7:31:04:49:A1:AA");
pref("extensions.hotfix.certs.2.sha1Fingerprint", "39:E7:2B:7A:5B:CF:37:78:F9:5D:4A:E0:53:2D:2F:3D:68:53:C5:60");

// Check AUS for system add-on updates.
pref("extensions.systemAddon.update.url", "https://aus5.mozilla.org/update/3/SystemAddons/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml");

// Disable add-ons that are not installed by the user in all scopes by default.
// See the SCOPE constants in AddonManager.jsm for values to use here.
pref("extensions.autoDisableScopes", 15);

// Require signed add-ons by default
pref("xpinstall.signatures.required", true);
pref("xpinstall.signatures.devInfoURL", "https://wiki.mozilla.org/Addons/Extension_Signing");

// Dictionary download preference
pref("browser.dictionaries.download.url", "https://addons.mozilla.org/%LOCALE%/firefox/dictionaries/");

// At startup, should we check to see if the installation
// date is older than some threshold
pref("app.update.checkInstallTime", true);

// The number of days a binary is permitted to be old without checking is defined in
// firefox-branding.js (app.update.checkInstallTime.days)

// The minimum delay in seconds for the timer to fire.
// default=2 minutes
pref("app.update.timerMinimumDelay", 120);

// App-specific update preferences

// The interval to check for updates (app.update.interval) is defined in
// firefox-branding.js

// Alternative windowtype for an application update user interface window. When
// a window with this windowtype is open the application update service won't
// open the normal application update user interface window.
pref("app.update.altwindowtype", "Browser:About");

// Enables some extra Application Update Logging (can reduce performance)
pref("app.update.log", false);

// The number of general background check failures to allow before notifying the
// user of the failure. User initiated update checks always notify the user of
// the failure.
pref("app.update.backgroundMaxErrors", 10);

// The aus update xml certificate checks for application update are disabled on
// Windows, Mac OS X, and Linux since the mar signature check are implemented on
// these platforms and is sufficient to prevent us from applying a mar that is
// not valid. Bug 1182352 will remove the update xml certificate checks and the
// following two preferences.
pref("app.update.cert.requireBuiltIn", false);
pref("app.update.cert.checkAttributes", false);

// Whether or not app updates are enabled
pref("app.update.enabled", true);

// This preference turns on app.update.mode and allows automatic download and
// install to take place. We use a separate boolean toggle for this to make
// the UI easier to construct.
pref("app.update.auto", true);

// See chart in nsUpdateService.js source for more details
pref("app.update.mode", 1);

// If set to true, the Update Service will present no UI for any event.
pref("app.update.silent", false);

// If set to true, the hamburger button will show badges for update events.
#ifndef RELEASE_BUILD
pref("app.update.badge", true);
#else
pref("app.update.badge", false);
#endif
// app.update.badgeWaitTime is in branding section

// If set to true, the Update Service will apply updates in the background
// when it finishes downloading them.
pref("app.update.staging.enabled", true);

// Update service URL:
pref("app.update.url", "https://aus5.mozilla.org/update/3/%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml");
// app.update.url.manual is in branding section
// app.update.url.details is in branding section

// User-settable override to app.update.url for testing purposes.
//pref("app.update.url.override", "");

// app.update.interval is in branding section
// app.update.promptWaitTime is in branding section

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

// Symmetric (can be overridden by individual extensions) update preferences.
// e.g.
//  extensions.{GUID}.update.enabled
//  extensions.{GUID}.update.url
//  .. etc ..
//
pref("extensions.update.enabled", true);
pref("extensions.update.url", "https://versioncheck.addons.mozilla.org/update/VersionCheck.php?reqVersion=%REQ_VERSION%&id=%ITEM_ID%&version=%ITEM_VERSION%&maxAppVersion=%ITEM_MAXAPPVERSION%&status=%ITEM_STATUS%&appID=%APP_ID%&appVersion=%APP_VERSION%&appOS=%APP_OS%&appABI=%APP_ABI%&locale=%APP_LOCALE%&currentAppVersion=%CURRENT_APP_VERSION%&updateType=%UPDATE_TYPE%&compatMode=%COMPATIBILITY_MODE%");
pref("extensions.update.background.url", "https://versioncheck-bg.addons.mozilla.org/update/VersionCheck.php?reqVersion=%REQ_VERSION%&id=%ITEM_ID%&version=%ITEM_VERSION%&maxAppVersion=%ITEM_MAXAPPVERSION%&status=%ITEM_STATUS%&appID=%APP_ID%&appVersion=%APP_VERSION%&appOS=%APP_OS%&appABI=%APP_ABI%&locale=%APP_LOCALE%&currentAppVersion=%CURRENT_APP_VERSION%&updateType=%UPDATE_TYPE%&compatMode=%COMPATIBILITY_MODE%");
pref("extensions.update.interval", 86400);  // Check for updates to Extensions and
                                            // Themes every day
// Non-symmetric (not shared by extensions) extension-specific [update] preferences
pref("extensions.dss.enabled", false);          // Dynamic Skin Switching
pref("extensions.dss.switchPending", false);    // Non-dynamic switch pending after next
                                                // restart.

pref("extensions.{972ce4c6-7e08-4474-a285-3208198ce6fd}.name", "chrome://browser/locale/browser.properties");
pref("extensions.{972ce4c6-7e08-4474-a285-3208198ce6fd}.description", "chrome://browser/locale/browser.properties");

pref("lightweightThemes.update.enabled", true);
pref("lightweightThemes.getMoreURL", "https://addons.mozilla.org/%LOCALE%/firefox/themes");
pref("lightweightThemes.recommendedThemes", "[{\"id\":\"recommended-1\",\"homepageURL\":\"https://addons.mozilla.org/firefox/addon/a-web-browser-renaissance/\",\"headerURL\":\"resource:///chrome/browser/content/browser/defaultthemes/1.header.jpg\",\"footerURL\":\"resource:///chrome/browser/content/browser/defaultthemes/1.footer.jpg\",\"textcolor\":\"#000000\",\"accentcolor\":\"#f2d9b1\",\"iconURL\":\"resource:///chrome/browser/content/browser/defaultthemes/1.icon.jpg\",\"previewURL\":\"resource:///chrome/browser/content/browser/defaultthemes/1.preview.jpg\",\"author\":\"Sean.Martell\",\"version\":\"0\"},{\"id\":\"recommended-2\",\"homepageURL\":\"https://addons.mozilla.org/firefox/addon/space-fantasy/\",\"headerURL\":\"resource:///chrome/browser/content/browser/defaultthemes/2.header.jpg\",\"footerURL\":\"resource:///chrome/browser/content/browser/defaultthemes/2.footer.jpg\",\"textcolor\":\"#ffffff\",\"accentcolor\":\"#d9d9d9\",\"iconURL\":\"resource:///chrome/browser/content/browser/defaultthemes/2.icon.jpg\",\"previewURL\":\"resource:///chrome/browser/content/browser/defaultthemes/2.preview.jpg\",\"author\":\"fx5800p\",\"version\":\"1.0\"},{\"id\":\"recommended-3\",\"homepageURL\":\"https://addons.mozilla.org/firefox/addon/linen-light/\",\"headerURL\":\"resource:///chrome/browser/content/browser/defaultthemes/3.header.png\",\"footerURL\":\"resource:///chrome/browser/content/browser/defaultthemes/3.footer.png\",\"accentcolor\":\"#ada8a8\",\"iconURL\":\"resource:///chrome/browser/content/browser/defaultthemes/3.icon.png\",\"previewURL\":\"resource:///chrome/browser/content/browser/defaultthemes/3.preview.png\",\"author\":\"DVemer\",\"version\":\"1.0\"},{\"id\":\"recommended-4\",\"homepageURL\":\"https://addons.mozilla.org/firefox/addon/pastel-gradient/\",\"headerURL\":\"resource:///chrome/browser/content/browser/defaultthemes/4.header.png\",\"footerURL\":\"resource:///chrome/browser/content/browser/defaultthemes/4.footer.png\",\"textcolor\":\"#000000\",\"accentcolor\":\"#000000\",\"iconURL\":\"resource:///chrome/browser/content/browser/defaultthemes/4.icon.png\",\"previewURL\":\"resource:///chrome/browser/content/browser/defaultthemes/4.preview.png\",\"author\":\"darrinhenein\",\"version\":\"1.0\"},{\"id\":\"recommended-5\",\"homepageURL\":\"https://addons.mozilla.org/firefox/addon/carbon-light/\",\"headerURL\":\"resource:///chrome/browser/content/browser/defaultthemes/5.header.png\",\"footerURL\":\"resource:///chrome/browser/content/browser/defaultthemes/5.footer.png\",\"textcolor\":\"#3b3b3b\",\"accentcolor\":\"#2e2e2e\",\"iconURL\":\"resource:///chrome/browser/content/browser/defaultthemes/5.icon.jpg\",\"previewURL\":\"resource:///chrome/browser/content/browser/defaultthemes/5.preview.jpg\",\"author\":\"Jaxivo\",\"version\":\"1.0\"}]");

pref("browser.eme.ui.enabled", false);

// UI tour experience.
pref("browser.uitour.enabled", true);
pref("browser.uitour.loglevel", "Error");
pref("browser.uitour.requireSecure", true);
pref("browser.uitour.themeOrigin", "https://addons.mozilla.org/%LOCALE%/firefox/themes/");
pref("browser.uitour.url", "https://www.mozilla.org/%LOCALE%/firefox/%VERSION%/tour/");
// This is used as a regexp match against the page's URL.
pref("browser.uitour.readerViewTrigger", "^https:\\/\\/www\\.mozilla\\.org\\/[^\\/]+\\/firefox\\/reading\\/start");

pref("browser.customizemode.tip0.shown", false);
pref("browser.customizemode.tip0.learnMoreUrl", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/customize");

pref("keyword.enabled", true);
pref("browser.fixup.domainwhitelist.localhost", true);

pref("general.useragent.locale", "@AB_CD@");
pref("general.skins.selectedSkin", "classic/1.0");

pref("general.smoothScroll", true);
#ifdef UNIX_BUT_NOT_MAC
pref("general.autoScroll", false);
#else
pref("general.autoScroll", true);
#endif

// At startup, check if we're the default browser and prompt user if not.
pref("browser.shell.checkDefaultBrowser", true);
pref("browser.shell.shortcutFavicons",true);
pref("browser.shell.mostRecentDateSetAsDefault", "");
pref("browser.shell.skipDefaultBrowserCheck", true);
pref("browser.shell.defaultBrowserCheckCount", 0);

// 0 = blank, 1 = home (browser.startup.homepage), 2 = last visited page, 3 = resume previous browser session
// The behavior of option 3 is detailed at: http://wiki.mozilla.org/Session_Restore
pref("browser.startup.page",                1);
pref("browser.startup.homepage",            "chrome://branding/locale/browserconfig.properties");

pref("browser.slowStartup.notificationDisabled", false);
pref("browser.slowStartup.timeThreshold", 40000);
pref("browser.slowStartup.maxSamples", 5);

// This url, if changed, MUST continue to point to an https url. Pulling arbitrary content to inject into
// this page over http opens us up to a man-in-the-middle attack that we'd rather not face. If you are a downstream
// repackager of this code using an alternate snippet url, please keep your users safe
pref("browser.aboutHomeSnippets.updateUrl", "https://snippets.cdn.mozilla.net/%STARTPAGE_VERSION%/%NAME%/%VERSION%/%APPBUILDID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/");

pref("browser.enable_automatic_image_resizing", true);
pref("browser.casting.enabled", false);
pref("browser.chrome.site_icons", true);
pref("browser.chrome.favicons", true);
// browser.warnOnQuit == false will override all other possible prompts when quitting or restarting
pref("browser.warnOnQuit", true);
// browser.showQuitWarning specifically controls the quit warning dialog. We
// might still show the window closing dialog with showQuitWarning == false.
pref("browser.showQuitWarning", false);
pref("browser.fullscreen.autohide", true);
pref("browser.fullscreen.animate", true);
pref("browser.overlink-delay", 80);

#ifdef UNIX_BUT_NOT_MAC
pref("browser.urlbar.clickSelectsAll", false);
#else
pref("browser.urlbar.clickSelectsAll", true);
#endif
#ifdef UNIX_BUT_NOT_MAC
pref("browser.urlbar.doubleClickSelectsAll", true);
#else
pref("browser.urlbar.doubleClickSelectsAll", false);
#endif

// Control autoFill behavior
pref("browser.urlbar.autoFill", true);
pref("browser.urlbar.autoFill.typed", true);

pref("browser.urlbar.unifiedcomplete", true);

// 0: Match anywhere (e.g., middle of words)
// 1: Match on word boundaries and then try matching anywhere
// 2: Match only on word boundaries (e.g., after / or .)
// 3: Match at the beginning of the url or title
pref("browser.urlbar.matchBehavior", 1);
pref("browser.urlbar.filter.javascript", true);

// the maximum number of results to show in autocomplete when doing richResults
pref("browser.urlbar.maxRichResults", 12);
// The amount of time (ms) to wait after the user has stopped typing
// before starting to perform autocomplete.  50 is the default set in
// autocomplete.xml.
pref("browser.urlbar.delay", 50);

// The special characters below can be typed into the urlbar to either restrict
// the search to visited history, bookmarked, tagged pages; or force a match on
// just the title text or url.
pref("browser.urlbar.restrict.history", "^");
pref("browser.urlbar.restrict.bookmark", "*");
pref("browser.urlbar.restrict.tag", "+");
pref("browser.urlbar.restrict.openpage", "%");
pref("browser.urlbar.restrict.typed", "~");
pref("browser.urlbar.restrict.searches", "$");
pref("browser.urlbar.match.title", "#");
pref("browser.urlbar.match.url", "@");

// The default behavior for the urlbar can be configured to use any combination
// of the match filters with each additional filter adding more results (union).
pref("browser.urlbar.suggest.history",              true);
pref("browser.urlbar.suggest.bookmark",             true);
pref("browser.urlbar.suggest.openpage",             true);
pref("browser.urlbar.suggest.searches",             false);
pref("browser.urlbar.userMadeSearchSuggestionsChoice", false);

// Limit the number of characters sent to the current search engine to fetch
// suggestions.
pref("browser.urlbar.maxCharsForSearchSuggestions", 20);

// Restrictions to current suggestions can also be applied (intersection).
// Typed suggestion works only if history is set to true.
pref("browser.urlbar.suggest.history.onlyTyped",    false);

pref("browser.urlbar.formatting.enabled", true);
pref("browser.urlbar.trimURLs", true);

pref("browser.altClickSave", false);

// Enable logging downloads operations to the Console.
pref("browser.download.loglevel", "Error");

// Number of milliseconds to wait for the http headers (and thus
// the Content-Disposition filename) before giving up and falling back to
// picking a filename without that info in hand so that the user sees some
// feedback from their action.
pref("browser.download.saveLinkAsFilenameTimeout", 4000);

pref("browser.download.useDownloadDir", true);
pref("browser.download.folderList", 1);
pref("browser.download.manager.addToRecentDocs", true);
pref("browser.download.manager.resumeOnWakeDelay", 10000);

// This allows disabling the animated notifications shown by
// the Downloads Indicator when a download starts or completes.
pref("browser.download.animateNotifications", true);

// This records whether or not the panel has been shown at least once.
pref("browser.download.panel.shown", false);

#ifndef XP_MACOSX
pref("browser.helperApps.deleteTempFileOnExit", true);
#endif

// search engines URL
pref("browser.search.searchEnginesURL",      "https://addons.mozilla.org/%LOCALE%/firefox/search-engines/");

// pointer to the default engine name
pref("browser.search.defaultenginename",      "chrome://browser-region/locale/region.properties");

// Ordering of Search Engines in the Engine list.
pref("browser.search.order.1",                "chrome://browser-region/locale/region.properties");
pref("browser.search.order.2",                "chrome://browser-region/locale/region.properties");
pref("browser.search.order.3",                "chrome://browser-region/locale/region.properties");

// Market-specific search defaults
// This is disabled globally, and then enabled for individual locales
// in firefox-l10n.js (eg. it's enabled for en-US).
pref("browser.search.geoSpecificDefaults", false);
pref("browser.search.geoSpecificDefaults.url", "https://search.services.mozilla.com/1/%APP%/%VERSION%/%CHANNEL%/%LOCALE%/%REGION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%");

// US specific default (used as a fallback if the geoSpecificDefaults request fails).
pref("browser.search.defaultenginename.US",      "data:text/plain,browser.search.defaultenginename.US=Yahoo");
pref("browser.search.order.US.1",                "data:text/plain,browser.search.order.US.1=Yahoo");
pref("browser.search.order.US.2",                "data:text/plain,browser.search.order.US.2=Google");
pref("browser.search.order.US.3",                "data:text/plain,browser.search.order.US.3=Bing");

// search bar results always open in a new tab
pref("browser.search.openintab", false);

// context menu searches open in the foreground
pref("browser.search.context.loadInBackground", false);

// comma seperated list of of engines to hide in the search panel.
pref("browser.search.hiddenOneOffs", "");

#ifdef XP_WIN
pref("browser.search.redirectWindowsSearch", true);
#else
pref("browser.search.redirectWindowsSearch", false);
#endif

pref("browser.usedOnWindows10", false);
pref("browser.usedOnWindows10.introURL", "https://www.mozilla.org/%LOCALE%/firefox/windows-10/welcome/?utm_source=firefox-browser&utm_medium=firefox-browser");

pref("browser.sessionhistory.max_entries", 50);

// Built-in default permissions.
pref("permissions.manager.defaultsUrl", "resource://app/defaults/permissions");

// handle links targeting new windows
// 1=current window/tab, 2=new window, 3=new tab in most recent window
pref("browser.link.open_newwindow", 3);

// handle external links (i.e. links opened from a different application)
// default: use browser.link.open_newwindow
// 1-3: see browser.link.open_newwindow for interpretation
pref("browser.link.open_newwindow.override.external", -1);

// 0: no restrictions - divert everything
// 1: don't divert window.open at all
// 2: don't divert window.open with features
pref("browser.link.open_newwindow.restriction", 2);

// If true, this pref causes windows opened by window.open to be forced into new
// tabs (rather than potentially opening separate windows, depending on
// window.open arguments) when the browser is in fullscreen mode.
// We set this differently on Mac because the fullscreen implementation there is
// different.
#ifdef XP_MACOSX
pref("browser.link.open_newwindow.disabled_in_fullscreen", true);
#else
pref("browser.link.open_newwindow.disabled_in_fullscreen", false);
#endif

// Tabbed browser
pref("browser.tabs.closeWindowWithLastTab", true);
pref("browser.tabs.insertRelatedAfterCurrent", true);
pref("browser.tabs.warnOnClose", true);
pref("browser.tabs.warnOnCloseOtherTabs", true);
pref("browser.tabs.warnOnOpen", true);
pref("browser.tabs.maxOpenBeforeWarn", 15);
pref("browser.tabs.loadInBackground", true);
pref("browser.tabs.opentabfor.middleclick", true);
pref("browser.tabs.loadDivertedInBackground", false);
pref("browser.tabs.loadBookmarksInBackground", false);
pref("browser.tabs.tabClipWidth", 140);
pref("browser.tabs.animate", true);
#ifdef UNIX_BUT_NOT_MAC
pref("browser.tabs.drawInTitlebar", false);
#else
pref("browser.tabs.drawInTitlebar", true);
#endif

// When tabs opened by links in other tabs via a combination of
// browser.link.open_newwindow being set to 3 and target="_blank" etc are
// closed:
// true   return to the tab that opened this tab (its owner)
// false  return to the adjacent tab (old default)
pref("browser.tabs.selectOwnerOnClose", true);

pref("browser.tabs.showAudioPlayingIcon", true);

pref("browser.tabs.dontfocusfordialogs", true);

pref("browser.ctrlTab.previews", false);

// By default, do not export HTML at shutdown.
// If true, at shutdown the bookmarks in your menu and toolbar will
// be exported as HTML to the bookmarks.html file.
pref("browser.bookmarks.autoExportHTML",          false);

// The maximum number of daily bookmark backups to
// keep in {PROFILEDIR}/bookmarkbackups. Special values:
// -1: unlimited
//  0: no backups created (and deletes all existing backups)
pref("browser.bookmarks.max_backups",             15);

// Scripts & Windows prefs
pref("dom.disable_open_during_load",              true);
pref("javascript.options.showInConsole",          true);
#ifdef DEBUG
pref("general.warnOnAboutConfig",                 false);
#endif

// This is the pref to control the location bar, change this to true to
// force this - this makes the origin of popup windows more obvious to avoid
// spoofing. We would rather not do it by default because it affects UE for web
// applications, but without it there isn't a really good way to prevent chrome
// spoofing, see bug 337344
pref("dom.disable_window_open_feature.location",  true);
// prevent JS from setting status messages
pref("dom.disable_window_status_change",          true);
// allow JS to move and resize existing windows
pref("dom.disable_window_move_resize",            false);
// prevent JS from monkeying with window focus, etc
pref("dom.disable_window_flip",                   true);

// Disable touch events on Desktop Firefox by default until they are properly
// supported (bug 736048)
pref("dom.w3c_touch_events.enabled",        0);

// popups.policy 1=allow,2=reject
pref("privacy.popups.policy",               1);
pref("privacy.popups.usecustom",            true);
pref("privacy.popups.showBrowserMessage",   true);

pref("privacy.item.cookies",                false);

pref("privacy.clearOnShutdown.history",     true);
pref("privacy.clearOnShutdown.formdata",    true);
pref("privacy.clearOnShutdown.downloads",   true);
pref("privacy.clearOnShutdown.cookies",     true);
pref("privacy.clearOnShutdown.cache",       true);
pref("privacy.clearOnShutdown.sessions",    true);
pref("privacy.clearOnShutdown.offlineApps", false);
pref("privacy.clearOnShutdown.siteSettings", false);
pref("privacy.clearOnShutdown.openWindows", false);

pref("privacy.cpd.history",                 true);
pref("privacy.cpd.formdata",                true);
pref("privacy.cpd.passwords",               false);
pref("privacy.cpd.downloads",               true);
pref("privacy.cpd.cookies",                 true);
pref("privacy.cpd.cache",                   true);
pref("privacy.cpd.sessions",                true);
pref("privacy.cpd.offlineApps",             false);
pref("privacy.cpd.siteSettings",            false);
pref("privacy.cpd.openWindows",             false);

// What default should we use for the time span in the sanitizer:
// 0 - Clear everything
// 1 - Last Hour
// 2 - Last 2 Hours
// 3 - Last 4 Hours
// 4 - Today
// 5 - Last 5 minutes
// 6 - Last 24 hours
pref("privacy.sanitize.timeSpan", 1);
pref("privacy.sanitize.sanitizeOnShutdown", false);

pref("privacy.sanitize.migrateFx3Prefs",    false);

pref("privacy.sanitize.migrateClearSavedPwdsOnExit", false);

pref("privacy.panicButton.enabled",         true);

pref("network.proxy.share_proxy_settings",  false); // use the same proxy settings for all protocols

// simple gestures support
pref("browser.gesture.swipe.left", "Browser:BackOrBackDuplicate");
pref("browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate");
pref("browser.gesture.swipe.up", "cmd_scrollTop");
pref("browser.gesture.swipe.down", "cmd_scrollBottom");
#ifdef XP_MACOSX
pref("browser.gesture.pinch.latched", true);
pref("browser.gesture.pinch.threshold", 150);
#else
pref("browser.gesture.pinch.latched", false);
pref("browser.gesture.pinch.threshold", 25);
#endif
#ifdef XP_WIN
// Enabled for touch input display zoom.
pref("browser.gesture.pinch.out", "cmd_fullZoomEnlarge");
pref("browser.gesture.pinch.in", "cmd_fullZoomReduce");
pref("browser.gesture.pinch.out.shift", "cmd_fullZoomReset");
pref("browser.gesture.pinch.in.shift", "cmd_fullZoomReset");
#else
// Disabled by default due to issues with track pad input.
pref("browser.gesture.pinch.out", "");
pref("browser.gesture.pinch.in", "");
pref("browser.gesture.pinch.out.shift", "");
pref("browser.gesture.pinch.in.shift", "");
#endif
pref("browser.gesture.twist.latched", false);
pref("browser.gesture.twist.threshold", 0);
pref("browser.gesture.twist.right", "cmd_gestureRotateRight");
pref("browser.gesture.twist.left", "cmd_gestureRotateLeft");
pref("browser.gesture.twist.end", "cmd_gestureRotateEnd");
pref("browser.gesture.tap", "cmd_fullZoomReset");

pref("browser.snapshots.limit", 0);

// 0: Nothing happens
// 1: Scrolling contents
// 2: Go back or go forward, in your history
// 3: Zoom in or out.
#ifdef XP_MACOSX
// On OS X, if the wheel has one axis only, shift+wheel comes through as a
// horizontal scroll event. Thus, we can't assign anything other than normal
// scrolling to shift+wheel.
pref("mousewheel.with_alt.action", 2);
pref("mousewheel.with_shift.action", 1);
// On MacOS X, control+wheel is typically handled by system and we don't
// receive the event.  So, command key which is the main modifier key for
// acceleration is the best modifier for zoom-in/out.  However, we should keep
// the control key setting for backward compatibility.
pref("mousewheel.with_meta.action", 3); // command key on Mac
// Disable control-/meta-modified horizontal mousewheel events, since
// those are used on Mac as part of modified swipe gestures (e.g.
// Left swipe+Cmd = go back in a new tab).
pref("mousewheel.with_control.action.override_x", 0);
pref("mousewheel.with_meta.action.override_x", 0);
#else
pref("mousewheel.with_alt.action", 1);
pref("mousewheel.with_shift.action", 2);
pref("mousewheel.with_meta.action", 1); // win key on Win, Super/Hyper on Linux
#endif
pref("mousewheel.with_control.action",3);
pref("mousewheel.with_win.action", 1);

pref("browser.xul.error_pages.enabled", true);
pref("browser.xul.error_pages.expert_bad_cert", false);

// If true, network link events will change the value of navigator.onLine
pref("network.manage-offline-status", true);

// We want to make sure mail URLs are handled externally...
pref("network.protocol-handler.external.mailto", true); // for mail
pref("network.protocol-handler.external.news", true);   // for news
pref("network.protocol-handler.external.snews", true);  // for secure news
pref("network.protocol-handler.external.nntp", true);   // also news
#ifdef XP_WIN
pref("network.protocol-handler.external.ms-windows-store", true);
#endif

// ...without warning dialogs
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.news", false);
pref("network.protocol-handler.warn-external.snews", false);
pref("network.protocol-handler.warn-external.nntp", false);
#ifdef XP_WIN
pref("network.protocol-handler.warn-external.ms-windows-store", false);
#endif

// By default, all protocol handlers are exposed.  This means that
// the browser will respond to openURL commands for all URL types.
// It will also try to open link clicks inside the browser before
// failing over to the system handlers.
pref("network.protocol-handler.expose-all", true);
pref("network.protocol-handler.expose.mailto", false);
pref("network.protocol-handler.expose.news", false);
pref("network.protocol-handler.expose.snews", false);
pref("network.protocol-handler.expose.nntp", false);

pref("accessibility.typeaheadfind", false);
pref("accessibility.typeaheadfind.timeout", 5000);
pref("accessibility.typeaheadfind.linksonly", false);
pref("accessibility.typeaheadfind.flashBar", 1);

pref("plugins.update.url", "https://www.mozilla.org/%LOCALE%/plugincheck/?utm_source=firefox-browser&utm_medium=firefox-browser&utm_campaign=plugincheck-update");
pref("plugins.update.notifyUser", false);

pref("plugins.click_to_play", true);
pref("plugins.testmode", false);

pref("plugin.default.state", 1);

// Plugins bundled in XPIs are enabled by default.
pref("plugin.defaultXpi.state", 2);

// Flash is enabled by default, and Java is click-to-activate by default on
// all channels.
pref("plugin.state.flash", 2);
pref("plugin.state.java", 1);

// Whitelist Requests

// Unity player, bug 979849
#ifdef XP_WIN
pref("plugin.state.npunity3d", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.unity web player", 2);
#endif

// Cisco Jabber SDK, bug 980133
#ifdef XP_WIN
pref("plugin.state.npciscowebcommunicator", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.ciscowebcommunicator", 2);
#endif

// McAfee Security Scanner detection plugin, bug 980772
#ifdef XP_WIN
pref("plugin.state.npmcafeemss", 2);
#endif

// Cisco VGConnect for directv.com, bug 981403 & bug 1051772
#ifdef XP_WIN
pref("plugin.state.npplayerplugin", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.playerplugin", 2);
pref("plugin.state.playerplugin.dtv", 2);
pref("plugin.state.playerplugin.ciscodrm", 2);
pref("plugin.state.playerplugin.charter", 2);
#endif

// Cisco Jabber Client, bug 981905
#ifdef XP_WIN
pref("plugin.state.npchip", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.cisco jabber guest plug-in", 2);
#endif

// Estonian ID-card plugin, bug 982045
#ifdef XP_WIN
pref("plugin.state.npesteid-firefox-plugin", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.esteidfirefoxplugin", 2);
#endif
#ifdef UNIX_BUT_NOT_MAC
pref("plugin.state.npesteid-firefox-plugin", 2);
#endif

// coupons.com, bug 984441
#ifdef XP_WIN
pref("plugin.state.npmozcouponprinter", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.couponprinter-firefox_v", 2);
#endif

// Nexus Personal BankID, bug 987056
pref("plugin.state.npbispbrowser", 2);

// Gradecam, bug 988119
#ifdef XP_WIN
pref("plugin.state.npgcplugin", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.gcplugin", 2);
#endif

// Smart Card Plugin, bug 988781
#ifdef XP_WIN
pref("plugin.state.npwebcard", 2);
#endif

// Cisco WebEx, bug 989096
#ifdef XP_WIN
pref("plugin.state.npatgpc", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.webex", 2);
#endif
#ifdef UNIX_BUT_NOT_MAC
pref("plugin.state.npatgpc", 2);
#endif

// Skype, bug 990067
#ifdef XP_WIN
pref("plugin.state.npskypewebplugin", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.skypewebplugin", 2);
#endif

// Facebook video calling, bug 990068
#ifdef XP_WIN
pref("plugin.state.npfacebookvideocalling", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.facebookvideocalling", 2);
#endif

// MS Office Lync plugin, bug 990069
#ifdef XP_WIN
pref("plugin.state.npmeetingjoinpluginoc", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.lwaplugin", 2);
#endif

// VidyoWeb, bug 990286
#ifdef XP_WIN
pref("plugin.state.npvidyoweb", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.npvidyoweb", 2);
pref("plugin.state.vidyoweb", 2);
#endif

// McAfee Virtual Technician, bug 981503
#ifdef XP_WIN
pref("plugin.state.npmvtplugin", 2);
#endif

// Verimatrix ViewRightWeb, bug 989872
#ifdef XP_WIN
pref("plugin.state.npviewright", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.viewrightwebplayer", 2);
#endif

// McAfee SiteAdvisor Enterprise, bug 987057
#ifdef XP_WIN
pref("plugin.state.npmcffplg", 2);
#endif

// F5 Networks SSLVPN plugin, bug 985640
#ifdef XP_MACOSX
pref("plugin.state.f5 ssl vpn plugin", 2);
pref("plugin.state.f5 sam inspection host plugin", 2);
#endif

// Roblox Launcher Plugin, bug 1024073
#ifdef XP_WIN
pref("plugin.state.nprobloxproxy", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.nproblox", 2);
#endif

// Box Edit, bug 1029654
#ifdef XP_WIN
pref("plugin.state.npboxedit", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.box edit", 2);
#endif

// Nexus Personal, bug 1024965
#ifdef XP_WIN
pref("plugin.state.np_prsnl", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.personalplugin", 2);
#endif
#ifdef UNIX_BUT_NOT_MAC
pref("plugin.state.libplugins", 2);
#endif

// Novell iPrint Client, bug 1036693
#ifdef XP_WIN
pref("plugin.state.npnipp", 2);
pref("plugin.state.npnisp", 2);
#endif
#ifdef XP_MACOSX
pref("plugin.state.iprint", 2);
#endif

#ifdef XP_MACOSX
pref("browser.preferences.animateFadeIn", true);
#else
pref("browser.preferences.animateFadeIn", false);
#endif

#ifdef XP_WIN
pref("browser.preferences.instantApply", false);
#else
pref("browser.preferences.instantApply", true);
#endif

pref("browser.download.show_plugins_in_list", true);
pref("browser.download.hide_plugins_without_extensions", true);

// Backspace and Shift+Backspace behavior
// 0 goes Back/Forward
// 1 act like PgUp/PgDown
// 2 and other values, nothing
#ifdef UNIX_BUT_NOT_MAC
pref("browser.backspace_action", 2);
#else
pref("browser.backspace_action", 0);
#endif

// this will automatically enable inline spellchecking (if it is available) for
// editable elements in HTML
// 0 = spellcheck nothing
// 1 = check multi-line controls [default]
// 2 = check multi/single line controls
pref("layout.spellcheckDefault", 1);

pref("browser.send_pings", false);

/* initial web feed readers list */
pref("browser.contentHandlers.types.0.title", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.0.uri", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.0.type", "application/vnd.mozilla.maybe.feed");
pref("browser.contentHandlers.types.1.title", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.1.uri", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.1.type", "application/vnd.mozilla.maybe.feed");
pref("browser.contentHandlers.types.2.title", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.2.uri", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.2.type", "application/vnd.mozilla.maybe.feed");
pref("browser.contentHandlers.types.3.title", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.3.uri", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.3.type", "application/vnd.mozilla.maybe.feed");
pref("browser.contentHandlers.types.4.title", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.4.uri", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.4.type", "application/vnd.mozilla.maybe.feed");
pref("browser.contentHandlers.types.5.title", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.5.uri", "chrome://browser-region/locale/region.properties");
pref("browser.contentHandlers.types.5.type", "application/vnd.mozilla.maybe.feed");

pref("browser.feeds.handler", "ask");
pref("browser.videoFeeds.handler", "ask");
pref("browser.audioFeeds.handler", "ask");

// At startup, if the handler service notices that the version number in the
// region.properties file is newer than the version number in the handler
// service datastore, it will add any new handlers it finds in the prefs (as
// seeded by this file) to its datastore.
pref("gecko.handlerService.defaultHandlersVersion", "chrome://browser-region/locale/region.properties");

// The default set of web-based protocol handlers shown in the application
// selection dialog for webcal: ; I've arbitrarily picked 4 default handlers
// per protocol, but if some locale wants more than that (or defaults for some
// protocol not currently listed here), we should go ahead and add those.

// webcal
pref("gecko.handlerService.schemes.webcal.0.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.webcal.0.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.webcal.1.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.webcal.1.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.webcal.2.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.webcal.2.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.webcal.3.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.webcal.3.uriTemplate", "chrome://browser-region/locale/region.properties");

// mailto
pref("gecko.handlerService.schemes.mailto.0.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.mailto.0.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.mailto.1.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.mailto.1.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.mailto.2.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.mailto.2.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.mailto.3.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.mailto.3.uriTemplate", "chrome://browser-region/locale/region.properties");

// irc
pref("gecko.handlerService.schemes.irc.0.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.irc.0.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.irc.1.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.irc.1.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.irc.2.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.irc.2.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.irc.3.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.irc.3.uriTemplate", "chrome://browser-region/locale/region.properties");

// ircs
pref("gecko.handlerService.schemes.ircs.0.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.ircs.0.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.ircs.1.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.ircs.1.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.ircs.2.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.ircs.2.uriTemplate", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.ircs.3.name", "chrome://browser-region/locale/region.properties");
pref("gecko.handlerService.schemes.ircs.3.uriTemplate", "chrome://browser-region/locale/region.properties");

// By default, we don't want protocol/content handlers to be registered from a different host, see bug 402287
pref("gecko.handlerService.allowRegisterFromDifferentHost", false);

#ifdef MOZ_SAFE_BROWSING
pref("browser.safebrowsing.enabled", true);
pref("browser.safebrowsing.malware.enabled", true);
pref("browser.safebrowsing.downloads.enabled", true);
pref("browser.safebrowsing.downloads.remote.enabled", true);
pref("browser.safebrowsing.downloads.remote.timeout_ms", 10000);
pref("browser.safebrowsing.debug", false);

pref("browser.safebrowsing.provider.google.lists", "goog-badbinurl-shavar,goog-downloadwhite-digest256,goog-phish-shavar,goog-malware-shavar,goog-unwanted-shavar");
pref("browser.safebrowsing.provider.google.updateURL", "https://safebrowsing.google.com/safebrowsing/downloads?client=SAFEBROWSING_ID&appver=%VERSION%&pver=2.2&key=%GOOGLE_API_KEY%");
pref("browser.safebrowsing.provider.google.gethashURL", "https://safebrowsing.google.com/safebrowsing/gethash?client=SAFEBROWSING_ID&appver=%VERSION%&pver=2.2");
pref("browser.safebrowsing.provider.google.reportURL", "https://safebrowsing.google.com/safebrowsing/diagnostic?client=%NAME%&hl=%LOCALE%&site=");
pref("browser.safebrowsing.provider.google.appRepURL", "https://sb-ssl.google.com/safebrowsing/clientreport/download?key=%GOOGLE_API_KEY%");

pref("browser.safebrowsing.reportPhishMistakeURL", "https://%LOCALE%.phish-error.mozilla.com/?hl=%LOCALE%&url=");
pref("browser.safebrowsing.reportPhishURL", "https://%LOCALE%.phish-report.mozilla.com/?hl=%LOCALE%&url=");
pref("browser.safebrowsing.reportMalwareMistakeURL", "https://%LOCALE%.malware-error.mozilla.com/?hl=%LOCALE%&url=");
#ifdef MOZILLA_OFFICIAL
// Normally the "client ID" sent in updates is appinfo.name, but for
// official Firefox releases from Mozilla we use a special identifier.
pref("browser.safebrowsing.id", "navclient-auto-ffox");
#endif

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
// Tables for application reputation.
pref("urlclassifier.downloadBlockTable", "goog-badbinurl-shavar");
#ifdef XP_WIN
// Only download the whitelist on Windows, since the whitelist is
// only useful for suppressing remote lookups for signed binaries which we can
// only verify on Windows (Bug 974579). Other platforms always do remote lookups.
pref("urlclassifier.downloadAllowTable", "goog-downloadwhite-digest256");
#endif
#endif

pref("browser.geolocation.warning.infoURL", "https://www.mozilla.org/%LOCALE%/firefox/geolocation/");

pref("browser.EULA.version", 3);
pref("browser.rights.version", 3);
pref("browser.rights.3.shown", false);

#ifdef DEBUG
// Don't show the about:rights notification in debug builds.
pref("browser.rights.override", true);
#endif

pref("browser.selfsupport.url", "https://self-repair.mozilla.org/%LOCALE%/repair");

pref("browser.sessionstore.resume_from_crash", true);
pref("browser.sessionstore.resume_session_once", false);

// minimal interval between two save operations in milliseconds
pref("browser.sessionstore.interval", 15000);
// on which sites to save text data, POSTDATA and cookies
// 0 = everywhere, 1 = unencrypted sites, 2 = nowhere
pref("browser.sessionstore.privacy_level", 0);
// the same as browser.sessionstore.privacy_level, but for saving deferred session data
pref("browser.sessionstore.privacy_level_deferred", 1);
// how many tabs can be reopened (per window)
pref("browser.sessionstore.max_tabs_undo", 10);
// how many windows can be reopened (per session) - on non-OS X platforms this
// pref may be ignored when dealing with pop-up windows to ensure proper startup
pref("browser.sessionstore.max_windows_undo", 3);
// number of crashes that can occur before the about:sessionrestore page is displayed
// (this pref has no effect if more than 6 hours have passed since the last crash)
pref("browser.sessionstore.max_resumed_crashes", 1);
// number of back button session history entries to restore (-1 = all of them)
pref("browser.sessionstore.max_serialize_back", 10);
// number of forward button session history entries to restore (-1 = all of them)
pref("browser.sessionstore.max_serialize_forward", -1);
// restore_on_demand overrides MAX_CONCURRENT_TAB_RESTORES (sessionstore constant)
// and restore_hidden_tabs. When true, tabs will not be restored until they are
// focused (also applies to tabs that aren't visible). When false, the values
// for MAX_CONCURRENT_TAB_RESTORES and restore_hidden_tabs are respected.
// Selected tabs are always restored regardless of this pref.
pref("browser.sessionstore.restore_on_demand", true);
// Whether to automatically restore hidden tabs (i.e., tabs in other tab groups) or not
pref("browser.sessionstore.restore_hidden_tabs", false);
// If restore_on_demand is set, pinned tabs are restored on startup by default.
// When set to true, this pref overrides that behavior, and pinned tabs will only
// be restored when they are focused.
pref("browser.sessionstore.restore_pinned_tabs_on_demand", false);
// The version at which we performed the latest upgrade backup
pref("browser.sessionstore.upgradeBackup.latestBuildID", "");
// How many upgrade backups should be kept
pref("browser.sessionstore.upgradeBackup.maxUpgradeBackups", 3);
// End-users should not run sessionstore in debug mode
pref("browser.sessionstore.debug", false);
// Forget closed windows/tabs after two weeks
pref("browser.sessionstore.cleanup.forget_closed_after", 1209600000);

// allow META refresh by default
pref("accessibility.blockautorefresh", false);

// Whether history is enabled or not.
pref("places.history.enabled", true);

// the (maximum) number of the recent visits to sample
// when calculating frecency
pref("places.frecency.numVisits", 10);

// buckets (in days) for frecency calculation
pref("places.frecency.firstBucketCutoff", 4);
pref("places.frecency.secondBucketCutoff", 14);
pref("places.frecency.thirdBucketCutoff", 31);
pref("places.frecency.fourthBucketCutoff", 90);

// weights for buckets for frecency calculations
pref("places.frecency.firstBucketWeight", 100);
pref("places.frecency.secondBucketWeight", 70);
pref("places.frecency.thirdBucketWeight", 50);
pref("places.frecency.fourthBucketWeight", 30);
pref("places.frecency.defaultBucketWeight", 10);

// bonus (in percent) for visit transition types for frecency calculations
pref("places.frecency.embedVisitBonus", 0);
pref("places.frecency.framedLinkVisitBonus", 0);
pref("places.frecency.linkVisitBonus", 100);
pref("places.frecency.typedVisitBonus", 2000);
pref("places.frecency.bookmarkVisitBonus", 75);
pref("places.frecency.downloadVisitBonus", 0);
pref("places.frecency.permRedirectVisitBonus", 0);
pref("places.frecency.tempRedirectVisitBonus", 0);
pref("places.frecency.defaultVisitBonus", 0);

// bonus (in percent) for place types for frecency calculations
pref("places.frecency.unvisitedBookmarkBonus", 140);
pref("places.frecency.unvisitedTypedBonus", 200);

// Controls behavior of the "Add Exception" dialog launched from SSL error pages
// 0 - don't pre-populate anything
// 1 - pre-populate site URL, but don't fetch certificate
// 2 - pre-populate site URL and pre-fetch certificate
pref("browser.ssl_override_behavior", 2);

// True if the user should be prompted when a web application supports
// offline apps.
pref("browser.offline-apps.notify", true);

// if true, use full page zoom instead of text zoom
pref("browser.zoom.full", true);

// Whether or not to save and restore zoom levels on a per-site basis.
pref("browser.zoom.siteSpecific", true);

// Whether or not to update background tabs to the current zoom level.
pref("browser.zoom.updateBackgroundTabs", true);

// The breakpad report server to link to in about:crashes
pref("breakpad.reportURL", "https://crash-stats.mozilla.com/report/index/");

// URL for "Learn More" for Crash Reporter
pref("toolkit.crashreporter.infoURL",
     "https://www.mozilla.org/legal/privacy/firefox.html#crash-reporter");

// base URL for web-based support pages
pref("app.support.baseURL", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/");

// base url for web-based feedback pages
#ifdef MOZ_DEV_EDITION
pref("app.feedback.baseURL", "https://input.mozilla.org/%LOCALE%/feedback/firefoxdev/%VERSION%/");
#else
pref("app.feedback.baseURL", "https://input.mozilla.org/%LOCALE%/feedback/%APP%/%VERSION%/");
#endif


// Name of alternate about: page for certificate errors (when undefined, defaults to about:neterror)
pref("security.alternate_certificate_error_page", "certerror");

// Whether to start the private browsing mode at application startup
pref("browser.privatebrowsing.autostart", false);

// Don't try to alter this pref, it'll be reset the next time you use the
// bookmarking dialog
pref("browser.bookmarks.editDialog.firstEditField", "namePicker");

pref("dom.ipc.plugins.flash.disable-protected-mode", false);

// Feature-disable the protected-mode auto-flip
pref("browser.flash-protected-mode-flip.enable", false);

// Whether we've already flipped protected mode automatically
pref("browser.flash-protected-mode-flip.done", false);

pref("dom.ipc.shims.enabledWarnings", false);

// Start the browser in e10s mode
pref("browser.tabs.remote.autostart", false);
pref("browser.tabs.remote.desktopbehavior", true);

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
// When this pref is true the Windows process sandbox will set up dummy
// interceptions and log to the browser console when calls fail in the sandboxed
// process and also if they are subsequently allowed by the broker process.
// This will require a restart.
pref("security.sandbox.windows.log", false);

// Controls whether and how the Windows NPAPI plugin process is sandboxed.
// To get a different setting for a particular plugin replace "default", with
// the plugin's nice file name, see: nsPluginTag::GetNiceFileName.
// On windows these levels are:
// 0 - no sandbox
// 1 - sandbox with USER_NON_ADMIN access token level
// 2 - a more strict sandbox, which might cause functionality issues. This now
//     includes running at low integrity.
// 3 - the strongest settings we seem to be able to use without breaking
//     everything, but will probably cause some functionality restrictions
pref("dom.ipc.plugins.sandbox-level.default", 0);
#if defined(_AMD64_)
// The lines in PluginModuleParent.cpp should be changed in line with this.
pref("dom.ipc.plugins.sandbox-level.flash", 2);
#else
pref("dom.ipc.plugins.sandbox-level.flash", 0);
#endif

#if defined(MOZ_CONTENT_SANDBOX)
// This controls the strength of the Windows content process sandbox for testing
// purposes. This will require a restart.
// On windows these levels are:
// See - security/sandbox/win/src/sandboxbroker/sandboxBroker.cpp
// SetSecurityLevelForContentProcess() for what the different settings mean.
#if defined(NIGHTLY_BUILD)
pref("security.sandbox.content.level", 2);
#else
pref("security.sandbox.content.level", 0);
#endif

// ID (a UUID when set by gecko) that is used as a per profile suffix to a low
// integrity temp directory.
pref("security.sandbox.content.tempDirSuffix", "");

#if defined(MOZ_STACKWALKING)
// This controls the depth of stack trace that is logged when Windows sandbox
// logging is turned on.  This is only currently available for the content
// process because the only other sandbox (for GMP) has too strict a policy to
// allow stack tracing.  This does not require a restart to take effect.
pref("security.sandbox.windows.log.stackTraceDepth", 0);
#endif
#endif
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX) && defined(MOZ_CONTENT_SANDBOX)
// This pref is discussed in bug 1083344, the naming is inspired from its Windows
// counterpart, but on Mac it's an integer which means:
// 0 -> "no sandbox"
// 1 -> "an imperfect sandbox designed to allow firefox to run reasonably well"
// 2 -> "an ideal sandbox which may break many things"
// This setting is read when the content process is started. On Mac the content
// process is killed when all windows are closed, so a change will take effect
// when the 1st window is opened.
pref("security.sandbox.content.level", 1);
#endif

// This pref governs whether we attempt to work around problems caused by
// plugins using OS calls to manipulate the cursor while running out-of-
// process.  These workarounds all involve intercepting (hooking) certain
// OS calls in the plugin process, then arranging to make certain OS calls
// in the browser process.  Eventually plugins will be required to use the
// NPAPI to manipulate the cursor, and these workarounds will be removed.
// See bug 621117.
#ifdef XP_MACOSX
pref("dom.ipc.plugins.nativeCursorSupport", true);
#endif

#ifdef XP_WIN
pref("browser.taskbar.previews.enable", false);
pref("browser.taskbar.previews.max", 20);
pref("browser.taskbar.previews.cachetime", 5);
pref("browser.taskbar.lists.enabled", true);
pref("browser.taskbar.lists.frequent.enabled", true);
pref("browser.taskbar.lists.recent.enabled", false);
pref("browser.taskbar.lists.maxListItemCount", 7);
pref("browser.taskbar.lists.tasks.enabled", true);
pref("browser.taskbar.lists.refreshInSeconds", 120);
#endif

#ifdef MOZ_SERVICES_SYNC
// The sync engines to use.
pref("services.sync.registerEngines", "Bookmarks,Form,History,Password,Prefs,Tab,Addons");
// Preferences to be synced by default
pref("services.sync.prefs.sync.accessibility.blockautorefresh", true);
pref("services.sync.prefs.sync.accessibility.browsewithcaret", true);
pref("services.sync.prefs.sync.accessibility.typeaheadfind", true);
pref("services.sync.prefs.sync.accessibility.typeaheadfind.linksonly", true);
pref("services.sync.prefs.sync.addons.ignoreUserEnabledChanges", true);
// The addons prefs related to repository verification are intentionally
// not synced for security reasons. If a system is compromised, a user
// could weaken the pref locally, install an add-on from an untrusted
// source, and this would propagate automatically to other,
// uncompromised Sync-connected devices.
pref("services.sync.prefs.sync.app.update.mode", true);
pref("services.sync.prefs.sync.browser.formfill.enable", true);
pref("services.sync.prefs.sync.browser.link.open_newwindow", true);
pref("services.sync.prefs.sync.browser.newtabpage.enabled", true);
pref("services.sync.prefs.sync.browser.newtabpage.enhanced", true);
pref("services.sync.prefs.sync.browser.newtabpage.pinned", true);
pref("services.sync.prefs.sync.browser.offline-apps.notify", true);
pref("services.sync.prefs.sync.browser.safebrowsing.enabled", true);
pref("services.sync.prefs.sync.browser.safebrowsing.malware.enabled", true);
pref("services.sync.prefs.sync.browser.search.update", true);
pref("services.sync.prefs.sync.browser.sessionstore.restore_on_demand", true);
pref("services.sync.prefs.sync.browser.startup.homepage", true);
pref("services.sync.prefs.sync.browser.startup.page", true);
pref("services.sync.prefs.sync.browser.tabs.loadInBackground", true);
pref("services.sync.prefs.sync.browser.tabs.warnOnClose", true);
pref("services.sync.prefs.sync.browser.tabs.warnOnOpen", true);
pref("services.sync.prefs.sync.browser.urlbar.autocomplete.enabled", true);
pref("services.sync.prefs.sync.browser.urlbar.maxRichResults", true);
pref("services.sync.prefs.sync.dom.disable_open_during_load", true);
pref("services.sync.prefs.sync.dom.disable_window_flip", true);
pref("services.sync.prefs.sync.dom.disable_window_move_resize", true);
pref("services.sync.prefs.sync.dom.event.contextmenu.enabled", true);
pref("services.sync.prefs.sync.extensions.personas.current", true);
pref("services.sync.prefs.sync.extensions.update.enabled", true);
pref("services.sync.prefs.sync.intl.accept_languages", true);
pref("services.sync.prefs.sync.javascript.enabled", true);
pref("services.sync.prefs.sync.layout.spellcheckDefault", true);
pref("services.sync.prefs.sync.lightweightThemes.selectedThemeID", true);
pref("services.sync.prefs.sync.lightweightThemes.usedThemes", true);
pref("services.sync.prefs.sync.network.cookie.cookieBehavior", true);
pref("services.sync.prefs.sync.network.cookie.lifetimePolicy", true);
pref("services.sync.prefs.sync.permissions.default.image", true);
pref("services.sync.prefs.sync.pref.advanced.images.disable_button.view_image", true);
pref("services.sync.prefs.sync.pref.advanced.javascript.disable_button.advanced", true);
pref("services.sync.prefs.sync.pref.downloads.disable_button.edit_actions", true);
pref("services.sync.prefs.sync.pref.privacy.disable_button.cookie_exceptions", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.cache", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.cookies", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.downloads", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.formdata", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.history", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.offlineApps", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.sessions", true);
pref("services.sync.prefs.sync.privacy.clearOnShutdown.siteSettings", true);
pref("services.sync.prefs.sync.privacy.donottrackheader.enabled", true);
pref("services.sync.prefs.sync.privacy.sanitize.sanitizeOnShutdown", true);
pref("services.sync.prefs.sync.privacy.trackingprotection.enabled", true);
pref("services.sync.prefs.sync.privacy.trackingprotection.pbmode.enabled", true);
pref("services.sync.prefs.sync.security.OCSP.enabled", true);
pref("services.sync.prefs.sync.security.OCSP.require", true);
pref("services.sync.prefs.sync.security.default_personal_cert", true);
pref("services.sync.prefs.sync.security.tls.version.min", true);
pref("services.sync.prefs.sync.security.tls.version.max", true);
pref("services.sync.prefs.sync.signon.rememberSignons", true);
pref("services.sync.prefs.sync.spellchecker.dictionary", true);
pref("services.sync.prefs.sync.xpinstall.whitelist.required", true);
#endif

// Developer edition preferences
#ifdef MOZ_DEV_EDITION
sticky_pref("lightweightThemes.selectedThemeID", "firefox-devedition@mozilla.org");
#else
sticky_pref("lightweightThemes.selectedThemeID", "");
#endif

// Whether the character encoding menu is under the main Firefox button. This
// preference is a string so that localizers can alter it.
pref("browser.menu.showCharacterEncoding", "chrome://browser/locale/browser.properties");

// Allow using tab-modal prompts when possible.
pref("prompts.tab_modal.enabled", true);
// Whether the Panorama should animate going in/out of tabs
pref("browser.panorama.animate_zoom", true);

// Activates preloading of the new tab url.
pref("browser.newtab.preload", true);

// Remembers if the about:newtab intro has been shown
pref("browser.newtabpage.introShown", false);

// Toggles the content of 'about:newtab'. Shows the grid when enabled.
pref("browser.newtabpage.enabled", true);

// Toggles the enhanced content of 'about:newtab'. Shows sponsored tiles.
sticky_pref("browser.newtabpage.enhanced", true);

// number of rows of newtab grid
pref("browser.newtabpage.rows", 3);

// number of columns of newtab grid
pref("browser.newtabpage.columns", 5);

// directory tiles download URL
pref("browser.newtabpage.directory.source", "https://tiles.services.mozilla.com/v3/links/fetch/%LOCALE%/%CHANNEL%");

// endpoint to send newtab click and view pings
pref("browser.newtabpage.directory.ping", "https://tiles.services.mozilla.com/v3/links/");

// activates the remote-hosted newtab page
pref("browser.newtabpage.remote", false);

// Enable the DOM fullscreen API.
pref("full-screen-api.enabled", true);

// Startup Crash Tracking
// number of startup crashes that can occur before starting into safe mode automatically
// (this pref has no effect if more than 6 hours have passed since the last crash)
pref("toolkit.startup.max_resumed_crashes", 3);

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

// Shumway is only bundled in Nightly.
#ifdef NIGHTLY_BUILD
pref("shumway.disabled", true);
#endif

// The maximum amount of decoded image data we'll willingly keep around (we
// might keep around more than this, but we'll try to get down to this value).
// (This is intentionally on the high side; see bug 746055.)
pref("image.mem.max_decoded_image_kb", 256000);

pref("loop.enabled", true);
pref("loop.textChat.enabled", true);
pref("loop.server", "https://loop.services.mozilla.com/v0");
pref("loop.linkClicker.url", "https://hello.firefox.com/");
pref("loop.gettingStarted.seen", false);
pref("loop.gettingStarted.url", "https://www.mozilla.org/%LOCALE%/firefox/%VERSION%/hello/start/");
pref("loop.gettingStarted.resumeOnFirstJoin", false);
pref("loop.learnMoreUrl", "https://www.firefox.com/hello/");
pref("loop.legal.ToS_url", "https://www.mozilla.org/about/legal/terms/firefox-hello/");
pref("loop.legal.privacy_url", "https://www.mozilla.org/privacy/firefox-hello/");
pref("loop.do_not_disturb", false);
pref("loop.ringtone", "chrome://browser/content/loop/shared/sounds/ringtone.ogg");
pref("loop.retry_delay.start", 60000);
pref("loop.retry_delay.limit", 300000);
pref("loop.ping.interval", 1800000);
pref("loop.ping.timeout", 10000);
pref("loop.feedback.baseUrl", "https://input.mozilla.org/api/v1/feedback");
pref("loop.feedback.product", "Loop");
pref("loop.debug.loglevel", "Error");
pref("loop.debug.dispatcher", false);
pref("loop.debug.sdk", false);
pref("loop.debug.twoWayMediaTelemetry", false);
pref("loop.feedback.dateLastSeenSec", 0);
pref("loop.feedback.periodSec", 15770000); // 6 months.
pref("loop.feedback.formURL", "https://www.mozilla.org/firefox/hello/npssurvey/");
#ifdef DEBUG
pref("loop.CSP", "default-src 'self' about: file: chrome: http://localhost:*; img-src * data:; font-src 'none'; connect-src wss://*.tokbox.com https://*.opentok.com https://*.tokbox.com wss://*.mozilla.com https://*.mozilla.org wss://*.mozaws.net http://localhost:* ws://localhost:*; media-src blob:");
#else
pref("loop.CSP", "default-src 'self' about: file: chrome:; img-src * data:; font-src 'none'; connect-src wss://*.tokbox.com https://*.opentok.com https://*.tokbox.com wss://*.mozilla.com https://*.mozilla.org wss://*.mozaws.net; media-src blob:");
#endif
pref("loop.fxa_oauth.tokendata", "");
pref("loop.fxa_oauth.profile", "");
pref("loop.support_url", "https://support.mozilla.org/kb/group-conversations-firefox-hello-webrtc");
pref("loop.browserSharing.showInfoBar", true);

pref("social.sidebar.unload_timeout_ms", 10000);

// Activation from inside of share panel is possible if activationPanelEnabled
// is true. Pref'd off for release while usage testing is done through beta.
pref("social.share.activationPanelEnabled", true);
pref("social.shareDirectory", "https://activations.cdn.mozilla.net/sharePanel.html");

pref("dom.identity.enabled", false);

// Block insecure active content on https pages
pref("security.mixed_content.block_active_content", true);

// Show degraded UI for http pages with password fields
#ifdef NIGHTLY_BUILD
pref("security.insecure_password.ui.enabled", true);
#else
pref("security.insecure_password.ui.enabled", false);
#endif

// 1 = allow MITM for certificate pinning checks.
pref("security.cert_pinning.enforcement_level", 1);

// 2 = allow SHA-1 only before 2016-01-01
pref("security.pki.sha1_enforcement_level", 2);

// Required blocklist freshness for OneCRL OCSP bypass
// (default is 1.25x extensions.blocklist.interval, or 30 hours)
pref("security.onecrl.maximum_staleness_in_seconds", 108000);

// Override the Gecko-default value of false for Firefox.
pref("plain_text.wrap_long_lines", true);

// If this turns true, Moz*Gesture events are not called stopPropagation()
// before content.
pref("dom.debug.propagate_gesture_events_through_content", false);

// The request URL of the GeoLocation backend.
#ifdef RELEASE_BUILD
pref("geo.wifi.uri", "https://www.googleapis.com/geolocation/v1/geolocate?key=%GOOGLE_API_KEY%");
#else
pref("geo.wifi.uri", "https://location.services.mozilla.com/v1/geolocate?key=%MOZILLA_API_KEY%");
#endif

#ifdef XP_MACOSX
#ifdef RELEASE_BUILD
pref("geo.provider.use_corelocation", false);
#else
pref("geo.provider.use_corelocation", true);
#endif
#endif

#ifdef XP_WIN
pref("geo.provider.ms-windows-location", false);
#endif

// Necko IPC security checks only needed for app isolation for cookies/cache/etc:
// currently irrelevant for desktop e10s
pref("network.disable.ipc.security", true);

// CustomizableUI debug logging.
pref("browser.uiCustomization.debug", false);

// CustomizableUI state of the browser's user interface
pref("browser.uiCustomization.state", "");

// The remote content URL shown for FxA signup. Must use HTTPS.
pref("identity.fxaccounts.remote.signup.uri", "https://accounts.firefox.com/signup?service=sync&context=fx_desktop_v1");

// The URL where remote content that forces re-authentication for Firefox Accounts
// should be fetched.  Must use HTTPS.
pref("identity.fxaccounts.remote.force_auth.uri", "https://accounts.firefox.com/force_auth?service=sync&context=fx_desktop_v1");

// The remote content URL shown for signin in. Must use HTTPS.
pref("identity.fxaccounts.remote.signin.uri", "https://accounts.firefox.com/signin?service=sync&context=fx_desktop_v1");

// The remote content URL where FxAccountsWebChannel messages originate.
pref("identity.fxaccounts.remote.webchannel.uri", "https://accounts.firefox.com/");

// The URL we take the user to when they opt to "manage" their Firefox Account.
// Note that this will always need to be in the same TLD as the
// "identity.fxaccounts.remote.signup.uri" pref.
pref("identity.fxaccounts.settings.uri", "https://accounts.firefox.com/settings");

// The remote URL of the FxA Profile Server
pref("identity.fxaccounts.remote.profile.uri", "https://profile.accounts.firefox.com/v1");

// The remote URL of the FxA OAuth Server
pref("identity.fxaccounts.remote.oauth.uri", "https://oauth.accounts.firefox.com/v1");

// Whether we display profile images in the UI or not.
pref("identity.fxaccounts.profile_image.enabled", true);

// Token server used by the FxA Sync identity.
pref("identity.sync.tokenserver.uri", "https://token.services.mozilla.com/1.0/sync/1.5");

// Migrate any existing Firefox Account data from the default profile to the
// Developer Edition profile.
#ifdef MOZ_DEV_EDITION
pref("identity.fxaccounts.migrateToDevEdition", true);
#else
pref("identity.fxaccounts.migrateToDevEdition", false);
#endif

// On GTK, we now default to showing the menubar only when alt is pressed:
#ifdef MOZ_WIDGET_GTK
pref("ui.key.menuAccessKeyFocuses", true);
#endif

// Encrypted media extensions.
pref("media.eme.enabled", true);
pref("media.eme.apiVisible", true);

// Decode using Gecko Media Plugins in <video>, if a system decoder is not
// availble and the preferred GMP is available.
pref("media.gmp.decoder.enabled", true);

// If decoding-via-GMP is turned on for <video>, use Adobe's GMP for decoding,
// if it's available. Note: We won't fallback to another GMP if Adobe's is not
// installed.
pref("media.gmp.decoder.aac", 2);
pref("media.gmp.decoder.h264", 2);

// Whether we should run a test-pattern through EME GMPs before assuming they'll
// decode H.264.
pref("media.gmp.trial-create.enabled", true);

#ifdef MOZ_ADOBE_EME
pref("browser.eme.ui.enabled", true);
pref("media.gmp-eme-adobe.enabled", true);
#endif

// Play with different values of the decay time and get telemetry,
// 0 means to randomize (and persist) the experiment value in users' profiles,
// -1 means no experiment is run and we use the preferred value for frecency (6h)
pref("browser.cache.frecency_experiment", 0);

pref("browser.translation.detectLanguage", false);
pref("browser.translation.neverForLanguages", "");
// Show the translation UI bits, like the info bar, notification icon and preferences.
pref("browser.translation.ui.show", false);
// Allows to define the translation engine. Bing is default, Yandex may optionally switched on.
pref("browser.translation.engine", "bing");

// Telemetry settings.
// Determines if Telemetry pings can be archived locally.
pref("toolkit.telemetry.archive.enabled", true);
// Whether we enable opt-out Telemetry for a sample of the release population.
pref("toolkit.telemetry.optoutSample", true);

// Telemetry experiments settings.
pref("experiments.enabled", true);
pref("experiments.manifest.fetchIntervalSeconds", 86400);
pref("experiments.manifest.uri", "https://telemetry-experiment.cdn.mozilla.net/manifest/v1/firefox/%VERSION%/%CHANNEL%");
// Whether experiments are supported by the current application profile.
pref("experiments.supported", true);

// Enable GMP support in the addon manager.
pref("media.gmp-provider.enabled", true);

pref("browser.apps.URL", "https://marketplace.firefox.com/discovery/");

#ifdef NIGHTLY_BUILD
pref("browser.polaris.enabled", false);
pref("privacy.trackingprotection.ui.enabled", false);
#endif
pref("privacy.trackingprotection.introCount", 0);
pref("privacy.trackingprotection.introURL", "https://www.mozilla.org/%LOCALE%/firefox/%VERSION%/tracking-protection/start/");

// Enable Contextual Identity Containers
pref("privacy.userContext.enabled", false);

#ifndef RELEASE_BUILD
// At the moment, autostart.2 is used, while autostart.1 is unused.
// We leave it here set to false to reset users' defaults and allow
// us to change everybody to true in the future, when desired.
pref("browser.tabs.remote.autostart.1", false);
pref("browser.tabs.remote.autostart.2", true);
#endif

// For the about:tabcrashed page
pref("browser.tabs.crashReporting.sendReport", true);
pref("browser.tabs.crashReporting.includeURL", false);
pref("browser.tabs.crashReporting.emailMe", false);
pref("browser.tabs.crashReporting.email", "");

#ifdef NIGHTLY_BUILD
#ifndef MOZ_MULET
pref("layers.async-pan-zoom.enabled", true);
#endif
#endif

#ifdef E10S_TESTING_ONLY
// Enable e10s add-on interposition by default.
pref("extensions.interposition.enabled", true);
pref("extensions.interposition.prefetching", true);
#endif

pref("browser.defaultbrowser.notificationbar", false);

// How often to check for CPOW timeouts. CPOWs are only timed out by
// the hang monitor.
pref("dom.ipc.cpow.timeout", 500);

// Enable e10s hang monitoring (slow script checking and plugin hang
// detection).
pref("dom.ipc.processHangMonitor", true);

#ifdef DEBUG
// Don't report hangs in DEBUG builds. They're too slow and often a
// debugger is attached.
pref("dom.ipc.reportProcessHangs", false);
#else
pref("dom.ipc.reportProcessHangs", true);
#endif

pref("browser.reader.detectedFirstArticle", false);
// Don't limit how many nodes we care about on desktop:
pref("reader.parse-node-limit", 0);

// On desktop, we want the URLs to be included here for ease of debugging,
// and because (normally) these errors are not persisted anywhere.
pref("reader.errors.includeURLs", true);

pref("browser.pocket.enabled", true);
pref("browser.pocket.api", "api.getpocket.com");
pref("browser.pocket.site", "getpocket.com");
pref("browser.pocket.oAuthConsumerKey", "40249-e88c401e1b1f2242d9e441c4");
pref("browser.pocket.useLocaleList", true);
pref("browser.pocket.enabledLocales", "cs de en-GB en-US en-ZA es-ES es-MX fr hu it ja ja-JP-mac ko nl pl pt-BR pt-PT ru zh-CN zh-TW");

pref("view_source.tab", true);

#ifndef RELEASE_BUILD
// Enable ServiceWorkers for Push API consumers.
// Interception is still disabled on beta and release.
pref("dom.serviceWorkers.enabled", true);

pref("dom.serviceWorkers.interception.enabled", true);

// Enable Push API.
pref("dom.push.enabled", true);

pref("dom.serviceWorkers.openWindow.enabled", true);
#endif

// These are the thumbnail width/height set in about:newtab.
// If you change this, ENSURE IT IS THE SAME SIZE SET
// by about:newtab. These values are in CSS pixels.
pref("toolkit.pageThumbs.minWidth", 280);
pref("toolkit.pageThumbs.minHeight", 190);

#ifdef NIGHTLY_BUILD
// Enable speech synthesis, only Nightly for now
pref("media.webspeech.synth.enabled", true);
#endif