# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## The Enterprise Policies feature is aimed at system administrators
## who want to deploy these settings across several Firefox installations
## all at once. This is traditionally done through the Windows Group Policy
## feature, but the system also supports other forms of deployment.
## These are short descriptions for individual policies, to be displayed
## in the documentation section in about:policies.

policy-3rdparty = Set policies that WebExtensions can access via chrome.storage.managed.

policy-AppAutoUpdate = Enable or disable automatic application update.

policy-AppUpdateURL = Set custom app update URL.

policy-Authentication = Configure integrated authentication for websites that support it.

policy-BlockAboutAddons = Block access to the Add-ons Manager (about:addons).

policy-BlockAboutConfig = Block access to the about:config page.

policy-BlockAboutProfiles = Block access to the about:profiles page.

policy-BlockAboutSupport = Block access to the about:support page.

policy-Bookmarks = Create bookmarks in the Bookmarks toolbar, Bookmarks menu, or a specified folder inside them.

policy-CaptivePortal = Enable or disable captive portal support.

policy-CertificatesDescription = Add certificates or use built-in certificates.

policy-Cookies = Allow or deny websites to set cookies.

policy-DisabledCiphers = Disable ciphers.

policy-DefaultDownloadDirectory = Set the default download directory.

policy-DisableAppUpdate = Prevent the browser from updating.

policy-DisableBuiltinPDFViewer = Disable PDF.js, the built-in PDF viewer in { -brand-short-name }.

policy-DisableDefaultBrowserAgent = Prevent the default browser agent from taking any actions. Only applicable to Windows; other platforms don’t have the agent.

policy-DisableDeveloperTools = Block access to the developer tools.

policy-DisableFeedbackCommands = Disable commands to send feedback from the Help menu (Submit Feedback and Report Deceptive Site).

policy-DisableFirefoxAccounts = Disable { -fxaccount-brand-name } based services, including Sync.

# Firefox Screenshots is the name of the feature, and should not be translated.
policy-DisableFirefoxScreenshots = Disable the Firefox Screenshots feature.

policy-DisableFirefoxStudies = Prevent { -brand-short-name } from running studies.

policy-DisableForgetButton = Prevent access to the Forget button.

policy-DisableFormHistory = Don’t remember search and form history.

policy-DisableMasterPasswordCreation = If true, a master password can’t be created.

policy-DisablePasswordReveal = Do not allow passwords to be revealed in saved logins.

policy-DisablePocket = Disable the feature to save webpages to Pocket.

policy-DisablePrivateBrowsing = Disable Private Browsing.

policy-DisableProfileImport = Disable the menu command to Import data from another browser.

policy-DisableProfileRefresh = Disable the Refresh { -brand-short-name } button in the about:support page.

policy-DisableSafeMode = Disable the feature to restart in Safe Mode. Note: the Shift key to enter Safe Mode can only be disabled on Windows using Group Policy.

policy-DisableSecurityBypass = Prevent the user from bypassing certain security warnings.

policy-DisableSetAsDesktopBackground = Disable the menu command Set as Desktop Background for images.

policy-DisableSystemAddonUpdate = Prevent the browser from installing and updating system add-ons.

policy-DisableTelemetry = Turn off Telemetry.

policy-DisplayBookmarksToolbar = Display the Bookmarks Toolbar by default.

policy-DisplayMenuBar = Display the Menu Bar by default.

policy-DNSOverHTTPS = Configure DNS over HTTPS.

policy-DontCheckDefaultBrowser = Disable check for default browser on startup.

policy-DownloadDirectory = Set and lock the download directory.

# “lock” means that the user won’t be able to change this setting
policy-EnableTrackingProtection = Enable or disable Content Blocking and optionally lock it.

# “lock” means that the user won’t be able to change this setting
policy-EncryptedMediaExtensions = Enable or disable Encrypted Media Extensions and optionally lock it.

# A “locked” extension can’t be disabled or removed by the user. This policy
# takes 3 keys (“Install”, ”Uninstall”, ”Locked”), you can either keep them in
# English or translate them as verbs.
policy-Extensions = Install, uninstall or lock extensions. The Install option takes URLs or paths as parameters. The Uninstall and Locked options take extension IDs.

policy-ExtensionSettings = Manage all aspects of extension installation.

policy-ExtensionUpdate = Enable or disable automatic extension updates.

policy-FirefoxHome = Configure Firefox Home.

policy-FlashPlugin = Allow or deny usage of the Flash plugin.

policy-HardwareAcceleration = If false, turn off hardware acceleration.

# “lock” means that the user won’t be able to change this setting
policy-Homepage = Set and optionally lock the homepage.

policy-InstallAddonsPermission = Allow certain websites to install add-ons.

policy-LegacyProfiles = Disable the feature enforcing a separate profile for each installation.

## Do not translate "SameSite", it's the name of a cookie attribute.

policy-LegacySameSiteCookieBehaviorEnabled = Enable default legacy SameSite cookie behavior setting.

policy-LegacySameSiteCookieBehaviorEnabledForDomainList = Revert to legacy SameSite behavior for cookies on specified sites.

##

policy-LocalFileLinks = Allow specific websites to link to local files.

policy-NetworkPrediction = Enable or disable network prediction (DNS prefetching).

policy-NewTabPage = Enable or disable the New Tab page.

policy-NoDefaultBookmarks = Disable creation of the default bookmarks bundled with { -brand-short-name }, and the Smart Bookmarks (Most Visited, Recent Tags). Note: this policy is only effective if used before the first run of the profile.

policy-OfferToSaveLogins = Enforce the setting to allow { -brand-short-name } to offer to remember saved logins and passwords. Both true and false values are accepted.

policy-OfferToSaveLoginsDefault = Set the default value for allowing { -brand-short-name } to offer to remember saved logins and passwords. Both true and false values are accepted.

policy-OverrideFirstRunPage = Override the first run page. Set this policy to blank if you want to disable the first run page.

policy-OverridePostUpdatePage = Override the post-update “What’s New” page. Set this policy to blank if you want to disable the post-update page.

policy-PasswordManagerEnabled = Enable saving passwords to the password manager.

policy-Permissions2 = Configure permissions for camera, microphone, location, notifications, and autoplay.

policy-PopupBlocking = Allow certain websites to display popups by default.

policy-Preferences = Set and lock the value for a subset of preferences.

policy-PromptForDownloadLocation = Ask where to save files when downloading.

policy-Proxy = Configure proxy settings.

policy-RequestedLocales = Set the list of requested locales for the application in order of preference.

policy-SanitizeOnShutdown2 = Clear navigation data on shutdown.

policy-SearchBar = Set the default location of the search bar. The user is still allowed to customize it.

policy-SearchEngines = Configure search engine settings. This policy is only available on the Extended Support Release (ESR) version.

policy-SearchSuggestEnabled = Enable or disable search suggestions.

# For more information, see https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSS/PKCS11/Module_Installation
policy-SecurityDevices = Install PKCS #11 modules.

policy-SSLVersionMax = Set the maximum SSL version.

policy-SSLVersionMin = Set the minimum SSL version.

policy-SupportMenu = Add a custom support menu item to the help menu.

policy-UserMessaging = Don’t show certain messages to the user.

# “format” refers to the format used for the value of this policy.
policy-WebsiteFilter = Block websites from being visited. See documentation for more details on the format.
