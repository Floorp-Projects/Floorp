# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

###################################################################### about:Dialog #################################################################################

about-floorp = <label data-l10n-name="floorp-browser-link">{ -brand-product-name }</label> is one of the domestic browsers developed in Japan. It is based on Firefox and continues to operate under <label data-l10n-name="ablaze-Link">{ -vendor-short-name }</label>, to improve the web. Want to help? <label data-l10n-name="helpus-donateLink">Make a donation</label>
icon-creator = Icon creator <label data-l10n-name="browser-logo-twitter">@CutterKnife_</label> and <label data-l10n-name="brand-logo-twitter">@mwxdxx.</label>
contributors = A list of <label data-l10n-name="about-contributor">contributors and Developers</label>

######################################################################### Themes #########################################################################

extension-floorp-material-name= Material Theme
extension-floorp-material-description=Follow the operating system setting for buttons, menus, and windows

extension-floorp-photon-name= Firefox Photon Theme
extension-floorp-photon-description= Follow the operating system setting for buttons, menus, and windows

#################################################################### about:preferences ####################################################################

feature-requires-restart = A reboot is required to change the settings

floorp-preference = Experimet Preferences
browser-design-settings = Tab Bar Settings

auto-reboot = If you change any of the settings below, your browser will be restarted to apply the changes. Please save the data you are working with before making any changes.
tab-width = Minimum width of tabs
enable-multitab = 
 .label = Enable multi-row tabs
multirow-tabs-limit = 
 .label = Enable row limit for multi-row tabs.
multirow-tabs-newtab = 
 .label = Always place the Open New Tab button within the column of multi-row tabs
multirow-tabs-value = Number of rows when multi-row tabs are enabled
enable-tab-sleep = 
 .label = Enable Tab Sleep
tab-sleep-timeout-minutes-value = Time to put tabs to sleep (minutes)
enable-tab-scroll-change = 
 .label = Switch Tabs by Scrolling
enable-doble-click-block = 
 .label = Double-click to Close the Tab
enable-show-pinned-tabs-title =
 .label = Show the title of pinned tabs
operation-settings = 
  .label = Browser Operation Settings
Mouse-side-button = 
  .label = Optimise browser for mouse with side buttons

tabbar-preference = Tabbar

None-mode = 
 .label= Normal mode

hide-horizontality-tabs = 
 .label= Hide horizontal tabbar

verticalTab-setting = 
 .label = Optimise browser for vertical tabs

move-tabbar-position =
 .label = Show tabbar at the bottom of the toolbar

tabbar-on-bottom = 
 .label = Display tabbar at the bottom of the window

treestyletabSettings-l10 = Tree Style vertical tab settings
treestyletab-Settings = 
 .label = Expand when mouse focus
treestyletab-open-option = TreeStyleTab Settings

bookmarks-bar-settings = Bookmark Bar Settings (Can`t be used in parallel)
bookmarks-focus-mode =
 .label = Show the bookmark bar when the mouse is focused on the toolbar
bookmarks-bottom-mode =
 .label = Bookmark bar at the bottom of the browser

nav-bar-settings = Navigation Bar Settings
show-nav-bar-bottom =
 .label = Show the Navigation bar at the Bottom of { -brand-short-name }

material-effect =
 .label = Allow Mica for Everyone to modify the browser design
other-preference = Other Preferences

operation-settings = Browser Operation

enable-userscript = 
 .label = Enable legacy components
about-legacy-components = Enabling this feature may cause unexpected bugs or fatal errors

Search-positon-top =
 .label = Display the search bar at the top of the page
allow-auto-restart =
 .label = Automatic restart when settings are changed that require a restart

browser-rest-mode =
 .label = Enable Rest-mode shortcut (F9)

disable-fullscreen-notification =
 .label = Disable video fullscreen notification

floorp-updater = { -brand-short-name } Updater Settings
enable-floorp-updater = 
 .label = Check for { -brand-short-name } updates at startup
floorp-update-latest = 
 .label = Notification that { -brand-short-name } is up-to-date during automatic update checks

## system theme color

system-color-settings = Both light and dark modes are available for this theme, and the design can be specified.
preferences-theme-appearance-header = Setting the system theme

system-theme-dark =
 .label = Enforce dark mode

system-theme-light = 
 .label = Enforce light mode
 
system-theme-auto =  
 .label = Default mode

## user interface prefernces

ui-preference = Browser appearance
preferences-browser-appearance-description = You can choose from a selection of great designs written by Floorp third parties. Some designs may not be compatible with your configuration.

firefox-proton =
 .label = Firefox modern Proton UI

firefox-proton-fix =
 .label = Firefox Proton FIX UI

firefox-photon-lepton = 
 .label = Firefox Photon・Lepton UI
 
floorp-legacy =  
 .label = Floorp Legacy material UI・Unsupported

floorp-fluentUI =
 .label = fluentUI
 
floorp-fluerialUI =
 .label = Floorp Fluerial UI

floorp-gnomeUI =
 .label = Gnome Theme
 
waterfox-lepton =
 .label = Firefox Lepton UI

## BlockMoreTracker

privacy-hub-header = Privacy Hub

block-more-tracker = Block more Ads and Trackers
block-tracker = This section contains a set of extensions designed to block ads and trackers

view-at-AMO = View at AMO
uBlock-Origin = uBlock Origin
about-uboori = uBlock Origin blocks ads, extensive trackers, and additional dangerous sites.

Privacy-Badger = Privacy Badger
about-PBadger = Privacy Badger automatically learns to block hidden trackers based on their behaviour across websites.

Duck-Duck-Go = DuckDuckGo Privacy Essentials
about-DDG = DuckDuckGo Privacy Essentials replaces a default search engine with DuckDuckGo while blocking trackers on visited sites.

## Fingerprint

fingerprint-header = Resist Fingerprinting & IP address leak
block-fingerprint = Fingerprinting is a tracking mechanism that relies on the unique features of your browser and operating system. This section contains settings to further enhance this protection beyond the default blocking.
enable-firefox-fingerprint-protections = Enable strong protection against fingerprinting
about-firefox-fingerprint-protection = Enabling protection by Firefox includes forced light mode, disabling some APIs, etc. Some sites may be broken.
fingerprint-Protection =
 .label =  Anti-fingerprinting protections
html5-canvas-prompt-settings =
 .label =  Automatically approve access confirmation prompts for HTML5 image data
canvas-prompt = Automatically reject the canvas-reading prompt
disable-webgl =
 .label =  Disable WebGL
about-webgl = WebGL is a Javascript API used to render graphics, which can be used to identify GPU.
Canvas-Blocker = Canvas Blocker
about-CB = This add-on spoofs data used by fingerprinting techniques.
WebRTC-connection = WebRTC is a standard that provides real-time calling. If you disable this setting, you will not be able to use Discord, etc.
WebRTC = 
 .label = Enable WebRTC Connection

## download mgr
download-notification-preferences = Download Notification
start-always-notify =
 .label = Notify Only at Start
finish-always-notify = 
 .label = Notify Only when Finished
always-notify =
 .label = Notify Both at Start and End
do-not-notify =
 .label = Do not Enable Notifications

floorp-translater = Translation Function Settings
click-to-option =
    .label = Open Settings...
    .accesskey = O

## sidebar
profiles-button-label = Manage profiles
floorp-help-button-label = { -brand-short-name } Support

appmenuitem-reboot =
 .label = Reboot

## useagent

UserAgent-preference = UserAgent
default-useragent-mode =
 .label = Use Firefox User Agent (Default)
windows-chrome-useragent-mode =
 .label = Spoof Chrome on Windows
macOS-chrome-useragent-mode =
 .label = Spoof Chrome on macOS
linux-chrome-useragent-mode =
 .label = Spoof Chrome on Linux
mobile-chrome-useragent-mode =
 .label = Impersonate Moblie

## DMR UI
download-mgr-UI =
 .label = Use a Simple UI download manager
downloading-red-color =
 .label = Highlight the Download Manager icon in red during download  

sidebar-preferences = Sidebar
view-sidebar2-right = 
 .label = View the Browser Manager Sidebar on the Right
enable-sidebar2 =
 .label = Enable the Browser Manager Sidebar

sidebar2-restore =
 .label = Restore the Sidebar mode when { -brand-short-name } is restarted or load new window

custom-URL-option = Set Webpanel URLs
set-custom-URL-button = 
    .label = Set Custom URLs...
    .accesskey = S
bsb-header = Browser Manager Sidebar
bsb-context = Select Container Tabs
bsb-userAgent-label = 
  .label = Use Mobile version UserAgent in this panel
bsb-width = Width (If set to 0, use global values)
bsb-page = Page to open

bsb-add = Add webpanel on Browser Manager Sidebar

bsb-setting = Webpanel Setting

bsb-add-title =
 .title = { bsb-add }

bsb-setting-title = 
 .title = { bsb-setting }

bsb-browser-manager-sidebar =
  .label = { sidebar2-browser-manager-sidebar }

bsb-bookmark-sidebar =
  .label = { sidebar2-bookmark-sidebar }

bsb-history-sidebar =
  .label = { sidebar2-history-sidebar }

bsb-download-sidebar =
  .label = { sidebar2-download-sidebar }

bsb-TST-sidebar =
  .label = { sidebar2-TST-sidebar }

bsb-website = 
  .label = Website

sidebar2-pref-delete =
 .label = Delete

sidebar2-pref-setting =
 .label = Setting

sidebar2-global-width = Global value of webpanel's width

use-icon-provider-option = Use Icon Provider

use-icon-provider-option-google =
 .label = Google

use-icon-provider-option-duckduckgo =
 .label = DuckDuckGo

use-icon-provider-option-yandex =
 .label = Yandex (Available in China)

use-icon-provider-option-hatena =
 .label = Hatena (Available in China)

memory-and-performance = Memory Performance Settings

min-memory = 
    .label = Minimum Memory Usage (low performance)

balance-memory = 
    .label = Balance Memory Usage and Performance

max-memory = 
    .label = Best Speed and Performance (high memory usage) 

## DualTheme
dualtheme-enable =
 .label = Enable Dual Theme

newtab-background = { -brand-short-name } Home Background

newtab-background-random-image =
    .label = Random Images from Unsplash

newtab-background-gradation =
    .label = Gradation

newtab-background-not-background =
    .label = Disable Background

newtab-background-selected-image =
    .label = Use Images from a Selected Folder

newtab-background-folder = Selected Folder

newtab-background-folder-reload = 
  .label = Reload Images

newtab-background-folder-default = 
  .label = Restore Default

newtab-background-folder-open = 
  .label = Open Folder

newtab-background-folder-choose = Choose Images Folder

newtab-background-extensions = Images' Extensions (Separated by ",")

## lepton preferences

about-lepton = Customize { -brand-short-name } with Lepton

lepton-preference-button =
    .label = Lepton Preferences...
    .accesskey = L

lepton-header = Lepton Preferences

lepton-preference = Lepton UI Settings
photon-mode = 
    .label = Photon Mode

lepton-mode = 
    .label = Lepton Mode

autohide-preference = Auto hide elements

floorp-lepton-enable-tab-autohide =
    .label = Auto hide tabbar
floorp-lepton-enable-navbar-autohide =
    .label = Auto hide navbar
floorp-lepton-enable-sidebar-autohide =
    .label = Auto hide sidebar
floorp-lepton-enable-urlbar-autohide =
    .label = Auto hide URLBar
floorp-lepton-enable-back-button-autohide =
    .label = Auto hide back button
floorp-lepton-enable-forward-button-autohide =
    .label = Auto hide forward button
floorp-lepton-enable-page-action-button-autohide =
    .label = Auto hide action button on addressbar
floorp-lepton-enable-toolbar-overlap =
    .label = Enable Toolbar overlay
floorp-lepton-enable-toolbar-overlap-allow-layout-shift-autohide =
    .label = Auto-hide toolbar when displaying "ltr" content

hide-preference = Manage display elements

floorp-lepton-enable-tab_icon-hide =
    .label = Hide Tab icons
floorp-lepton-enable-tabbar-hide =
    .label = Hide Tabbar
floorp-lepton-enable-navbar-hide =
    .label = Hide Navbar
floorp-lepton-enable-sidebar_header-hide =
    .label = Hide Sidebar Header
floorp-lepton-enable-urlbar_iconbox-hide =
    .label = Hide URLBar icons
floorp-lepton-enable-bookmarkbar_icon-hide =
    .label = Hide bookmarks-bar icons
floorp-lepton-enable-bookmarkbar_label-hide =
    .label = Hide bookmarks-bar labels
floorp-lepton-enable-disabled_menu-hide =
    .label = Hide disabled context menu

positon-preferences = Position adjustment

floorp-lepton-enable-centered-tab =
    .label = Centered Tab label
floorp-lepton-enable-centered-urlbar =
    .label = Centered URLBar
floorp-lepton-enable-centered-bookmarkbar =
    .label = Centered Bookmark Bar

urlbar-preferences = URLbar

floorp-lepton-enable-urlbar-icon-move-to-left =
    .label = Move urlbar icons to the left side
floorp-lepton-enable-urlname-go_button_when_typing =
    .label = When typing, reduce the urlbar space and show a Go Button
floorp-lepton-enable-always-show-page_action =
    .label = Reduce the urlbar space and always show the addon action button

tabbar-preferences = Tabbar

floorp-lepton-enable-tabbar-positon-as-titlebar =
    .label = Tabbar in the titlebar
floorp-lepton-enable-tabbar-as-urlbar =
    .label = Tabbar in the urlbar

lepton-sidebar-preferences = Sidebar
floorp-lepton-enable-overlap-sidebar =
    .label = Show Sidebar Overlap main Viewer

floorp-home-mode-choice-default =
    .label = Floorp Home (Default)
floorp-home-prefs-content-header = Floorp Home Content
floorp-home-prefs-content-description = Choose what content you want on your Floorp Home screen.

## tool attribute
################################################################### browser ###############################################################

rest-mode = Taking a break...
rest-mode-description = Browser is stopped. Press ENTER or OK to continue.

Sidebar2 =
  .label = Browser Manager Sidebar
  .tooltiptext = Change Sidebar Visibility

sidebar2-mute-and-unmute =
  .label = Mute/Unmute this panel

sidebar2-unload-panel =
  .label = Unload this panel

sidebar2-change-ua-panel =
  .label = Switch User agent to Mobile/Desktop version at this panel

sidebar2-delete-panel =
  .label = Delete this panel

sidebar-close-button =
  .tooltiptext = Close sidebar

sidebar-back-button =
  .tooltiptext = Back

sidebar-forward-button =
  .tooltiptext = Forward

sidebar-reload-button = 
  .tooltiptext = Reload

sidebar-muteAndUnmute-button =
  .tooltiptext = Mute/Unmute sidebar

sidebar2-browser-manager-sidebar = Browser Manager

show-browser-manager-sidebar =
  .tooltiptext = Show { sidebar2-browser-manager-sidebar } Sidebar

sidebar2-bookmark-sidebar = Bookmarks

show-bookmark-sidebar =
  .tooltiptext = Show { sidebar2-bookmark-sidebar } Sidebar

sidebar2-history-sidebar = History

show-history-sidebar =
  .tooltiptext = Show { sidebar2-history-sidebar } Sidebar

sidebar2-download-sidebar = Download

show-download-sidebar =
  .tooltiptext = Show { sidebar2-download-sidebar } Sidebar

sidebar2-TST-sidebar = Tree Style Tab

show-TST-sidebar =
  .tooltiptext = Show { sidebar2-TST-sidebar } Sidebar


show-CustomURL-sidebar =
 .label = Show Custom URL Sidebar

Edit-Custom-URL =
 .label = Edit Custom URL in Sidebar

sidebar-add-button =
  .tooltiptext = { bsb-add }

sidebar-addons-button =
  .tooltiptext = Open Addon manager

sidebar-passwords-button =
  .tooltiptext = Open Password manager

sidebar-preferences-button =
  .tooltiptext = Open Preferences

sidebar-keepWidth-button =
  .tooltiptext = Keep This Panel Width

sidebar2-keep-width-for-global =
  .label = Set the current panel size to all web panels that don't have a unique size

bsb-context-add = 
  .label = Add This Page on Webpanel

bsb-context-link-add = 
  .label = Add Link on Webpanel
#################################################################### menu panel ############################################################

open-profile-dir = 
    .label = Open Profile Directory
appmenuitem-reboot =
    .label = Reboot

####################################################################### menu ###############################################################

css-menu =
    .label = CSS
    .accesskey = C

css-menubar =
    .label = CSS
    .accesskey = C

rebuild-css =
    .label = Rebuild browser CSS files
    .accesskey = R

make-browsercss-file =
    .label = Create browser CSS file
    .accesskey = M

open-css-folder =
    .label = Open CSS folder
    .accesskey = O

edit-userChromeCss-editor =
    .label = Edit userChrome.css file

edit-userContentCss-editor =
    .label = Edit userContent.css file

test-chrome-css =
    .label = Test userChrome.css
    .accesskey = C
test-content-css =
    .label = Test userContent.css
    .accesskey = W

not-found-editor-path = Path to the editor was not found
set-pref-description = This operation requires a path to the editor. Set "view_source.editor.path" in "about:config"
rebuild-complete = Rebuild has been completed.
please-enter-filename = Please enter a file name.
confirmed-update = Confirmed the update of "{ $leafName }".

################################################################### Undo-Closed-Tab ###############################################################

undo-closed-tab = Undo close Tab

################################################################### about:addons ###############################################################

# DualTheme
dual-theme-enable-addon-button = Enable as a sub theme
dual-theme-disable-addon-button = Disable as a sub theme
dual-theme-enabled-heading = Enabled as a sub theme

##################################################################### migration  ###############################################################

import-from-vivaldi =
    .label = Vivaldi
    .accesskey = V

##################################################################### toolbar ###############################################################

status-bar =
    .label = Status Bar
     .accesskey = S

##################################################################### Gesturefy ###############################################################

gf-floorp-open-tree-style-tab-name = [Floorp] Open Tree Style Tab Panel
gf-floorp-open-tree-style-tab-description = Open Tree Style Tab Panel of Sidebar

gf-floorp-open-bookmarks-sidebar-name = [Floorp] Open Bookmarks Panel of Sidebar
gf-floorp-open-bookmarks-sidebar-description = Open Bookmarks Panel of Sidebar

gf-floorp-open-history-sidebar-name = [Floorp] Open History Panel of Sidebar
gf-floorp-open-history-sidebar-description = Open History Panel of Sidebar

gf-floorp-open-synctabs-sidebar-name = [Floorp] Open Synced Tabs Panel of Sidebar
gf-floorp-open-synctabs-sidebar-description = Open Synced Tabs Panel of Sidebar

gf-floorp-close-sidebar-name = [Floorp] Close Sidebar
gf-floorp-close-sidebar-description = Close Sidebar


gf-floorp-open-browser-manager-sidebar-name = [Floorp] Open BMS
gf-floorp-open-browser-manager-sidebar-description = Open Browser Manager Sidebar

gf-floorp-close-browser-manager-sidebar-name = [Floorp] Close BMS
gf-floorp-close-browser-manager-sidebar-description = Close Browser Manager Sidebar

gf-floorp-show-statusbar-name = [Floorp] Show Status Bar
gf-floorp-show-statusbar-description = Show Status Bar

gf-floorp-hide-statusbar-name = [Floorp] Hide Status Bar
gf-floorp-hide-statusbar-description = Hide Status Bar

gf-floorp-toggle-statusbar-name = [Floorp] Toggle Status Bar
gf-floorp-toggle-statusbar-description = Show or Hide Status Bar

##################################################################### Floorp System Update Portable Version ###############################################################

update-portable-notification-found-title = Updates found!
update-portable-notification-found-message = Downloading updates...
update-portable-notification-ready-title = Ready to update!
update-portable-notification-ready-message = The next time the browser is launched, the update will begin.
update-portable-notification-success-title = Update succeeded!
update-portable-notification-success-message = Update succeeded! Hope you enjoy the new version of Floorp!
update-portable-notification-failed-title = Update failed.
update-portable-notification-failed-redirector-message = Update failed. Restarting your browser may solve the problem.
update-portable-notification-failed-prepare-message = Failed to prepare update.
