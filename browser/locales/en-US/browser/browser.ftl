# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## The main browser window's title

# These are the default window titles everywhere except macOS. The first two
# attributes are used when the web content opened has no title:
#
# default - "Mozilla Firefox"
# private - "Mozilla Firefox (Private Browsing)"
#
# The last two are for use when there *is* a content title.
# Variables:
#  $content-title (String): the title of the web content.
browser-main-window =
  .data-title-default = { -brand-full-name }
  .data-title-private = { -brand-full-name } (Private Browsing)
  .data-content-title-default = { $content-title } — { -brand-full-name }
  .data-content-title-private = { $content-title } — { -brand-full-name } (Private Browsing)

# These are the default window titles on macOS. The first two are for use when
# there is no content title:
#
# "default" - "Mozilla Firefox"
# "private" - "Mozilla Firefox — (Private Browsing)"
#
# The last two are for use when there *is* a content title.
# Do not use the brand name in the last two attributes, as we do on non-macOS.
#
# Also note the other subtle difference here: we use a `-` to separate the
# brand name from `(Private Browsing)`, which does not happen on other OSes.
#
# Variables:
#  $content-title (String): the title of the web content.
browser-main-window-mac =
  .data-title-default = { -brand-full-name }
  .data-title-private = { -brand-full-name } — (Private Browsing)
  .data-content-title-default = { $content-title }
  .data-content-title-private = { $content-title } — (Private Browsing)

# This gets set as the initial title, and is overridden as soon as we start
# updating the titlebar based on loaded tabs or private browsing state.
# This should match the `data-title-default` attribute in both
# `browser-main-window` and `browser-main-window-mac`.
browser-main-window-title = { -brand-full-name }

##

urlbar-identity-button =
    .aria-label = View site information

## Tooltips for images appearing in the address bar

urlbar-services-notification-anchor =
    .tooltiptext = Open install message panel
urlbar-web-notification-anchor =
    .tooltiptext = Change whether you can receive notifications from the site
urlbar-midi-notification-anchor =
    .tooltiptext = Open MIDI panel
urlbar-eme-notification-anchor =
    .tooltiptext = Manage use of DRM software
urlbar-web-authn-anchor =
    .tooltiptext = Open Web Authentication panel
urlbar-canvas-notification-anchor =
    .tooltiptext = Manage canvas extraction permission
urlbar-web-rtc-share-microphone-notification-anchor =
    .tooltiptext = Manage sharing your microphone with the site
urlbar-default-notification-anchor =
    .tooltiptext = Open message panel
urlbar-geolocation-notification-anchor =
    .tooltiptext = Open location request panel
urlbar-xr-notification-anchor =
    .tooltiptext = Open virtual reality permission panel
urlbar-storage-access-anchor =
    .tooltiptext = Open browsing activity permission panel
urlbar-translate-notification-anchor =
    .tooltiptext = Translate this page
urlbar-web-rtc-share-screen-notification-anchor =
    .tooltiptext = Manage sharing your windows or screen with the site
urlbar-indexed-db-notification-anchor =
    .tooltiptext = Open offline storage message panel
urlbar-password-notification-anchor =
    .tooltiptext = Open save password message panel
urlbar-translated-notification-anchor =
    .tooltiptext = Manage page translation
urlbar-plugins-notification-anchor =
    .tooltiptext = Manage plug-in use
urlbar-web-rtc-share-devices-notification-anchor =
    .tooltiptext = Manage sharing your camera and/or microphone with the site
urlbar-autoplay-notification-anchor =
    .tooltiptext = Open autoplay panel
urlbar-persistent-storage-notification-anchor =
    .tooltiptext = Store data in Persistent Storage
urlbar-addons-notification-anchor =
    .tooltiptext = Open add-on installation message panel
urlbar-tip-help-icon =
    .title = Get help
urlbar-search-tips-confirm = Okay, Got It
# Read out before Urlbar Tip text content so screenreader users know the
# subsequent text is a tip offered by the browser. It should end in a colon or
# localized equivalent.
urlbar-tip-icon-description =
    .alt = Tip:

## Prompts users to use the Urlbar when they open a new tab or visit the
## homepage of their default search engine.
## Variables:
##  $engineName (String): The name of the user's default search engine. e.g. "Google" or "DuckDuckGo".

urlbar-search-tips-onboard = Type less, find more: Search { $engineName } right from your address bar.
urlbar-search-tips-redirect-2 = Start your search in the address bar to see suggestions from { $engineName } and your browsing history.

# Prompts users to use the Urlbar when they are typing in the domain of a
# search engine, e.g. google.com or amazon.com.
urlbar-tabtosearch-onboard = Select this shortcut to find what you need faster.

## Local search mode indicator labels in the urlbar

urlbar-search-mode-bookmarks = Bookmarks
urlbar-search-mode-tabs = Tabs
urlbar-search-mode-history = History

##

urlbar-geolocation-blocked =
    .tooltiptext = You have blocked location information for this website.
urlbar-xr-blocked =
    .tooltiptext = You have blocked virtual reality device access for this website.
urlbar-web-notifications-blocked =
    .tooltiptext = You have blocked notifications for this website.
urlbar-camera-blocked =
    .tooltiptext = You have blocked your camera for this website.
urlbar-microphone-blocked =
    .tooltiptext = You have blocked your microphone for this website.
urlbar-screen-blocked =
    .tooltiptext = You have blocked this website from sharing your screen.
urlbar-persistent-storage-blocked =
    .tooltiptext = You have blocked persistent storage for this website.
urlbar-popup-blocked =
    .tooltiptext = You have blocked pop-ups for this website.
urlbar-autoplay-media-blocked =
    .tooltiptext = You have blocked autoplay media with sound for this website.
urlbar-canvas-blocked =
    .tooltiptext = You have blocked canvas data extraction for this website.
urlbar-midi-blocked =
    .tooltiptext = You have blocked MIDI access for this website.
urlbar-install-blocked =
    .tooltiptext = You have blocked add-on installation for this website.

# Variables
#   $shortcut (String) - A keyboard shortcut for the edit bookmark command.
urlbar-star-edit-bookmark =
    .tooltiptext = Edit this bookmark ({ $shortcut })

# Variables
#   $shortcut (String) - A keyboard shortcut for the add bookmark command.
urlbar-star-add-bookmark =
    .tooltiptext = Bookmark this page ({ $shortcut })

## Page Action Context Menu

page-action-manage-extension =
    .label = Manage Extension…
page-action-remove-extension =
    .label = Remove Extension

## Page Action menu

# Variables
# $tabCount (integer) - Number of tabs selected
page-action-send-tabs-panel =
  .label = { $tabCount ->
      [1] Send Tab to Device
     *[other] Send { $tabCount } Tabs to Device
  }
page-action-send-tabs-urlbar =
  .tooltiptext = { $tabCount ->
      [1] Send Tab to Device
     *[other] Send { $tabCount } Tabs to Device
  }
page-action-copy-url-panel =
  .label = Copy Link
page-action-copy-url-urlbar =
  .tooltiptext = Copy Link
page-action-email-link-panel =
  .label = Email Link…
page-action-email-link-urlbar =
  .tooltiptext = Email Link…
page-action-share-url-panel =
  .label = Share
page-action-share-url-urlbar =
  .tooltiptext = Share
page-action-share-more-panel =
  .label = More…
page-action-send-tab-not-ready =
  .label = Syncing Devices…
# "Pin" is being used as a metaphor for expressing the fact that these tabs
# are "pinned" to the left edge of the tabstrip. Really we just want the
# string to express the idea that this is a lightweight and reversible
# action that keeps your tab where you can reach it easily.
page-action-pin-tab-panel =
  .label = Pin Tab
page-action-pin-tab-urlbar =
  .tooltiptext = Pin Tab
page-action-unpin-tab-panel =
  .label = Unpin Tab
page-action-unpin-tab-urlbar =
  .tooltiptext = Unpin Tab

## Auto-hide Context Menu

full-screen-autohide =
    .label = Hide Toolbars
    .accesskey = H
full-screen-exit =
    .label = Exit Full Screen Mode
    .accesskey = F

## Search Engine selection buttons (one-offs)

# This string prompts the user to use the list of search shortcuts in
# the Urlbar and searchbar.
search-one-offs-with-title = This time, search with:

# This string won't wrap, so if the translated string is longer,
# consider translating it as if it said only "Search Settings".
search-one-offs-change-settings-button =
    .label = Change Search Settings
search-one-offs-change-settings-compact-button =
    .tooltiptext = Change search settings

search-one-offs-context-open-new-tab =
    .label = Search in New Tab
    .accesskey = T
search-one-offs-context-set-as-default =
    .label = Set as Default Search Engine
    .accesskey = D
search-one-offs-context-set-as-default-private =
    .label = Set as Default Search Engine for Private Windows
    .accesskey = P

# Search engine one-off buttons with an @alias shortcut/keyword.
# Variables:
#  $engineName (String): The name of the engine.
#  $alias (String): The @alias shortcut/keyword.
search-one-offs-engine-with-alias =
    .tooltiptext = { $engineName } ({ $alias })

## Local search mode one-off buttons
## Variables:
##  $restrict (String): The restriction token corresponding to the search mode.
##    Restriction tokens are special characters users can type in the urlbar to
##    restrict their searches to certain sources (e.g., "*" to search only
##    bookmarks).

search-one-offs-bookmarks =
    .tooltiptext = Bookmarks ({ $restrict })
search-one-offs-tabs =
    .tooltiptext = Tabs ({ $restrict })
search-one-offs-history =
    .tooltiptext = History ({ $restrict })

## Bookmark Panel

bookmarks-add-bookmark = Add bookmark
bookmarks-edit-bookmark = Edit bookmark
bookmark-panel-cancel =
    .label = Cancel
    .accesskey = C
# Variables:
#  $count (number): number of bookmarks that will be removed
bookmark-panel-remove =
    .label =
        { $count ->
            [1] Remove bookmark
           *[other] Remove { $count } bookmarks
        }
    .accesskey = R
bookmark-panel-show-editor-checkbox =
    .label = Show editor when saving
    .accesskey = S
bookmark-panel-save-button =
    .label = Save

# Width of the bookmark panel.
# Should be large enough to fully display the Done and
# Cancel/Remove Bookmark buttons.
bookmark-panel =
    .style = min-width: 23em

## Identity Panel

# Variables
#  $host (String): the hostname of the site that is being displayed.
identity-site-information = Site information for { $host }
# Variables
#  $host (String): the hostname of the site that is being displayed.
identity-header-security-with-host =
    .title = Connection security for { $host }
identity-connection-not-secure = Connection not secure
identity-connection-secure = Connection secure
identity-connection-internal = This is a secure { -brand-short-name } page.
identity-connection-file = This page is stored on your computer.
identity-extension-page = This page is loaded from an extension.
identity-active-blocked = { -brand-short-name } has blocked parts of this page that are not secure.
identity-custom-root = Connection verified by a certificate issuer that is not recognized by Mozilla.
identity-passive-loaded = Parts of this page are not secure (such as images).
identity-active-loaded = You have disabled protection on this page.
identity-weak-encryption = This page uses weak encryption.
identity-insecure-login-forms = Logins entered on this page could be compromised.

identity-https-only-connection-upgraded = (upgraded to HTTPS)
identity-https-only-label = HTTPS-Only Mode
identity-https-only-dropdown-on =
    .label = On
identity-https-only-dropdown-off =
    .label = Off
identity-https-only-dropdown-off-temporarily =
    .label = Off temporarily
identity-https-only-info-turn-on2 = Turn on HTTPS-Only Mode for this site if you want { -brand-short-name } to upgrade the connection when possible.
identity-https-only-info-turn-off2 = If the page seems broken, you may want to turn off HTTPS-Only Mode for this site to reload using insecure HTTP.
identity-https-only-info-no-upgrade = Unable to upgrade connection from HTTP.

identity-permissions-storage-access-header = Cross-site cookies
identity-permissions-storage-access-hint = These parties can use cross-site cookies and site data while you are on this site.
identity-permissions-storage-access-learn-more = Learn more

identity-permissions-reload-hint = You may need to reload the page for changes to apply.
identity-clear-site-data =
    .label = Clear cookies and site data…
identity-connection-not-secure-security-view = You are not securely connected to this site.
identity-connection-verified = You are securely connected to this site.
identity-ev-owner-label = Certificate issued to:
identity-description-custom-root = Mozilla does not recognize this certificate issuer. It may have been added from your operating system or by an administrator. <label data-l10n-name="link">Learn More</label>
identity-remove-cert-exception =
    .label = Remove Exception
    .accesskey = R
identity-description-insecure = Your connection to this site is not private. Information you submit could be viewed by others (like passwords, messages, credit cards, etc.).
identity-description-insecure-login-forms = The login information you enter on this page is not secure and could be compromised.
identity-description-weak-cipher-intro = Your connection to this website uses weak encryption and is not private.
identity-description-weak-cipher-risk = Other people can view your information or modify the website’s behavior.
identity-description-active-blocked = { -brand-short-name } has blocked parts of this page that are not secure. <label data-l10n-name="link">Learn More</label>
identity-description-passive-loaded = Your connection is not private and information you share with the site could be viewed by others.
identity-description-passive-loaded-insecure = This website contains content that is not secure (such as images). <label data-l10n-name="link">Learn More</label>
identity-description-passive-loaded-mixed = Although { -brand-short-name } has blocked some content, there is still content on the page that is not secure (such as images). <label data-l10n-name="link">Learn More</label>
identity-description-active-loaded = This website contains content that is not secure (such as scripts) and your connection to it is not private.
identity-description-active-loaded-insecure = Information you share with this site could be viewed by others (like passwords, messages, credit cards, etc.).
identity-learn-more =
    .value = Learn More
identity-disable-mixed-content-blocking =
    .label = Disable protection for now
    .accesskey = D
identity-enable-mixed-content-blocking =
    .label = Enable protection
    .accesskey = E
identity-more-info-link-text =
    .label = More Information

## Window controls

browser-window-minimize-button =
    .tooltiptext = Minimize
browser-window-maximize-button =
    .tooltiptext = Maximize
browser-window-restore-down-button =
    .tooltiptext = Restore Down
browser-window-close-button =
    .tooltiptext = Close

## Tab actions

# This label should be written in all capital letters if your locale supports them.
browser-tab-audio-playing2 = PLAYING
# This label should be written in all capital letters if your locale supports them.
browser-tab-audio-muted2 = MUTED
# This label should be written in all capital letters if your locale supports them.
browser-tab-audio-blocked = AUTOPLAY BLOCKED
# This label should be written in all capital letters if your locale supports them.
browser-tab-audio-pip = PICTURE-IN-PICTURE

## These labels should be written in all capital letters if your locale supports them.
## Variables:
##  $count (number): number of affected tabs

browser-tab-mute =
    { $count ->
        [1] MUTE TAB
       *[other] MUTE { $count } TABS
    }

browser-tab-unmute =
    { $count ->
        [1] UNMUTE TAB
       *[other] UNMUTE { $count } TABS
    }

browser-tab-unblock =
    { $count ->
        [1] PLAY TAB
       *[other] PLAY { $count } TABS
    }

## Bookmarks toolbar items

browser-import-button2 =
    .label = Import bookmarks…
    .tooltiptext = Import bookmarks from another browser to { -brand-short-name }.

bookmarks-toolbar-empty-message = For quick access, place your bookmarks here on the bookmarks toolbar. <a data-l10n-name="manage-bookmarks">Manage bookmarks…</a>

## WebRTC Pop-up notifications

popup-select-camera-device =
    .value = Camera:
    .accesskey = C
popup-select-camera-icon =
    .tooltiptext = Camera
popup-select-microphone-device =
    .value = Microphone:
    .accesskey = M
popup-select-microphone-icon =
    .tooltiptext = Microphone
popup-all-windows-shared = All visible windows on your screen will be shared.

popup-screen-sharing-block =
  .label = Block
  .accesskey = B

popup-screen-sharing-always-block =
  .label = Always block
  .accesskey = w

popup-mute-notifications-checkbox = Mute website notifications while sharing

## WebRTC window or screen share tab switch warning

sharing-warning-window = You are sharing { -brand-short-name }. Other people can see when you switch to a new tab.
sharing-warning-screen = You are sharing your entire screen. Other people can see when you switch to a new tab.
sharing-warning-proceed-to-tab =
  .label = Proceed to Tab
sharing-warning-disable-for-session =
  .label = Disable sharing protection for this session

## DevTools F12 popup

enable-devtools-popup-description = To use the F12 shortcut, first open DevTools via the Web Developer menu.

## URL Bar

# This placeholder is used when not in search mode and the user's default search
# engine is unknown.
urlbar-placeholder =
  .placeholder = Search or enter address

# This placeholder is used in search mode with search engines that search the
# entire web.
# Variables
#  $name (String): the name of a search engine that searches the entire Web
#  (e.g. Google).
urlbar-placeholder-search-mode-web-2 =
  .placeholder = Search the Web
  .aria-label = Search with { $name }

# This placeholder is used in search mode with search engines that search a
# specific site (e.g., Amazon).
# Variables
#  $name (String): the name of a search engine that searches a specific site
#  (e.g. Amazon).
urlbar-placeholder-search-mode-other-engine =
  .placeholder = Enter search terms
  .aria-label = Search { $name }

# This placeholder is used when searching bookmarks.
urlbar-placeholder-search-mode-other-bookmarks =
  .placeholder = Enter search terms
  .aria-label = Search bookmarks

# This placeholder is used when searching history.
urlbar-placeholder-search-mode-other-history =
  .placeholder = Enter search terms
  .aria-label = Search history

# This placeholder is used when searching open tabs.
urlbar-placeholder-search-mode-other-tabs =
  .placeholder = Enter search terms
  .aria-label = Search tabs

# Variables
#  $name (String): the name of the user's default search engine
urlbar-placeholder-with-name =
  .placeholder = Search with { $name } or enter address
urlbar-remote-control-notification-anchor =
  .tooltiptext = Browser is under remote control
urlbar-permissions-granted =
  .tooltiptext = You have granted this website additional permissions.
urlbar-switch-to-tab =
  .value = Switch to tab:

# Used to indicate that a selected autocomplete entry is provided by an extension.
urlbar-extension =
  .value = Extension:

urlbar-go-button =
  .tooltiptext = Go to the address in the Location Bar
urlbar-page-action-button =
  .tooltiptext = Page actions

## Action text shown in urlbar results, usually appended after the search
## string or the url, like "result value - action text".

# Used when the private browsing engine differs from the default engine.
# The "with" format was chosen because the search engine name can end with
# "Search", and we would like to avoid strings like "Search MSN Search".
# Variables
#  $engine (String): the name of a search engine
urlbar-result-action-search-in-private-w-engine = Search with { $engine } in a Private Window
# Used when the private browsing engine is the same as the default engine.
urlbar-result-action-search-in-private = Search in a Private Window
# The "with" format was chosen because the search engine name can end with
# "Search", and we would like to avoid strings like "Search MSN Search".
# Variables
#  $engine (String): the name of a search engine
urlbar-result-action-search-w-engine = Search with { $engine }
urlbar-result-action-sponsored = Sponsored
urlbar-result-action-switch-tab = Switch to Tab
urlbar-result-action-visit = Visit
# Directs a user to press the Tab key to perform a search with the specified
# engine.
# Variables
#  $engine (String): the name of a search engine that searches the entire Web
#  (e.g. Google).
urlbar-result-action-before-tabtosearch-web = Press Tab to search with { $engine }
# Directs a user to press the Tab key to perform a search with the specified
# engine.
# Variables
#  $engine (String): the name of a search engine that searches a specific site
#  (e.g. Amazon).
urlbar-result-action-before-tabtosearch-other = Press Tab to search { $engine }
# Variables
#  $engine (String): the name of a search engine that searches the entire Web
#  (e.g. Google).
urlbar-result-action-tabtosearch-web = Search with { $engine } directly from the address bar
# Variables
#  $engine (String): the name of a search engine that searches a specific site
#  (e.g. Amazon).
urlbar-result-action-tabtosearch-other-engine = Search { $engine } directly from the address bar
# Action text for copying to clipboard.
urlbar-result-action-copy-to-clipboard = Copy
# Shows the result of a formula expression being calculated, the last = sign will be shown
# as part of the result (e.g. "= 2").
# Variables
#  $result (String): the string representation for a formula result
urlbar-result-action-calculator-result = = { $result }

## Action text shown in urlbar results, usually appended after the search
## string or the url, like "result value - action text".
## In these actions "Search" is a verb, followed by where the search is performed.

urlbar-result-action-search-bookmarks = Search Bookmarks
urlbar-result-action-search-history = Search History
urlbar-result-action-search-tabs = Search Tabs

## Full Screen and Pointer Lock UI

# Please ensure that the domain stays in the `<span data-l10n-name="domain">` markup.
# Variables
#  $domain (String): the domain that is full screen, e.g. "mozilla.org"
fullscreen-warning-domain = <span data-l10n-name="domain">{ $domain }</span> is now full screen
fullscreen-warning-no-domain = This document is now full screen


fullscreen-exit-button = Exit Full Screen (Esc)
# "esc" is lowercase on mac keyboards, but uppercase elsewhere.
fullscreen-exit-mac-button = Exit Full Screen (esc)

# Please ensure that the domain stays in the `<span data-l10n-name="domain">` markup.
# Variables
#  $domain (String): the domain that is using pointer-lock, e.g. "mozilla.org"
pointerlock-warning-domain = <span data-l10n-name="domain">{ $domain }</span> has control of your pointer. Press Esc to take back control.
pointerlock-warning-no-domain = This document has control of your pointer. Press Esc to take back control.

## Subframe crash notification

crashed-subframe-message = <strong>Part of this page crashed.</strong> To let { -brand-product-name } know about this issue and get it fixed faster, please submit a report.
crashed-subframe-learnmore-link =
  .value = Learn more
crashed-subframe-submit =
  .label = Submit report
  .accesskey = S

## Bookmarks panels, menus and toolbar

bookmarks-manage-bookmarks =
  .label = Manage Bookmarks
bookmarks-recent-bookmarks-panel-subheader = Recent Bookmarks
bookmarks-toolbar-chevron =
  .tooltiptext = Show more bookmarks
bookmarks-sidebar-content =
  .aria-label = Bookmarks
bookmarks-menu-button =
  .label = Bookmarks Menu
bookmarks-other-bookmarks-menu =
  .label = Other Bookmarks
bookmarks-mobile-bookmarks-menu =
  .label = Mobile Bookmarks
bookmarks-tools-sidebar-visibility =
  .label = { $isVisible ->
     [true] Hide Bookmarks Sidebar
    *[other] View Bookmarks Sidebar
  }
bookmarks-tools-toolbar-visibility-menuitem =
  .label = { $isVisible ->
     [true] Hide Bookmarks Toolbar
    *[other] View Bookmarks Toolbar
  }
bookmarks-tools-toolbar-visibility-panel =
  .label = { $isVisible ->
     [true] Hide Bookmarks Toolbar
    *[other] Show Bookmarks Toolbar
  }
bookmarks-tools-menu-button-visibility =
  .label = { $isVisible ->
     [true] Remove Bookmarks Menu from Toolbar
    *[other] Add Bookmarks Menu to Toolbar
  }
bookmarks-search =
  .label = Search Bookmarks
bookmarks-tools =
  .label = Bookmarking Tools
bookmarks-bookmark-edit-panel =
  .label = Edit This Bookmark

# The aria-label is a spoken label that should not include the word "toolbar" or
# such, because screen readers already know that this container is a toolbar.
# This avoids double-speaking.
bookmarks-toolbar =
  .toolbarname = Bookmarks Toolbar
  .accesskey = B
  .aria-label = Bookmarks
bookmarks-toolbar-menu =
  .label = Bookmarks Toolbar
bookmarks-toolbar-placeholder =
  .title = Bookmarks Toolbar Items
bookmarks-toolbar-placeholder-button =
  .label = Bookmarks Toolbar Items

# "Bookmark" is a verb, as in "Add current tab to bookmarks".
bookmarks-current-tab =
  .label = Bookmark Current Tab

## Library Panel items

library-bookmarks-menu =
  .label = Bookmarks
library-recent-activity-title =
  .value = Recent Activity

## Pocket toolbar button

save-to-pocket-button =
  .label = Save to { -pocket-brand-name }
  .tooltiptext = Save to { -pocket-brand-name }

## Customize Toolbar Buttons

# Variables:
#  $shortcut (String): keyboard shortcut to open the add-ons manager
toolbar-addons-themes-button =
  .label = Add-ons and themes
  .tooltiptext = Manage your add-ons and themes ({ $shortcut })

# Variables:
#  $shortcut (String): keyboard shortcut to open settings (only on macOS)
toolbar-settings-button =
  .label = Settings
  .tooltiptext = { PLATFORM() ->
      [macos] Open settings ({ $shortcut })
     *[other] Open settings
  }

## More items

more-menu-go-offline =
  .label = Work Offline
  .accesskey = k

## EME notification panel

eme-notifications-drm-content-playing = Some audio or video on this site uses DRM software, which may limit what { -brand-short-name } can let you do with it.
eme-notifications-drm-content-playing-manage = Manage settings
eme-notifications-drm-content-playing-manage-accesskey = M
eme-notifications-drm-content-playing-dismiss = Dismiss
eme-notifications-drm-content-playing-dismiss-accesskey = D

## Password save/update panel

panel-save-update-username = Username
panel-save-update-password = Password

## Add-on removal warning

# Variables:
#  $name (String): The name of the addon that will be removed.
addon-removal-title = Remove { $name }?
addon-removal-abuse-report-checkbox = Report this extension to { -vendor-short-name }

## Remote / Synced tabs

remote-tabs-manage-account =
  .label = Manage Account
remote-tabs-sync-now = Sync Now
