// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Non-static prefs that are specific to desktop Firefox belong in this file
// (unless there is a compelling and documented reason for them to belong in
// another file).
//
// Please indent all prefs defined within #ifdef/#ifndef conditions. This
// improves readability, particular for conditional blocks that exceed a single
// screen.

#filter substitution

#ifdef XP_UNIX
  #ifndef XP_MACOSX
    #define UNIX_BUT_NOT_MAC
  #endif
#endif

pref("browser.hiddenWindowChromeURL", "chrome://browser/content/hiddenWindowMac.xhtml");

// Enables some extra Extension System Logging (can reduce performance)
pref("extensions.logging.enabled", false);

// Disables strict compatibility, making addons compatible-by-default.
pref("extensions.strictCompatibility", false);

// Temporary preference to forcibly make themes more safe with Australis even if
// extensions.checkCompatibility=false has been set.
pref("extensions.checkCompatibility.temporaryThemeOverride_minAppVersion", "29.0a1");

pref("extensions.webextPermissionPrompts", true);
pref("extensions.webextOptionalPermissionPrompts", true);

// Preferences for AMO integration
pref("extensions.getAddons.cache.enabled", true);
pref("extensions.getAddons.get.url", "https://services.addons.mozilla.org/api/v3/addons/search/?guid=%IDS%&lang=%LOCALE%");
pref("extensions.getAddons.search.browseURL", "https://addons.mozilla.org/%LOCALE%/firefox/search?q=%TERMS%&platform=%OS%&appver=%VERSION%");
pref("extensions.getAddons.link.url", "https://addons.mozilla.org/%LOCALE%/firefox/");
pref("extensions.getAddons.langpacks.url", "https://services.addons.mozilla.org/api/v3/addons/language-tools/?app=firefox&type=language&appversion=%VERSION%");
pref("extensions.getAddons.discovery.api_url", "https://services.addons.mozilla.org/api/v4/discovery/?lang=%LOCALE%&edition=%DISTRIBUTION%");

// The URL for the privacy policy related to recommended extensions.
pref("extensions.recommendations.privacyPolicyUrl", "https://www.mozilla.org/privacy/firefox/?utm_source=firefox-browser&utm_medium=firefox-browser&utm_content=privacy-policy-link#addons");
// The URL for Firefox Color, recommended on the theme page in about:addons.
pref("extensions.recommendations.themeRecommendationUrl", "https://color.firefox.com/?utm_source=firefox-browser&utm_medium=firefox-browser&utm_content=theme-footer-link");

pref("extensions.update.autoUpdateDefault", true);

// Check AUS for system add-on updates.
pref("extensions.systemAddon.update.url", "https://aus5.mozilla.org/update/3/SystemAddons/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml");
pref("extensions.systemAddon.update.enabled", true);

// Disable add-ons that are not installed by the user in all scopes by default.
// See the SCOPE constants in AddonManager.jsm for values to use here.
pref("extensions.autoDisableScopes", 15);
// Scopes to scan for changes at startup.
pref("extensions.startupScanScopes", 0);

pref("extensions.geckoProfiler.acceptedExtensionIds", "geckoprofiler@mozilla.com,quantum-foxfooding@mozilla.com,raptor@mozilla.org");


// Add-on content security policies.
pref("extensions.webextensions.base-content-security-policy", "script-src 'self' https://* moz-extension: blob: filesystem: 'unsafe-eval' 'unsafe-inline'; object-src 'self' https://* moz-extension: blob: filesystem:;");
pref("extensions.webextensions.default-content-security-policy", "script-src 'self'; object-src 'self';");

pref("extensions.webextensions.remote", true);
pref("extensions.webextensions.background-delayed-startup", true);

// Require signed add-ons by default
pref("extensions.langpacks.signatures.required", true);
pref("xpinstall.signatures.required", true);
pref("xpinstall.signatures.devInfoURL", "https://wiki.mozilla.org/Addons/Extension_Signing");

// Enable extensionStorage storage actor by default
pref("devtools.storage.extensionStorage.enabled", true);

// Dictionary download preference
pref("browser.dictionaries.download.url", "https://addons.mozilla.org/%LOCALE%/firefox/language-tools/");

// At startup, should we check to see if the installation
// date is older than some threshold
pref("app.update.checkInstallTime", true);

// The number of days a binary is permitted to be old without checking is defined in
// firefox-branding.js (app.update.checkInstallTime.days)

// The minimum delay in seconds for the timer to fire between the notification
// of each consumer of the timer manager.
// minimum=30 seconds, default=120 seconds, and maximum=300 seconds
pref("app.update.timerMinimumDelay", 120);

// The minimum delay in milliseconds for the first firing after startup of the timer
// to notify consumers of the timer manager.
// minimum=10 seconds, default=30 seconds, and maximum=120 seconds
pref("app.update.timerFirstInterval", 30000);

// App-specific update preferences

// The interval to check for updates (app.update.interval) is defined in
// firefox-branding.js

// Enables some extra Application Update Logging (can reduce performance)
pref("app.update.log", false);
// Causes Application Update Logging to be sent to a file in the profile
// directory. This preference is automatically disabled on application start to
// prevent it from being left on accidentally. Turning this pref on enables
// logging, even if app.update.log is false.
pref("app.update.log.file", false);

// The number of general background check failures to allow before notifying the
// user of the failure. User initiated update checks always notify the user of
// the failure.
pref("app.update.backgroundMaxErrors", 10);

// Ids of the links to the "What's new" update documentation
pref("app.update.link.updateAvailableWhatsNew", "update-available-whats-new");
pref("app.update.link.updateManualWhatsNew", "update-manual-whats-new");

// How many times we should let downloads fail before prompting the user to
// download a fresh installer.
pref("app.update.download.promptMaxAttempts", 2);

// How many times we should let an elevation prompt fail before prompting the user to
// download a fresh installer.
pref("app.update.elevation.promptMaxAttempts", 2);

// If set to true, the Update Service will automatically download updates if the
// user can apply updates. This pref is no longer used on Windows, except as the
// default value to migrate to the new location that this data is now stored
// (which is in a file in the update directory). Because of this, this pref
// should no longer be used directly. Instead, getAppUpdateAutoEnabled and
// getAppUpdateAutoEnabled from UpdateUtils.jsm should be used.
#ifndef XP_WIN
  pref("app.update.auto", true);
#endif

// If set to true, the Update Service will apply updates in the background
// when it finishes downloading them.
pref("app.update.staging.enabled", true);

// Update service URL:
// app.update.url was removed in Bug 1568994
// app.update.url.manual is in branding section
// app.update.url.details is in branding section

// app.update.badgeWaitTime is in branding section
// app.update.interval is in branding section
// app.update.promptWaitTime is in branding section

// Whether or not to attempt using the service for updates.
#ifdef MOZ_MAINTENANCE_SERVICE
  pref("app.update.service.enabled", true);
#endif

#ifdef MOZ_BITS_DOWNLOAD
  // If set to true, the Update Service will attempt to use Windows BITS to
  // download updates and will fallback to downloading internally if that fails.
  pref("app.update.BITS.enabled", true);
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

pref("lightweightThemes.getMoreURL", "https://addons.mozilla.org/%LOCALE%/firefox/themes");

#if defined(MOZ_WIDEVINE_EME)
  pref("browser.eme.ui.enabled", true);
#else
  pref("browser.eme.ui.enabled", false);
#endif

// UI tour experience.
pref("browser.uitour.enabled", true);
pref("browser.uitour.loglevel", "Error");
pref("browser.uitour.requireSecure", true);
pref("browser.uitour.themeOrigin", "https://addons.mozilla.org/%LOCALE%/firefox/themes/");
pref("browser.uitour.url", "https://www.mozilla.org/%LOCALE%/firefox/%VERSION%/tour/");
// How long to show a Hearbeat survey (two hours, in seconds)
pref("browser.uitour.surveyDuration", 7200);

pref("keyword.enabled", true);
pref("browser.fixup.domainwhitelist.localhost", true);

#ifdef UNIX_BUT_NOT_MAC
  pref("general.autoScroll", false);
#else
  pref("general.autoScroll", true);
#endif

// UI density of the browser chrome. This mostly affects toolbarbutton
// and urlbar spacing. The possible values are 0=normal, 1=compact, 2=touch.
pref("browser.uidensity", 0);
// Whether Firefox will automatically override the uidensity to "touch"
// while the user is in a touch environment (such as Windows tablet mode).
pref("browser.touchmode.auto", true);

// At startup, check if we're the default browser and prompt user if not.
pref("browser.shell.checkDefaultBrowser", true);
pref("browser.shell.shortcutFavicons",true);
pref("browser.shell.mostRecentDateSetAsDefault", "");
pref("browser.shell.skipDefaultBrowserCheckOnFirstRun", true);
pref("browser.shell.didSkipDefaultBrowserCheckOnFirstRun", false);
pref("browser.shell.defaultBrowserCheckCount", 0);
pref("browser.defaultbrowser.notificationbar", false);

// 0 = blank, 1 = home (browser.startup.homepage), 2 = last visited page, 3 = resume previous browser session
// The behavior of option 3 is detailed at: http://wiki.mozilla.org/Session_Restore
pref("browser.startup.page",                1);
pref("browser.startup.homepage",            "about:home");
pref("browser.startup.homepage.abouthome_cache.enabled", false);

// Whether we should skip the homepage when opening the first-run page
pref("browser.startup.firstrunSkipsHomepage", true);

// Show an about:blank window as early as possible for quick startup feedback.
// Held to nightly on Linux due to bug 1450626.
// Disabled on Mac because the bouncing dock icon already provides feedback.
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) && defined(NIGHTLY_BUILD)
  pref("browser.startup.blankWindow", true);
#else
  pref("browser.startup.blankWindow", false);
#endif

// Don't create the hidden window during startup on
// platforms that don't always need it (Win/Linux).
pref("toolkit.lazyHiddenWindow", true);

pref("browser.slowStartup.notificationDisabled", false);
pref("browser.slowStartup.timeThreshold", 20000);
pref("browser.slowStartup.maxSamples", 5);

pref("browser.chrome.site_icons", true);
// browser.warnOnQuit == false will override all other possible prompts when quitting or restarting
pref("browser.warnOnQuit", true);
pref("browser.fullscreen.autohide", true);
pref("browser.overlink-delay", 80);

// Whether using `ctrl` when hitting return/enter in the URL bar
// (or clicking 'go') should prefix 'www.' and suffix
// browser.fixup.alternate.suffix to the URL bar value prior to
// navigating.
pref("browser.urlbar.ctrlCanonizesURLs", true);

// Control autoFill behavior
pref("browser.urlbar.autoFill", true);
pref("browser.urlbar.speculativeConnect.enabled", true);

// Whether bookmarklets should be filtered out of Address Bar matches.
// This is enabled for security reasons, when true it is still possible to
// search for bookmarklets typing "javascript: " followed by the actual query.
pref("browser.urlbar.filter.javascript", true);

// the maximum number of results to show in autocomplete when doing richResults
pref("browser.urlbar.maxRichResults", 10);
// The amount of time (ms) to wait after the user has stopped typing
// before starting to perform autocomplete.  50 is the default set in
// autocomplete.xml.
pref("browser.urlbar.delay", 50);

// The maximum number of historical search results to show.
pref("browser.urlbar.maxHistoricalSearchSuggestions", 0);

// When true, URLs in the user's history that look like search result pages
// are styled to look like search engine results instead of the usual history
// results.
pref("browser.urlbar.restyleSearches", false);

// The default behavior for the urlbar can be configured to use any combination
// of the match filters with each additional filter adding more results (union).
pref("browser.urlbar.suggest.history",              true);
pref("browser.urlbar.suggest.bookmark",             true);
pref("browser.urlbar.suggest.openpage",             true);
pref("browser.urlbar.suggest.searches",             true);

// As a user privacy measure, don't fetch search suggestions if a pasted string
// is longer than this.
pref("browser.urlbar.maxCharsForSearchSuggestions", 100);

pref("browser.urlbar.formatting.enabled", true);
pref("browser.urlbar.trimURLs", true);

// If changed to true, copying the entire URL from the location bar will put the
// human readable (percent-decoded) URL on the clipboard.
pref("browser.urlbar.decodeURLsOnCopy", false);

// Whether or not to move tabs into the active window when using the "Switch to
// Tab" feature of the awesomebar.
pref("browser.urlbar.switchTabs.adoptIntoActiveWindow", false);

// Whether addresses and search results typed into the address bar
// should be opened in new tabs by default.
pref("browser.urlbar.openintab", false);

// This is disabled until Bug 1340663 figures out the remaining requirements.
pref("browser.urlbar.usepreloadedtopurls.enabled", false);
pref("browser.urlbar.usepreloadedtopurls.expire_days", 14);

// If true, we show actionable tips in the Urlbar when the user is searching
// for those actions.
pref("browser.urlbar.update1.interventions", true);

// If true, we show new users and those about to start an organic search a tip
// encouraging them to use the Urlbar.
pref("browser.urlbar.update1.searchTips", true);

pref("browser.urlbar.openViewOnFocus", true);

// Whether we expand the font size when when the urlbar is
// focused in design update 2.
pref("browser.urlbar.update2.expandTextOnFocus", false);

pref("browser.urlbar.eventTelemetry.enabled", false);

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

// This controls whether the button is automatically shown/hidden depending
// on whether there are downloads to show.
pref("browser.download.autohideButton", true);

#ifndef XP_MACOSX
  pref("browser.helperApps.deleteTempFileOnExit", true);
#endif

// search engines URL
pref("browser.search.searchEnginesURL",      "https://addons.mozilla.org/%LOCALE%/firefox/search-engines/");

// Market-specific search defaults
pref("browser.search.geoSpecificDefaults", true);
pref("browser.search.geoSpecificDefaults.url", "https://search.services.mozilla.com/1/%APP%/%VERSION%/%CHANNEL%/%LOCALE%/%REGION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%");

// search bar results always open in a new tab
pref("browser.search.openintab", false);

// context menu searches open in the foreground
pref("browser.search.context.loadInBackground", false);

// comma seperated list of of engines to hide in the search panel.
pref("browser.search.hiddenOneOffs", "");

// Mirrors whether the search-container widget is in the navigation toolbar.
pref("browser.search.widget.inNavBar", false);

// Enables display of the options for the user using a separate default search
// engine in private browsing mode.
#ifdef NIGHTLY_BUILD
  pref("browser.search.separatePrivateDefault.ui.enabled", true);
#endif
// The maximum amount of times the private default banner is shown.
pref("browser.search.separatePrivateDefault.ui.banner.max", 0);

#ifdef NIGHTLY_BUILD
  pref("browser.search.modernConfig", true);
#endif

pref("browser.sessionhistory.max_entries", 50);

// Built-in default permissions.
pref("permissions.manager.defaultsUrl", "resource://app/defaults/permissions");

// Set default fallback values for site permissions we want
// the user to be able to globally change.
pref("permissions.default.camera", 0);
pref("permissions.default.microphone", 0);
pref("permissions.default.geo", 0);
pref("permissions.default.xr", 0);
pref("permissions.default.desktop-notification", 0);
pref("permissions.default.shortcuts", 0);

pref("permissions.desktop-notification.postPrompt.enabled", true);
pref("permissions.desktop-notification.notNow.enabled", false);

pref("permissions.fullscreen.allowed", false);

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
pref("browser.tabs.closeTabByDblclick", false);
pref("browser.tabs.closeWindowWithLastTab", true);
pref("browser.tabs.allowTabDetach", true);
// Open related links to a tab, e.g., link in current tab, at next to the
// current tab if |insertRelatedAfterCurrent| is true.  Otherwise, always
// append new tab to the end.
pref("browser.tabs.insertRelatedAfterCurrent", true);
// Open all links, e.g., bookmarks, history items at next to current tab
// if |insertAfterCurrent| is true.  Otherwise, append new tab to the end
// for non-related links. Note that if this is set to true, it will trump
// the value of browser.tabs.insertRelatedAfterCurrent.
pref("browser.tabs.insertAfterCurrent", false);
pref("browser.tabs.warnOnClose", true);
pref("browser.tabs.warnOnCloseOtherTabs", true);
pref("browser.tabs.warnOnOpen", true);
pref("browser.tabs.maxOpenBeforeWarn", 15);
pref("browser.tabs.loadInBackground", true);
pref("browser.tabs.opentabfor.middleclick", true);
pref("browser.tabs.loadDivertedInBackground", false);
pref("browser.tabs.loadBookmarksInBackground", false);
pref("browser.tabs.loadBookmarksInTabs", false);
pref("browser.tabs.tabClipWidth", 140);
pref("browser.tabs.tabMinWidth", 76);
// Initial titlebar state is managed by -moz-gtk-csd-hide-titlebar-by-default
// on Linux.
#ifndef UNIX_BUT_NOT_MAC
  pref("browser.tabs.drawInTitlebar", true);
#endif

//Control the visibility of Tab Manager Menu.
pref("browser.tabs.tabmanager.enabled", false);

// Offer additional drag space to the user. The drag space
// will only be shown if browser.tabs.drawInTitlebar is true.
pref("browser.tabs.extraDragSpace", false);

// When tabs opened by links in other tabs via a combination of
// browser.link.open_newwindow being set to 3 and target="_blank" etc are
// closed:
// true   return to the tab that opened this tab (its owner)
// false  return to the adjacent tab (old default)
pref("browser.tabs.selectOwnerOnClose", true);

// This should match Chromium's audio indicator delay.
pref("browser.tabs.delayHidingAudioPlayingIconMS", 3000);

// Pref to control whether we use a separate privileged content process
// for about: pages. This pref name did not age well: we will have multiple
// types of privileged content processes, each with different privileges.
// types of privleged content processes, each with different privleges.
#if defined(MOZ_CODE_COVERAGE) && defined(XP_LINUX) || defined(MOZ_ASAN)
  // Disabled on Linux ccov builds due to bug 1621269.
  pref("browser.tabs.remote.separatePrivilegedContentProcess", false);
#else
  pref("browser.tabs.remote.separatePrivilegedContentProcess", true);
#endif

#if defined(NIGHTLY_BUILD) && !defined(MOZ_ASAN)
  // This pref will cause assertions when a remoteType triggers a process switch
  // to a new remoteType it should not be able to trigger.
  pref("browser.tabs.remote.enforceRemoteTypeRestrictions", true);
#endif

// Pref to control whether we use a separate privileged content process
// for certain mozilla webpages (which are listed in the pref
// browser.tabs.remote.separatedMozillaDomains).
pref("browser.tabs.remote.separatePrivilegedMozillaWebContentProcess", true);

// allow_eval_* is enabled on Firefox Desktop only at this
// point in time
pref("security.allow_eval_with_system_principal", false);
pref("security.allow_eval_in_parent_process", false);

#if defined(NIGHTLY_BUILD)
  pref("security.allow_parent_unrestricted_js_loads", false);
#endif

// Unload tabs when available memory is running low
pref("browser.tabs.unloadOnLowMemory", false);

pref("browser.ctrlTab.recentlyUsedOrder", true);

// By default, do not export HTML at shutdown.
// If true, at shutdown the bookmarks in your menu and toolbar will
// be exported as HTML to the bookmarks.html file.
pref("browser.bookmarks.autoExportHTML",          false);

// The maximum number of daily bookmark backups to
// keep in {PROFILEDIR}/bookmarkbackups. Special values:
// -1: unlimited
//  0: no backups created (and deletes all existing backups)
pref("browser.bookmarks.max_backups",             15);

// Whether menu should close after Ctrl-click, middle-click, etc.
pref("browser.bookmarks.openInTabClosesMenu", true);

// Scripts & Windows prefs
pref("dom.disable_open_during_load",              true);
pref("javascript.options.showInConsole",          true);

// This is the pref to control the location bar, change this to true to
// force this - this makes the origin of popup windows more obvious to avoid
// spoofing. We would rather not do it by default because it affects UE for web
// applications, but without it there isn't a really good way to prevent chrome
// spoofing, see bug 337344
pref("dom.disable_window_open_feature.location",  true);
// allow JS to move and resize existing windows
pref("dom.disable_window_move_resize",            false);
// prevent JS from monkeying with window focus, etc
pref("dom.disable_window_flip",                   true);

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

pref("privacy.history.custom",              false);

// What default should we use for the time span in the sanitizer:
// 0 - Clear everything
// 1 - Last Hour
// 2 - Last 2 Hours
// 3 - Last 4 Hours
// 4 - Today
// 5 - Last 5 minutes
// 6 - Last 24 hours
pref("privacy.sanitize.timeSpan", 1);

pref("privacy.sanitize.migrateFx3Prefs",    false);

pref("privacy.panicButton.enabled",         true);

// Time until temporary permissions expire, in ms
pref("privacy.temporary_permission_expire_time_ms",  3600000);

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
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
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

pref("browser.history_swipe_animation.disabled", false);

// 0: Nothing happens
// 1: Scrolling contents
// 2: Go back or go forward, in your history
// 3: Zoom in or out (reflowing zoom).
// 4: Treat vertical wheel as horizontal scroll
// 5: Zoom in or out (pinch zoom).
#ifdef XP_MACOSX
  // On macOS, if the wheel has one axis only, shift+wheel comes through as a
  // horizontal scroll event. Thus, we can't assign anything other than normal
  // scrolling to shift+wheel.
  pref("mousewheel.with_shift.action", 1);
  pref("mousewheel.with_alt.action", 2);
  // On MacOS X, control+wheel is typically handled by system and we don't
  // receive the event.  So, command key which is the main modifier key for
  // acceleration is the best modifier for zoom-in/out.  However, we should keep
  // the control key setting for backward compatibility.
  pref("mousewheel.with_meta.action", 3); // command key on Mac
  // Disable control-/meta-modified horizontal wheel events, since those are
  // used on Mac as part of modified swipe gestures (e.g. Left swipe+Cmd is
  // "go back" in a new tab).
  pref("mousewheel.with_control.action.override_x", 0);
  pref("mousewheel.with_meta.action.override_x", 0);
#else
  // On the other platforms (non-macOS), user may use legacy mouse which
  // supports only vertical wheel but want to scroll horizontally.  For such
  // users, we should provide horizontal scroll with shift+wheel (same as
  // Chrome). However, shift+wheel was used for navigating history.  For users
  // who want to keep using this feature, let's enable it with alt+wheel.  This
  // is better for consistency with macOS users.
  pref("mousewheel.with_shift.action", 4);
  pref("mousewheel.with_alt.action", 2);
  pref("mousewheel.with_meta.action", 1); // win key on Win, Super/Hyper on Linux
#endif
pref("mousewheel.with_control.action",3);
pref("mousewheel.with_win.action", 1);

pref("browser.xul.error_pages.expert_bad_cert", false);
pref("browser.xul.error_pages.show_safe_browsing_details_on_load", false);

// Enable captive portal detection.
pref("network.captive-portal-service.enabled", true);

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

// Accessibility indicator preferences such as support URL, enabled flag.
pref("accessibility.support.url", "https://support.mozilla.org/%LOCALE%/kb/accessibility-services");
pref("accessibility.indicator.enabled", false);

pref("plugins.testmode", false);

// Should plugins that are hidden show the infobar UI?
pref("plugins.show_infobar", false);

#if defined(_ARM64_) && defined(XP_WIN)
  pref("plugin.default.state", 0);
#else
  pref("plugin.default.state", 1);
#endif

// Enables the download and use of the flash blocklists.
pref("plugins.flashBlock.enabled", true);

// Prefer HTML5 video over Flash content, and don't
// load plugin instances with no src declared.
// These prefs are documented in details on all.js.
// With the "follow-ctp" setting, this will only
// apply to users that have plugin.state.flash = 1.
pref("plugins.favorfallback.mode", "follow-ctp");
pref("plugins.favorfallback.rules", "nosrc,video");

#ifdef XP_WIN
  pref("browser.preferences.instantApply", false);
#else
  pref("browser.preferences.instantApply", true);
#endif

// Toggling Search bar on and off in about:preferences
pref("browser.preferences.search", true);

pref("browser.preferences.defaultPerformanceSettings.enabled", true);

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

pref("intl.regional_prefs.use_os_locales", false);

// this will automatically enable inline spellchecking (if it is available) for
// editable elements in HTML
// 0 = spellcheck nothing
// 1 = check multi-line controls [default]
// 2 = check multi/single line controls
pref("layout.spellcheckDefault", 1);

pref("browser.send_pings", false);

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

pref("browser.geolocation.warning.infoURL", "https://www.mozilla.org/%LOCALE%/firefox/geolocation/");
pref("browser.xr.warning.infoURL", "https://www.mozilla.org/%LOCALE%/firefox/xr/");

pref("browser.sessionstore.resume_from_crash", true);
pref("browser.sessionstore.resume_session_once", false);
pref("browser.sessionstore.resuming_after_os_restart", false);

// Minimal interval between two save operations in milliseconds (while the user is active).
pref("browser.sessionstore.interval", 15000); // 15 seconds

// Minimal interval between two save operations in milliseconds (while the user is idle).
pref("browser.sessionstore.interval.idle", 3600000); // 1h

// Time (ms) before we assume that the user is idle and that we don't need to
// collect/save the session quite as often.
pref("browser.sessionstore.idleDelay", 180000); // 3 minutes

// on which sites to save text data, POSTDATA and cookies
// 0 = everywhere, 1 = unencrypted sites, 2 = nowhere
pref("browser.sessionstore.privacy_level", 0);
// how many tabs can be reopened (per window)
pref("browser.sessionstore.max_tabs_undo", 25);
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
// Causes SessionStore to ignore non-final update messages from
// browser tabs that were not caused by a flush from the parent.
// This is a testing flag and should not be used by end-users.
pref("browser.sessionstore.debug.no_auto_updates", false);
// Forget closed windows/tabs after two weeks
pref("browser.sessionstore.cleanup.forget_closed_after", 1209600000);
// Amount of failed SessionFile writes until we restart the worker.
pref("browser.sessionstore.max_write_failures", 5);

// Whether to warn the user when quitting, even though their tabs will be restored.
pref("browser.sessionstore.warnOnQuit", false);

// allow META refresh by default
pref("accessibility.blockautorefresh", false);

// Whether history is enabled or not.
pref("places.history.enabled", true);

// Whether or not diacritics must match in history text searches.
pref("places.search.matchDiacritics", false);

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
// The bookmarks bonus is always added on top of any other bonus, including
// the redirect source and the typed ones.
pref("places.frecency.bookmarkVisitBonus", 75);
// The redirect source bonus overwrites any transition bonus.
// 0 would hide these pages, instead we want them low ranked.  Thus we use
// linkVisitBonus - bookmarkVisitBonus, so that a bookmarked source is in par
// with a common link.
pref("places.frecency.redirectSourceVisitBonus", 25);
pref("places.frecency.downloadVisitBonus", 0);
// The perm/temp redirects here relate to redirect targets, not sources.
pref("places.frecency.permRedirectVisitBonus", 50);
pref("places.frecency.tempRedirectVisitBonus", 40);
pref("places.frecency.reloadVisitBonus", 0);
pref("places.frecency.defaultVisitBonus", 0);

// bonus (in percent) for place types for frecency calculations
pref("places.frecency.unvisitedBookmarkBonus", 140);
pref("places.frecency.unvisitedTypedBonus", 200);

// Controls behavior of the "Add Exception" dialog launched from SSL error pages
// 0 - don't pre-populate anything
// 1 - pre-populate site URL, but don't fetch certificate
// 2 - pre-populate site URL and pre-fetch certificate
pref("browser.ssl_override_behavior", 2);

// if true, use full page zoom instead of text zoom
pref("browser.zoom.full", true);

// Whether or not to save and restore zoom levels on a per-site basis.
pref("browser.zoom.siteSpecific", true);

// Whether or not to update background tabs to the current zoom level.
pref("browser.zoom.updateBackgroundTabs", true);

// The breakpad report server to link to in about:crashes
pref("breakpad.reportURL", "https://crash-stats.mozilla.org/report/index/");

// URL for "Learn More" for DataCollection
pref("toolkit.datacollection.infoURL",
     "https://www.mozilla.org/legal/privacy/firefox.html");

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

pref("security.certerrors.recordEventTelemetry", true);
pref("security.certerrors.permanentOverride", true);
pref("security.certerrors.mitm.priming.enabled", true);
pref("security.certerrors.mitm.priming.endpoint", "https://mitmdetection.services.mozilla.com/");
pref("security.certerrors.mitm.auto_enable_enterprise_roots", true);

pref("security.aboutcertificate.enabled", true);

// Whether the bookmark panel should be shown when bookmarking a page.
pref("browser.bookmarks.editDialog.showForNewBookmarks", true);

// Don't try to alter this pref, it'll be reset the next time you use the
// bookmarking dialog
pref("browser.bookmarks.editDialog.firstEditField", "namePicker");

// The number of recently selected folders in the edit bookmarks dialog.
pref("browser.bookmarks.editDialog.maxRecentFolders", 7);

pref("dom.ipc.plugins.flash.disable-protected-mode", false);

// Feature-disable the protected-mode auto-flip
pref("browser.flash-protected-mode-flip.enable", false);

// Whether we've already flipped protected mode automatically
pref("browser.flash-protected-mode-flip.done", false);

pref("dom.ipc.shims.enabledWarnings", false);

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
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
    // The base sandbox level in nsPluginTag::InitSandboxLevel must be
    // updated to keep in sync with this value.
    pref("dom.ipc.plugins.sandbox-level.flash", 3);
  #else
    pref("dom.ipc.plugins.sandbox-level.flash", 0);
  #endif

  // This controls the strength of the Windows content process sandbox for
  // testing purposes. This will require a restart.
  // On windows these levels are:
  // See - security/sandbox/win/src/sandboxbroker/sandboxBroker.cpp
  // SetSecurityLevelForContentProcess() for what the different settings mean.
  pref("security.sandbox.content.level", 6);

  // This controls the depth of stack trace that is logged when Windows sandbox
  // logging is turned on.  This is only currently available for the content
  // process because the only other sandbox (for GMP) has too strict a policy to
  // allow stack tracing.  This does not require a restart to take effect.
  pref("security.sandbox.windows.log.stackTraceDepth", 0);

  // This controls the strength of the Windows GPU process sandbox.  Changes
  // will require restart.
  // For information on what the level number means, see
  // SetSecurityLevelForGPUProcess() in
  // security/sandbox/win/src/sandboxbroker/sandboxBroker.cpp
  pref("security.sandbox.gpu.level", 0);

  // Controls whether we disable win32k for the processes.
  // true means that win32k system calls are not permitted.
  pref("security.sandbox.rdd.win32k-disable", true);
  // Note: win32k is currently _not_ disabled for GMP due to intermittent test
  // failures, where the GMP process fails very early. See bug 1449348.
  pref("security.sandbox.gmp.win32k-disable", false);
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  // This pref is discussed in bug 1083344, the naming is inspired from its
  // Windows counterpart, but on Mac it's an integer which means:
  // 0 -> "no sandbox" (nightly only)
  // 1 -> "preliminary content sandboxing enabled: write access to
  //       home directory is prevented"
  // 2 -> "preliminary content sandboxing enabled with profile protection:
  //       write access to home directory is prevented, read and write access
  //       to ~/Library and profile directories are prevented (excluding
  //       $PROFILE/{extensions,chrome})"
  // 3 -> "no global read/write access, read access permitted to
  //       $PROFILE/{extensions,chrome}"
  // This setting is read when the content process is started. On Mac the
  // content process is killed when all windows are closed, so a change will
  // take effect when the 1st window is opened.
  pref("security.sandbox.content.level", 3);

  // Prefs for controlling whether and how the Mac NPAPI Flash plugin process is
  // sandboxed. On Mac these levels are:
  // 0 - "no sandbox"
  // 1 - "global read access, limited write access for Flash functionality"
  // 2 - "read access triggered by file dialog activity, limited read/write"
  //     "access for Flash functionality"
  // 3 - "limited read/write access for Flash functionality"
  pref("dom.ipc.plugins.sandbox-level.flash", 1);
  // Controls the level used on older OS X versions. Is overriden when the
  // "dom.ipc.plugins.sandbox-level.flash" is set to 0.
  pref("dom.ipc.plugins.sandbox-level.flash.legacy", 1);
  // The max OS minor version where we use the above legacy sandbox level.
  pref("dom.ipc.plugins.sandbox-level.flash.max-legacy-os-minor", 10);
  // Controls the sandbox level used by plugins other than Flash. On Mac,
  // no other plugins are supported and this pref is only used for test
  // plugins used in automated tests.
  pref("dom.ipc.plugins.sandbox-level.default", 1);
#endif

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  // This pref is introduced as part of bug 742434, the naming is inspired from
  // its Windows/Mac counterpart, but on Linux it's an integer which means:
  // 0 -> "no sandbox"
  // 1 -> "content sandbox using seccomp-bpf when available" + ipc restrictions
  // 2 -> "seccomp-bpf + write file broker"
  // 3 -> "seccomp-bpf + read/write file brokering"
  // 4 -> all of the above + network/socket restrictions + chroot
  //
  // The purpose of this setting is to allow Linux users or distros to disable
  // the sandbox while we fix their problems, or to allow running Firefox with
  // exotic configurations we can't reasonably support out of the box.
  //
  pref("security.sandbox.content.level", 4);
  // Introduced as part of bug 1608558.  Linux is currently the only platform
  // that uses a sandbox level for the socket process.  There are currently
  // only 2 levels:
  // 0 -> "no sandbox"
  // 1 -> "sandboxed, allows socket operations and reading necessary certs"
  pref("security.sandbox.socket.process.level", 1);
  pref("security.sandbox.content.write_path_whitelist", "");
  pref("security.sandbox.content.read_path_whitelist", "");
  pref("security.sandbox.content.syscall_whitelist", "");
#endif

#if defined(XP_OPENBSD) && defined(MOZ_SANDBOX)
  pref("security.sandbox.content.level", 1);
#endif

#if defined(MOZ_SANDBOX)
  // ID (a UUID when set by gecko) that is used to form the name of a
  // sandbox-writable temporary directory to be used by content processes
  // when a temporary writable file is required in a level 1 sandbox.
  pref("security.sandbox.content.tempDirSuffix", "");
  pref("security.sandbox.plugin.tempDirSuffix", "");

  // This pref determines if messages relevant to sandbox violations are
  // logged.
  #if defined(XP_WIN) || defined(XP_MACOSX)
    pref("security.sandbox.logging.enabled", false);
  #endif
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

// Preferences to be synced by default
pref("services.sync.prefs.sync.accessibility.blockautorefresh", true);
pref("services.sync.prefs.sync.accessibility.browsewithcaret", true);
pref("services.sync.prefs.sync.accessibility.typeaheadfind", true);
pref("services.sync.prefs.sync.accessibility.typeaheadfind.linksonly", true);
pref("services.sync.prefs.sync.addons.ignoreUserEnabledChanges", true);
pref("services.sync.prefs.sync.app.shield.optoutstudies.enabled", true);
// The addons prefs related to repository verification are intentionally
// not synced for security reasons. If a system is compromised, a user
// could weaken the pref locally, install an add-on from an untrusted
// source, and this would propagate automatically to other,
// uncompromised Sync-connected devices.
pref("services.sync.prefs.sync.browser.contentblocking.category", true);
pref("services.sync.prefs.sync.browser.contentblocking.features.strict", true);
pref("services.sync.prefs.sync.browser.crashReports.unsubmittedCheck.autoSubmit2", true);
pref("services.sync.prefs.sync.browser.ctrlTab.recentlyUsedOrder", true);
pref("services.sync.prefs.sync.browser.discovery.enabled", true);
pref("services.sync.prefs.sync.browser.download.useDownloadDir", true);
pref("services.sync.prefs.sync.browser.formfill.enable", true);
pref("services.sync.prefs.sync.browser.link.open_newwindow", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.showSearch", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.showSponsored", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.feeds.topsites", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.topSitesRows", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.feeds.snippets", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.feeds.section.topstories", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.section.topstories.rows", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.feeds.section.highlights", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.section.highlights.includeVisited", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.section.highlights.includeBookmarks", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.section.highlights.includeDownloads", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.section.highlights.includePocket", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.section.highlights.rows", true);
pref("services.sync.prefs.sync.browser.newtabpage.enabled", true);
pref("services.sync.prefs.sync.browser.newtabpage.pinned", true);
pref("services.sync.prefs.sync.browser.offline-apps.notify", true);
pref("services.sync.prefs.sync.browser.search.update", true);
pref("services.sync.prefs.sync.browser.search.widget.inNavBar", true);
pref("services.sync.prefs.sync.browser.startup.homepage", true);
pref("services.sync.prefs.sync.browser.startup.page", true);
pref("services.sync.prefs.sync.browser.tabs.loadInBackground", true);
pref("services.sync.prefs.sync.browser.tabs.warnOnClose", true);
pref("services.sync.prefs.sync.browser.tabs.warnOnOpen", true);
pref("services.sync.prefs.sync.browser.taskbar.previews.enable", true);
pref("services.sync.prefs.sync.browser.urlbar.matchBuckets", true);
pref("services.sync.prefs.sync.browser.urlbar.maxRichResults", true);
pref("services.sync.prefs.sync.browser.urlbar.suggest.bookmark", true);
pref("services.sync.prefs.sync.browser.urlbar.suggest.history", true);
pref("services.sync.prefs.sync.browser.urlbar.suggest.openpage", true);
pref("services.sync.prefs.sync.browser.urlbar.suggest.searches", true);
pref("services.sync.prefs.sync.dom.disable_open_during_load", true);
pref("services.sync.prefs.sync.dom.disable_window_flip", true);
pref("services.sync.prefs.sync.dom.disable_window_move_resize", true);
pref("services.sync.prefs.sync.dom.event.contextmenu.enabled", true);
pref("services.sync.prefs.sync.extensions.update.enabled", true);
pref("services.sync.prefs.sync.extensions.activeThemeID", true);
pref("services.sync.prefs.sync.intl.accept_languages", true);
pref("services.sync.prefs.sync.intl.regional_prefs.use_os_locales", true);
pref("services.sync.prefs.sync.layout.spellcheckDefault", true);
pref("services.sync.prefs.sync.media.autoplay.default", true);
pref("services.sync.prefs.sync.media.eme.enabled", true);
pref("services.sync.prefs.sync.network.cookie.cookieBehavior", true);
pref("services.sync.prefs.sync.network.cookie.lifetimePolicy", true);
pref("services.sync.prefs.sync.network.cookie.thirdparty.sessionOnly", true);
pref("services.sync.prefs.sync.permissions.default.image", true);
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
pref("services.sync.prefs.sync.privacy.fuzzyfox.enabled", false);
pref("services.sync.prefs.sync.privacy.fuzzyfox.clockgrainus", false);
pref("services.sync.prefs.sync.privacy.sanitize.sanitizeOnShutdown", true);
pref("services.sync.prefs.sync.privacy.trackingprotection.enabled", true);
pref("services.sync.prefs.sync.privacy.trackingprotection.cryptomining.enabled", true);
pref("services.sync.prefs.sync.privacy.trackingprotection.fingerprinting.enabled", true);
pref("services.sync.prefs.sync.privacy.trackingprotection.pbmode.enabled", true);
pref("services.sync.prefs.sync.privacy.resistFingerprinting", true);
pref("services.sync.prefs.sync.privacy.reduceTimerPrecision", true);
pref("services.sync.prefs.sync.privacy.resistFingerprinting.reduceTimerPrecision.microseconds", true);
pref("services.sync.prefs.sync.privacy.resistFingerprinting.reduceTimerPrecision.jitter", true);
pref("services.sync.prefs.sync.security.default_personal_cert", true);
pref("services.sync.prefs.sync.services.sync.syncedTabs.showRemoteIcons", true);
pref("services.sync.prefs.sync.signon.rememberSignons", true);
pref("services.sync.prefs.sync.spellchecker.dictionary", true);

// A preference which, if false, means sync will only apply incoming preference
// changes if there's already a local services.sync.prefs.sync.* control pref.
// If true, all incoming preferences will be applied and the local "control
// pref" updated accordingly.
pref("services.sync.prefs.dangerously_allow_arbitrary", false);

// A preference that controls whether we should show the icon for a remote tab.
// This pref has no UI but exists because some people may be concerned that
// fetching these icons to show remote tabs may leak information about that
// user's tabs and bookmarks. Note this pref is also synced.
pref("services.sync.syncedTabs.showRemoteIcons", true);

// Whether the character encoding menu is under the main Firefox button. This
// preference is a string so that localizers can alter it.
pref("browser.menu.showCharacterEncoding", "chrome://browser/locale/browser.properties");

// Allow using tab-modal prompts when possible.
pref("prompts.tab_modal.enabled", true);

// Whether prompts should be content modal (1) tab modal (2) or window modal(3) by default
// This is a fallback value for when prompt callers do not specify a modalType.
pref("prompts.defaultModalType", 3);

// Activates preloading of the new tab url.
pref("browser.newtab.preload", true);

// Activity Stream prefs that control to which page to redirect
#ifndef RELEASE_OR_BETA
  pref("browser.newtabpage.activity-stream.debug", false);
#endif

pref("browser.library.activity-stream.enabled", true);

// The remote FxA root content URL for the Activity Stream firstrun page.
pref("browser.newtabpage.activity-stream.fxaccounts.endpoint", "https://accounts.firefox.com/");

// The pref that controls if the search shortcuts experiment is on
pref("browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts", true);

// ASRouter provider configuration
pref("browser.newtabpage.activity-stream.asrouter.providers.cfr", "{\"id\":\"cfr\",\"enabled\":true,\"type\":\"remote-settings\",\"bucket\":\"cfr\",\"frequency\":{\"custom\":[{\"period\":\"daily\",\"cap\":1}]},\"categories\":[\"cfrAddons\",\"cfrFeatures\"],\"updateCycleInMs\":3600000}");
pref("browser.newtabpage.activity-stream.asrouter.providers.whats-new-panel", "{\"id\":\"whats-new-panel\",\"enabled\":true,\"type\":\"remote-settings\",\"bucket\":\"whats-new-panel\",\"updateCycleInMs\":3600000}");
pref("browser.newtabpage.activity-stream.asrouter.providers.message-groups", "{\"id\":\"message-groups\",\"enabled\":false,\"type\":\"remote-settings\",\"bucket\":\"message-groups\",\"updateCycleInMs\":3600000}");
// This url, if changed, MUST continue to point to an https url. Pulling arbitrary content to inject into
// this page over http opens us up to a man-in-the-middle attack that we'd rather not face. If you are a downstream
// repackager of this code using an alternate snippet url, please keep your users safe
pref("browser.newtabpage.activity-stream.asrouter.providers.snippets", "{\"id\":\"snippets\",\"enabled\":true,\"type\":\"remote\",\"url\":\"https://snippets.cdn.mozilla.net/%STARTPAGE_VERSION%/%NAME%/%VERSION%/%APPBUILDID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/\",\"updateCycleInMs\":14400000}");
pref("browser.newtabpage.activity-stream.asrouter.providers.messaging-experiments", "{\"id\":\"messaging-experiments\",\"enabled\":true,\"type\":\"remote-experiments\",\"messageGroups\":[\"cfr\",\"whats-new-panel\",\"moments-page\",\"snippets\",\"cfr-fxa\"],\"updateCycleInMs\":3600000}");

// The pref that controls if ASRouter uses the remote fluent files.
// It's enabled by default, but could be disabled to force ASRouter to use the local files.
pref("browser.newtabpage.activity-stream.asrouter.useRemoteL10n", true);

// These prefs control if Discovery Stream is enabled.
pref("browser.newtabpage.activity-stream.discoverystream.enabled", true);
pref("browser.newtabpage.activity-stream.discoverystream.hardcoded-basic-layout", false);
pref("browser.newtabpage.activity-stream.discoverystream.spocs-endpoint", "");
// List of regions that get stories by default.
pref("browser.newtabpage.activity-stream.discoverystream.region-stories-config", "US,DE,CA");
// List of regions that get spocs by default.
pref("browser.newtabpage.activity-stream.discoverystream.region-spocs-config", "US");
// List of regions that get the 7 row layout.
pref("browser.newtabpage.activity-stream.discoverystream.region-layout-config", "US,CA");
// Allows Pocket story collections to be dismissed.
pref("browser.newtabpage.activity-stream.discoverystream.isCollectionDismissible", false);
// Switch between different versions of the recommendation provider.
pref("browser.newtabpage.activity-stream.discoverystream.personalization.version", 1);
// Configurable keys used by personalization version 2.
pref("browser.newtabpage.activity-stream.discoverystream.personalization.modelKeys", "nb_model_arts_and_entertainment, nb_model_autos_and_vehicles, nb_model_beauty_and_fitness, nb_model_blogging_resources_and_services, nb_model_books_and_literature, nb_model_business_and_industrial, nb_model_computers_and_electronics, nb_model_finance, nb_model_food_and_drink, nb_model_games, nb_model_health, nb_model_hobbies_and_leisure, nb_model_home_and_garden, nb_model_internet_and_telecom, nb_model_jobs_and_education, nb_model_law_and_government, nb_model_online_communities, nb_model_people_and_society, nb_model_pets_and_animals, nb_model_real_estate, nb_model_reference, nb_model_science, nb_model_shopping, nb_model_sports, nb_model_travel");

// The pref controls if search hand-off is enabled for Activity Stream.
#ifdef NIGHTLY_BUILD
  pref("browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar", true);
#else
  pref("browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar", false);
#endif

pref("trailhead.firstrun.branches", "join-dynamic");

// Separate about welcome
pref("browser.aboutwelcome.enabled", true);

// The pref that controls if the What's New panel is enabled.
pref("browser.messaging-system.whatsNewPanel.enabled", true);
// Used for CFR messages with scores. See Bug 1594422.
pref("browser.messaging-system.personalized-cfr.scores", "{}");
pref("browser.messaging-system.personalized-cfr.score-threshold", 5000);

// Experiment Manager
// See Console.jsm LOG_LEVELS for all possible values
pref("messaging-system.log", "warn");
pref("messaging-system.rsexperimentloader.enabled", true);

// Enable the DOM fullscreen API.
pref("full-screen-api.enabled", true);

// Startup Crash Tracking
// number of startup crashes that can occur before starting into safe mode automatically
// (this pref has no effect if more than 6 hours have passed since the last crash)
pref("toolkit.startup.max_resumed_crashes", 3);

// Whether to use RegisterApplicationRestart to restart the browser and resume
// the session on next Windows startup
#if defined(XP_WIN)
  pref("toolkit.winRegisterApplicationRestart", true);
#endif

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

// Is the sidebar positioned ahead of the content browser
pref("sidebar.position_start", true);

pref("security.identitypopup.recordEventTelemetry", true);
pref("security.protectionspopup.recordEventTelemetry", true);
pref("security.app_menu.recordEventTelemetry", true);

// Block insecure active content on https pages
pref("security.mixed_content.block_active_content", true);

// Show in-content login form warning UI for insecure login fields
pref("security.insecure_field_warning.contextual.enabled", true);

// Show degraded UI for http pages.
pref("security.insecure_connection_icon.enabled", true);
// Show degraded UI for http pages in private mode.
pref("security.insecure_connection_icon.pbmode.enabled", true);

// For secure connections, show gray instead of green lock icon
pref("security.secure_connection_icon_color_gray", true);

// Show "Not Secure" text for http pages; disabled for now
pref("security.insecure_connection_text.enabled", false);
pref("security.insecure_connection_text.pbmode.enabled", false);

// 1 = allow MITM for certificate pinning checks.
pref("security.cert_pinning.enforcement_level", 1);


// If this turns true, Moz*Gesture events are not called stopPropagation()
// before content.
pref("dom.debug.propagate_gesture_events_through_content", false);

// CustomizableUI debug logging.
pref("browser.uiCustomization.debug", false);

// CustomizableUI state of the browser's user interface
pref("browser.uiCustomization.state", "");

// If set to false, FxAccounts and Sync will be unavailable.
// A restart is mandatory after flipping that preference.
pref("identity.fxaccounts.enabled", true);

// The remote FxA root content URL. Must use HTTPS.
pref("identity.fxaccounts.remote.root", "https://accounts.firefox.com/");

// The value of the context query parameter passed in fxa requests.
pref("identity.fxaccounts.contextParam", "fx_desktop_v3");

// The remote URL of the FxA Profile Server
pref("identity.fxaccounts.remote.profile.uri", "https://profile.accounts.firefox.com/v1");

// The remote URL of the FxA OAuth Server
pref("identity.fxaccounts.remote.oauth.uri", "https://oauth.accounts.firefox.com/v1");

// Whether FxA pairing using QR codes is enabled.
pref("identity.fxaccounts.pairing.enabled", true);

// The remote URI of the FxA pairing server
pref("identity.fxaccounts.remote.pairing.uri", "wss://channelserver.services.mozilla.com");

// Token server used by the FxA Sync identity.
pref("identity.sync.tokenserver.uri", "https://token.services.mozilla.com/1.0/sync/1.5");

// Fetch Sync tokens using the OAuth token function
pref("identity.sync.useOAuthForSyncToken", false);

// Auto-config URL for FxA self-hosters, makes an HTTP request to
// [identity.fxaccounts.autoconfig.uri]/.well-known/fxa-client-configuration
// This is now the prefered way of pointing to a custom FxA server, instead
// of making changes to "identity.fxaccounts.*.uri".
pref("identity.fxaccounts.autoconfig.uri", "");

// URL for help link about Send Tab.
pref("identity.sendtabpromo.url", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/send-tab");

// URLs for promo links to mobile browsers. Note that consumers are expected to
// append a value for utm_campaign.
pref("identity.mobilepromo.android", "https://www.mozilla.org/firefox/android/?utm_source=firefox-browser&utm_medium=firefox-browser&utm_campaign=");
pref("identity.mobilepromo.ios", "https://www.mozilla.org/firefox/ios/?utm_source=firefox-browser&utm_medium=firefox-browser&utm_campaign=");

// Migrate any existing Firefox Account data from the default profile to the
// Developer Edition profile.
#ifdef MOZ_DEV_EDITION
  pref("identity.fxaccounts.migrateToDevEdition", true);
#else
  pref("identity.fxaccounts.migrateToDevEdition", false);
#endif

// If activated, send tab will use the new FxA commands backend.
pref("identity.fxaccounts.commands.enabled", true);
// How often should we try to fetch missed FxA commands on sync (in seconds).
// Default is 24 hours.
pref("identity.fxaccounts.commands.missed.fetch_interval", 86400);

// Whether we should run a test-pattern through EME GMPs before assuming they'll
// decode H.264.
pref("media.gmp.trial-create.enabled", true);

// Note: when media.gmp-*.visible is true, provided we're running on a
// supported platform/OS version, the corresponding CDM appears in the
// plugins list, Firefox will download the GMP/CDM if enabled, and our
// UI to re-enable EME prompts the user to re-enable EME if it's disabled
// and script requests EME. If *.visible is false, we won't show the UI
// to enable the CDM if its disabled; it's as if the keysystem is completely
// unsupported.

#ifdef MOZ_WIDEVINE_EME
  pref("media.gmp-widevinecdm.visible", true);
  pref("media.gmp-widevinecdm.enabled", true);
#endif

pref("media.gmp-gmpopenh264.visible", true);
pref("media.gmp-gmpopenh264.enabled", true);

// Switch block autoplay logic to v2, and enable UI.
pref("media.autoplay.enabled.user-gestures-needed", true);
// Set Firefox to block autoplay, asking for permission by default.
pref("media.autoplay.default", 1); // 0=Allowed, 1=Blocked, 5=All Blocked

#ifdef NIGHTLY_BUILD
  // Block WebAudio from playing automatically.
  pref("media.autoplay.block-webaudio", true);
#else
  pref("media.autoplay.block-webaudio", false);
#endif

pref("media.videocontrols.picture-in-picture.enabled", true);
pref("media.videocontrols.picture-in-picture.video-toggle.enabled", true);

#ifdef NIGHTLY_BUILD
  // Show the audio toggle for Picture-in-Picture.
  pref("media.videocontrols.picture-in-picture.audio-toggle.enabled", true);
  // Enable keyboard controls for Picture-in-Picture.
  pref("media.videocontrols.picture-in-picture.keyboard-controls.enabled", true);
#else
  pref("media.videocontrols.picture-in-picture.audio-toggle.enabled", false);
  pref("media.videocontrols.picture-in-picture.keyboard-controls.enabled", false);
#endif

pref("browser.translation.detectLanguage", false);
pref("browser.translation.neverForLanguages", "");
// Show the translation UI bits, like the info bar, notification icon and preferences.
pref("browser.translation.ui.show", false);
// Allows to define the translation engine. Google is default, Bing or Yandex are other options.
pref("browser.translation.engine", "Google");

// Telemetry settings.
// Determines if Telemetry pings can be archived locally.
pref("toolkit.telemetry.archive.enabled", true);
// Enables sending the shutdown ping when Firefox shuts down.
pref("toolkit.telemetry.shutdownPingSender.enabled", true);
// Enables sending the shutdown ping using the pingsender from the first session.
pref("toolkit.telemetry.shutdownPingSender.enabledFirstSession", false);
// Enables sending a duplicate of the first shutdown ping from the first session.
pref("toolkit.telemetry.firstShutdownPing.enabled", true);
// Enables sending the 'new-profile' ping on new profiles.
pref("toolkit.telemetry.newProfilePing.enabled", true);
// Enables sending 'update' pings on Firefox updates.
pref("toolkit.telemetry.updatePing.enabled", true);
// Enables sending 'bhr' pings when the browser hangs.
pref("toolkit.telemetry.bhrPing.enabled", true);
// Whether to enable Ecosystem Telemetry, requires a restart.
pref("toolkit.telemetry.ecosystemtelemetry.enabled", false);

// Ping Centre Telemetry settings.
pref("browser.ping-centre.telemetry", true);
pref("browser.ping-centre.log", false);

// Enable GMP support in the addon manager.
pref("media.gmp-provider.enabled", true);

// Enable blocking access to storage from tracking resources by default.
pref("network.cookie.cookieBehavior", 4 /* BEHAVIOR_REJECT_TRACKER */);

// Enable fingerprinting blocking by default for all channels, only on desktop.
pref("privacy.trackingprotection.fingerprinting.enabled", true);

// Enable cryptomining blocking by default for all channels, only on desktop.
pref("privacy.trackingprotection.cryptomining.enabled", true);

pref("browser.contentblocking.database.enabled", true);

pref("dom.storage_access.enabled", true);

pref("browser.contentblocking.cryptomining.preferences.ui.enabled", true);
pref("browser.contentblocking.fingerprinting.preferences.ui.enabled", true);
#ifdef NIGHTLY_BUILD
  // Enable cookieBehavior = BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN as an option in the custom category ui
  pref("browser.contentblocking.reject-and-isolate-cookies.preferences.ui.enabled", true);
#endif

// Possible values for browser.contentblocking.features.strict pref:
//   Tracking Protection:
//     "tp": tracking protection enabled
//     "-tp": tracking protection disabled
//   Tracking Protection in private windows:
//     "tpPrivate": tracking protection in private windows enabled
//     "-tpPrivate": tracking protection in private windows disabled
//   Fingerprinting:
//     "fp": fingerprinting blocking enabled
//     "-fp": fingerprinting blocking disabled
//   Cryptomining:
//     "cm": cryptomining blocking enabled
//     "-cm": cryptomining blocking disabled
//   Social Tracking Protection:
//     "stp": social tracking protection enabled
//     "-stp": social tracking protection disabled
//   Cookie behavior:
//     "cookieBehavior0": cookie behaviour BEHAVIOR_ACCEPT
//     "cookieBehavior1": cookie behaviour BEHAVIOR_REJECT_FOREIGN
//     "cookieBehavior2": cookie behaviour BEHAVIOR_REJECT
//     "cookieBehavior3": cookie behaviour BEHAVIOR_LIMIT_FOREIGN
//     "cookieBehavior4": cookie behaviour BEHAVIOR_REJECT_TRACKER
//     "cookieBehavior5": cookie behaviour BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
// One value from each section must be included in the browser.contentblocking.features.strict pref.
pref("browser.contentblocking.features.strict", "tp,tpPrivate,cookieBehavior4,cm,fp,stp");

// Hide the "Change Block List" link for trackers/tracking content in the custom
// Content Blocking/ETP panel. By default, it will not be visible. There is also
// an UI migration in place to set this pref to true if a user has a custom block
// lists enabled.
pref("browser.contentblocking.customBlockList.preferences.ui.enabled", false);

pref("browser.contentblocking.reportBreakage.url", "https://tracking-protection-issues.herokuapp.com/new");

// Enable Protections report's Lockwise card by default.
pref("browser.contentblocking.report.lockwise.enabled", true);

// Enable Protections report's Monitor card by default.
pref("browser.contentblocking.report.monitor.enabled", true);

// Disable Protections report's Proxy card by default.
pref("browser.contentblocking.report.proxy.enabled", false);

// Disable the mobile promotion by default.
pref("browser.contentblocking.report.show_mobile_app", false);

pref("browser.contentblocking.report.monitor.url", "https://monitor.firefox.com/?entrypoint=protection_report_monitor&utm_source=about-protections");
pref("browser.contentblocking.report.monitor.how_it_works.url", "https://monitor.firefox.com/about");
pref("browser.contentblocking.report.monitor.sign_in_url", "https://monitor.firefox.com/oauth/init?entrypoint=protection_report_monitor&utm_source=about-protections&email=");
pref("browser.contentblocking.report.manage_devices.url", "https://accounts.firefox.com/settings/clients");
pref("browser.contentblocking.report.proxy_extension.url", "https://fpn.firefox.com/browser?utm_source=firefox-desktop&utm_medium=referral&utm_campaign=about-protections&utm_content=about-protections");
pref("browser.contentblocking.report.lockwise.mobile-ios.url", "https://apps.apple.com/app/id1314000270");
pref("browser.contentblocking.report.lockwise.mobile-android.url", "https://play.google.com/store/apps/details?id=mozilla.lockbox&referrer=utm_source%3Dprotection_report%26utm_content%3Dmobile_promotion");
pref("browser.contentblocking.report.mobile-ios.url", "https://apps.apple.com/app/firefox-private-safe-browser/id989804926");
pref("browser.contentblocking.report.mobile-android.url", "https://play.google.com/store/apps/details?id=org.mozilla.firefox&referrer=utm_source%3Dprotection_report%26utm_content%3Dmobile_promotion");

// Protection Report's SUMO urls
pref("browser.contentblocking.report.lockwise.how_it_works.url", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/password-manager-report");
pref("browser.contentblocking.report.social.url", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/social-media-tracking-report");
pref("browser.contentblocking.report.cookie.url", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/cross-site-tracking-report");
pref("browser.contentblocking.report.tracker.url", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/tracking-content-report");
pref("browser.contentblocking.report.fingerprinter.url", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/fingerprinters-report");
pref("browser.contentblocking.report.cryptominer.url", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/cryptominers-report");

pref("browser.contentblocking.cfr-milestone.enabled", true);
pref("browser.contentblocking.cfr-milestone.milestone-achieved", 0);
// Milestones should always be in increasing order
pref("browser.contentblocking.cfr-milestone.milestones", "[1000, 5000, 10000, 25000, 50000, 100000, 500000]");

// Enables the new Protections Panel.
#ifdef NIGHTLY_BUILD
  pref("browser.protections_panel.enabled", true);
  pref("browser.protections_panel.infoMessage.seen", false);
#endif

// Always enable newtab segregation using containers
pref("privacy.usercontext.about_newtab_segregation.enabled", true);
// Enable Contextual Identity Containers
#ifdef NIGHTLY_BUILD
  pref("privacy.userContext.enabled", true);
  pref("privacy.userContext.ui.enabled", true);
#else
  pref("privacy.userContext.enabled", false);
  pref("privacy.userContext.ui.enabled", false);
#endif
pref("privacy.userContext.extension", "");
// allows user to open container menu on a left click instead of a new
// tab in the default container
pref("privacy.userContext.newTabContainerOnLeftClick.enabled", false);

// Start the browser in e10s mode
pref("browser.tabs.remote.autostart", true);
pref("browser.tabs.remote.desktopbehavior", true);

// Run media transport in a separate process?
#ifdef NIGHTLY_BUILD
  pref("media.peerconnection.mtransport_process", true);
#else
  pref("media.peerconnection.mtransport_process", false);
#endif

// For speculatively warming up tabs to improve perceived
// performance while using the async tab switcher.
pref("browser.tabs.remote.warmup.enabled", true);

// Caches tab layers to improve perceived performance
// of tab switches.
pref("browser.tabs.remote.tabCacheSize", 0);

pref("browser.tabs.remote.warmup.maxTabs", 3);
pref("browser.tabs.remote.warmup.unloadDelayMs", 2000);

// For the about:tabcrashed page
pref("browser.tabs.crashReporting.sendReport", true);
pref("browser.tabs.crashReporting.includeURL", false);
pref("browser.tabs.crashReporting.requestEmail", false);
pref("browser.tabs.crashReporting.emailMe", false);
pref("browser.tabs.crashReporting.email", "");

// If true, unprivileged extensions may use experimental APIs on
// nightly and developer edition.
pref("extensions.experiments.enabled", false);

// Causes access on unsafe CPOWs from browser code to throw by default.
pref("dom.ipc.cpows.forbid-unsafe-from-browser", true);

#if defined(XP_WIN)
  // Allows us to deprioritize the processes of background tabs at an OS level
  pref("dom.ipc.processPriorityManager.enabled", true);
#endif

// Don't limit how many nodes we care about on desktop:
pref("reader.parse-node-limit", 0);

// On desktop, we want the URLs to be included here for ease of debugging,
// and because (normally) these errors are not persisted anywhere.
pref("reader.errors.includeURLs", true);

pref("view_source.tab", true);

pref("dom.serviceWorkers.enabled", true);

// Enable Push API.
pref("dom.push.enabled", true);

// These are the thumbnail width/height set in about:newtab.
// If you change this, ENSURE IT IS THE SAME SIZE SET
// by about:newtab. These values are in CSS pixels.
pref("toolkit.pageThumbs.minWidth", 280);
pref("toolkit.pageThumbs.minHeight", 190);

// Enable speech synthesis
pref("media.webspeech.synth.enabled", true);

pref("browser.esedbreader.loglevel", "Error");

pref("browser.laterrun.enabled", false);

pref("dom.ipc.processPrelaunch.enabled", true);

// See comments in bug 1340115 on how we got to these numbers.
pref("browser.migrate.chrome.history.limit", 2000);
pref("browser.migrate.chrome.history.maxAgeInDays", 180);

pref("extensions.pocket.api", "api.getpocket.com");
pref("extensions.pocket.enabled", true);
pref("extensions.pocket.oAuthConsumerKey", "40249-e88c401e1b1f2242d9e441c4");
pref("extensions.pocket.site", "getpocket.com");

// Can be removed once Bug 1618058 is resolved.
pref("signon.generation.confidenceThreshold", "0.75");

pref("signon.management.page.os-auth.enabled", true);
pref("signon.management.page.breach-alerts.enabled", true);
pref("signon.management.page.vulnerable-passwords.enabled", true);
pref("signon.management.page.sort", "name");
// The utm_creative value is appended within the code (specific to the location on
// where it is clicked). Be sure that if these two prefs are updated, that
// the utm_creative param be last.
pref("signon.management.page.mobileAndroidURL", "https://app.adjust.com/6tteyjo?redirect=https%3A%2F%2Fplay.google.com%2Fstore%2Fapps%2Fdetails%3Fid%3Dmozilla.lockbox&utm_campaign=Desktop&utm_adgroup=InProduct&utm_creative=");
pref("signon.management.page.mobileAppleURL", "https://app.adjust.com/6tteyjo?redirect=https%3A%2F%2Fitunes.apple.com%2Fapp%2Fid1314000270%3Fmt%3D8&utm_campaign=Desktop&utm_adgroup=InProduct&utm_creative=");
pref("signon.management.page.breachAlertUrl",
     "https://monitor.firefox.com/breach-details/");
pref("signon.management.page.hideMobileFooter", false);
pref("signon.management.page.showPasswordSyncNotification", true);
pref("signon.passwordEditCapture.enabled", true);
pref("signon.showAutoCompleteFooter", true);
pref("signon.showAutoCompleteImport", "");

// Enable the "Simplify Page" feature in Print Preview. This feature
// is disabled by default in toolkit.
pref("print.use_simplify_page", true);

// Space separated list of URLS that are allowed to send objects (instead of
// only strings) through webchannels. This list is duplicated in mobile/android/app/mobile.js
pref("webchannel.allowObject.urlWhitelist", "https://content.cdn.mozilla.net https://support.mozilla.org https://install.mozilla.org");

// Whether or not the browser should scan for unsubmitted
// crash reports, and then show a notification for submitting
// those reports.
#ifdef NIGHTLY_BUILD
  pref("browser.crashReports.unsubmittedCheck.enabled", true);
#else
  pref("browser.crashReports.unsubmittedCheck.enabled", false);
#endif

// chancesUntilSuppress is how many times we'll show the unsubmitted
// crash report notification across different days and shutdown
// without a user choice before we suppress the notification for
// some number of days.
pref("browser.crashReports.unsubmittedCheck.chancesUntilSuppress", 4);
pref("browser.crashReports.unsubmittedCheck.autoSubmit2", false);

// Preferences for the form autofill system extension
// The truthy values of "extensions.formautofill.available" are "on" and "detect",
// any other value means autofill isn't available.
// "detect" means it's enabled if conditions defined in the extension are met.
#ifdef NIGHTLY_BUILD
  pref("extensions.formautofill.available", "on");
#else
  pref("extensions.formautofill.available", "detect");
#endif
pref("extensions.formautofill.creditCards.available", false);
pref("extensions.formautofill.addresses.enabled", true);
pref("extensions.formautofill.creditCards.enabled", true);
// Pref for shield/heartbeat to recognize users who have used Credit Card
// Autofill. The valid values can be:
// 0: none
// 1: submitted a manually-filled credit card form (but didn't see the doorhanger
//    because of a duplicate profile in the storage)
// 2: saw the doorhanger
// 3: submitted an autofill'ed credit card form
pref("extensions.formautofill.creditCards.used", 0);
pref("extensions.formautofill.firstTimeUse", true);
pref("extensions.formautofill.heuristics.enabled", true);
// Whether the user enabled the OS re-auth dialog.
pref("extensions.formautofill.reauth.enabled", false);
pref("extensions.formautofill.section.enabled", true);
pref("extensions.formautofill.loglevel", "Warn");

pref("toolkit.osKeyStore.loglevel", "Warn");

#ifdef NIGHTLY_BUILD
  // Comma separated list of countries Form Autofill is available in.
  pref("extensions.formautofill.supportedCountries", "US,CA,DE");
  pref("extensions.formautofill.supportRTL", true);
#else
  pref("extensions.formautofill.supportedCountries", "US");
  pref("extensions.formautofill.supportRTL", false);
#endif

// Whether or not to restore a session with lazy-browser tabs.
pref("browser.sessionstore.restore_tabs_lazily", true);

pref("browser.suppress_first_window_animation", true);

// Preference that allows individual users to disable Screenshots.
pref("extensions.screenshots.disabled", false);
// Preference that allows individual users to leave Screenshots enabled, but
// disable uploading to the server.
pref("extensions.screenshots.upload-disabled", false);

// DoH Rollout: the earliest date of profile creation for which we don't need
// to show the doorhanger. This is when the version of the privacy statement
// that includes DoH went live - Oct 31, 2019. This has to be a string because
// the number is outside the signed 32-bit integer range.
pref("doh-rollout.profileCreationThreshold", "1572476400000");

// URL for Learn More link for browser error logging in preferences
pref("browser.chrome.errorReporter.infoURL",
     "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/nightly-error-collection");

// Normandy client preferences
pref("app.normandy.api_url", "https://normandy.cdn.mozilla.net/api/v1");
pref("app.normandy.dev_mode", false);
pref("app.normandy.enabled", true);
pref("app.normandy.first_run", true);
pref("app.normandy.logging.level", 50); // Warn
pref("app.normandy.run_interval_seconds", 21600); // 6 hours
pref("app.normandy.shieldLearnMoreUrl", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/shield");
pref("app.normandy.last_seen_buildid", "");
pref("app.normandy.onsync_skew_sec", 600);
#ifdef MOZ_DATA_REPORTING
  pref("app.shield.optoutstudies.enabled", true);
#else
  pref("app.shield.optoutstudies.enabled", false);
#endif

// Web apps support
pref("browser.ssb.enabled", false);

// Multi-lingual preferences
#if defined(RELEASE_OR_BETA) && !defined(MOZ_DEV_EDITION)
  pref("intl.multilingual.enabled", true);
  pref("intl.multilingual.downloadEnabled", true);
#else
  pref("intl.multilingual.enabled", false);
  // AMO only serves language packs for release and beta versions.
  pref("intl.multilingual.downloadEnabled", false);
#endif

// Simulate conditions that will happen when the browser
// is running with Fission enabled. This is meant to assist
// development and testing of Fission.
// The current simulated conditions are:
// - Don't propagate events from subframes to JS child actors
pref("fission.frontend.simulate-events", false);
// - Only deliver subframe messages that specifies
//   their destination (using the BrowsingContext id).
pref("fission.frontend.simulate-messages", false);

// Coverage ping is disabled by default.
pref("toolkit.coverage.enabled", false);
pref("toolkit.coverage.endpoint.base", "https://coverage.mozilla.org");

// Discovery prefs
pref("browser.discovery.enabled", true);
pref("browser.discovery.containers.enabled", true);
pref("browser.discovery.sites", "addons.mozilla.org");

pref("browser.engagement.recent_visited_origins.expiry", 86400); // 24 * 60 * 60 (24 hours in seconds)

pref("browser.aboutConfig.showWarning", true);

pref("browser.toolbars.keyboard_navigation", true);

// Prefs to control the Firefox Account toolbar menu.
// This pref will surface existing Firefox Account information
// as a button next to the hamburger menu. It allows
// quick access to sign-in and manage your Firefox Account.
pref("identity.fxaccounts.toolbar.enabled", true);
pref("identity.fxaccounts.toolbar.accessed", false);

// Prefs for different services supported by Firefox Account
pref("identity.fxaccounts.service.sendLoginUrl", "https://send.firefox.com/login/");
pref("identity.fxaccounts.service.monitorLoginUrl", "https://monitor.firefox.com/");

// Check bundled omni JARs for corruption.
pref("corroborator.enabled", true);

// Toolbox preferences
pref("devtools.toolbox.footer.height", 250);
pref("devtools.toolbox.sidebar.width", 500);
pref("devtools.toolbox.host", "bottom");
pref("devtools.toolbox.previousHost", "right");
pref("devtools.toolbox.selectedTool", "inspector");
pref("devtools.toolbox.sideEnabled", true);
pref("devtools.toolbox.zoomValue", "1");
pref("devtools.toolbox.splitconsoleEnabled", false);
pref("devtools.toolbox.splitconsoleHeight", 100);
pref("devtools.toolbox.tabsOrder", "");

// The fission pref for enabling the "Multiprocess Browser Toolbox", which will
// make it possible to debug anything in Firefox (See Bug 1570639 for more
// information).
#if defined(NIGHTLY_BUILD)
pref("devtools.browsertoolbox.fission", true);
#else
pref("devtools.browsertoolbox.fission", false);
#endif

// The fission pref for enabling Fission frame debugging directly from the
// regular web/content toolbox.
// ⚠ This is a work in progress. Expect weirdness when the pref is enabled. ⚠
pref("devtools.contenttoolbox.fission", false);

// This pref is also related to fission, but not only. It allows the toolbox
// to stay open even if the debugged tab switches to another process.
// It can happen between two documents, one running in the parent process like
// about:sessionrestore and another one running in the content process like
// any web page. Or between two distinct domain when running with fission turned
// on. See bug 1565263.
// ⚠ This is a work in progress. Expect weirdness when the pref is flipped on ⚠
pref("devtools.target-switching.enabled", false);

// Toolbox Button preferences
pref("devtools.command-button-pick.enabled", true);
pref("devtools.command-button-frames.enabled", true);
pref("devtools.command-button-splitconsole.enabled", true);
pref("devtools.command-button-paintflashing.enabled", false);
pref("devtools.command-button-responsive.enabled", true);
pref("devtools.command-button-screenshot.enabled", false);
pref("devtools.command-button-rulers.enabled", false);
pref("devtools.command-button-measure.enabled", false);
pref("devtools.command-button-noautohide.enabled", false);
#ifndef MOZILLA_OFFICIAL
  pref("devtools.command-button-fission-prefs.enabled", true);
#endif

// Inspector preferences
// Enable the Inspector
pref("devtools.inspector.enabled", true);
// What was the last active sidebar in the inspector
pref("devtools.inspector.activeSidebar", "layoutview");
pref("devtools.inspector.remote", false);

// Enable the 3 pane mode in the inspector
pref("devtools.inspector.three-pane-enabled", true);
// Enable the 3 pane mode in the chrome inspector
pref("devtools.inspector.chrome.three-pane-enabled", false);
// Collapse pseudo-elements by default in the rule-view
pref("devtools.inspector.show_pseudo_elements", false);
// The default size for image preview tooltips in the rule-view/computed-view/markup-view
pref("devtools.inspector.imagePreviewTooltipSize", 300);
// Enable user agent style inspection in rule-view
pref("devtools.inspector.showUserAgentStyles", false);
// Show native anonymous content and user agent shadow roots
pref("devtools.inspector.showAllAnonymousContent", false);
// Enable the new Rules View
pref("devtools.inspector.new-rulesview.enabled", false);
// Enable the compatibility tool in the inspector.
#if defined(NIGHTLY_BUILD) || defined(MOZ_DEV_EDITION)
pref("devtools.inspector.compatibility.enabled", true);
#else
pref("devtools.inspector.compatibility.enabled", false);
#endif
// Enable color scheme simulation in the inspector.
pref("devtools.inspector.color-scheme-simulation.enabled", false);

// Grid highlighter preferences
pref("devtools.gridinspector.gridOutlineMaxColumns", 50);
pref("devtools.gridinspector.gridOutlineMaxRows", 50);
pref("devtools.gridinspector.showGridAreas", false);
pref("devtools.gridinspector.showGridLineNumbers", false);
pref("devtools.gridinspector.showInfiniteLines", false);
// Max number of grid highlighters that can be displayed
pref("devtools.gridinspector.maxHighlighters", 3);

// Compatibility preferences
// Stringified array of target browsers that users investigate.
pref("devtools.inspector.compatibility.target-browsers", "");

// Whether or not the box model panel is opened in the layout view
pref("devtools.layout.boxmodel.opened", true);
// Whether or not the flexbox panel is opened in the layout view
pref("devtools.layout.flexbox.opened", true);
// Whether or not the flexbox container panel is opened in the layout view
pref("devtools.layout.flex-container.opened", true);
// Whether or not the flexbox item panel is opened in the layout view
pref("devtools.layout.flex-item.opened", true);
// Whether or not the grid inspector panel is opened in the layout view
pref("devtools.layout.grid.opened", true);

// Enable hovering Box Model values and jumping to their source CSS rule in the
// rule-view.
#if defined(NIGHTLY_BUILD)
  pref("devtools.layout.boxmodel.highlightProperty", true);
#else
  pref("devtools.layout.boxmodel.highlightProperty", false);
#endif

// By how many times eyedropper will magnify pixels
pref("devtools.eyedropper.zoom", 6);

// Enable to collapse attributes that are too long.
pref("devtools.markup.collapseAttributes", true);
// Length to collapse attributes
pref("devtools.markup.collapseAttributeLength", 120);
// Whether to auto-beautify the HTML on copy.
pref("devtools.markup.beautifyOnCopy", false);
// Whether or not the DOM mutation breakpoints context menu are enabled in the
// markup view.
pref("devtools.markup.mutationBreakpoints.enabled", true);

// DevTools default color unit
pref("devtools.defaultColorUnit", "authored");

// Enable the Memory tools
pref("devtools.memory.enabled", true);

pref("devtools.memory.custom-census-displays", "{}");
pref("devtools.memory.custom-label-displays", "{}");
pref("devtools.memory.custom-tree-map-displays", "{}");

pref("devtools.memory.max-individuals", 1000);
pref("devtools.memory.max-retaining-paths", 10);

// Enable the Performance tools
pref("devtools.performance.enabled", true);

// The default Performance UI settings
pref("devtools.performance.memory.sample-probability", "0.05");
// Can't go higher than this without causing internal allocation overflows while
// serializing the allocations data over the RDP.
pref("devtools.performance.memory.max-log-length", 125000);
pref("devtools.performance.timeline.hidden-markers",
  "[\"Composite\",\"CompositeForwardTransaction\"]");
pref("devtools.performance.profiler.buffer-size", 10000000);
pref("devtools.performance.profiler.sample-frequency-hz", 1000);
pref("devtools.performance.ui.invert-call-tree", true);
pref("devtools.performance.ui.invert-flame-graph", false);
pref("devtools.performance.ui.flatten-tree-recursion", true);
pref("devtools.performance.ui.show-platform-data", false);
pref("devtools.performance.ui.show-idle-blocks", true);
pref("devtools.performance.ui.enable-memory", false);
pref("devtools.performance.ui.enable-allocations", false);
pref("devtools.performance.ui.enable-framerate", true);
pref("devtools.performance.ui.show-jit-optimizations", false);
pref("devtools.performance.ui.show-triggers-for-gc-types",
  "TOO_MUCH_MALLOC ALLOC_TRIGGER LAST_DITCH EAGER_ALLOC_TRIGGER");

// Temporary pref disabling memory flame views
// TODO remove once we have flame charts via bug 1148663
pref("devtools.performance.ui.enable-memory-flame", false);

// Enable experimental options in the UI only in Nightly
#if defined(NIGHTLY_BUILD)
  pref("devtools.performance.ui.experimental", true);
#else
  pref("devtools.performance.ui.experimental", false);
#endif

// The default cache UI setting
pref("devtools.cache.disabled", false);

// The default service workers UI setting
pref("devtools.serviceWorkers.testing.enabled", false);

// Enable the Network Monitor
pref("devtools.netmonitor.enabled", true);

pref("devtools.netmonitor.features.search", true);
pref("devtools.netmonitor.features.requestBlocking", true);

// Enable the Application panel on Nightly
#if defined(NIGHTLY_BUILD) || defined(MOZ_DEV_EDITION)
  pref("devtools.application.enabled", true);
#else
  pref("devtools.application.enabled", false);
#endif

// The default Network Monitor UI settings
pref("devtools.netmonitor.panes-network-details-width", 550);
pref("devtools.netmonitor.panes-network-details-height", 450);
pref("devtools.netmonitor.panes-search-width", 550);
pref("devtools.netmonitor.panes-search-height", 450);
pref("devtools.netmonitor.filters", "[\"all\"]");
pref("devtools.netmonitor.visibleColumns",
    "[\"status\",\"method\",\"domain\",\"file\",\"initiator\",\"type\",\"transferred\",\"contentSize\",\"waterfall\"]"
);
pref("devtools.netmonitor.columnsData",
  '[{"name":"status","minWidth":30,"width":5}, {"name":"method","minWidth":30,"width":5}, {"name":"domain","minWidth":30,"width":10}, {"name":"file","minWidth":30,"width":25}, {"name":"url","minWidth":30,"width":25},{"name":"initiator","minWidth":30,"width":10},{"name":"type","minWidth":30,"width":5},{"name":"transferred","minWidth":30,"width":10},{"name":"contentSize","minWidth":30,"width":5},{"name":"waterfall","minWidth":150,"width":15}]');
pref("devtools.netmonitor.ws.payload-preview-height", 128);
pref("devtools.netmonitor.ws.visibleColumns",
  '["data", "time"]'
);
pref("devtools.netmonitor.ws.displayed-frames.limit", 500);

pref("devtools.netmonitor.response.ui.limit", 10240);

// Save request/response bodies yes/no.
pref("devtools.netmonitor.saveRequestAndResponseBodies", true);

// The default Network monitor HAR export setting
pref("devtools.netmonitor.har.defaultLogDir", "");
pref("devtools.netmonitor.har.defaultFileName", "%hostname_Archive [%date]");
pref("devtools.netmonitor.har.jsonp", false);
pref("devtools.netmonitor.har.jsonpCallback", "");
pref("devtools.netmonitor.har.includeResponseBodies", true);
pref("devtools.netmonitor.har.compress", false);
pref("devtools.netmonitor.har.forceExport", false);
pref("devtools.netmonitor.har.pageLoadedTimeout", 1500);
pref("devtools.netmonitor.har.enableAutoExportToFile", false);

pref("devtools.netmonitor.features.webSockets", true);

// Enable the Storage Inspector
pref("devtools.storage.enabled", true);

// Enable the Style Editor.
pref("devtools.styleeditor.enabled", true);
pref("devtools.styleeditor.autocompletion-enabled", true);
pref("devtools.styleeditor.showMediaSidebar", true);
pref("devtools.styleeditor.mediaSidebarWidth", 238);
pref("devtools.styleeditor.navSidebarWidth", 245);
pref("devtools.styleeditor.transitions", true);

// Screenshot Option Settings.
pref("devtools.screenshot.clipboard.enabled", false);
pref("devtools.screenshot.audio.enabled", true);

// Make sure the DOM panel is hidden by default
pref("devtools.dom.enabled", false);

// Enable the Accessibility panel.
pref("devtools.accessibility.enabled", true);
#if defined(NIGHTLY_BUILD)
  pref("devtools.accessibility.auto-init.enabled", true);
#else
  pref("devtools.accessibility.auto-init.enabled", false);
#endif

// Web console filters
pref("devtools.webconsole.filter.error", true);
pref("devtools.webconsole.filter.warn", true);
pref("devtools.webconsole.filter.info", true);
pref("devtools.webconsole.filter.log", true);
pref("devtools.webconsole.filter.debug", true);
pref("devtools.webconsole.filter.css", false);
pref("devtools.webconsole.filter.net", false);
pref("devtools.webconsole.filter.netxhr", false);

// Webconsole autocomplete preference
pref("devtools.webconsole.input.autocomplete",true);

// Show context selector in console input, in the browser toolbox
#if defined(NIGHTLY_BUILD)
  pref("devtools.webconsole.input.context", true);
#else
  pref("devtools.webconsole.input.context", false);
#endif

// Show context selector in console input, in the content toolbox
pref("devtools.contenttoolbox.webconsole.input.context", false);

// Set to true to eagerly show the results of webconsole terminal evaluations
// when they don't have side effects.
pref("devtools.webconsole.input.eagerEvaluation", true);

// Browser console filters
pref("devtools.browserconsole.filter.error", true);
pref("devtools.browserconsole.filter.warn", true);
pref("devtools.browserconsole.filter.info", true);
pref("devtools.browserconsole.filter.log", true);
pref("devtools.browserconsole.filter.debug", true);
pref("devtools.browserconsole.filter.css", false);
pref("devtools.browserconsole.filter.net", false);
pref("devtools.browserconsole.filter.netxhr", false);

// Max number of inputs to store in web console history.
pref("devtools.webconsole.inputHistoryCount", 300);

// Persistent logging: |true| if you want the relevant tool to keep all of the
// logged messages after reloading the page, |false| if you want the output to
// be cleared each time page navigation happens.
pref("devtools.webconsole.persistlog", false);
pref("devtools.netmonitor.persistlog", false);

// Web Console timestamp: |true| if you want the logs and instructions
// in the Web Console to display a timestamp, or |false| to not display
// any timestamps.
pref("devtools.webconsole.timestampMessages", false);

// Enable the webconsole sidebar toggle in Nightly builds.
#if defined(NIGHTLY_BUILD)
  pref("devtools.webconsole.sidebarToggle", true);
#else
  pref("devtools.webconsole.sidebarToggle", false);
#endif

// Saved editor mode state in the console.
pref("devtools.webconsole.input.editor", false);
pref("devtools.browserconsole.input.editor", false);

// Editor width for webconsole and browserconsole.
pref("devtools.webconsole.input.editorWidth", 0);
pref("devtools.browserconsole.input.editorWidth", 0);

// Display an onboarding UI for the Editor mode.
pref("devtools.webconsole.input.editorOnboarding", true);

// Disable the new performance recording panel by default
pref("devtools.performance.new-panel-enabled", false);

// Enable message grouping in the console, true by default
pref("devtools.webconsole.groupWarningMessages", true);

// Saved state of the Display content messages checkbox in the browser console.
pref("devtools.browserconsole.contentMessages", false);

// Enable client-side mapping service for source maps
pref("devtools.source-map.client-service.enabled", true);

// The number of lines that are displayed in the web console.
pref("devtools.hud.loglimit", 10000);

// The developer tools editor configuration:
// - tabsize: how many spaces to use when a Tab character is displayed.
// - expandtab: expand Tab characters to spaces.
// - keymap: which keymap to use (can be 'default', 'emacs' or 'vim')
// - autoclosebrackets: whether to permit automatic bracket/quote closing.
// - detectindentation: whether to detect the indentation from the file
// - enableCodeFolding: Whether to enable code folding or not.
pref("devtools.editor.tabsize", 2);
pref("devtools.editor.expandtab", true);
pref("devtools.editor.keymap", "default");
pref("devtools.editor.autoclosebrackets", true);
pref("devtools.editor.detectindentation", true);
pref("devtools.editor.enableCodeFolding", true);
pref("devtools.editor.autocomplete", true);

// The angle of the viewport.
pref("devtools.responsive.viewport.angle", 0);
// The width of the viewport.
pref("devtools.responsive.viewport.width", 320);
// The height of the viewport.
pref("devtools.responsive.viewport.height", 480);
// The pixel ratio of the viewport.
pref("devtools.responsive.viewport.pixelRatio", 0);
// Whether or not the viewports are left aligned.
pref("devtools.responsive.leftAlignViewport.enabled", false);
// Whether to reload when touch simulation is toggled
pref("devtools.responsive.reloadConditions.touchSimulation", false);
// Whether to reload when user agent is changed
pref("devtools.responsive.reloadConditions.userAgent", false);
// Whether to show the notification about reloading to apply emulation
pref("devtools.responsive.reloadNotification.enabled", true);
// Whether or not touch simulation is enabled.
pref("devtools.responsive.touchSimulation.enabled", false);
// Whether or not meta viewport is enabled, if and only if touchSimulation
// is also enabled.
pref("devtools.responsive.metaViewport.enabled", true);
// The user agent of the viewport.
pref("devtools.responsive.userAgent", "");

// Show the custom user agent input and browser embedded RDM UI in
// Nightly builds.
#if defined(NIGHTLY_BUILD)
  pref("devtools.responsive.showUserAgentInput", true);
  pref("devtools.responsive.browserUI.enabled", true);
#else
  pref("devtools.responsive.showUserAgentInput", false);
  pref("devtools.responsive.browserUI.enabled", false);
#endif

// Show tab debug targets for This Firefox (on by default for local builds).
#ifdef MOZILLA_OFFICIAL
  pref("devtools.aboutdebugging.local-tab-debugging", false);
#else
  pref("devtools.aboutdebugging.local-tab-debugging", true);
#endif

// Show process debug targets.
pref("devtools.aboutdebugging.process-debugging", true);
// Stringified array of network locations that users can connect to.
pref("devtools.aboutdebugging.network-locations", "[]");
// Debug target pane collapse/expand settings.
pref("devtools.aboutdebugging.collapsibilities.installedExtension", false);
pref("devtools.aboutdebugging.collapsibilities.otherWorker", false);
pref("devtools.aboutdebugging.collapsibilities.serviceWorker", false);
pref("devtools.aboutdebugging.collapsibilities.sharedWorker", false);
pref("devtools.aboutdebugging.collapsibilities.tab", false);
pref("devtools.aboutdebugging.collapsibilities.temporaryExtension", false);

// about:debugging: only show system and hidden extensions in local builds by
// default.
#ifdef MOZILLA_OFFICIAL
  pref("devtools.aboutdebugging.showHiddenAddons", false);
#else
  pref("devtools.aboutdebugging.showHiddenAddons", true);
#endif

// Map top-level await expressions in the console
pref("devtools.debugger.features.map-await-expression", true);

#if defined(NIGHTLY_BUILD) || defined(MOZ_DEV_EDITION)
pref("devtools.debugger.features.async-captured-stacks", true);
pref("devtools.debugger.features.async-live-stacks", false);
#else
pref("devtools.debugger.features.async-live-stacks", true);
pref("devtools.debugger.features.async-captured-stacks", false);
#endif

// Disable autohide for DevTools popups and tooltips.
// This is currently not exposed by any UI to avoid making
// about:devtools-toolbox tabs unusable by mistake.
pref("devtools.popup.disable_autohide", false);

// Visibility switch preference for the WhatsNew panel.
pref("devtools.whatsnew.enabled", true);

// Temporary preference to fully disable the WhatsNew panel on any target.
// Should be removed in https://bugzilla.mozilla.org/show_bug.cgi?id=1596037
pref("devtools.whatsnew.feature-enabled", true);

// FirstStartup service time-out in ms
pref("first-startup.timeout", 30000);

// Enable the default browser agent.
// The agent still runs as scheduled if this pref is disabled,
// but it exits immediately before taking any action.
#ifdef XP_WIN
  pref("default-browser-agent.enabled", true);
#endif
