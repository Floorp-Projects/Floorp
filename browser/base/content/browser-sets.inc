# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef XP_UNIX
#ifndef XP_MACOSX
#define XP_GNOME 1
#endif
#endif

  <stringbundleset id="stringbundleset">
    <stringbundle id="bundle_brand" src="chrome://branding/locale/brand.properties"/>
    <stringbundle id="bundle_shell" src="chrome://browser/locale/shellservice.properties"/>
  </stringbundleset>

  <commandset id="mainCommandSet">
    <command id="cmd_newNavigator" oncommand="OpenBrowserWindow()"/>
    <command id="cmd_handleBackspace" oncommand="BrowserHandleBackspace();" />
    <command id="cmd_handleShiftBackspace" oncommand="BrowserHandleShiftBackspace();" />

    <command id="cmd_newNavigatorTab" oncommand="BrowserOpenTab(event);"/>
    <command id="cmd_newNavigatorTabNoEvent" oncommand="BrowserOpenTab();"/>
    <command id="Browser:OpenFile"  oncommand="BrowserOpenFileWindow();"/>
    <command id="Browser:SavePage" oncommand="saveBrowser(gBrowser.selectedBrowser);"/>

    <command id="Browser:SendLink"
             oncommand="MailIntegration.sendLinkForBrowser(gBrowser.selectedBrowser);"/>

    <command id="cmd_pageSetup" oncommand="PrintUtils.showPageSetup();"/>
    <command id="cmd_print" oncommand="PrintUtils.printWindow(window.gBrowser.selectedBrowser.browsingContext);"/>
    <command id="cmd_printPreview" oncommand="PrintUtils.printPreview(PrintPreviewListener);"/>
    <command id="cmd_file_importFromAnotherBrowser" oncommand="MigrationUtils.showMigrationWizard(window, [MigrationUtils.MIGRATION_ENTRYPOINT_FILE_MENU]);"/>
    <command id="cmd_help_importFromAnotherBrowser" oncommand="MigrationUtils.showMigrationWizard(window, [MigrationUtils.MIGRATION_ENTRYPOINT_HELP_MENU]);"/>
    <command id="cmd_close" oncommand="BrowserCloseTabOrWindow(event);"/>
    <command id="cmd_closeWindow" oncommand="BrowserTryToCloseWindow()"/>
    <command id="cmd_toggleMute" oncommand="gBrowser.toggleMuteAudioOnMultiSelectedTabs(gBrowser.selectedTab)"/>
    <command id="cmd_CustomizeToolbars" oncommand="gCustomizeMode.enter()"/>
    <command id="cmd_toggleOfflineStatus" oncommand="BrowserOffline.toggleOfflineStatus();"/>
    <command id="cmd_quitApplication" oncommand="goQuitApplication()"/>

    <command id="View:PageSource" oncommand="BrowserViewSource(window.gBrowser.selectedBrowser);"/>
    <command id="View:PageInfo" oncommand="BrowserPageInfo();"/>
    <command id="View:FullScreen" oncommand="BrowserFullScreen();"/>
    <command id="View:ReaderView" oncommand="ReaderParent.toggleReaderMode(event);"/>
    <command id="View:PictureInPicture" oncommand="PictureInPicture.onCommand(event);"/>
    <command id="cmd_find" oncommand="gLazyFindCommand('onFindCommand')"/>
    <command id="cmd_findAgain" oncommand="gLazyFindCommand('onFindAgainCommand', false)"/>
    <command id="cmd_findPrevious" oncommand="gLazyFindCommand('onFindAgainCommand', true)"/>
#ifdef XP_MACOSX
    <command id="cmd_findSelection" oncommand="gLazyFindCommand('onFindSelectionCommand')"/>
#endif
    <!-- work-around bug 392512 -->
    <command id="Browser:AddBookmarkAs"
             oncommand="PlacesCommandHook.bookmarkPage();"/>
    <command id="Browser:BookmarkAllTabs"
             oncommand="PlacesUIUtils.showBookmarkPagesDialog(PlacesCommandHook.uniqueCurrentPages);"/>
    <command id="Browser:Back"    oncommand="BrowserBack();" disabled="true"/>
    <command id="Browser:BackOrBackDuplicate" oncommand="BrowserBack(event);" disabled="true">
      <observes element="Browser:Back" attribute="disabled"/>
    </command>
    <command id="Browser:Forward" oncommand="BrowserForward();" disabled="true"/>
    <command id="Browser:ForwardOrForwardDuplicate" oncommand="BrowserForward(event);" disabled="true">
      <observes element="Browser:Forward" attribute="disabled"/>
    </command>
    <command id="Browser:Stop"    oncommand="BrowserStop();" disabled="true"/>
    <command id="Browser:Reload"  oncommand="if (event.shiftKey) BrowserReloadSkipCache(); else BrowserReload()" disabled="true"/>
    <command id="Browser:ReloadOrDuplicate" oncommand="BrowserReloadOrDuplicate(event)" disabled="true">
      <observes element="Browser:Reload" attribute="disabled"/>
    </command>
    <command id="Browser:ReloadSkipCache" oncommand="BrowserReloadSkipCache()" disabled="true">
      <observes element="Browser:Reload" attribute="disabled"/>
    </command>
    <command id="Browser:NextTab" oncommand="gBrowser.tabContainer.advanceSelectedTab(1, true);"/>
    <command id="Browser:PrevTab" oncommand="gBrowser.tabContainer.advanceSelectedTab(-1, true);"/>
    <command id="Browser:ShowAllTabs" oncommand="gTabsPanel.showAllTabsPanel();"/>
    <command id="cmd_fullZoomReduce"  oncommand="FullZoom.reduce()"/>
    <command id="cmd_fullZoomEnlarge" oncommand="FullZoom.enlarge()"/>
    <command id="cmd_fullZoomReset"   oncommand="FullZoom.reset()"/>
    <command id="cmd_fullZoomToggle"  oncommand="ZoomManager.toggleZoom();"/>
    <command id="cmd_gestureRotateLeft" oncommand="gGestureSupport.rotate(event.sourceEvent)"/>
    <command id="cmd_gestureRotateRight" oncommand="gGestureSupport.rotate(event.sourceEvent)"/>
    <command id="cmd_gestureRotateEnd" oncommand="gGestureSupport.rotateEnd()"/>
    <command id="Browser:OpenLocation" oncommand="openLocation(event);"/>
    <command id="Browser:RestoreLastSession" oncommand="SessionStore.restoreLastSession();" disabled="true"/>
    <command id="Browser:NewUserContextTab" oncommand="openNewUserContextTab(event.sourceEvent);"/>
    <command id="Browser:OpenAboutContainers" oncommand="openPreferences('paneContainers');"/>
    <command id="Tools:Search" oncommand="BrowserSearch.webSearch();"/>
    <command id="Tools:Downloads" oncommand="BrowserDownloadsUI();"/>
    <command id="Tools:Addons" oncommand="BrowserOpenAddonsMgr();"/>
    <command id="Tools:Sanitize" oncommand="Sanitizer.showUI(window);"/>
    <command id="Tools:PrivateBrowsing"
      oncommand="OpenBrowserWindow({private: true});"/>
#ifdef NIGHTLY_BUILD
    <command id="Tools:FissionWindow"
      oncommand="OpenBrowserWindow({fission: true});"
      hidden="true"/>
    <command id="Tools:NonFissionWindow"
      oncommand="OpenBrowserWindow({fission: false});"
      hidden="true"/>
#endif
    <command id="History:UndoCloseTab" oncommand="undoCloseTab();"/>
    <command id="History:UndoCloseWindow" oncommand="undoCloseWindow();"/>

    <command id="wrCaptureCmd" oncommand="gGfxUtils.webrenderCapture();" disabled="true"/>
    <command id="wrToggleCaptureSequenceCmd" oncommand="gGfxUtils.toggleWebrenderCaptureSequence();" disabled="true"/>
#ifdef NIGHTLY_BUILD
    <command id="windowRecordingCmd" oncommand="gGfxUtils.toggleWindowRecording();"/>
    <command id="wrTransactionLoggingCmd" oncommand="gGfxUtils.toggleTransactionLogging();"/>
#endif
#ifdef XP_MACOSX
    <command id="minimizeWindow"
             data-l10n-id="window-minimize-command"
             oncommand="window.minimize();" />
    <command id="zoomWindow"
             data-l10n-id="window-zoom-command"
             oncommand="zoomWindow();" />
#endif
  </commandset>

#include ../../components/places/content/placesCommands.inc.xhtml

  <keyset id="mainKeyset">
    <key id="key_newNavigator"
         data-l10n-id="window-new-shortcut"
         command="cmd_newNavigator"
         modifiers="accel" reserved="true"/>
    <key id="key_newNavigatorTab" data-l10n-id="tab-new-shortcut" modifiers="accel"
         command="cmd_newNavigatorTabNoEvent" reserved="true"/>
    <key id="focusURLBar" data-l10n-id="location-open-shortcut" command="Browser:OpenLocation"
         modifiers="accel"/>
#ifndef XP_MACOSX
    <key id="focusURLBar2" data-l10n-id="location-open-shortcut-alt" command="Browser:OpenLocation"
         modifiers="alt"/>
#endif

#
# Search Command Key Logic works like this:
#
# Unix: Ctrl+K (cross platform binding)
#       Ctrl+J (in case of emacs Ctrl-K conflict)
# Mac:  Cmd+K (cross platform binding)
#       Cmd+Opt+F (platform convention)
# Win:  Ctrl+K (cross platform binding)
#       Ctrl+E (IE compat)
#
# We support Ctrl+K on all platforms now and advertise it in the menu since it is
# our standard - it is a "safe" choice since it is near no harmful keys like "W" as
# "E" is. People mourning the loss of Ctrl+K for emacs compat can switch their GTK
# system setting to use emacs emulation, and we should respect it. Focus-Search-Box
# is a fundamental keybinding and we are maintaining a XP binding so that it is easy
# for people to switch to Linux.
#
    <key id="key_search" data-l10n-id="search-focus-shortcut" command="Tools:Search" modifiers="accel"/>
    <key id="key_search2"
#ifdef XP_MACOSX
         data-l10n-id="find-shortcut"
         modifiers="accel,alt"
#else
         data-l10n-id="search-focus-shortcut-alt"
         modifiers="accel"
#endif
         command="Tools:Search"/>
    <key id="key_openDownloads"
         data-l10n-id="downloads-shortcut"
#ifdef XP_GNOME
         modifiers="accel,shift"
#else
         modifiers="accel"
#endif
         command="Tools:Downloads"/>
    <key id="key_openAddons" data-l10n-id="addons-shortcut" command="Tools:Addons" modifiers="accel,shift"/>
    <key id="openFileKb" data-l10n-id="file-open-shortcut" command="Browser:OpenFile"  modifiers="accel"/>
    <key id="key_savePage" data-l10n-id="save-page-shortcut" command="Browser:SavePage" modifiers="accel"/>
    <key id="printKb" data-l10n-id="print-shortcut" command="cmd_print"  modifiers="accel"/>
    <key id="key_close" data-l10n-id="close-shortcut" command="cmd_close" modifiers="accel" reserved="true"/>
    <key id="key_closeWindow" data-l10n-id="close-shortcut" command="cmd_closeWindow" modifiers="accel,shift" reserved="true"/>
    <key id="key_toggleMute" data-l10n-id="mute-toggle-shortcut" command="cmd_toggleMute" modifiers="control"/>
    <key id="key_undo"
         data-l10n-id="text-action-undo-shortcut"
         modifiers="accel"/>
    <key id="key_redo"
#ifdef XP_UNIX
         data-l10n-id="text-action-undo-shortcut"
         modifiers="accel,shift"
#else
         data-l10n-id="text-action-redo-shortcut"
         modifiers="accel"
#endif
         />
    <key id="key_cut"
         data-l10n-id="text-action-cut-shortcut"
         modifiers="accel"/>
    <key id="key_copy"
         data-l10n-id="text-action-copy-shortcut"
         modifiers="accel"/>
    <key id="key_paste"
         data-l10n-id="text-action-paste-shortcut"
         modifiers="accel"/>
    <key id="key_delete" keycode="VK_DELETE" command="cmd_delete" reserved="false"/>
    <key id="key_selectAll" data-l10n-id="text-action-select-all-shortcut" modifiers="accel"/>

    <key keycode="VK_BACK" command="cmd_handleBackspace" reserved="false"/>
    <key keycode="VK_BACK" command="cmd_handleShiftBackspace" modifiers="shift" reserved="false"/>
#ifndef XP_MACOSX
    <key id="goBackKb"  keycode="VK_LEFT" command="Browser:Back" modifiers="alt"/>
    <key id="goForwardKb"  keycode="VK_RIGHT" command="Browser:Forward" modifiers="alt"/>
#else
    <key id="goBackKb" keycode="VK_LEFT" command="Browser:Back" modifiers="accel" />
    <key id="goForwardKb" keycode="VK_RIGHT" command="Browser:Forward" modifiers="accel" />
#endif
#ifdef XP_UNIX
    <key id="goBackKb2" data-l10n-id="nav-back-shortcut-alt" command="Browser:Back" modifiers="accel"/>
    <key id="goForwardKb2" data-l10n-id="nav-fwd-shortcut-alt" command="Browser:Forward" modifiers="accel"/>
#endif
    <key id="goHome" keycode="VK_HOME" oncommand="BrowserHome();" modifiers="alt"/>
    <key keycode="VK_F5" command="Browser:Reload"/>
#ifndef XP_MACOSX
    <key id="showAllHistoryKb" data-l10n-id="history-show-all-shortcut" command="Browser:ShowAllHistory" modifiers="accel,shift"/>
    <key keycode="VK_F5" command="Browser:ReloadSkipCache" modifiers="accel"/>
    <key id="key_fullScreen" keycode="VK_F11" command="View:FullScreen"/>
#else
    <key id="key_fullScreen" data-l10n-id="full-screen-shortcut" command="View:FullScreen" modifiers="accel,control"/>
    <key id="key_fullScreen_old" data-l10n-id="full-screen-shortcut" command="View:FullScreen" modifiers="accel,shift"/>
    <key keycode="VK_F11" command="View:FullScreen"/>
#endif
    <key id="key_toggleReaderMode"
         command="View:ReaderView"
#ifdef XP_WIN
         data-l10n-id="reader-mode-toggle-shortcut-windows"
#else
         data-l10n-id="reader-mode-toggle-shortcut-other"
         modifiers="accel,alt"
#endif
         disabled="true"/>

#ifndef XP_MACOSX
    <key id="key_togglePictureInPicture" data-l10n-id="picture-in-picture-toggle-shortcut" command="View:PictureInPicture" modifiers="accel,shift"/>
    <key                                 data-l10n-id="picture-in-picture-toggle-shortcut-alt" command="View:PictureInPicture" modifiers="accel,shift"/>
#endif

    <key data-l10n-id="nav-reload-shortcut" command="Browser:Reload" modifiers="accel" id="key_reload"/>
    <key data-l10n-id="nav-reload-shortcut" command="Browser:ReloadSkipCache" modifiers="accel,shift" id="key_reload_skip_cache"/>
    <key id="key_viewSource" data-l10n-id="page-source-shortcut" command="View:PageSource" modifiers="accel"/>
#ifdef XP_MACOSX
    <key id="key_viewSourceSafari" data-l10n-id="page-source-shortcut-safari" command="View:PageSource" modifiers="accel,alt"/>
#endif
    <key id="key_viewInfo" data-l10n-id="page-info-shortcut" command="View:PageInfo"   modifiers="accel"/>
    <key id="key_find" data-l10n-id="find-shortcut" command="cmd_find" modifiers="accel"/>
    <key id="key_findAgain" data-l10n-id="search-find-again-shortcut" command="cmd_findAgain" modifiers="accel"/>
    <key id="key_findPrevious" data-l10n-id="search-find-again-shortcut" command="cmd_findPrevious" modifiers="accel,shift"/>
#ifdef XP_MACOSX
    <key id="key_findSelection" data-l10n-id="search-find-selection-shortcut" command="cmd_findSelection" modifiers="accel"/>
#endif
    <key data-l10n-id="search-find-again-shortcut-alt" command="cmd_findAgain"/>
    <key data-l10n-id="search-find-again-shortcut-alt" command="cmd_findPrevious" modifiers="shift"/>

    <key id="addBookmarkAsKb" data-l10n-id="bookmark-this-page-shortcut" command="Browser:AddBookmarkAs" modifiers="accel"/>
    <key id="bookmarkAllTabsKb"
         data-l10n-id="bookmark-this-page-shortcut"
         oncommand="PlacesUIUtils.showBookmarkPagesDialog(PlacesCommandHook.uniqueCurrentPages);"
         modifiers="accel,shift"/>
    <key id="manBookmarkKb" data-l10n-id="bookmark-show-all-shortcut" command="Browser:ShowAllBookmarks" modifiers="accel,shift"/>
    <key id="viewBookmarksSidebarKb"
         data-l10n-id="bookmark-show-sidebar-shortcut"
         modifiers="accel"
         oncommand="SidebarUI.toggle('viewBookmarksSidebar');"/>

    <key id="key_stop" keycode="VK_ESCAPE" command="Browser:Stop"/>

#ifdef XP_MACOSX
    <key id="key_stop_mac" modifiers="accel" data-l10n-id="nav-stop-shortcut" command="Browser:Stop"/>
#endif

    <key id="key_gotoHistory"
         data-l10n-id="history-sidebar-shortcut"
#ifdef XP_MACOSX
         modifiers="accel,shift"
#else
         modifiers="accel"
#endif
         oncommand="SidebarUI.toggle('viewHistorySidebar');"/>

    <key id="key_fullZoomReduce" data-l10n-id="full-zoom-reduce-shortcut" command="cmd_fullZoomReduce"  modifiers="accel"/>
    <key                         data-l10n-id="full-zoom-reduce-shortcut-alt" command="cmd_fullZoomReduce"  modifiers="accel"/>
    <key id="key_fullZoomEnlarge" data-l10n-id="full-zoom-enlarge-shortcut" command="cmd_fullZoomEnlarge" modifiers="accel"/>
    <key                          data-l10n-id="full-zoom-enlarge-shortcut-alt" command="cmd_fullZoomEnlarge" modifiers="accel"/>
    <key                          data-l10n-id="full-zoom-enlarge-shortcut-alt2" command="cmd_fullZoomEnlarge" modifiers="accel"/>
    <key id="key_fullZoomReset"   data-l10n-id="full-zoom-reset-shortcut"    command="cmd_fullZoomReset"   modifiers="accel"/>
    <key                          data-l10n-id="full-zoom-reset-shortcut-alt"   command="cmd_fullZoomReset"   modifiers="accel"/>

    <key id="key_showAllTabs" keycode="VK_TAB" modifiers="control,shift"/>

    <key id="key_switchTextDirection" data-l10n-id="bidi-switch-direction-shortcut" command="cmd_switchTextDirection" modifiers="accel,shift" />

    <key id="key_privatebrowsing" command="Tools:PrivateBrowsing" data-l10n-id="private-browsing-shortcut"
         modifiers="accel,shift" reserved="true"/>
    <key id="key_sanitize" command="Tools:Sanitize" keycode="VK_DELETE" modifiers="accel,shift"/>
#ifdef XP_MACOSX
    <key id="key_sanitize_mac" command="Tools:Sanitize" keycode="VK_BACK" modifiers="accel,shift"/>
#endif
    <key id="key_quitApplication" data-l10n-id="quit-app-shortcut"
#ifdef XP_WIN
         modifiers="accel,shift"
#else
         modifiers="accel"
#endif
# On OS X, dark voodoo magic invokes the quit code for this key.
# So we're not adding the attribute on OSX because of backwards/add-on compat.
# See bug 1369909 for background on this.
#ifndef XP_MACOSX
         command="cmd_quitApplication"
#endif
         reserved="true"/>

    <key id="key_undoCloseTab" command="History:UndoCloseTab" data-l10n-id="tab-new-shortcut" modifiers="accel,shift"/>
    <key id="key_undoCloseWindow" command="History:UndoCloseWindow" data-l10n-id="window-new-shortcut" modifiers="accel,shift"/>

#ifdef XP_GNOME
#define NUM_SELECT_TAB_MODIFIER alt
#else
#define NUM_SELECT_TAB_MODIFIER accel
#endif

#expand    <key id="key_selectTab1" oncommand="gBrowser.selectTabAtIndex(0, event);" key="1" modifiers="__NUM_SELECT_TAB_MODIFIER__"/>
#expand    <key id="key_selectTab2" oncommand="gBrowser.selectTabAtIndex(1, event);" key="2" modifiers="__NUM_SELECT_TAB_MODIFIER__"/>
#expand    <key id="key_selectTab3" oncommand="gBrowser.selectTabAtIndex(2, event);" key="3" modifiers="__NUM_SELECT_TAB_MODIFIER__"/>
#expand    <key id="key_selectTab4" oncommand="gBrowser.selectTabAtIndex(3, event);" key="4" modifiers="__NUM_SELECT_TAB_MODIFIER__"/>
#expand    <key id="key_selectTab5" oncommand="gBrowser.selectTabAtIndex(4, event);" key="5" modifiers="__NUM_SELECT_TAB_MODIFIER__"/>
#expand    <key id="key_selectTab6" oncommand="gBrowser.selectTabAtIndex(5, event);" key="6" modifiers="__NUM_SELECT_TAB_MODIFIER__"/>
#expand    <key id="key_selectTab7" oncommand="gBrowser.selectTabAtIndex(6, event);" key="7" modifiers="__NUM_SELECT_TAB_MODIFIER__"/>
#expand    <key id="key_selectTab8" oncommand="gBrowser.selectTabAtIndex(7, event);" key="8" modifiers="__NUM_SELECT_TAB_MODIFIER__"/>
#expand    <key id="key_selectLastTab" oncommand="gBrowser.selectTabAtIndex(-1, event);" key="9" modifiers="__NUM_SELECT_TAB_MODIFIER__"/>

    <key id="key_wrCaptureCmd"
#ifdef XP_MACOSX
    key="3" modifiers="control,shift"
#else
    key="#" modifiers="control"
#endif
    command="wrCaptureCmd"/>
    <key id="key_wrToggleCaptureSequenceCmd"
#ifdef XP_MACOSX
    key="6" modifiers="control,shift"
#else
    key="^" modifiers="control"
#endif
    command="wrToggleCaptureSequenceCmd"/>
#ifdef NIGHTLY_BUILD
    <key id="key_windowRecordingCmd"
#ifdef XP_MACOSX
    key="4" modifiers="control,shift"
#else
    key="$" modifiers="control"
#endif
    command="windowRecordingCmd"/>
    <key id="key_transactionLoggingCmd"
#ifdef XP_MACOSX
    key="5" modifiers="control,shift"
#else
    key="%" modifiers="control"
#endif
    command="wrTransactionLoggingCmd"/>
#endif
#ifdef XP_MACOSX
    <key id="key_minimizeWindow"
         command="minimizeWindow"
         data-l10n-id="window-minimize-shortcut"
         modifiers="accel"/>
    <key id="key_openHelpMac"
         oncommand="openHelpLink('firefox-osxkey');"
         data-l10n-id="help-shortcut"
         modifiers="accel"/>
    <!-- These are used to build the Application menu -->
    <key id="key_preferencesCmdMac"
         data-l10n-id="preferences-shortcut"
         modifiers="accel"/>
    <key id="key_hideThisAppCmdMac"
         data-l10n-id="hide-app-shortcut"
         modifiers="accel"/>
    <key id="key_hideOtherAppsCmdMac"
         data-l10n-id="hide-other-apps-shortcut"
         modifiers="accel,alt"/>
#endif
  </keyset>
