# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

###################################################################### about:Dialog #################################################################################

about-floorp = <label data-l10n-name="floorp-browser-link">{ -brand-product-name }</label> is one of the domestic browsers developed in Japan. It is based on Firefox and continues to operate under <label data-l10n-name="ablaze-Link">{ -vendor-short-name }</label>, to improve the web. Want to help? <label data-l10n-name="helpus-donateLink">Make a donation</label>
icon-creator = Icon creator <label data-l10n-name="browser-logo-twitter">@CutterKnife_</label> and <label data-l10n-name="brand-logo-twitter">@mwxdxx.</label>
contributors = A list of <label data-l10n-name="about-contributor">contributors and Developers</label>

#################################################################### about:preferences ####################################################################

pane-design-title = Design
category-design =
    .tooltiptext = { pane-design-title }
design-header = Design

feature-requires-restart = A restart is required to apply changes

tab-width = Minimum width of tabs
preferences-tabs-newtab-position = New Tab position
open-new-tab-use-default =
 .label = Open new tabs at default position
open-new-tab-at-the-end =
 .label = Open new tabs at the end of the Tab Bar
open-new-tab-next-to-current =
 .label = Open new tabs next to the current tab
enable-multitab = 
 .label = Enable multi-row tabs
multirow-tabs-limit = 
 .label = Enable row limit for multi-row tabs
multirow-tabs-newtab = 
 .label = Place the "Open a new tab" button at the end of the lowest row of tabs
multirow-tabs-value = Number of rows when multi-row tabs are enabled
enable-tab-sleep = 
 .label = Enable Sleeping Tabs
tab-sleep-timeout-minutes-value = Tabs will sleep after being inactive for (minutes)
tab-sleep-settings-button = Settings...
tab-sleep-settings-dialog-title =
 .title = Sleeping Tabs Settings
tab-sleep-settings-dialog-excludehosts-label = Exclude hosts
tab-sleep-settings-dialog-excludehosts-label-2 = Enter one host per line.
tab-sleep-tab-context-menu-excludetab = Keep Tab awake
enable-floorp-workspace =
 .label = Enable Workspaces
workspace-warring = Workspaces cannot be used with Tab Group add-ons. If you want to use tab group add-ons, please disable Workspaces and restart { -brand-short-name }.
enable-tab-scroll-change =
 .label = Switch tabs by scrolling with your mouse
enable-tab-scroll-reverse =
 .label = Reverse direction of scrolling tabs
enable-tab-scroll-wrap =
 .label = Wrap scrolling tabs at the edge
enable-double-click-block =
 .label = Close tabs when double-clicking them
enable-show-pinned-tabs-title =
 .label = Show the title of pinned tabs
Mouse-side-button =
  .label = Optimise browser for mouse with side buttons

tabbar-preference = Tab Bar (not supported in macOS)

None-mode = 
 .label= Normal mode

hide-horizontality-tabs =
 .label= Hide tabs on Horizontal Tab Bar

verticalTab-setting =
 .label = Optimise browser for Vertical Tab Bar

move-tabbar-position =
 .label = Display Tab Bar underneath the Toolbar

tabbar-on-bottom =
 .label = Display Tab Bar at the bottom of the window

tabbar-favicon-color =
 .label = Color the Tab Bar using the current website's favicon color

tabbar-style-preference = Tab Bar Style

horizontal-tabbar =
 .label = Horizontal Tab Bar

tabbar-style-description = A restart of { -brand-short-name } is required to fully apply this setting.
multirow-tabbar =
 .label = Multi-Row Tab Bar
vertical-tabbar =
 .label = Vertical Tab Bar (experimental)
native-tabbar-tip = Sidebar add-ons and in-built sidebars cannot be used at the same time as vertical tabs as this leverages the Firefox sidebar.

hover-vertical-tab =
 .label = Collapse Vertical Tab Bar

TST = Tree Style Tab
about-TST = Tree Style Tab is a popular add-on that allows you to display tabs in a tree structure. Floorp 10 has a built-in this add-on. Please install the add-on restore Floorp 10's built-in Tree Style Tab.
treestyletab-Settings = 
 .label = Collapse Tree Style Tab

sidebar-reverse-position-toolbar = Show Sidebars on the other side

bookmarks-bar-settings = Bookmarks Toolbar (only one option can be used at a time)
bookmarks-focus-mode =
 .label = Hide the Bookmarks Toolbar unless hovering over the navigation bar
bookmarks-bottom-mode =
 .label = Show the Bookmarks Toolbar at the bottom of { -brand-short-name }

nav-bar-settings = Toolbar
show-nav-bar-bottom =
 .label = Show the Toolbar at the bottom of { -brand-short-name } (experimental)

material-effect =
 .label = Allow Mica For Everyone to modify the browser design
disable-extension-check-compatibility-option =
 .label = Do not check for compatibility with add-ons
other-preference = Other Preferences

enable-userscript =
 .label = Enable legacy components
about-legacy-components = Enabling this feature may cause unexpected bugs or fatal errors

Search-positon-top =
 .label = Display the Find Bar at the top of the page
allow-auto-restart =
 .label = Restart automatically when settings that require a restart are changed

browser-rest-mode =
 .label = Enable Rest Mode shortcut (F9)

disable-fullscreen-notification =
 .label = Do not show a notification when entering full screen

floorp-updater = { -brand-short-name } Updates
enable-floorp-updater =
 .label = Check for { -brand-short-name } updates on startup
floorp-update-latest =
 .label = Notify me if { -brand-short-name } is up-to-date during automatic update checks

## system theme color

system-color-settings = Some themes have both light and dark modes - choose which mode you'd like these themes to use.
preferences-theme-appearance-header = Theme Mode

system-theme-dark =
 .label = Dark

system-theme-light =
 .label = Light

system-theme-auto =
 .label = Follow my system appearance

## user interface prefernces

ui-preference = Browser appearance
preferences-browser-appearance-description = Choose a built-in third-party design to use in { -brand-short-name }. Some designs may not be compatible with your configuration.

firefox-proton =
 .label = Firefox Proton UI

firefox-proton-fix =
 .label = Firefox Proton Fix UI

firefox-photon-lepton =
 .label = Firefox Photon・Lepton UI
 
floorp-legacy =
 .label = Floorp Legacy UI・Unsupported

floorp-fluentUI =
 .label = Microsoft Fluent UI
 
floorp-fluerialUI =
 .label = Floorp Fluerial UI

floorp-gnomeUI =
 .label = GNOME Theme

## download mgr
download-notification-preferences = Download Notifications
start-always-notify =
 .label = Notify only when starting downloads
finish-always-notify =
 .label = Notify only when a download finishes
always-notify =
 .label = Notify when starting downloads and when a download finishes
do-not-notify =
 .label = Disable download notifications

floorp-translater = Translator Settings
click-to-option =
    .label = Open Settings...
    .accesskey = O

## sidebar
profiles-button-label = Manage Profiles
floorp-help-button-label = { -brand-short-name } Support

appmenuitem-reboot =
 .label = Restart

## useagent

UserAgent-preference = User Agent
default-useragent-mode =
 .label = Use Firefox User Agent (Default)
windows-chrome-useragent-mode =
 .label = Spoof Chrome on Windows
macOS-chrome-useragent-mode =
 .label = Spoof Chrome on macOS
linux-chrome-useragent-mode =
 .label = Spoof Chrome on Linux
mobile-chrome-useragent-mode =
 .label = Spoof Chrome on iOS
use-custom-useragent-mode =
 .label = Use Custom User Agent

## DMR UI
download-mgr-UI =
 .label = Enable a simple downloads management interface
downloading-red-color =
 .label = Use a red downloads icon when downloading

sidebar-preferences = Sidebar

bsb-preferences = Browser Manager Sidebar Settings
view-sidebar2-right = 
 .label = Display the Browser Manager Sidebar on the right
enable-sidebar2 =
 .label = Enable the Browser Manager Sidebar
visible-bms = 
 .label = Show the Browser Manager Sidebar

custom-URL-option = Set Web Panel URLs
set-custom-URL-button =
    .label = Set Custom URLs...
    .accesskey = S

pane-BSB-title = { bsb-header }
category-BSB =
    .tooltiptext = { pane-BSB-title }

category-downloads = 
    .tooltiptext = { files-and-applications-title }

bsb-header = Browser Manager Sidebar
bsb-context = Use the following Container Tab
bsb-userAgent-label = 
  .label = Use Mobile User Agent in this Web Panel
bsb-width = Width (if set to 0, the global value will be used)
bsb-page = Page to open

bsb-add = Add Web Panel on Browser Manager Sidebar

bsb-setting = Web Panel Settings

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

bsb-notes-sidebar =
  .label = { sidebar2-notes-sidebar }

bsb-website = 
  .label = Website

sidebar2-pref-delete =
 .label = Delete

sidebar2-pref-setting =
 .label = Settings

sidebar2-global-width = Global Web Panels width

use-icon-provider-option = Use Icon Provider

use-icon-provider-option-google =
 .label = Google

use-icon-provider-option-duckduckgo =
 .label = DuckDuckGo

use-icon-provider-option-yandex =
 .label = Yandex (available in China)

use-icon-provider-option-hatena =
 .label = Hatena (available in China)

memory-and-performance = Memory and Performance

min-memory = 
    .label = Minimum Memory Usage (low performance)

balance-memory = 
    .label = Balance Memory Usage and Performance

max-memory = 
    .label = Best Speed and Performance (high memory usage)

delete-border-and-roundup-option =
  .label = Round the corners of pages

## DualTheme
dualtheme-enable =
 .label = Enable Dual Theme

newtab-background = { -brand-short-name } Home Background

newtab-background-random-image =
    .label = Random images from Unsplash

newtab-background-gradation =
    .label = Gradient

newtab-background-not-background =
    .label = Disable background

newtab-background-selected-image =
    .label = Custom folder...

newtab-background-folder = Use images from this folder:

newtab-background-folder-reload = 
  .label = Reload images

newtab-background-folder-default = 
  .label = Restore defaults

newtab-background-folder-open = 
  .label = Open folder

newtab-background-folder-choose = Choose images folder...

newtab-background-extensions = Use images with these file extensions (separated by ",")

disable-blur-on-newtab = 
  .label = Disable blur effect on { -brand-short-name } Home

## lepton preferences

about-lepton = Customize { -brand-short-name } with Lepton

lepton-preference-button =
    .label = Lepton Settings...
    .accesskey = L

lepton-header = Lepton Settings

lepton-preference = Lepton Settings
photon-mode =
    .label = Use Photon design

lepton-mode = 
    .label = Use Lepton design

protonfix-mode =
    .label = Use tweaked Proton design

autohide-preference = Automatically hide browser elements

floorp-lepton-enable-tab-autohide =
    .label = Automatically hide tabs
floorp-lepton-enable-navbar-autohide =
    .label = Automatically hide Toolbar
floorp-lepton-enable-sidebar-autohide =
    .label = Automatically hide Sidebar
floorp-lepton-enable-urlbar-autohide =
    .label = Automatically hide Address Bar
floorp-lepton-enable-back-button-autohide =
    .label = Automatically hide back button
floorp-lepton-enable-forward-button-autohide =
    .label = Automatically hide forward button
floorp-lepton-enable-page-action-button-autohide =
    .label = Automatically hide buttons on the Address Bar
floorp-lepton-enable-toolbar-overlap =
    .label = Show Toolbar over website content
floorp-lepton-enable-toolbar-overlap-allow-layout-shift-autohide =
    .label = Automatically hide Toolbar when displaying "ltr" content

hide-preference = Manage browser elements

floorp-lepton-enable-tab_icon-hide =
    .label = Hide Tab icons
floorp-lepton-enable-tabbar-hide =
    .label = Hide Tab Bar
floorp-lepton-enable-navbar-hide =
    .label = Hide Toolbar
floorp-lepton-enable-sidebar_header-hide =
    .label = Hide Sidebar Headers
floorp-lepton-enable-urlbar_iconbox-hide =
    .label = Hide Address Bar icons
floorp-lepton-enable-bookmarkbar_icon-hide =
    .label = Hide Bookmarks Bar icons
floorp-lepton-enable-bookmarkbar_label-hide =
    .label = Hide Bookmarks Bar labels
floorp-lepton-enable-disabled_menu-hide =
    .label = Hide disabled context menu items

floorp-lepton-disable-userChrome-icon =
    .label = Disable Lepton's context menu and panel menu icons

positon-preferences = Position adjustments

floorp-lepton-enable-centered-tab =
    .label = Center labels in tabs
floorp-lepton-enable-centered-urlbar =
    .label = Center text in the Address Bar
floorp-lepton-enable-centered-bookmarkbar =
    .label = Center Bookmarks Bar items

urlbar-preferences = Address Bar

floorp-lepton-enable-urlbar-icon-move-to-left =
    .label = Move Address Bar icons to the left side
floorp-lepton-enable-urlname-go_button_when_typing =
    .label = When typing, show a Go button
floorp-lepton-enable-always-show-page_action =
    .label = Always show page actions in the Address Bar

tabbar-preferences = Tab Bar

floorp-lepton-enable-tabbar-positon-as-titlebar =
    .label = Tab Bar in the titlebar
floorp-lepton-enable-tabbar-as-urlbar =
    .label = Combine Tab Bar and Toolbar

lepton-sidebar-preferences = Sidebar
floorp-lepton-enable-overlap-sidebar =
    .label = Show Sidebar over website content

floorp-home-mode-choice-default =
    .label = { -brand-short-name } Home (Default)
floorp-home-prefs-content-header = { -brand-short-name } Home Content
floorp-home-prefs-content-description = Choose the content you want to see on the { -brand-short-name } Home Page.

## Notes
floorp-notes = { -brand-short-name } Notes
restore-from-backup = Restore Notes from backup
enable-notes-sync = 
 .label = Enable { -brand-short-name } Notes Sync
about-notes-backup-tips = Floorp Notes uses Firefox Sync to sync your notes with other devices. If you lose your notes, you can restore them from a backup. A backup is created when you start { -brand-short-name }.
notes-sync-description = This can solve the problem of losing content due to overwriting notes during synchronization.
backuped-time = Backed up at
notes-backup-option = Backup Settings
backup-option-button = Backup Settings...

restore-from-backup-prompt-title = Floorp Notes Restore Service
restore-from-this-backup = Restore Notes back to the state they were in this backup?

restore-button = Restore

## user.js
header-userjs = user.js
userjs-customize = Customize { -brand-short-name } with user.js
about-userjs-customize = user.js is a configuration file that allows you to customize { -brand-short-name }. user.js files are downloaded from the Internet and overwrite your current user.js file. Please back up your current user.js file before continuing. user.js configurations will be applied automatically after restarting { -brand-short-name }.

userjs-label = user.js list
userjs-prompt = Apply this user.js?
apply-userjs-attention = Applying a new user.js will overwrite your current user.js file.
apply-userjs-attention2 = Please back up your current user.js file before continuing.

userjs-button = user.js Settings...
userjs-select-option = Manage the user.js currently used in { -brand-short-name } to improve performance and privacy.

apply-userjs-button = Apply
## userjs Options

default-userjs-label = Floorp Default
about-default-userjs = Telemetry disabled. Well balanced { -brand-short-name } with various customizations enabled.

Securefox-label = Yokoffing Securefox
about-Securefox = HTTPS-by-Default. Total Cookie Protection with site isolation. Enhanced state and network partitioning. Various other enhancements.

default-label = Yokoffing Default
about-default = All the essentials. None of the breakage. This is your user.js.

Fastfox-label = Yokoffing Fastfox
about-Fastfox = Immensely increase Firefox's browsing speed. Give Chrome a run for its money!

Peskyfox-label = Yokoffing Peskyfox
about-Peskyfox = Unclutter the new tab page. Remove Pocket. Restore compact mode as an option. Stop webpage notifications, pop-ups, and other annoyances.

Smoothfox-label = Yokoffing Smoothfox
about-Smoothfox = Get Edge-like smooth scrolling on your favorite browser — or choose something more your style. 

## Workspaces
floorp-workspaces-title = { -brand-short-name } Workspaces
workspaces-backup-discription = Backup and restore your Workspaces

workspaces-restore-service-title = Floorp Workspaces Backup Service
workspaces-restore-warning = Warning! Running this operation will cause the browser to freeze temporarily and restart automatically.
floorp-workspace-settings-button = Workspace Settings...

change-to-close-workspace-popup-option = 
 .label = Close workspaces popup when selecting a Workspace
pinned-tabs-exclude-workspace-option = 
 .label = Exclude pinned tabs from Workspaces

workspaces-reset-title = Reset Workspaces
workspaces-reset-label =
    .label = Reset Workspaces
workspaces-reset-description = If a backup does not work and the Workspace does not start, reset the Workspace.
workspaces-reset-button = Reset Workspaces

workspaces-reset-service-title = Floorp Workspaces
workspaces-reset-warning = Warning! Running this operation will delete all your Workspaces and restart the browser.

manage-workspace-on-bms-option =
    .label = Manage Workspace on Browser Manager Sidebar

workspaces-manage-title = Manage Workspaces
workspaces-manage-description = Manage your Workspaces. Change workspace icon.
workspaces-manage-label =
    .label = Manage Workspaces

workspaces-manage-button = Open Workspace Manager...

select-workspace = Select Workspace
workspace-select-icon = Select Workspace Icon
 .label = Select Workspace Icon

workspace-customize = 
 .title = Customize Workspace

workspace-icon-briefcase =
 .label = Job
workspace-icon-cart =
 .label = Shopping
workspace-icon-circle =
 .label = Circle
workspace-icon-dollar =
 .label = Bank
workspace-icon-fence =
 .label = Fence
workspace-icon-fingerprint =
 .label = Personal
workspace-icon-gift =
 .label = Gift
workspace-icon-vacation =
 .label = Vacation
workspace-icon-food =
 .label = Food
workspace-icon-fruit =
 .label = Fruit
workspace-icon-pet =
 .label = Pet
workspace-icon-tree =
 .label = Tree
workspace-icon-chill =
 .label = Private

## Mouse Gesture
mouse-gesture = Mouse Gestures
mouse-gesture-description = Gesturefy must be installed to use mouse gestures with { -brand-short-name }.
Gesturefy = Gesturefy
about-Gesturefy = Gesturefy is an extension that adds mouse gestures to your browser. If { -brand-short-name } detects the installation of this add-on, it will add gesture commands to Gesturefy that are only available in { -brand-short-name }. Also, this add-on can work with new tabs!

# Translate
TWS = Translate Web Page
about-TWS = Translate your page in real time using Google or Yandex. You can also translate selected text or the entire page.

# Privacy Hub
## BlockMoreTracker
privacy-hub-header = Privacy Hub
block-more-tracker = Block more Ads and Trackers
block-tracker = This section contains a set of extensions designed to block ads and trackers
view-at-AMO = View this addon in addons.mozilla.org
uBlock-Origin = uBlock Origin
about-uboori = uBlock Origin blocks ads, extensive trackers, and additional dangerous sites.
Facebook-Container = Facebook Container
about-Facebook-Container = Prevent Facebook from tracking you around the web. Facebook Container extension helps you take control and isolate your web activity from Facebook.

## Fingerprint
fingerprint-header = Resist Fingerprinting & IP address leaks
block-fingerprint = Fingerprinting is a tracking mechanism that relies on the unique features of your browser and operating system. This section contains settings to further enhance this protection beyond the default blocking.
enable-firefox-fingerprint-protections = Enable strong protection against fingerprinting
about-firefox-fingerprint-protection = Enabling protection by Firefox includes forced light mode, disabling some APIs, etc. Some sites may be broken.
fingerprint-Protection =
 .label =  Anti-fingerprinting protections
html5-canvas-prompt-settings =
 .label =  Automatically dismiss access confirmation prompts for HTML5 image data
canvas-prompt = Prevents websites from using the canvas-reading prompt unless manually permitted.
disable-webgl =
 .label =  Disable WebGL
about-webgl = WebGL is a Javascript API used to render graphics, which can be used to identify GPU.
Canvas-Blocker = Canvas Blocker
about-CB = This add-on spoofs data used by fingerprinting techniques.
WebRTC-connection = WebRTC is a standard that provides real-time calling. If you disable this setting, you will not be able to use Discord, etc.
WebRTC = 
 .label = Enable WebRTC Connection

################################################################### browser ###############################################################

rest-mode = Taking a break...
rest-mode-description = Floorp is currently suspended. Press ENTER or OK to continue.

Sidebar2 =
  .label = Browser Manager Sidebar
  .tooltiptext = Change Sidebar visibility

sidebar2-mute-and-unmute =
  .label = Mute/Unmute this Panel

sidebar2-unload-panel =
  .label = Unload this Panel

sidebar2-change-ua-panel =
  .label = Toggle Mobile User Agent

sidebar2-delete-panel =
  .label = Delete this Panel

sidebar2-close-button =
  .tooltiptext = Close Sidebar

sidebar-back-button =
  .tooltiptext = Back

sidebar-forward-button =
  .tooltiptext = Forward

sidebar-reload-button = 
  .tooltiptext = Reload

sidebar-go-index-button =
  .tooltiptext = Go Home

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

sidebar2-download-sidebar = Downloads

show-download-sidebar =
  .tooltiptext = Show { sidebar2-download-sidebar } Sidebar

sidebar2-notes-sidebar = Notes

show-notes-sidebar =
  .tooltiptext = Show { sidebar2-notes-sidebar } Sidebar

sidebar-add-button =
  .tooltiptext = { bsb-add }

sidebar-addons-button =
  .tooltiptext = Add-ons and themes

sidebar-passwords-button =
  .tooltiptext = Passwords

sidebar-preferences-button =
  .tooltiptext = Settings

sidebar-keepWidth-button =
  .tooltiptext = Keep using this width on this Panel

sidebar2-keep-width-for-global =
  .label = Apply this width to all panels without a custom width

bsb-context-add = 
  .label = Add page to Web Panel...

bsb-context-link-add = 
  .label = Add link to Web Panel...
#################################################################### menu panel ############################################################

open-profile-dir = 
    .label = Open Profile Directory

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

not-found-editor-path = Could not find the editor path.
set-pref-description = Set the path to the editor you want to use.
rebuild-complete = Rebuild has been completed.
please-enter-filename = Please enter a file name.

################################################################### Undo-Closed-Tab ###############################################################

undo-closed-tab = Reopen closed tab

################################################################### about:addons ###############################################################

# DualTheme
dual-theme-enable-addon-button = Enable as a sub-theme
dual-theme-disable-addon-button = Disable as a sub-theme
dual-theme-enabled-heading = Enabled as a sub-theme

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
gf-floorp-open-browser-manager-sidebar-description =  Open Browser Manager Sidebar, if webpanel of Browser Manager Sidebar was loaded

gf-floorp-close-browser-manager-sidebar-name = [Floorp] Close BMS
gf-floorp-close-browser-manager-sidebar-description = Close Browser Manager Sidebar

gf-floorp-toggle-browser-manager-sidebar-name = [Floorp] Toggle BMS
gf-floorp-toggle-browser-manager-sidebar-description = Toggle Browser Manager Sidebar

gf-floorp-show-statusbar-name = [Floorp] Show Status Bar
gf-floorp-show-statusbar-description = Show Status Bar

gf-floorp-hide-statusbar-name = [Floorp] Hide Status Bar
gf-floorp-hide-statusbar-description = Hide Status Bar

gf-floorp-toggle-statusbar-name = [Floorp] Toggle Status Bar
gf-floorp-toggle-statusbar-description = Show or Hide Status Bar

gf-floorp-open-extension-sidebar-name = [Floorp] Open selected add-on of Sidebar
gf-floorp-open-extension-sidebar-description = Open selected add-on of Sidebar
gf-floorp-open-extension-sidebar-settings-addons-id = Add-on of Sidebar
gf-floorp-open-extension-sidebar-settings-addons-id-description = The extension of the add-on open of sidebar
gf-floorp-open-extension-sidebar-settings-list-default = Please select add-on
gf-floorp-open-extension-sidebar-settings-list-unknwon = Unknown add-on
##################################################################### Floorp System Update Portable Version ###############################################################

update-portable-notification-found-title = Updates found!
update-portable-notification-found-message = Downloading updates...
update-portable-notification-ready-title = Ready to update!
update-portable-notification-ready-message = { -brand-short-name } will be updated when it launches next.
update-portable-notification-success-title = Update succeeded!
update-portable-notification-success-message = Update succeeded! Hope you enjoy the new version of Floorp!
update-portable-notification-failed-title = Update failed.
update-portable-notification-failed-redirector-message = Update failed. Restarting your browser may solve the problem.
update-portable-notification-failed-prepare-message = Failed to prepare update.

##################################################################### Open link in external ###############################################################
openInExternal-title = Open in external browser
open-link-in-external-enabled-option =
 .label = Enable the "Open in external browser" feature
open-link-in-external-select-browser-option = Choose what browser will be opened
open-link-in-external-select-browser-option-default =
 .label = Default browser
open-link-in-external-tab-context-menu = Open in external browser
open-link-in-external-tab-dialog-title-error = An error occurred:
open-link-in-external-tab-dialog-message-default-browser-not-found = Default browser is not found or is not configured.
open-link-in-external-tab-dialog-message-selected-browser-not-found = The selected browser does not exist.


######################################################################### Floorp Notes ###############################################################

new-memo = New
memo-title-input-placeholder = Write a title here
memo-input-placeholder = Write or paste a memo here
delete-memo = Delete
save-memo = Save
memo-welcome-title = Welcome!
memo-first-tip = Welcome to Floorp Notes! Here are some instructions on how to use it!
memo-second-tip = Floorp Notes is a notepad that lets you store multiple notes that sync across devices. To enable synchronization, you need to sign in to Floorp with your Firefox account.
memo-third-tip = Floorp Notes will be saved in your Floorp settings and synchronized across devices using Firefox Sync. Firefox Sync encrypts the contents of the sync with your Firefox account password, so no one but you knows its contents.
memo-import-data-tip = Firefox Sync is not a backup service. We recommend you to create backups.
memo-new-title = New Note
chage-view-mode = Toggle View/Edit Mode
readonly-mode = Offline (Read-only)

######################################################################### Default bookmarks ###############################################################
default-bookmark-ablaze-support = Ablaze Support
default-bookmark-notes = Floorp Notes

######################################################################### Like Chrome Download mgr ###############################################################

floorp-delete-all-downloads = 
  .label = Clear Downloads
  .accesskey = D
  .tooltiptext = Clear Downloads

floorp-show-all-downloads =
  .label = Show all downloads
  .accesskey = S
  .tooltiptext = Show all downloads

######################################################################### workspace ###############################################################

workspace-prompt-title = Floorp Workspace
please-enter-workspace-name = Please enter the Workspace's new name.
please-enter-workspace-name-2 = The Workspace's name cannot contain symbols and spaces.
workspace-error = An error occurred:
workspace-error-discription = Either a Workspace with this name exists or the name is invalid.

workspace-button = Workspaces
  .label = Workspaces
  .tooltiptext = Select a Workspace...

workspace-default = Default
workspace-add = 
 .label= New Workspace...

workspace-context-menu-selected-tab =
 .label = Selected tab cannot be moved
move-tab-another-workspace =
 .label = Move to another Workspace
workspace-rename = 
  .label = Rename this Workspace...

workspace-delete = 
  .label = Delete workspace

######################################################################### menubar item ###############################################################

sharemode-menuitem =
  .label = Share Mode
  .accesskey = S


############################################################################## Welcome page ###############################################################

welcome-login-to-firefox-account = Login to Firefox Account
welcome-to-floorp = Welcome to { -brand-short-name }!
welcome-discribe-floorp = { -brand-short-name } is a feature-rich flexible browser that supports various environments and is based on Firefox.
welcome-start-setup = Ready to jump in?
welcome-skip-to-start-browsing = Skip to Start Browsing
welcome-select-preferences-template = Select a template
welcome-minimum-template = Basic
welcome-enable-basic-features = Enable basic features & settings for a simple experience.
welcome-medium-template = Default
welcome-enable-some-features = Enable additional features & settings for a better experience.
welcome-maximum-template = Advanced
welcome-enable-most-of-features = Enable advanced features & settings. Recommended for experienced users.
welcome-go-next-setup = Go to Next Setup
welcome-select-browser-design = Select a Browser Design
welcome-discribe-browser-design = You can choose one of the wonderful third-party { -brand-short-name } designs. OS specific designs are also available at Preferences.
welcome-design-lepton-name = Lepton Original Design
welcome-design-photon-name = Lepton Photon Design
welcome-design-ProtonFix-name = Lepton ProtonFix Design
welcome-design-floorp-fluerial-name = Floorp Fluerial Design
welcome-design-firefox-proton-name = Firefox Proton Design
welcome-import-data = Import Your Browser Data
welcome-import-data-description = Fast setup! Import your bookmarks, passwords, and more from your old browser. Firefox user can import data from Firefox Sync.
welcome-import-data-button = Import Data...
welcome-import-data-skip = Skip Import
welcome-select-button = Select
welcome-finish-setup = Setup Complete!
welcome-finish-setup-description = You're all set! Other settings like Vertical Tabs & Add-ons can be found in about:preferences. Enjoy { -brand-short-name }!
welcomet-finish-setup = Start Browsing the Web
