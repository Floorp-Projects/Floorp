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
  .data-content-title-default = { $content-title } - { -brand-full-name }
  .data-content-title-private = { $content-title } - { -brand-full-name } (Private Browsing)

# These are the default window titles on macOS. The first two are for use when
# there is no content title:
#
# "default" - "Mozilla Firefox"
# "private" - "Mozilla Firefox - (Private Browsing)"
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
  .data-title-private = { -brand-full-name } - (Private Browsing)
  .data-content-title-default = { $content-title }
  .data-content-title-private = { $content-title } - (Private Browsing)

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

page-action-add-to-urlbar =
    .label = Add to Address Bar
page-action-manage-extension =
    .label = Manage Extension…
page-action-remove-from-urlbar =
    .label = Remove from Address Bar
page-action-remove-extension =
    .label = Remove Extension

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

bookmark-panel-show-editor-checkbox =
    .label = Show editor when saving
    .accesskey = S

bookmark-panel-done-button =
    .label = Done

# Width of the bookmark panel.
# Should be large enough to fully display the Done and
# Cancel/Remove Bookmark buttons.
bookmark-panel =
    .style = min-width: 23em

## Identity Panel

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
identity-permissions =
    .value = Permissions
identity-permissions-reload-hint = You may need to reload the page for changes to apply.
identity-permissions-empty = You have not granted this site any special permissions.
identity-clear-site-data =
    .label = Clear Cookies and Site Data…
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

## WebRTC Pop-up notifications

popup-select-camera =
    .value = Camera to share:
    .accesskey = C
popup-select-microphone =
    .value = Microphone to share:
    .accesskey = M
popup-all-windows-shared = All visible windows on your screen will be shared.

popup-screen-sharing-not-now =
  .label = Not Now
  .accesskey = w

popup-screen-sharing-never =
  .label = Never Allow
  .accesskey = N

popup-silence-notifications-checkbox = Disable notifications from { -brand-short-name } while sharing
popup-silence-notifications-checkbox-warning = { -brand-short-name } will not display notifications while you are sharing.

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
urlbar-pocket-button =
  .tooltiptext = Save to { -pocket-brand-name }


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

