# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla.org Code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@bengoodger.com> (v2.0)
#   Blake Ross <blakeross@telocity.com>
#   Shawn Wilsher <me@shawnwilsher.com>
#   Ehsan Akhgari <ehsan.akhgari@gmail.com>
#   Rob Campbell <rcampbell@mozilla.com>
#   Paul Rouget <paul@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

#ifdef XP_UNIX
#ifndef XP_MACOSX
#define XP_GNOME 1
#endif
#endif

  <stringbundleset id="stringbundleset">
    <stringbundle id="bundle_brand" src="chrome://branding/locale/brand.properties"/>
    <stringbundle id="bundle_shell" src="chrome://browser/locale/shellservice.properties"/>
    <stringbundle id="bundle_preferences" src="chrome://browser/locale/preferences/preferences.properties"/>
  </stringbundleset>

  <commandset id="mainCommandSet">
    <command id="cmd_newNavigator"                 oncommand="OpenBrowserWindow()"/>
    <command id="cmd_handleBackspace" oncommand="BrowserHandleBackspace();" />
    <command id="cmd_handleShiftBackspace" oncommand="BrowserHandleShiftBackspace();" />

    <command id="cmd_newNavigatorTab" oncommand="BrowserOpenTab();"/>
    <command id="Browser:OpenFile"  oncommand="BrowserOpenFileWindow();"/>
    <command id="Browser:SavePage" oncommand="saveDocument(window.content.document);"/>

    <command id="Browser:SendLink"
             oncommand="MailIntegration.sendLinkForWindow(window.content);"/>

    <command id="cmd_pageSetup" oncommand="PrintUtils.showPageSetup();"/>
    <command id="cmd_print" oncommand="PrintUtils.print();"/>
    <command id="cmd_printPreview" oncommand="PrintUtils.printPreview(PrintPreviewListener);"/>
    <command id="cmd_close" oncommand="BrowserCloseTabOrWindow()"/>
    <command id="cmd_closeWindow" oncommand="BrowserTryToCloseWindow()"/>
    <command id="cmd_ToggleTabsOnTop" oncommand="TabsOnTop.toggle()"/>
    <command id="cmd_CustomizeToolbars" oncommand="BrowserCustomizeToolbar()"/>
    <command id="cmd_quitApplication" oncommand="goQuitApplication()"/>


    <commandset id="editMenuCommands"/>

    <command id="View:PageSource" oncommand="BrowserViewSourceOfDocument(content.document);" observes="isImage"/>
    <command id="View:PageInfo" oncommand="BrowserPageInfo();"/>
    <command id="View:FullScreen" oncommand="BrowserFullScreen();"/>
    <command id="cmd_find"
             oncommand="gFindBar.onFindCommand();"
             observes="isImage"/>
    <command id="cmd_findAgain"
             oncommand="gFindBar.onFindAgainCommand(false);"
             observes="isImage"/>
    <command id="cmd_findPrevious"
             oncommand="gFindBar.onFindAgainCommand(true);"
             observes="isImage"/>
    <!-- work-around bug 392512 -->
    <command id="Browser:AddBookmarkAs"
             oncommand="PlacesCommandHook.bookmarkCurrentPage(true, PlacesUtils.bookmarksMenuFolderId);"/>
    <!-- The command disabled state must be manually updated through
         PlacesCommandHook.updateBookmarkAllTabsCommand() -->
    <command id="Browser:BookmarkAllTabs"
             oncommand="PlacesCommandHook.bookmarkCurrentPages();"/>
    <command id="Browser:Home"    oncommand="BrowserHome();"/>
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
    <command id="Browser:ShowAllTabs" oncommand="allTabs.open();"/>
    <command id="Browser:ToggleTabView" oncommand="TabView.toggle();"/>
    <command id="Browser:FocusNextFrame" oncommand="focusNextFrame(event);"/>
    <command id="cmd_fullZoomReduce"  oncommand="FullZoom.reduce()"/>
    <command id="cmd_fullZoomEnlarge" oncommand="FullZoom.enlarge()"/>
    <command id="cmd_fullZoomReset"   oncommand="FullZoom.reset()"/>
    <command id="cmd_fullZoomToggle"  oncommand="ZoomManager.toggleZoom();"/>
    <command id="Browser:OpenLocation" oncommand="openLocation();"/>

    <command id="Tools:Search" oncommand="BrowserSearch.webSearch();"/>
    <command id="Tools:Downloads" oncommand="BrowserDownloadsUI();"/>
    <command id="Tools:WebConsole" oncommand="HUDConsoleUI.toggleHUD();"/>
    <command id="Tools:Inspect" oncommand="InspectorUI.toggleInspectorUI();" disabled="true"/>
    <command id="Tools:Debugger" oncommand="DebuggerUI.toggleDebugger();" disabled="true"/>
    <command id="Tools:Scratchpad" oncommand="Scratchpad.openScratchpad();" disabled="true"/>
    <command id="Tools:StyleEditor" oncommand="StyleEditor.openChrome();" disabled="true"/>
    <command id="Tools:Addons" oncommand="BrowserOpenAddonsMgr();"/>
    <command id="Tools:Sanitize"
     oncommand="Cc['@mozilla.org/browser/browserglue;1'].getService(Ci.nsIBrowserGlue).sanitize(window);"/>
    <command id="Tools:PrivateBrowsing" oncommand="gPrivateBrowsingUI.toggleMode();"/>
    <command id="History:UndoCloseTab" oncommand="undoCloseTab();"/>
    <command id="History:UndoCloseWindow" oncommand="undoCloseWindow();"/>
    <command id="Browser:ToggleAddonBar" oncommand="toggleAddonBar();"/>
  </commandset>

  <commandset id="placesCommands">
    <command id="Browser:ShowAllBookmarks"
             oncommand="PlacesCommandHook.showPlacesOrganizer('AllBookmarks');"/>
    <command id="Browser:ShowAllHistory"
             oncommand="PlacesCommandHook.showPlacesOrganizer('History');"/>
  </commandset>

  <commandset id="inspectorCommands">
    <command id="Inspector:Inspect"
             oncommand="InspectorUI.toggleInspection();"/>
    <command id="Inspector:Sidebar"
             oncommand="InspectorUI.toggleSidebar();"/>
    <command id="Inspector:Tilt"
             oncommand="Tilt.initialize();"/>
    <command id="Inspector:HTMLPanel"
             oncommand="InspectorUI.toggleHTMLPanel();"/>
    <command id="Inspector:CopyInner"
             oncommand="InspectorUI.copyInnerHTML();"/>
    <command id="Inspector:CopyOuter"
             oncommand="InspectorUI.copyOuterHTML();"/>
    <command id="Inspector:DeleteNode"
             oncommand="InspectorUI.deleteNode();"/>
  </commandset>

  <broadcasterset id="mainBroadcasterSet">
    <broadcaster id="viewBookmarksSidebar" autoCheck="false" label="&bookmarksButton.label;"
                 type="checkbox" group="sidebar" sidebarurl="chrome://browser/content/bookmarks/bookmarksPanel.xul"
                 oncommand="toggleSidebar('viewBookmarksSidebar');"/>

    <!-- for both places and non-places, the sidebar lives at
         chrome://browser/content/history/history-panel.xul so there are no
         problems when switching between versions -->
    <broadcaster id="viewHistorySidebar" autoCheck="false" sidebartitle="&historyButton.label;"
                 type="checkbox" group="sidebar"
                 sidebarurl="chrome://browser/content/history/history-panel.xul"
                 oncommand="toggleSidebar('viewHistorySidebar');"/>

    <broadcaster id="viewWebPanelsSidebar" autoCheck="false"
                 type="checkbox" group="sidebar" sidebarurl="chrome://browser/content/web-panels.xul"
                 oncommand="toggleSidebar('viewWebPanelsSidebar');"/>

    <!-- popup blocking menu items -->
    <broadcaster id="blockedPopupAllowSite"
                 accesskey="&allowPopups.accesskey;"
                 oncommand="gPopupBlockerObserver.toggleAllowPopupsForSite(event);"/>
    <broadcaster id="blockedPopupEditSettings"
#ifdef XP_WIN
                 label="&editPopupSettings.label;"
#else
                 label="&editPopupSettingsUnix.label;"
#endif
                 accesskey="&editPopupSettings.accesskey;"
                 oncommand="gPopupBlockerObserver.editPopupSettings();"/>
    <broadcaster id="blockedPopupDontShowMessage"
                 accesskey="&dontShowMessage.accesskey;"
                 type="checkbox"
                 oncommand="gPopupBlockerObserver.dontShowMessage();"/>
    <broadcaster id="blockedPopupsSeparator"/>
    <broadcaster id="isImage"/>
    <broadcaster id="isFrameImage"/>
    <broadcaster id="singleFeedMenuitemState" disabled="true"/>
    <broadcaster id="multipleFeedsMenuState" hidden="true"/>
    <broadcaster id="tabviewGroupsNumber" groups="1"/>
#ifdef MOZ_SERVICES_SYNC
    <broadcaster id="sync-setup-state"/>
    <broadcaster id="sync-syncnow-state"/>
#endif
    <broadcaster id="workOfflineMenuitemState"/>
  </broadcasterset>

  <keyset id="mainKeyset">
    <key id="key_newNavigator"
         key="&newNavigatorCmd.key;"
         command="cmd_newNavigator"
         modifiers="accel"/>
    <key id="key_newNavigatorTab" key="&tabCmd.commandkey;" modifiers="accel" command="cmd_newNavigatorTab"/>
    <key id="focusURLBar" key="&openCmd.commandkey;" command="Browser:OpenLocation"
         modifiers="accel"/>
#ifndef XP_MACOSX
    <key id="focusURLBar2" key="&urlbar.accesskey;" command="Browser:OpenLocation"
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
    <key id="key_search" key="&searchFocus.commandkey;" command="Tools:Search" modifiers="accel"/>
#ifdef XP_MACOSX
    <key id="key_search2" key="&findOnCmd.commandkey;" command="Tools:Search" modifiers="accel,alt"/>
#endif
#ifdef XP_WIN
    <key id="key_search2" key="&searchFocus.commandkey2;" command="Tools:Search" modifiers="accel"/>
#endif
#ifdef XP_GNOME
    <key id="key_search2" key="&searchFocusUnix.commandkey;" command="Tools:Search" modifiers="accel"/>
    <key id="key_openDownloads" key="&downloadsUnix.commandkey;" command="Tools:Downloads" modifiers="accel,shift"/>
#else
    <key id="key_openDownloads" key="&downloads.commandkey;" command="Tools:Downloads" modifiers="accel"/>
#endif
    <key id="key_openAddons" key="&addons.commandkey;" command="Tools:Addons" modifiers="accel,shift"/>
    <key id="key_errorConsole" key="&errorConsoleCmd.commandkey;" oncommand="toJavaScriptConsole();" modifiers="accel,shift" disabled="true"/>
    <key id="key_webConsole" key="&webConsoleCmd.commandkey;" oncommand="HUDConsoleUI.toggleHUD();"
#ifdef XP_MACOSX
        modifiers="accel,alt"
#else
        modifiers="accel,shift"
#endif
    />
    <key id="key_debugger" key="&debuggerMenu.commandkey;" command="Tools:Debugger"
#ifdef XP_MACOSX
        modifiers="accel,alt"
#else
        modifiers="accel,shift"
#endif
    />
    <key id="key_inspect" key="&inspectMenu.commandkey;" command="Tools:Inspect"
#ifdef XP_MACOSX
        modifiers="accel,alt"
#else
        modifiers="accel,shift"
#endif
    />
    <key id="key_scratchpad" keycode="&scratchpad.keycode;" modifiers="shift"
         keytext="&scratchpad.keytext;" command="Tools:Scratchpad"/>
    <key id="key_styleeditor" keycode="&styleeditor.keycode;" modifiers="shift"
         keytext="&styleeditor.keytext;" command="Tools:StyleEditor"/>
    <key id="openFileKb" key="&openFileCmd.commandkey;" command="Browser:OpenFile"  modifiers="accel"/>
    <key id="key_savePage" key="&savePageCmd.commandkey;" command="Browser:SavePage" modifiers="accel"/>
    <key id="printKb" key="&printCmd.commandkey;" command="cmd_print"  modifiers="accel"/>
    <key id="key_close" key="&closeCmd.key;" command="cmd_close" modifiers="accel"/>
    <key id="key_closeWindow" key="&closeCmd.key;" command="cmd_closeWindow" modifiers="accel,shift"/>
    <key id="key_undo"
         key="&undoCmd.key;"
         modifiers="accel"/>
#ifdef XP_UNIX
    <key id="key_redo" key="&undoCmd.key;" modifiers="accel,shift"/>
#else
    <key id="key_redo" key="&redoCmd.key;" modifiers="accel"/>
#endif
    <key id="key_cut"
         key="&cutCmd.key;"
         modifiers="accel"/>
    <key id="key_copy"
         key="&copyCmd.key;"
         modifiers="accel"/>
    <key id="key_paste"
         key="&pasteCmd.key;"
         modifiers="accel"/>
    <key id="key_delete" keycode="VK_DELETE" command="cmd_delete"/>
    <key id="key_selectAll" key="&selectAllCmd.key;" modifiers="accel"/>

    <key keycode="VK_BACK" command="cmd_handleBackspace"/>
    <key keycode="VK_BACK" command="cmd_handleShiftBackspace" modifiers="shift"/>
#ifndef XP_MACOSX
    <key id="goBackKb"  keycode="VK_LEFT" command="Browser:Back" modifiers="alt"/>
    <key id="goForwardKb"  keycode="VK_RIGHT" command="Browser:Forward" modifiers="alt"/>
#else
    <key id="goBackKb" keycode="VK_LEFT" command="Browser:Back" modifiers="accel" />
    <key id="goForwardKb" keycode="VK_RIGHT" command="Browser:Forward" modifiers="accel" />
#endif
#ifdef XP_UNIX
    <key id="goBackKb2" key="&goBackCmd.commandKey;" command="Browser:Back" modifiers="accel"/>
    <key id="goForwardKb2" key="&goForwardCmd.commandKey;" command="Browser:Forward" modifiers="accel"/>
#endif
    <key id="goHome" keycode="VK_HOME" command="Browser:Home" modifiers="alt"/>
    <key keycode="VK_F5" command="Browser:Reload"/>
#ifndef XP_MACOSX
    <key id="showAllHistoryKb" key="&showAllHistoryCmd.commandkey;" command="Browser:ShowAllHistory" modifiers="accel,shift"/>
    <key keycode="VK_F5" command="Browser:ReloadSkipCache" modifiers="accel"/>
    <key keycode="VK_F6" command="Browser:FocusNextFrame"/>
    <key id="key_fullScreen" keycode="VK_F11" command="View:FullScreen"/>
#else
    <key id="key_fullScreen" key="&fullScreenCmd.macCommandKey;" command="View:FullScreen" modifiers="accel,shift"/>
    <key keycode="VK_F11" command="View:FullScreen"/>
#endif
    <key key="&reloadCmd.commandkey;" command="Browser:Reload" modifiers="accel" id="key_reload"/>
    <key key="&reloadCmd.commandkey;" command="Browser:ReloadSkipCache" modifiers="accel,shift"/>
    <key id="key_viewSource" key="&pageSourceCmd.commandkey;" command="View:PageSource" modifiers="accel"/>
#ifndef XP_WIN
    <key id="key_viewInfo"   key="&pageInfoCmd.commandkey;"   command="View:PageInfo"   modifiers="accel"/>
#endif
    <key id="key_find" key="&findOnCmd.commandkey;" command="cmd_find" modifiers="accel"/>
    <key id="key_findAgain" key="&findAgainCmd.commandkey;" command="cmd_findAgain" modifiers="accel"/>
    <key id="key_findPrevious" key="&findAgainCmd.commandkey;" command="cmd_findPrevious" modifiers="accel,shift"/>
    <key keycode="&findAgainCmd.commandkey2;" command="cmd_findAgain"/>
    <key keycode="&findAgainCmd.commandkey2;"  command="cmd_findPrevious" modifiers="shift"/>

    <key id="addBookmarkAsKb" key="&bookmarkThisPageCmd.commandkey;" command="Browser:AddBookmarkAs" modifiers="accel"/>
# Accel+Shift+A-F are reserved on GTK2
#ifndef MOZ_WIDGET_GTK2
    <key id="bookmarkAllTabsKb" key="&bookmarkThisPageCmd.commandkey;" oncommand="PlacesCommandHook.bookmarkCurrentPages();" modifiers="accel,shift"/>
    <key id="manBookmarkKb" key="&bookmarksCmd.commandkey;" command="Browser:ShowAllBookmarks" modifiers="accel,shift"/>
#else
    <key id="manBookmarkKb" key="&bookmarksGtkCmd.commandkey;" command="Browser:ShowAllBookmarks" modifiers="accel,shift"/>
#endif
    <key id="viewBookmarksSidebarKb" key="&bookmarksCmd.commandkey;" command="viewBookmarksSidebar" modifiers="accel"/>
#ifdef XP_WIN
# Cmd+I is conventially mapped to Info on MacOS X, thus it should not be
# overridden for other purposes there.
    <key id="viewBookmarksSidebarWinKb" key="&bookmarksWinCmd.commandkey;" command="viewBookmarksSidebar" modifiers="accel"/>
#endif

# don't use |command="Browser:Stop"|, ESC is being used to freeze animated gifs,
# even if the stop button and menuitem are disabled (see Bug 284140)
    <key id="key_stop" keycode="VK_ESCAPE" oncommand="BrowserStop();"/>

#ifdef XP_MACOSX
    <key id="key_stop_mac" modifiers="accel" key="&stopCmd.macCommandKey;" oncommand="BrowserStop();"/>
#endif

    <key id="key_gotoHistory"
         key="&historySidebarCmd.commandKey;"
#ifdef XP_MACOSX
         modifiers="accel,shift"
#else
         modifiers="accel"
#endif
         command="viewHistorySidebar"/>

    <key id="key_fullZoomReduce"  key="&fullZoomReduceCmd.commandkey;"   command="cmd_fullZoomReduce"  modifiers="accel"/>
    <key                          key="&fullZoomReduceCmd.commandkey2;"  command="cmd_fullZoomReduce"  modifiers="accel"/>
    <key id="key_fullZoomEnlarge" key="&fullZoomEnlargeCmd.commandkey;"  command="cmd_fullZoomEnlarge" modifiers="accel"/>
    <key                          key="&fullZoomEnlargeCmd.commandkey2;" command="cmd_fullZoomEnlarge" modifiers="accel"/>
    <key                          key="&fullZoomEnlargeCmd.commandkey3;" command="cmd_fullZoomEnlarge" modifiers="accel"/>
    <key id="key_fullZoomReset"   key="&fullZoomResetCmd.commandkey;"    command="cmd_fullZoomReset"   modifiers="accel"/>
    <key                          key="&fullZoomResetCmd.commandkey2;"   command="cmd_fullZoomReset"   modifiers="accel"/>

    <key id="key_showAllTabs" command="Browser:ShowAllTabs" keycode="VK_TAB" modifiers="control,shift"/>

    <key id="key_switchTextDirection" key="&bidiSwitchTextDirectionItem.commandkey;" command="cmd_switchTextDirection" modifiers="accel,shift" />

    <key id="key_tabview" key="&tabView.commandkey;" command="Browser:ToggleTabView" modifiers="accel,shift"/>

    <key id="key_privatebrowsing" command="Tools:PrivateBrowsing" key="&privateBrowsingCmd.commandkey;" modifiers="accel,shift"/>
    <key id="key_sanitize" command="Tools:Sanitize" keycode="VK_DELETE" modifiers="accel,shift"/>
#ifdef XP_MACOSX
    <key id="key_sanitize_mac" command="Tools:Sanitize" keycode="VK_BACK" modifiers="accel,shift"/>
#endif
#ifdef XP_UNIX
    <key id="key_quitApplication" key="&quitApplicationCmdMac.key;" command="cmd_quitApplication" modifiers="accel"/>
#endif

#ifdef FULL_BROWSER_WINDOW
    <key id="key_undoCloseTab" command="History:UndoCloseTab" key="&tabCmd.commandkey;" modifiers="accel,shift"/>
#endif
    <key id="key_undoCloseWindow" command="History:UndoCloseWindow" key="&newNavigatorCmd.key;" modifiers="accel,shift"/>

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

    <key id="key_toggleAddonBar" command="Browser:ToggleAddonBar" key="&toggleAddonBarCmd.key;" modifiers="accel"/>

  </keyset>

# Used by baseMenuOverlay
#ifdef XP_MACOSX
  <commandset id="baseMenuCommandSet" />
#endif
  <keyset id="baseMenuKeyset" />
