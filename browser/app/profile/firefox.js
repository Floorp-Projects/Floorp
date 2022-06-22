#filter dumbComments emptyLines substitution

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

pref("extensions.webextOptionalPermissionPrompts", true);
// If enabled, install origin permission verification happens after addons are downloaded.
pref("extensions.postDownloadThirdPartyPrompt", true);

// Preferences for AMO integration
pref("extensions.getAddons.cache.enabled", true);
pref("extensions.getAddons.get.url", "https://services.addons.mozilla.org/api/v4/addons/search/?guid=%IDS%&lang=%LOCALE%");
pref("extensions.getAddons.search.browseURL", "https://addons.mozilla.org/%LOCALE%/firefox/search?q=%TERMS%&platform=%OS%&appver=%VERSION%");
pref("extensions.getAddons.link.url", "https://addons.mozilla.org/%LOCALE%/firefox/");
pref("extensions.getAddons.langpacks.url", "https://services.addons.mozilla.org/api/v4/addons/language-tools/?app=firefox&type=language&appversion=%VERSION%");
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


pref("extensions.webextensions.remote", true);

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

#ifdef NIGHTLY_BUILD
  // Whether to delay popup notifications when an update is available and
  // suppress them when an update is installed and waiting for user to restart.
  // If set to true, these notifications will immediately be shown as banners in
  // the app menu and as badges on the app menu button. Update available
  // notifications will not create popup prompts until a week has passed without
  // the user installing the update. Update restart notifications will not
  // create popup prompts at all. This doesn't affect update notifications
  // triggered by errors/failures or manual install prompts.
  pref("app.update.suppressPrompts", false);
#endif

// If set to true, a message will be displayed in the hamburger menu while
// an update is being downloaded.
pref("app.update.notifyDuringDownload", false);

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

pref("app.update.langpack.enabled", true);

#if defined(MOZ_UPDATE_AGENT)
  pref("app.update.background.loglevel", "error");
  pref("app.update.background.timeoutSec", 600);
  // By default, check for updates when the browser is not running every 7 hours.
  pref("app.update.background.interval", 25200);
#endif

#ifdef XP_MACOSX
  // If set to true, Firefox will automatically restart if it is left running
  // with no browser windows open.
  pref("app.update.noWindowAutoRestart.enabled", true);
  // How long to wait after all browser windows are closed before restarting,
  // in milliseconds. 5 min = 300000 ms
  pref("app.update.noWindowAutoRestart.delayMs", 300000);
#endif

#if defined(MOZ_BACKGROUNDTASKS)
  // The amount of time, in seconds, before background tasks time out and exit.
  // Tasks can override this default (10 minutes).
  pref("toolkit.backgroundtasks.defaultTimeoutSec", 600);
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

// Fixup whitelists, the urlbar won't try to search for these words, but will
// instead consider them valid TLDs. Don't check these directly, use
// Services.uriFixup.isDomainKnown() instead.
pref("browser.fixup.domainwhitelist.localhost", true);
// https://tools.ietf.org/html/rfc2606
pref("browser.fixup.domainsuffixwhitelist.test", true);
pref("browser.fixup.domainsuffixwhitelist.example", true);
pref("browser.fixup.domainsuffixwhitelist.invalid", true);
pref("browser.fixup.domainsuffixwhitelist.localhost", true);
// https://tools.ietf.org/html/draft-wkumari-dnsop-internal-00
pref("browser.fixup.domainsuffixwhitelist.internal", true);
// https://tools.ietf.org/html/rfc6762
pref("browser.fixup.domainsuffixwhitelist.local", true);

// Whether to always go through the DNS server before sending a single word
// search string, that may contain a valid host, to a search engine.
pref("browser.fixup.dns_first_for_single_words", false);

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
// Whether Firefox will show the Compact Mode UIDensity option.
pref("browser.compactmode.show", false);

// At startup, check if we're the default browser and prompt user if not.
pref("browser.shell.checkDefaultBrowser", true);
pref("browser.shell.shortcutFavicons",true);
pref("browser.shell.mostRecentDateSetAsDefault", "");
pref("browser.shell.skipDefaultBrowserCheckOnFirstRun", true);
pref("browser.shell.didSkipDefaultBrowserCheckOnFirstRun", false);
pref("browser.shell.defaultBrowserCheckCount", 0);
#if defined(XP_WIN)
// Attempt to set the default browser on Windows 10 using the UserChoice registry keys,
// before falling back to launching the modern Settings dialog.
pref("browser.shell.setDefaultBrowserUserChoice", true);
// When setting the default browser on Windows 10 using the UserChoice
// registry keys, also try to set Firefox as the default PDF handler.
pref("browser.shell.setDefaultPDFHandler", false);
// When setting Firefox as the default PDF handler (subject to conditions
// above), only set Firefox as the default PDF handler when the existing handler
// is a known browser, and not when existing handler is another PDF handler such
// as Acrobat Reader or Nitro PDF.
pref("browser.shell.setDefaultPDFHandler.onlyReplaceBrowsers", true);
#endif


// 0 = blank, 1 = home (browser.startup.homepage), 2 = last visited page, 3 = resume previous browser session
// The behavior of option 3 is detailed at: http://wiki.mozilla.org/Session_Restore
pref("browser.startup.page",                1);
pref("browser.startup.homepage",            "about:home");
#ifdef NIGHTLY_BUILD
pref("browser.startup.homepage.abouthome_cache.enabled", true);
#else
pref("browser.startup.homepage.abouthome_cache.enabled", false);
#endif
pref("browser.startup.homepage.abouthome_cache.loglevel", "Warn");

// Whether we should skip the homepage when opening the first-run page
pref("browser.startup.firstrunSkipsHomepage", true);

// Whether we should show the session-restore infobar on startup
pref("browser.startup.couldRestoreSession.count", 0);

// Show an about:blank window as early as possible for quick startup feedback.
// Held to nightly on Linux due to bug 1450626.
// Disabled on Mac because the bouncing dock icon already provides feedback.
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) && defined(NIGHTLY_BUILD)
  pref("browser.startup.blankWindow", true);
#else
  pref("browser.startup.blankWindow", false);
#endif

// Show a skeleton UI window prior to loading libxul. Only visible for windows
// users as it is not implemented anywhere else.
#if defined(XP_WIN)
pref("browser.startup.preXulSkeletonUI", true);
#endif

// Show an upgrade dialog on major upgrades.
pref("browser.startup.upgradeDialog.enabled", false);

// Don't create the hidden window during startup on
// platforms that don't always need it (Win/Linux).
pref("toolkit.lazyHiddenWindow", true);

pref("browser.chrome.site_icons", true);
// browser.warnOnQuit == false will override all other possible prompts when quitting or restarting
pref("browser.warnOnQuit", true);

// Whether to warn when quitting when using the shortcut key.
#if defined(XP_WIN)
  pref("browser.warnOnQuitShortcut", false);
#else
  pref("browser.warnOnQuitShortcut", true);
#endif

// TODO bug 1702563: Renable fullscreen autohide by default on macOS.
#ifdef XP_MACOSX
  pref("browser.fullscreen.autohide", false);
#else
  pref("browser.fullscreen.autohide", true);
#endif

pref("browser.overlink-delay", 80);

pref("browser.theme.colorway-closet", false);

// Whether using `ctrl` when hitting return/enter in the URL bar
// (or clicking 'go') should prefix 'www.' and suffix
// browser.fixup.alternate.suffix to the URL bar value prior to
// navigating.
pref("browser.urlbar.ctrlCanonizesURLs", true);

// Whether we announce to screen readers when tab-to-search results are
// inserted.
pref("browser.urlbar.accessibility.tabToSearch.announceResults", true);

// Control autoFill behavior
pref("browser.urlbar.autoFill", true);

// Whether enabling adaptive history autofill.
pref("browser.urlbar.autoFill.adaptiveHistory.enabled", false);

// Minimum char length of the user's search string to enable adaptive history
// autofill.
pref("browser.urlbar.autoFill.adaptiveHistory.minCharsThreshold", 0);

// Whether to warm up network connections for autofill or search results.
pref("browser.urlbar.speculativeConnect.enabled", true);

// Whether bookmarklets should be filtered out of Address Bar matches.
// This is enabled for security reasons, when true it is still possible to
// search for bookmarklets typing "javascript: " followed by the actual query.
pref("browser.urlbar.filter.javascript", true);

// Enable a certain level of urlbar logging to the Browser Console. See Log.jsm.
pref("browser.urlbar.loglevel", "Error");

// the maximum number of results to show in autocomplete when doing richResults
pref("browser.urlbar.maxRichResults", 10);

// The maximum number of historical search results to show.
pref("browser.urlbar.maxHistoricalSearchSuggestions", 2);

// The default behavior for the urlbar can be configured to use any combination
// of the match filters with each additional filter adding more results (union).
pref("browser.urlbar.suggest.bookmark",             true);
pref("browser.urlbar.suggest.history",              true);
pref("browser.urlbar.suggest.openpage",             true);
pref("browser.urlbar.suggest.remotetab",            true);
pref("browser.urlbar.suggest.searches",             true);
pref("browser.urlbar.suggest.topsites",             true);
pref("browser.urlbar.suggest.engines",              true);
pref("browser.urlbar.suggest.calculator",           false);

// When `browser.urlbar.bestMatch.enabled` is true, this controls whether best
// match results are shown in the urlbar. This pref is exposed to the user in
// the UI, and it's sticky so that its user-branch value persists regardless of
// whatever Firefox Suggest experiments or rollouts the user is enrolled in over
// time.
pref("browser.urlbar.suggest.bestmatch", true, sticky);

// Whether non-sponsored quick suggest results are shown in the urlbar. This
// pref is exposed to the user in the UI, and it's sticky so that its
// user-branch value persists regardless of whatever Firefox Suggest scenarios,
// with their various default-branch values, the user is enrolled in over time.
pref("browser.urlbar.suggest.quicksuggest.nonsponsored", false, sticky);

// Whether sponsored quick suggest results are shown in the urlbar. This pref is
// exposed to the user in the UI, and it's sticky so that its user-branch value
// persists regardless of whatever Firefox Suggest scenarios, with their various
// default-branch values, the user is enrolled in over time.
pref("browser.urlbar.suggest.quicksuggest.sponsored", false, sticky);

// Whether data collection is enabled for quick suggest results in the urlbar.
// This pref is exposed to the user in the UI, and it's sticky so that its
// user-branch value persists regardless of whatever Firefox Suggest scenarios,
// with their various default-branch values, the user is enrolled in over time.
pref("browser.urlbar.quicksuggest.dataCollection.enabled", false, sticky);

// Whether the quick suggest feature in the urlbar is enabled.
pref("browser.urlbar.quicksuggest.enabled", false);

// Whether to show the QuickSuggest onboarding dialog.
pref("browser.urlbar.quicksuggest.shouldShowOnboardingDialog", true);

// Show QuickSuggest onboarding dialog on the nth browser restarts.
pref("browser.urlbar.quicksuggest.showOnboardingDialogAfterNRestarts", 0);

// The indexes of the sponsored and non-sponsored quick suggest results within
// the general results group.
pref("browser.urlbar.quicksuggest.sponsoredIndex", -1);
pref("browser.urlbar.quicksuggest.nonSponsoredIndex", -1);

// Whether Remote Settings is enabled as a quick suggest source.
pref("browser.urlbar.quicksuggest.remoteSettings.enabled", true);

// Whether quick suggest results can be shown in position specified in the
// suggestions.
pref("browser.urlbar.quicksuggest.allowPositionInSuggestions", true);

// Whether non-sponsored quick suggest results are subject to impression
// frequency caps.
pref("browser.urlbar.quicksuggest.impressionCaps.nonSponsoredEnabled", false);

// Whether sponsored quick suggest results are subject to impression frequency
// caps.
pref("browser.urlbar.quicksuggest.impressionCaps.sponsoredEnabled", false);

// Whether the usual non-best-match quick suggest results can be blocked. This
// pref is a fallback for the Nimbus variable `quickSuggestBlockingEnabled`.
pref("browser.urlbar.quicksuggest.blockingEnabled", false);

// Whether unit conversion is enabled.
#ifdef NIGHTLY_BUILD
pref("browser.urlbar.unitConversion.enabled", true);
#else
pref("browser.urlbar.unitConversion.enabled", false);
#endif

// Whether to show search suggestions before general results like history and
// bookmarks.
pref("browser.urlbar.showSearchSuggestionsFirst", true);

// As a user privacy measure, don't fetch search suggestions if a pasted string
// is longer than this.
pref("browser.urlbar.maxCharsForSearchSuggestions", 100);

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

// If true, we show tail suggestions when available.
pref("browser.urlbar.richSuggestions.tail", true);

// If true, top sites may include sponsored ones.
pref("browser.urlbar.sponsoredTopSites", false);

// Controls the empty search behavior in Search Mode:
//  0 - Show nothing
//  1 - Show search history
//  2 - Show search and browsing history
pref("browser.urlbar.update2.emptySearchBehavior", 0);

// Whether the urlbar displays one-offs to filter searches to history,
// bookmarks, or tabs.
pref("browser.urlbar.shortcuts.bookmarks", true);
pref("browser.urlbar.shortcuts.tabs", true);
pref("browser.urlbar.shortcuts.history", true);

pref("browser.urlbar.eventTelemetry.enabled", false);

// When we send events to Urlbar extensions, we wait this amount of time in
// milliseconds for them to respond before timing out.
pref("browser.urlbar.extension.timeout", 400);

// Controls when to DNS resolve single word search strings, after they were
// searched for. If the string is resolved as a valid host, show a
// "Did you mean to go to 'host'" prompt.
// 0 - never resolve; 1 - use heuristics (default); 2 - always resolve
pref("browser.urlbar.dnsResolveSingleWordsAfterSearch", 1);

// Whether the results panel should be kept open during IME composition.
// The default value is false because some IME open a picker panel, and we end
// up with two panels on top of each other. Since for now we can't detect that
// we leave this choice to the user, hopefully in the future this can be flipped
// for everyone.
pref("browser.urlbar.keepPanelOpenDuringImeComposition", false);

// Whether Firefox Suggest group labels are shown in the urlbar view.
pref("browser.urlbar.groupLabels.enabled", true);

// Whether Merino is enabled as a quick suggest source in the urlbar.
pref("browser.urlbar.merino.enabled", true);

// The Merino endpoint URL, not including parameters.
pref("browser.urlbar.merino.endpointURL", "https://merino.services.mozilla.com/api/v1/suggest");

// Timeout for Merino fetches (ms).
pref("browser.urlbar.merino.timeoutMs", 200);

// Comma-separated list of providers to request from Merino
pref("browser.urlbar.merino.providers", "");

// Comma-separated list of client variants to send to Merino
pref("browser.urlbar.merino.clientVariants", "");

// Whether the best match feature in the urlbar is enabled.
pref("browser.urlbar.bestMatch.enabled", false);

// Whether best match results can be blocked. This pref is a fallback for the
// Nimbus variable `bestMatchBlockingEnabled`.
pref("browser.urlbar.bestMatch.blockingEnabled", false);

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

// This records whether or not to show the 'Open in system viewer' context menu item when appropriate
pref("browser.download.openInSystemViewerContextMenuItem", true);

// This records whether or not to show the 'Always open...' context menu item when appropriate
pref("browser.download.alwaysOpenInSystemViewerContextMenuItem", true);

// Open downloaded file types internally for the given types.
// This is a comma-separated list, the empty string ("") means no types are
// viewable internally.
pref("browser.download.viewableInternally.enabledTypes", "xml,svg,webp,avif,jxl");


// This controls whether the button is automatically shown/hidden depending
// on whether there are downloads to show.
pref("browser.download.autohideButton", true);

// Controls whether to open the downloads panel every time a download begins.
// The first download ever run in a new profile will still open the panel.
pref("browser.download.alwaysOpenPanel", true);

// Determines the behavior of the "Delete" item in the downloads context menu.
// Valid values are 0, 1, and 2.
//   0 - Don't remove the download from session list or history.
//   1 - Remove the download from session list, but not history.
//   2 - Remove the download from both session list and history.
pref("browser.download.clearHistoryOnDelete", 0);

#ifndef XP_MACOSX
  pref("browser.helperApps.deleteTempFileOnExit", true);
#endif

// This controls the visibility of the radio button in the
// Unknown Content Type (Helper App) dialog that will open
// the content in the browser for PDF and for other
// Viewable Internally types
// (see browser.download.viewableInternally.enabledTypes)
pref("browser.helperApps.showOpenOptionForPdfJS", true);
pref("browser.helperApps.showOpenOptionForViewableInternally", true);

// search engine removal URL
pref("browser.search.searchEngineRemoval", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/search-engine-removal");

// search engines URL
pref("browser.search.searchEnginesURL",      "https://addons.mozilla.org/%LOCALE%/firefox/search-engines/");

// search bar results always open in a new tab
pref("browser.search.openintab", false);

// context menu searches open in the foreground
pref("browser.search.context.loadInBackground", false);

// comma separated list of of engines to hide in the search panel.
pref("browser.search.hiddenOneOffs", "");

// Mirrors whether the search-container widget is in the navigation toolbar.
pref("browser.search.widget.inNavBar", false);

// Enables display of the options for the user using a separate default search
// engine in private browsing mode.
pref("browser.search.separatePrivateDefault.ui.enabled", false);
// The maximum amount of times the private default banner is shown.
pref("browser.search.separatePrivateDefault.ui.banner.max", 0);

// Enables the display of the Mozilla VPN banner in private browsing windows
pref("browser.privatebrowsing.vpnpromourl", "https://vpn.mozilla.org/?utm_source=firefox-browser&utm_medium=firefox-%CHANNEL%-browser&utm_campaign=private-browsing-vpn-link");

// Whether the user is opted-in to privacy segmentation.
pref("browser.privacySegmentation.enabled", false);

// Use dark theme variant for PBM windows. This is only supported if the theme
// sets darkTheme data.
pref("browser.theme.dark-private-windows", false);

// Controls visibility of the privacy segmentation preferences section.
pref("browser.privacySegmentation.preferences.show", false);

// Suffix for the SUMO learn more link for the privacy segmentation checkbox.
pref("browser.privacySegmentation.preferences.learnMoreURLSuffix", "data-features");

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
pref("browser.tabs.warnOnClose", false);
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
// Users running in any of the following language codes will have the
// secondary text on tabs hidden due to size constraints and readability
// of the text at small font sizes.
pref("browser.tabs.secondaryTextUnsupportedLocales", "ar,bn,bo,ckb,fa,gu,he,hi,ja,km,kn,ko,lo,mr,my,ne,pa,si,ta,te,th,ur,zh");

//Control the visibility of Tab Manager Menu.
pref("browser.tabs.tabmanager.enabled", false);

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
pref("browser.tabs.remote.separatePrivilegedContentProcess", true);

#if defined(NIGHTLY_BUILD) && !defined(MOZ_ASAN)
  // This pref will cause assertions when a remoteType triggers a process switch
  // to a new remoteType it should not be able to trigger.
  pref("browser.tabs.remote.enforceRemoteTypeRestrictions", true);
#endif

// Pref to control whether we use a separate privileged content process
// for certain mozilla webpages (which are listed in the pref
// browser.tabs.remote.separatedMozillaDomains).
pref("browser.tabs.remote.separatePrivilegedMozillaWebContentProcess", true);

#ifdef NIGHTLY_BUILD
pref("browser.tabs.tooltipsShowPidAndActiveness", true);
#else
pref("browser.tabs.tooltipsShowPidAndActiveness", false);
#endif

pref("browser.tabs.firefox-view", false);

// allow_eval_* is enabled on Firefox Desktop only at this
// point in time
pref("security.allow_eval_with_system_principal", false);
pref("security.allow_eval_in_parent_process", false);

pref("security.allow_parent_unrestricted_js_loads", false);

// Unload tabs when available memory is running low
pref("browser.tabs.unloadOnLowMemory", true);

// Tab Unloader does not unload tabs whose last inactive period is longer than
// this value (in milliseconds).
pref("browser.tabs.min_inactive_duration_before_unload", 600000);

#if defined(XP_MACOSX)
  // During low memory periods, poll with this frequency (milliseconds)
  // until memory is no longer low. Changes to the pref take effect immediately.
  // Browser restart not required. Chosen to be consistent with the windows
  // implementation, but otherwise the 10s value is arbitrary.
  pref("browser.lowMemoryPollingIntervalMS", 10000);

  // Pref to control the reponse taken on macOS when the OS is under memory
  // pressure. Changes to the pref take effect immediately. Browser restart not
  // required. The pref value is a bitmask:
  // 0x0: No response (other than recording for telemetry, crash reporting)
  // 0x1: Use the tab unloading feature to reduce memory use. Requires that
  //      the above "browser.tabs.unloadOnLowMemory" pref be set to true for tab
  //      unloading to occur.
  // 0x2: Issue the internal "memory-pressure" notification to reduce memory use
  // 0x3: Both 0x1 and 0x2.
  #if defined(NIGHTLY_BUILD)
  pref("browser.lowMemoryResponseMask", 3);
  #else
  pref("browser.lowMemoryResponseMask", 0);
  #endif

  // Controls which macOS memory-pressure level triggers the browser low memory
  // response. Changes to the pref take effect immediately. Browser restart not
  // required. By default, use the "critical" level as that occurs after "warn"
  // and we only want to trigger the low memory reponse when necessary.
  // The macOS system memory-pressure level is either none, "warn", or
  // "critical". The OS notifies the browser when the level changes. A false
  // value for the pref indicates the low memory response should occur when
  // reaching the "critical" level. A true value indicates the response should
  // occur when reaching the "warn" level.
  pref("browser.lowMemoryResponseOnWarn", false);
#endif

pref("browser.ctrlTab.sortByRecentlyUsed", false);

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

// Where new bookmarks go by default.
// Use PlacesUIUtils.defaultParentGuid to read this; do NOT read the pref
// directly.
// The value is one of:
// - a bookmarks guid
// - "toolbar", "menu" or "unfiled" for those folders.
// If we use the pref but the value isn't any of these, we'll fall back to
// the bookmarks toolbar as a default.
pref("browser.bookmarks.defaultLocation", "toolbar");

// Scripts & Windows prefs
pref("dom.disable_open_during_load",              true);

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
pref("browser.gesture.pinch.latched", false);
pref("browser.gesture.pinch.threshold", 25);
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
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  pref("browser.gesture.tap", "cmd_fullZoomReset");
#else
  pref("browser.gesture.tap", "");
#endif

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

// Prefer HTML5 video over Flash content, and don't
// load plugin instances with no src declared.
// These prefs are documented in details on all.js.
// With the "follow-ctp" setting, this will only
// apply to users that have plugin.state.flash = 1.
pref("plugins.favorfallback.mode", "follow-ctp");
pref("plugins.favorfallback.rules", "nosrc,video");

// Toggling Search bar on and off in about:preferences
pref("browser.preferences.search", true);
#if defined(NIGHTLY_BUILD)
pref("browser.preferences.experimental", true);
#else
pref("browser.preferences.experimental", false);
#endif
pref("browser.preferences.moreFromMozilla", true);
pref("browser.preferences.experimental.hidden", false);
pref("browser.preferences.defaultPerformanceSettings.enabled", true);

pref("browser.proton.toolbar.version", 0);

// Backspace and Shift+Backspace behavior
// 0 goes Back/Forward
// 1 act like PgUp/PgDown
// 2 and other values, nothing
pref("browser.backspace_action", 2);

pref("intl.regional_prefs.use_os_locales", false);

// this will automatically enable inline spellchecking (if it is available) for
// editable elements in HTML
// 0 = spellcheck nothing
// 1 = check multi-line controls [default]
// 2 = check multi/single line controls
pref("layout.spellcheckDefault", 1);

pref("browser.send_pings", false);

pref("browser.geolocation.warning.infoURL", "https://www.mozilla.org/%LOCALE%/firefox/geolocation/");
pref("browser.xr.warning.infoURL", "https://www.mozilla.org/%LOCALE%/firefox/xr/");

pref("browser.sessionstore.resume_from_crash", true);
pref("browser.sessionstore.resume_session_once", false);
pref("browser.sessionstore.resuming_after_os_restart", false);

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
// Forget closed windows/tabs after two weeks
pref("browser.sessionstore.cleanup.forget_closed_after", 1209600000);

// Don't quit the browser when Ctrl + Q is pressed.
pref("browser.quitShortcut.disabled", false);

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

// Clear data by base domain (including partitioned storage) when the user
// selects "Forget About This Site".
pref("places.forgetThisSite.clearByBaseDomain", true);

// Whether to warm up network connections for places: menus and places: toolbar.
pref("browser.places.speculativeConnect.enabled", true);

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
pref("app.feedback.baseURL", "https://ideas.mozilla.org/");

// Name of alternate about: page for certificate errors (when undefined, defaults to about:neterror)
pref("security.alternate_certificate_error_page", "certerror");

pref("security.certerrors.recordEventTelemetry", true);
pref("security.certerrors.permanentOverride", true);
pref("security.certerrors.mitm.priming.enabled", true);
pref("security.certerrors.mitm.priming.endpoint", "https://mitmdetection.services.mozilla.com/");
pref("security.certerrors.mitm.auto_enable_enterprise_roots", true);

// Whether the bookmark panel should be shown when bookmarking a page.
pref("browser.bookmarks.editDialog.showForNewBookmarks", true);

// Don't try to alter this pref, it'll be reset the next time you use the
// bookmarking dialog
pref("browser.bookmarks.editDialog.firstEditField", "namePicker");

// The number of recently selected folders in the edit bookmarks dialog.
pref("browser.bookmarks.editDialog.maxRecentFolders", 7);

// By default the Edit Bookmark dialog is instant-apply. This feature pref will allow to
// just save on Accept, once the project is complete.
pref("browser.bookmarks.editDialog.delayedApply.enabled", false);

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  // This controls the strength of the Windows content process sandbox for
  // testing purposes. This will require a restart.
  // On windows these levels are:
  // See - security/sandbox/win/src/sandboxbroker/sandboxBroker.cpp
  // SetSecurityLevelForContentProcess() for what the different settings mean.
  pref("security.sandbox.content.level", 6);

  // This controls the strength of the Windows GPU process sandbox.  Changes
  // will require restart.
  // For information on what the level number means, see
  // SetSecurityLevelForGPUProcess() in
  // security/sandbox/win/src/sandboxbroker/sandboxBroker.cpp
  pref("security.sandbox.gpu.level", 0);
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

  // Disconnect content processes from the window server. Depends on
  // out-of-process WebGL and non-native theming. i.e., both in-process WebGL
  // and native theming depend on content processes having a connection to the
  // window server. Window server disconnection is automatically disabled (and
  // this pref overridden) if OOP WebGL is disabled. OOP WebGL is disabled
  // for some tests.
  pref("security.sandbox.content.mac.disconnect-windowserver", true);
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
pref("services.sync.prefs.sync.browser.ctrlTab.sortByRecentlyUsed", true);
pref("services.sync.prefs.sync.browser.discovery.enabled", true);
pref("services.sync.prefs.sync.browser.download.useDownloadDir", true);
pref("services.sync.prefs.sync.browser.formfill.enable", true);
pref("services.sync.prefs.sync.browser.link.open_newwindow", true);
pref("services.sync.prefs.sync.browser.menu.showViewImageInfo", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.asrouter.userprefs.cfr.addons", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.showSearch", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.showSponsored", true);
pref("services.sync.prefs.sync.browser.newtabpage.activity-stream.showSponsoredTopSites", true);
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
pref("services.sync.prefs.sync.browser.safebrowsing.downloads.enabled", true);
pref("services.sync.prefs.sync.browser.safebrowsing.downloads.remote.block_potentially_unwanted", true);
pref("services.sync.prefs.sync.browser.safebrowsing.malware.enabled", true);
pref("services.sync.prefs.sync.browser.safebrowsing.phishing.enabled", true);
pref("services.sync.prefs.sync.browser.search.update", true);
pref("services.sync.prefs.sync.browser.search.widget.inNavBar", true);
pref("services.sync.prefs.sync.browser.startup.homepage", true);
pref("services.sync.prefs.sync.browser.startup.page", true);
pref("services.sync.prefs.sync.browser.tabs.loadInBackground", true);
pref("services.sync.prefs.sync.browser.tabs.warnOnClose", true);
pref("services.sync.prefs.sync.browser.tabs.warnOnOpen", true);
pref("services.sync.prefs.sync.browser.taskbar.previews.enable", true);
pref("services.sync.prefs.sync.browser.urlbar.maxRichResults", true);
pref("services.sync.prefs.sync.browser.urlbar.showSearchSuggestionsFirst", true);
pref("services.sync.prefs.sync.browser.urlbar.suggest.bookmark", true);
pref("services.sync.prefs.sync.browser.urlbar.suggest.history", true);
pref("services.sync.prefs.sync.browser.urlbar.suggest.openpage", true);
pref("services.sync.prefs.sync.browser.urlbar.suggest.searches", true);
pref("services.sync.prefs.sync.browser.urlbar.suggest.topsites", true);
pref("services.sync.prefs.sync.browser.urlbar.suggest.engines", true);
pref("services.sync.prefs.sync.dom.disable_open_during_load", true);
pref("services.sync.prefs.sync.dom.disable_window_flip", true);
pref("services.sync.prefs.sync.dom.disable_window_move_resize", true);
pref("services.sync.prefs.sync.dom.event.contextmenu.enabled", true);
pref("services.sync.prefs.sync.dom.security.https_only_mode", true);
pref("services.sync.prefs.sync.dom.security.https_only_mode_ever_enabled", true);
pref("services.sync.prefs.sync.dom.security.https_only_mode_ever_enabled_pbm", true);
pref("services.sync.prefs.sync.dom.security.https_only_mode_pbm", true);
pref("services.sync.prefs.sync.extensions.update.enabled", true);
pref("services.sync.prefs.sync.extensions.activeThemeID", true);
pref("services.sync.prefs.sync.general.autoScroll", true);
pref("services.sync.prefs.sync.general.smoothScroll", true);
pref("services.sync.prefs.sync.intl.accept_languages", true);
pref("services.sync.prefs.sync.intl.regional_prefs.use_os_locales", true);
pref("services.sync.prefs.sync.layout.spellcheckDefault", true);
pref("services.sync.prefs.sync.media.autoplay.default", true);
pref("services.sync.prefs.sync.media.eme.enabled", true);
pref("services.sync.prefs.sync.media.videocontrols.picture-in-picture.video-toggle.enabled", true);
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
pref("services.sync.prefs.sync.privacy.sanitize.sanitizeOnShutdown", true);
pref("services.sync.prefs.sync.privacy.trackingprotection.enabled", true);
pref("services.sync.prefs.sync.privacy.trackingprotection.cryptomining.enabled", true);
pref("services.sync.prefs.sync.privacy.trackingprotection.fingerprinting.enabled", true);
pref("services.sync.prefs.sync.privacy.trackingprotection.pbmode.enabled", true);
// We do not sync `privacy.resistFingerprinting` by default as it's an undocumented,
// not-recommended footgun - see bug 1763278 for more.
pref("services.sync.prefs.sync.privacy.reduceTimerPrecision", true);
pref("services.sync.prefs.sync.privacy.resistFingerprinting.reduceTimerPrecision.microseconds", true);
pref("services.sync.prefs.sync.privacy.resistFingerprinting.reduceTimerPrecision.jitter", true);
pref("services.sync.prefs.sync.privacy.userContext.enabled", true);
pref("services.sync.prefs.sync.privacy.userContext.newTabContainerOnLeftClick.enabled", true);
pref("services.sync.prefs.sync.security.default_personal_cert", true);
pref("services.sync.prefs.sync.services.sync.syncedTabs.showRemoteIcons", true);
pref("services.sync.prefs.sync.signon.autofillForms", true);
pref("services.sync.prefs.sync.signon.generation.enabled", true);
pref("services.sync.prefs.sync.signon.management.page.breach-alerts.enabled", true);
pref("services.sync.prefs.sync.signon.rememberSignons", true);
pref("services.sync.prefs.sync.spellchecker.dictionary", true);
pref("services.sync.prefs.sync.ui.osk.enabled", true);

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

// A preference (in milliseconds) controlling if we sync after a tab change and
// how long to delay before we schedule the sync
// Anything <= 0 means disabled
pref("services.sync.syncedTabs.syncDelayAfterTabChange", 0);

// Whether the character encoding menu is under the main Firefox button. This
// preference is a string so that localizers can alter it.
pref("browser.menu.showCharacterEncoding", "chrome://browser/locale/browser.properties");

// Whether prompts should be content modal (1) tab modal (2) or window modal(3) by default
// This is a fallback value for when prompt callers do not specify a modalType.
pref("prompts.defaultModalType", 3);

pref("browser.topsites.useRemoteSetting", true);
// Fetch sponsored Top Sites from Mozilla Tiles Service (Contile)
pref("browser.topsites.contile.enabled", true);
pref("browser.topsites.contile.endpoint", "https://contile.services.mozilla.com/v1/tiles");

// The base URL for the Quick Suggest anonymizing proxy. To make a request to
// the proxy, include a campaign ID in the path.
pref("browser.partnerlink.attributionURL", "https://topsites.services.mozilla.com/cid/");
pref("browser.partnerlink.campaign.topsites", "amzn_2020_a1");

// Whether to show tab level system prompts opened via nsIPrompt(Service) as
// SubDialogs in the TabDialogBox (true) or as TabModalPrompt in the
// TabModalPromptBox (false).
pref("prompts.tabChromePromptSubDialog", true);

// Whether to show the dialogs opened at the content level, such as
// alert() or prompt(), using a SubDialogManager in the TabDialogBox.
pref("prompts.contentPromptSubDialog", true);

// Whether to show window-modal dialogs opened for browser windows
// in a SubDialog inside their parent, instead of an OS level window.
pref("prompts.windowPromptSubDialog", true);

// Activates preloading of the new tab url.
pref("browser.newtab.preload", true);

pref("browser.newtabpage.activity-stream.newNewtabExperience.colors", "#0090ED,#FF4F5F,#2AC3A2,#FF7139,#A172FF,#FFA437,#FF2A8A");

// Activity Stream prefs that control to which page to redirect
#ifndef RELEASE_OR_BETA
  pref("browser.newtabpage.activity-stream.debug", false);
#endif

// The remote FxA root content URL for the Activity Stream firstrun page.
pref("browser.newtabpage.activity-stream.fxaccounts.endpoint", "https://accounts.firefox.com/");

// The pref that controls if the search shortcuts experiment is on
pref("browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts", true);

// ASRouter provider configuration
pref("browser.newtabpage.activity-stream.asrouter.providers.cfr", "{\"id\":\"cfr\",\"enabled\":true,\"type\":\"remote-settings\",\"bucket\":\"cfr\",\"updateCycleInMs\":3600000}");
pref("browser.newtabpage.activity-stream.asrouter.providers.whats-new-panel", "{\"id\":\"whats-new-panel\",\"enabled\":true,\"type\":\"remote-settings\",\"bucket\":\"whats-new-panel\",\"updateCycleInMs\":3600000}");
pref("browser.newtabpage.activity-stream.asrouter.providers.message-groups", "{\"id\":\"message-groups\",\"enabled\":true,\"type\":\"remote-settings\",\"bucket\":\"message-groups\",\"updateCycleInMs\":3600000}");
// This url, if changed, MUST continue to point to an https url. Pulling arbitrary content to inject into
// this page over http opens us up to a man-in-the-middle attack that we'd rather not face. If you are a downstream
// repackager of this code using an alternate snippet url, please keep your users safe
pref("browser.newtabpage.activity-stream.asrouter.providers.snippets", "{\"id\":\"snippets\",\"enabled\":false,\"type\":\"remote\",\"url\":\"https://snippets.cdn.mozilla.net/%STARTPAGE_VERSION%/%NAME%/%VERSION%/%APPBUILDID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/\",\"updateCycleInMs\":14400000}");
pref("browser.newtabpage.activity-stream.asrouter.providers.messaging-experiments", "{\"id\":\"messaging-experiments\",\"enabled\":true,\"type\":\"remote-experiments\",\"messageGroups\":[\"cfr\",\"aboutwelcome\",\"infobar\",\"spotlight\",\"moments-page\",\"pbNewtab\"],\"updateCycleInMs\":3600000}");

// ASRouter user prefs
pref("browser.newtabpage.activity-stream.asrouter.userprefs.cfr.addons", true);
pref("browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features", true);

// The pref that controls if ASRouter uses the remote fluent files.
// It's enabled by default, but could be disabled to force ASRouter to use the local files.
pref("browser.newtabpage.activity-stream.asrouter.useRemoteL10n", true);

// These prefs control if Discovery Stream is enabled.
pref("browser.newtabpage.activity-stream.discoverystream.enabled", true);
pref("browser.newtabpage.activity-stream.discoverystream.hardcoded-basic-layout", false);
pref("browser.newtabpage.activity-stream.discoverystream.hybridLayout.enabled", false);
pref("browser.newtabpage.activity-stream.discoverystream.hideCardBackground.enabled", false);
pref("browser.newtabpage.activity-stream.discoverystream.fourCardLayout.enabled", false);
pref("browser.newtabpage.activity-stream.discoverystream.loadMore.enabled", false);
pref("browser.newtabpage.activity-stream.discoverystream.lastCardMessage.enabled", false);
pref("browser.newtabpage.activity-stream.discoverystream.newFooterSection.enabled", false);
pref("browser.newtabpage.activity-stream.discoverystream.saveToPocketCard.enabled", false);
pref("browser.newtabpage.activity-stream.discoverystream.hideDescriptions.enabled", false);
pref("browser.newtabpage.activity-stream.discoverystream.compactGrid.enabled", false);
pref("browser.newtabpage.activity-stream.discoverystream.compactImages.enabled", false);
pref("browser.newtabpage.activity-stream.discoverystream.imageGradient.enabled", false);
pref("browser.newtabpage.activity-stream.discoverystream.titleLines", 3);
pref("browser.newtabpage.activity-stream.discoverystream.descLines", 3);
pref("browser.newtabpage.activity-stream.discoverystream.readTime.enabled", true);
pref("browser.newtabpage.activity-stream.discoverystream.newSponsoredLabel.enabled", false);
pref("browser.newtabpage.activity-stream.discoverystream.essentialReadsHeader.enabled", false);
pref("browser.newtabpage.activity-stream.discoverystream.editorsPicksHeader.enabled", false);
pref("browser.newtabpage.activity-stream.discoverystream.spoc-positions", "1,5,7,11,18,20");
pref("browser.newtabpage.activity-stream.discoverystream.widget-positions", "");

pref("browser.newtabpage.activity-stream.discoverystream.spocs-endpoint", "");
pref("browser.newtabpage.activity-stream.discoverystream.spocs-endpoint-query", "");
pref("browser.newtabpage.activity-stream.discoverystream.sponsored-collections.enabled", false);

pref("browser.newtabpage.activity-stream.discoverystream.sendToPocket.enabled", false);

// List of regions that do not get stories, regardless of locale-list-config.
pref("browser.newtabpage.activity-stream.discoverystream.region-stories-block", "FR");
// List of locales that get stories, regardless of region-stories-config.
#ifdef NIGHTLY_BUILD
  pref("browser.newtabpage.activity-stream.discoverystream.locale-list-config", "en-US,en-CA,en-GB");
#else
  pref("browser.newtabpage.activity-stream.discoverystream.locale-list-config", "");
#endif
// List of regions that get stories by default.
pref("browser.newtabpage.activity-stream.discoverystream.region-stories-config", "US,DE,CA,GB,IE,CH,AT,BE,IN");

// List of regions that get spocs by default.
pref("browser.newtabpage.activity-stream.discoverystream.region-spocs-config", "US,CA,DE,GB");
// List of regions that don't get the 7 row layout.
pref("browser.newtabpage.activity-stream.discoverystream.region-basic-config", "");

// Allows Pocket story collections to be dismissed.
pref("browser.newtabpage.activity-stream.discoverystream.isCollectionDismissible", true);
pref("browser.newtabpage.activity-stream.discoverystream.personalization.enabled", true);
// Configurable keys used by personalization.
pref("browser.newtabpage.activity-stream.discoverystream.personalization.modelKeys", "nb_model_arts_and_entertainment, nb_model_autos_and_vehicles, nb_model_beauty_and_fitness, nb_model_blogging_resources_and_services, nb_model_books_and_literature, nb_model_business_and_industrial, nb_model_computers_and_electronics, nb_model_finance, nb_model_food_and_drink, nb_model_games, nb_model_health, nb_model_hobbies_and_leisure, nb_model_home_and_garden, nb_model_internet_and_telecom, nb_model_jobs_and_education, nb_model_law_and_government, nb_model_online_communities, nb_model_people_and_society, nb_model_pets_and_animals, nb_model_real_estate, nb_model_reference, nb_model_science, nb_model_shopping, nb_model_sports, nb_model_travel");
// System pref to allow Pocket stories personalization to be turned on/off.
pref("browser.newtabpage.activity-stream.discoverystream.recs.personalized", false);
// System pref to allow Pocket sponsored content personalization to be turned on/off.
pref("browser.newtabpage.activity-stream.discoverystream.spocs.personalized", true);

// User pref to show stories on newtab (feeds.system.topstories has to be set to true as well)
pref("browser.newtabpage.activity-stream.feeds.section.topstories", true);

// The pref controls if search hand-off is enabled for Activity Stream.
pref("browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar", true);

pref("browser.newtabpage.activity-stream.logowordmark.alwaysVisible", true);

// URLs from the user's history that contain this search param will be hidden
// from the top sites. The value is a string with one of the following forms:
// - "" (empty) - Disable this feature
// - "key" - Search param named "key" with any or no value
// - "key=" - Search param named "key" with no value
// - "key=value" - Search param named "key" with value "value"
pref("browser.newtabpage.activity-stream.hideTopSitesWithSearchParam", "mfadid=adm");

// Used to display triplet cards on newtab
pref("trailhead.firstrun.newtab.triplets", "");
// Separate about welcome
pref("browser.aboutwelcome.enabled", true);
// Used to set multistage welcome UX
pref("browser.aboutwelcome.screens", "");
pref("browser.aboutwelcome.skipFocus", true);
// Used to enable template for MR 2022 Onboarding
pref("browser.aboutwelcome.templateMR", false);

// The pref that controls if the What's New panel is enabled.
pref("browser.messaging-system.whatsNewPanel.enabled", true);

// Experiment Manager
// See Console.jsm LOG_LEVELS for all possible values
pref("messaging-system.log", "warn");
pref("messaging-system.rsexperimentloader.enabled", true);
pref("messaging-system.rsexperimentloader.collection_id", "nimbus-desktop-experiments");
pref("nimbus.debug", false);

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

// Used by pdf.js to know the first time firefox is run with it installed so it
// can become the default pdf viewer.
pref("pdfjs.firstRun", true);
// The values of preferredAction and alwaysAskBeforeHandling before pdf.js
// became the default.
pref("pdfjs.previousHandler.preferredAction", 0);
pref("pdfjs.previousHandler.alwaysAskBeforeHandling", false);

// Try to convert PDFs sent as octet-stream
pref("pdfjs.handleOctetStream", true);

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

// Set Firefox to block autoplay, asking for permission by default.
pref("media.autoplay.default", 1); // 0=Allowed, 1=Blocked, 5=All Blocked

// Block WebAudio from playing automatically.
pref("media.autoplay.block-webaudio", true);

pref("media.videocontrols.picture-in-picture.enabled", true);
pref("media.videocontrols.picture-in-picture.audio-toggle.enabled", true);
pref("media.videocontrols.picture-in-picture.video-toggle.enabled", true);
pref("media.videocontrols.picture-in-picture.video-toggle.visibility-threshold", "1.0");
pref("media.videocontrols.picture-in-picture.keyboard-controls.enabled", true);

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

// Ping Centre Telemetry settings.
pref("browser.ping-centre.telemetry", true);
pref("browser.ping-centre.log", false);

// Enable GMP support in the addon manager.
pref("media.gmp-provider.enabled", true);

#ifdef NIGHTLY_BUILD
// Enable Dynamic First-Party Isolation in Nightly.
pref("network.cookie.cookieBehavior", 5 /* BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN */);
#else
// Enable blocking access to storage from tracking resources by default.
pref("network.cookie.cookieBehavior", 4 /* BEHAVIOR_REJECT_TRACKER */);
#endif

// Whether to show the section in preferences which allows users to opt-in to
// Total Cookie Protection (dFPI) in standard mode.
pref("privacy.restrict3rdpartystorage.rollout.preferences.TCPToggleInStandard", false);

// Target URL for the learn more link of the TCP in standard mode section.
pref("privacy.restrict3rdpartystorage.rollout.preferences.learnMoreURLSuffix", "total-cookie-protection");

// Enable Dynamic First-Party Isolation in the private browsing mode.
pref("network.cookie.cookieBehavior.pbmode", 5 /* BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN */);

// Enable fingerprinting blocking by default for all channels, only on desktop.
pref("privacy.trackingprotection.fingerprinting.enabled", true);

// Enable cryptomining blocking by default for all channels, only on desktop.
pref("privacy.trackingprotection.cryptomining.enabled", true);

pref("browser.contentblocking.database.enabled", true);

pref("dom.storage_access.enabled", true);

// Enable URL query stripping in regular browsing in Nightly.
#ifdef NIGHTLY_BUILD
pref("privacy.query_stripping.enabled", true);
#endif
// Enable URL query stripping in Private Browsing Mode for all desktop channels.
pref("privacy.query_stripping.enabled.pbmode", true);

pref("browser.contentblocking.cryptomining.preferences.ui.enabled", true);
pref("browser.contentblocking.fingerprinting.preferences.ui.enabled", true);
// Enable cookieBehavior = BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN as an option in the custom category ui
pref("browser.contentblocking.reject-and-isolate-cookies.preferences.ui.enabled", true);

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
//   Level 2 Tracking list:
//     "lvl2": Level 2 tracking list enabled
//     "-lvl2": Level 2 tracking list disabled
//   Restrict relaxing default referrer policy:
//     "rp": Restrict relaxing default referrer policy enabled
//     "-rp": Restrict relaxing default referrer policy disabled
//   Restrict relaxing default referrer policy for top navigation:
//     "rpTop": Restrict relaxing default referrer policy enabled
//     "-rpTop": Restrict relaxing default referrer policy disabled
//   OCSP cache partitioning:
//     "ocsp": OCSP cache partitioning enabled
//     "-ocsp": OCSP cache partitioning disabled
//   Query parameter stripping:
//     "qps": Query parameter stripping enabled
//     "-qps": Query parameter stripping disabled
//   Cookie behavior:
//     "cookieBehavior0": cookie behaviour BEHAVIOR_ACCEPT
//     "cookieBehavior1": cookie behaviour BEHAVIOR_REJECT_FOREIGN
//     "cookieBehavior2": cookie behaviour BEHAVIOR_REJECT
//     "cookieBehavior3": cookie behaviour BEHAVIOR_LIMIT_FOREIGN
//     "cookieBehavior4": cookie behaviour BEHAVIOR_REJECT_TRACKER
//     "cookieBehavior5": cookie behaviour BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
//   Cookie behavior for private windows:
//     "cookieBehaviorPBM0": cookie behaviour BEHAVIOR_ACCEPT
//     "cookieBehaviorPBM1": cookie behaviour BEHAVIOR_REJECT_FOREIGN
//     "cookieBehaviorPBM2": cookie behaviour BEHAVIOR_REJECT
//     "cookieBehaviorPBM3": cookie behaviour BEHAVIOR_LIMIT_FOREIGN
//     "cookieBehaviorPBM4": cookie behaviour BEHAVIOR_REJECT_TRACKER
//     "cookieBehaviorPBM5": cookie behaviour BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
// One value from each section must be included in the browser.contentblocking.features.strict pref.
pref("browser.contentblocking.features.strict", "tp,tpPrivate,cookieBehavior5,cookieBehaviorPBM5,cm,fp,stp,lvl2,rp,rpTop,ocsp,qps");

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
pref("browser.contentblocking.report.show_mobile_app", true);

// Locales in which Send to Device emails are supported
// The most recent list of supported locales can be found at https://github.com/mozilla/bedrock/blob/6a08c876f65924651554decc57b849c00874b4e7/bedrock/settings/base.py#L963
pref("browser.send_to_device_locales", "de,en-GB,en-US,es-AR,es-CL,es-ES,es-MX,fr,id,pl,pt-BR,ru,zh-TW");

// Avoid advertising in certain regions. Comma separated string of two letter ISO 3166-1 country codes.
// We're currently blocking all of Ukraine (ua), but would prefer to block just Crimea (ua-43). Currently, the Mozilla Location Service APIs used by Region.jsm only exposes the country, not the subdivision.
pref("browser.vpn_promo.disallowed_regions", "ae,by,cn,cu,iq,ir,kp,om,ru,sd,sy,tm,tr,ua");

// Default to enabling VPN promo messages to be shown when specified and allowed
pref("browser.vpn_promo.enabled", true);
// Only show vpn card to certain regions. Comma separated string of two letter ISO 3166-1 country codes.
// The most recent list of supported countries can be found at https://support.mozilla.org/en-US/kb/mozilla-vpn-countries-available-subscribe
// The full list of supported country codes can also be found at https://github.com/mozilla/bedrock/search?q=VPN_COUNTRY_CODES
pref("browser.contentblocking.report.vpn_regions", "as,at,be,ca,ch,de,es,fi,fr,gb,gg,ie,im,io,it,je,mp,my,nl,nz,pr,se,sg,uk,um,us,vg,vi"
);

// Avoid advertising Focus in certain regions.  Comma separated string of two letter
// ISO 3166-1 country codes.
pref("browser.promo.focus.disallowed_regions", "cn");

// Default to enabling focus promos to be shown where allowed.
pref("browser.promo.focus.enabled", true);

// Comma separated string of mozilla vpn supported platforms.
pref("browser.contentblocking.report.vpn_platforms", "win,mac,linux");
pref("browser.contentblocking.report.hide_vpn_banner", false);
pref("browser.contentblocking.report.vpn_sub_id", "sub_HrfCZF7VPHzZkA");

pref("browser.contentblocking.report.monitor.url", "https://monitor.firefox.com/?entrypoint=protection_report_monitor&utm_source=about-protections");
pref("browser.contentblocking.report.monitor.how_it_works.url", "https://monitor.firefox.com/about");
pref("browser.contentblocking.report.monitor.sign_in_url", "https://monitor.firefox.com/oauth/init?entrypoint=protection_report_monitor&utm_source=about-protections&email=");
pref("browser.contentblocking.report.monitor.preferences_url", "https://monitor.firefox.com/user/preferences");
pref("browser.contentblocking.report.monitor.home_page_url", "https://monitor.firefox.com/user/dashboard");
pref("browser.contentblocking.report.manage_devices.url", "https://accounts.firefox.com/settings/clients");
pref("browser.contentblocking.report.endpoint_url", "https://monitor.firefox.com/user/breach-stats?includeResolved=true");
pref("browser.contentblocking.report.proxy_extension.url", "https://fpn.firefox.com/browser?utm_source=firefox-desktop&utm_medium=referral&utm_campaign=about-protections&utm_content=about-protections");
pref("browser.contentblocking.report.mobile-ios.url", "https://apps.apple.com/app/firefox-private-safe-browser/id989804926");
pref("browser.contentblocking.report.mobile-android.url", "https://play.google.com/store/apps/details?id=org.mozilla.firefox&referrer=utm_source%3Dprotection_report%26utm_content%3Dmobile_promotion");
pref("browser.contentblocking.report.vpn.url", "https://vpn.mozilla.org/?utm_source=firefox-browser&utm_medium=firefox-browser&utm_campaign=about-protections-card");
pref("browser.contentblocking.report.vpn-promo.url", "https://vpn.mozilla.org/?utm_source=firefox-browser&utm_medium=firefox-browser&utm_campaign=about-protections-top-promo");
pref("browser.contentblocking.report.vpn-android.url", "https://play.google.com/store/apps/details?id=org.mozilla.firefox.vpn&referrer=utm_source%3Dfirefox-browser%26utm_medium%3Dfirefox-browser%26utm_campaign%3Dabout-protections-mobile-vpn%26anid%3D--");
pref("browser.contentblocking.report.vpn-ios.url", "https://apps.apple.com/us/app/firefox-private-network-vpn/id1489407738");

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
pref("browser.contentblocking.cfr-milestone.milestones", "[1000, 5000, 10000, 25000, 50000, 100000, 250000, 314159, 500000, 750000, 1000000, 1250000, 1500000, 1750000, 2000000, 2250000, 2500000, 8675309]");

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

#if defined(NIGHTLY_BUILD) || defined(XP_WIN) || defined(XP_MACOSX)
// Set to true to allow the user to silence all notifications when
// sharing the screen. Only shipping on Windows and macOS out to
// release. Enabled for all desktop platforms on Nightly.
pref("privacy.webrtc.allowSilencingNotifications", true);
// Set to true to use the legacy WebRTC global indicator
pref("privacy.webrtc.legacyGlobalIndicator", false);
pref("privacy.webrtc.hideGlobalIndicator", false);
#else
pref("privacy.webrtc.allowSilencingNotifications", false);
pref("privacy.webrtc.legacyGlobalIndicator", true);
#endif

// Set to true to add toggles to the WebRTC indicator for globally
// muting the camera and microphone.
pref("privacy.webrtc.globalMuteToggles", false);

// Set to true to enable a warning displayed when attempting
// to switch tabs in a window that's being shared over WebRTC.
pref("privacy.webrtc.sharedTabWarning", false);

// Defines a grace period after camera or microphone use ends, where permission
// is granted (even past navigation) to this tab + origin + device. This avoids
// re-prompting without the user having to persist permission to the site, in a
// common case of a web conference asking them for the camera in a lobby page,
// before navigating to the actual meeting room page. Doesn't survive tab close.
pref("privacy.webrtc.deviceGracePeriodTimeoutMs", 3600000);

// Start the browser in e10s mode
pref("browser.tabs.remote.autostart", true);
pref("browser.tabs.remote.desktopbehavior", true);

// Run media transport in a separate process?
pref("media.peerconnection.mtransport_process", true);

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

// If true, unprivileged extensions may use experimental APIs on
// nightly and developer edition.
pref("extensions.experiments.enabled", false);

#if defined(XP_WIN)
  // Allows us to deprioritize the processes of background tabs at an OS level
  pref("dom.ipc.processPriorityManager.enabled", true);
#endif

// Don't limit how many nodes we care about on desktop:
pref("reader.parse-node-limit", 0);

// On desktop, we want the URLs to be included here for ease of debugging,
// and because (normally) these errors are not persisted anywhere.
pref("reader.errors.includeURLs", true);

// What version of Pocket CTA to show in Reader Mode (Empty string is no CTA)
pref("reader.pocket.ctaVersion", "");

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

#ifdef FUZZING_SNAPSHOT
pref("dom.ipc.processPrelaunch.enabled", false);
#else
pref("dom.ipc.processPrelaunch.enabled", true);
#endif

// See comments in bug 1340115 on how we got to these numbers.
pref("browser.migrate.chrome.history.limit", 2000);
pref("browser.migrate.chrome.history.maxAgeInDays", 180);
pref("browser.migrate.showBookmarksToolbarAfterMigration", true);

pref("extensions.pocket.api", "api.getpocket.com");
pref("extensions.pocket.enabled", true);
pref("extensions.pocket.oAuthConsumerKey", "40249-e88c401e1b1f2242d9e441c4");
pref("extensions.pocket.site", "getpocket.com");
pref("extensions.pocket.onSaveRecs", true);
pref("extensions.pocket.onSaveRecs.locales", "en-US,en-GB,en-CA");

// Enable Pocket button home panel for non link pages.
pref("extensions.pocket.showHome", true);

// Control what version of the logged out doorhanger is displayed
// Possibilities are: `control`, `control-one-button`, `variant_a`, `variant_b`, `variant_c`
pref("extensions.pocket.loggedOutVariant", "control");

// Enable the new Pocket panels.
pref("extensions.pocket.refresh.layout.enabled", true);

// Just for the new Pocket panels, enables the email signup button.
pref("extensions.pocket.refresh.emailButton.enabled", false);
// Hides the recently saved section in the home panel.
pref("extensions.pocket.refresh.hideRecentSaves.enabled", false);

pref("signon.management.page.fileImport.enabled", false);

#ifdef NIGHTLY_BUILD
pref("signon.management.page.os-auth.enabled", true);
#else
pref("signon.management.page.os-auth.enabled", false);
#endif
pref("signon.management.page.breach-alerts.enabled", true);
pref("signon.management.page.vulnerable-passwords.enabled", true);
pref("signon.management.page.sort", "name");
// The utm_creative value is appended within the code (specific to the location on
// where it is clicked). Be sure that if these two prefs are updated, that
// the utm_creative param be last.
pref("signon.management.page.breachAlertUrl",
     "https://monitor.firefox.com/breach-details/");
pref("signon.passwordEditCapture.enabled", true);
pref("signon.relatedRealms.enabled", false);
pref("signon.showAutoCompleteFooter", true);
pref("signon.showAutoCompleteImport", "import");
pref("signon.suggestImportCount", 3);

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

// Preferences for the form autofill toolkit component.
// Checkbox in sync options for credit card data sync service
pref("services.sync.engine.creditcards.available", true);
// Whether the user enabled the OS re-auth dialog.
pref("extensions.formautofill.reauth.enabled", false);

// Whether or not to restore a session with lazy-browser tabs.
pref("browser.sessionstore.restore_tabs_lazily", true);

pref("browser.suppress_first_window_animation", true);

// Preference that allows individual users to disable Screenshots.
pref("extensions.screenshots.disabled", false);

// Preference that determines whether Screenshots is opened as a dedicated browser component
pref("screenshots.browser.component.enabled", false);

// DoH Rollout: whether to clear the mode value at shutdown.
pref("doh-rollout.clearModeOnShutdown", false);

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

// Multi-lingual preferences:
//  *.enabled - Are langpacks available for the build of Firefox?
//  *.downloadEnabled - Langpacks are allowed to be downloaded from AMO. AMO only serves
//      langpacks for release and beta. Unsupported releases (like Nightly) can be
//      manually tested with the following preference:
//      extensions.getAddons.langpacks.url: https://mock-amo-language-tools.glitch.me/?app=firefox&type=language&appversion=%VERSION%
//  *.liveReload - Switching a langpack will change the language without a restart.
//  *.liveReloadBidirectional - Allows switching when moving between LTR and RTL
//      languages without a full restart.
//  *.aboutWelcome.languageMismatchEnabled - Enables an onboarding menu in about:welcome
//      to allow a user to change their language when there is a language mismatch between
//      the app and browser.
#if defined(RELEASE_OR_BETA) && !defined(MOZ_DEV_EDITION)
  pref("intl.multilingual.enabled", true);
  pref("intl.multilingual.downloadEnabled", true);
  pref("intl.multilingual.liveReload", true);
  pref("intl.multilingual.liveReloadBidirectional", false);
  pref("intl.multilingual.aboutWelcome.languageMismatchEnabled", true);
#else
  pref("intl.multilingual.enabled", false);
  pref("intl.multilingual.downloadEnabled", false);
  pref("intl.multilingual.liveReload", false);
  pref("intl.multilingual.liveReloadBidirectional", false);
  pref("intl.multilingual.aboutWelcome.languageMismatchEnabled", false);
#endif

// Coverage ping is disabled by default.
pref("toolkit.coverage.enabled", false);
pref("toolkit.coverage.endpoint.base", "https://coverage.mozilla.org");

// Discovery prefs
pref("browser.discovery.enabled", true);
pref("browser.discovery.containers.enabled", true);
pref("browser.discovery.sites", "addons.mozilla.org");

pref("browser.engagement.recent_visited_origins.expiry", 86400); // 24 * 60 * 60 (24 hours in seconds)
pref("browser.engagement.downloads-button.has-used", false);
pref("browser.engagement.fxa-toolbar-menu-button.has-used", false);
pref("browser.engagement.home-button.has-used", false);
pref("browser.engagement.sidebar-button.has-used", false);
pref("browser.engagement.library-button.has-used", false);
pref("browser.engagement.ctrlTab.has-used", false);

pref("browser.aboutConfig.showWarning", true);

pref("browser.toolbars.keyboard_navigation", true);

// The visibility of the bookmarks toolbar.
// "newtab": Show on the New Tab Page
// "always": Always show
// "never": Never show
pref("browser.toolbars.bookmarks.visibility", "newtab");

// Visibility of the "Show Other Bookmarks" menuitem in the
// bookmarks toolbar contextmenu.
pref("browser.toolbars.bookmarks.showOtherBookmarks", true);

// Prefs to control the Firefox Account toolbar menu.
// This pref will surface existing Firefox Account information
// as a button next to the hamburger menu. It allows
// quick access to sign-in and manage your Firefox Account.
pref("identity.fxaccounts.toolbar.enabled", true);
pref("identity.fxaccounts.toolbar.accessed", false);

// Prefs for different services supported by Firefox Account
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

// When the Multiprocess Browser Toolbox is enabled, you can configure the scope of it:
// - "everything" will enable debugging absolutely everything in the browser
//   All processes, all documents, all workers, all add-ons.
// - "parent-process" will restrict debugging to the parent process
//   All privileged javascript, documents and workers running in the parent process.
pref("devtools.browsertoolbox.scope", "everything");

// This preference will enable watching top-level targets from the server side.
pref("devtools.target-switching.server.enabled", true);

// In DevTools, create a target for each frame (i.e. not only for top-level document and
// remote frames).
pref("devtools.every-frame-target.enabled", true);

// Controls the hability to debug popups from the same DevTools
// of the original tab the popups are coming from
pref("devtools.popups.debug", false);

// Toolbox Button preferences
pref("devtools.command-button-pick.enabled", true);
pref("devtools.command-button-frames.enabled", true);
pref("devtools.command-button-splitconsole.enabled", true);
pref("devtools.command-button-responsive.enabled", true);
pref("devtools.command-button-screenshot.enabled", false);
pref("devtools.command-button-rulers.enabled", false);
pref("devtools.command-button-measure.enabled", false);
pref("devtools.command-button-noautohide.enabled", false);
pref("devtools.command-button-errorcount.enabled", true);
#ifndef MOZILLA_OFFICIAL
  pref("devtools.command-button-experimental-prefs.enabled", true);
#endif

// Inspector preferences
// Enable the Inspector
pref("devtools.inspector.enabled", true);
// What was the last active sidebar in the inspector
pref("devtools.inspector.selectedSidebar", "layoutview");
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
// Enable the inline CSS compatiblity warning in inspector rule view
pref("devtools.inspector.ruleview.inline-compatibility-warning.enabled", false);
// Enable the compatibility tool in the inspector.
pref("devtools.inspector.compatibility.enabled", true);
// Enable overflow debugging in the inspector.
pref("devtools.overflow.debugging.enabled", true);
// Enable drag to edit properties in the inspector rule view.
pref("devtools.inspector.draggable_properties", true);

// Grid highlighter preferences
pref("devtools.gridinspector.gridOutlineMaxColumns", 50);
pref("devtools.gridinspector.gridOutlineMaxRows", 50);
pref("devtools.gridinspector.showGridAreas", false);
pref("devtools.gridinspector.showGridLineNumbers", false);
pref("devtools.gridinspector.showInfiniteLines", false);
// Max number of grid highlighters that can be displayed
pref("devtools.gridinspector.maxHighlighters", 3);

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

// The default cache UI setting
pref("devtools.cache.disabled", false);

// The default service workers UI setting
pref("devtools.serviceWorkers.testing.enabled", false);

// Enable the Network Monitor
pref("devtools.netmonitor.enabled", true);

pref("devtools.netmonitor.features.search", true);
pref("devtools.netmonitor.features.requestBlocking", true);

// Enable the Application panel
pref("devtools.application.enabled", true);

// Enable the custom formatters feature
// TODO remove once the custom formatters feature is stable (see bug 1734614)
pref("devtools.custom-formatters", false);
// This preference represents the user's choice to enable the custom formatters feature.
// While the preference above will be removed once the feature is stable, this one is menat to stay.
pref("devtools.custom-formatters.enabled", false);

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
pref("devtools.netmonitor.msg.payload-preview-height", 128);
pref("devtools.netmonitor.msg.visibleColumns",
  '["data", "time"]'
);
pref("devtools.netmonitor.msg.displayed-messages.limit", 500);

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

// netmonitor audit
pref("devtools.netmonitor.audits.slow", 500);

// Enable the new Edit and Resend panel
  pref("devtools.netmonitor.features.newEditAndResend", true);

pref("devtools.netmonitor.customRequest", '{}');

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

// Show context selector in console input
pref("devtools.webconsole.input.context", true);

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

// Enable message grouping in the console, true by default
pref("devtools.webconsole.groupWarningMessages", true);

// Saved state of the Display content messages checkbox in the browser console.
pref("devtools.browserconsole.contentMessages", false);

// Enable network monitoring the browser toolbox console/browser console.
pref("devtools.browserconsole.enableNetworkMonitoring", false);

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
// The user agent of the viewport.
pref("devtools.responsive.userAgent", "");

// Show the custom user agent input only in Nightly.
#if defined(NIGHTLY_BUILD)
  pref("devtools.responsive.showUserAgentInput", true);
#else
  pref("devtools.responsive.showUserAgentInput", false);
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

// This relies on javascript.options.asyncstack as well or it has no effect.
pref("devtools.debugger.features.async-captured-stacks", true);
pref("devtools.debugger.features.async-live-stacks", false);

// Disable autohide for DevTools popups and tooltips.
// This is currently not exposed by any UI to avoid making
// about:devtools-toolbox tabs unusable by mistake.
pref("devtools.popup.disable_autohide", false);

// FirstStartup service time-out in ms
pref("first-startup.timeout", 30000);

// Enable the default browser agent.
// The agent still runs as scheduled if this pref is disabled,
// but it exits immediately before taking any action.
#ifdef XP_WIN
  pref("default-browser-agent.enabled", true);
#endif

// Test Prefs that do nothing for testing
#if defined(EARLY_BETA_OR_EARLIER)
  pref("app.normandy.test-prefs.bool", false);
  pref("app.normandy.test-prefs.integer", 0);
  pref("app.normandy.test-prefs.string", "");
#endif

// Shows 'View Image Info' item in the image context menu
#ifdef MOZ_DEV_EDITION
  pref("browser.menu.showViewImageInfo", true);
#else
  pref("browser.menu.showViewImageInfo", false);
#endif

// Mozilla-controlled domains that are allowed to use non-standard
// context properties for SVG images for use in the browser UI. Please
// keep this list short. This preference (and SVG `context-` keyword support)
// are expected to go away once a standardized alternative becomes
// available.
pref("svg.context-properties.content.allowed-domains", "profile.accounts.firefox.com,profile.stage.mozaws.net");

// Preference that allows individual users to disable Firefox Translations.
#ifdef NIGHTLY_BUILD
  pref("extensions.translations.disabled", true);
#endif

// A set of scores for rating the relevancy of snapshots. The suffixes after the
// last decimal are prefixed by `_score` and reference the functions called in
// SnapshotScorer.
pref("browser.snapshots.score.Visit", 1);
pref("browser.snapshots.score.CurrentSession", 1);
pref("browser.snapshots.score.IsUserPersisted", 1);
pref("browser.snapshots.score.IsUsedRemoved", -10);

// A set of weights for the snapshot recommendation sources. The suffixes after
// the last decimal map to the keys of `Snapshots.recommendationSources`.
pref("browser.snapshots.source.CommonReferrer", 3);
pref("browser.snapshots.source.Overlapping", 3);
pref("browser.snapshots.source.TimeOfDay", 3);

// Other preferences affecting snapshots scoring.
pref("browser.snapshots.relevancy.timeOfDayIntervalSeconds", 3600);

// Expiration days for snapshots.
pref("browser.places.snapshots.expiration.days", 210);
// For user managed snapshots we use more than a year, to support yearly tasks.
pref("browser.places.snapshots.expiration.userManaged.days", 420);
