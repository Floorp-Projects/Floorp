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

  <commandset id="globalEditCommandSet"
              commandupdater="true"
              events="focus,select"
              oncommandupdate="goUpdateGlobalEditMenuItems()"/>
  <commandset id="selectEditCommandSet"
              commandupdater="true"
              events="select"
              oncommandupdate="goUpdateSelectEditMenuItems()"/>
  <commandset id="undoEditCommandSet"
              commandupdater="true"
              events="undo"
              oncommandupdate="goUpdateUndoEditMenuItems()"/>
  <commandset id="pasteCommandSet"
              commandupdater="true"
              events="clipboard"
              oncommandupdate="goUpdatePasteMenuItems()"/>
  
  <commandset id="mainCommandSet">
    <command id="cmd_newNavigator"                 oncommand="OpenBrowserWindow()"/>
    <command id="cmd_bm_open"                      oncommand="goDoCommand('cmd_bm_open');"/>
    <command id="cmd_bm_openinnewwindow"           oncommand="goDoCommand('cmd_bm_openinnewwindow');"/>
    <command id="cmd_bm_openinnewtab"              oncommand="goDoCommand('cmd_bm_openinnewtab');"/>
    <command id="cmd_bm_openfolder"                oncommand="goDoCommand('cmd_bm_openfolder');"/>
    <command id="cmd_bm_managefolder"              oncommand="goDoCommand('cmd_bm_managefolder');"/>
    <command id="cmd_bm_newfolder"                 oncommand="goDoCommand('cmd_bm_newfolder');"/>
    <command id="cmd_bm_newbookmark"               oncommand="goDoCommand('cmd_bm_newbookmark');"/>
    <command id="cmd_bm_newseparator"              oncommand="goDoCommand('cmd_bm_newseparator');"/>
    <command id="cmd_bm_properties"                oncommand="goDoCommand('cmd_bm_properties');"/>
    <command id="cmd_bm_refreshlivemark"           oncommand="goDoCommand('cmd_bm_refreshlivemark');"/>
    <command id="cmd_bm_refreshmicrosummary"       oncommand="goDoCommand('cmd_bm_refreshmicrosummary');"/>
    <command id="cmd_bm_rename"                    oncommand="goDoCommand('cmd_bm_rename');"/>
    <command id="cmd_bm_moveBookmark"              oncommand="goDoCommand('cmd_bm_moveBookmark');"/>
    <command id="cmd_bm_sortbyname"                oncommand="goDoCommand('cmd_bm_sortbyname');"/>

    <command id="cmd_handleBackspace" oncommand="BrowserHandleBackspace();" />
    <command id="cmd_handleShiftBackspace" oncommand="BrowserHandleShiftBackspace();" />

    <command id="cmd_newNavigatorTab" oncommand="BrowserOpenTab();"/>
    <command id="Browser:OpenFile"  oncommand="BrowserOpenFileWindow();"/>
    <command id="Browser:SavePage" oncommand="saveDocument(window.content.document);"/>
    <command id="Browser:SaveFrame" oncommand="saveFrameDocument();"/>

    <command id="Browser:SendLink" oncommand="MailIntegration.sendLinkForContent();"/>

    <command id="cmd_pageSetup" oncommand="PrintUtils.showPageSetup();"/>
    <command id="cmd_print" oncommand="PrintUtils.print();"/>
    <command id="cmd_printPreview" oncommand="PrintUtils.printPreview(onEnterPrintPreview, onExitPrintPreview);"/>
    <command id="cmd_close" oncommand="BrowserCloseTabOrWindow()"/>
    <command id="cmd_closeWindow" oncommand="BrowserTryToCloseWindow()"/>
    <command id="cmd_toggleTaskbar" oncommand="goToggleToolbar('status-bar','toggle_taskbar');"/>
    <command id="cmd_CustomizeToolbars" oncommand="BrowserCustomizeToolbar()"/>


    <command id="cmd_undo" 
             oncommand="goDoCommand('cmd_undo')"
             disabled="true"/>
    <command id="cmd_redo" 
             oncommand="goDoCommand('cmd_redo')"
             disabled="true"/>
    <command id="cmd_cut" 
             oncommand="goDoCommand('cmd_cut')"
             disabled="true"/>
    <command id="cmd_copy" 
             oncommand="goDoCommand('cmd_copy')"
             disabled="true"/>
    <command id="cmd_paste" 
             oncommand="goDoCommand('cmd_paste')"
             disabled="true"/>
    <command id="cmd_delete"
             oncommand="goDoCommand('cmd_delete')"
             valueDefault="&deleteCmd.label;"
             disabled="true"/>
    <command id="cmd_selectAll"
             oncommand="goDoCommand('cmd_selectAll')"
             disabled="true"/>
    <command id="cmd_switchTextDirection"
             oncommand="goDoCommand('cmd_switchTextDirection');"
             disabled="true"/>

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
#ifdef MOZ_PLACES_BOOKMARKS
    <command id="Browser:AddBookmarkAs" 
             oncommand="PlacesCommandHook.bookmarkCurrentPage();"/>
#else
    <command id="Browser:AddBookmarkAs" oncommand="addBookmarkAs(document.getElementById('content'), false);"/>
#endif
    <!-- The command is disabled for the hidden window. Otherwise its enabled
         state is handler by the BookmarkAllTabsHandler object. -->
    <command id="Browser:BookmarkAllTabs"
             oncommand="gBookmarkAllTabsHandler.doCommand();"
             disabled="true"/>
    <command id="Browser:Home"    oncommand="BrowserHome();"/>
    <command id="Browser:Back"    oncommand="BrowserBack();" disabled="true"/>
    <command id="Browser:Forward" oncommand="BrowserForward();" disabled="true"/>
    <command id="Browser:Stop"    oncommand="BrowserStop();" disabled="true"/>
    <command id="Browser:Reload"  oncommand="if (event.shiftKey) BrowserReloadSkipCache(); else BrowserReload()" disabled="true"/>
    <command id="Browser:ReloadSkipCache"  oncommand="BrowserReloadSkipCache()" disabled="true"/>
    <command id="cmd_textZoomReduce"  oncommand="ZoomManager.prototype.getInstance().reduce();"/>
    <command id="cmd_textZoomEnlarge" oncommand="ZoomManager.prototype.getInstance().enlarge();"/>
    <command id="cmd_textZoomReset" oncommand="ZoomManager.prototype.getInstance().reset();"/>
    <command id="Browser:OpenLocation" oncommand="openLocation();"/>

    <command id="Tools:Search" oncommand="BrowserSearch.webSearch();"/>    
    <command id="Tools:Downloads" oncommand="toOpenWindowByType('Download:Manager', 
                                                                'chrome://mozapps/content/downloads/downloads.xul',
                                                                'chrome,dialog=no,resizable');"/>
    <command id="Tools:Addons" oncommand="BrowserOpenAddonsMgr();"/>
    <command id="Tools:Sanitize" oncommand="gBrowserGlue.sanitize(window || null);"/>
    <command id="History:UndoCloseTab" oncommand="undoCloseTab();"/>
  </commandset>

#ifdef MOZ_PLACES
  <commandset id="placesCommands">
    <command id="Browser:ShowHistory" 
             oncommand="PlacesCommandHook.showPlacesOrganizer(ORGANIZER_ROOT_HISTORY);"/>
    <command id="Browser:SearchHistory"
             oncommand="HistoryMenu.showPlacesSearch();"/>
#ifdef MOZ_PLACES_BOOKMARKS
    <command id="Browser:ShowBookmarks" 
             oncommand="PlacesCommandHook.showPlacesOrganizer(ORGANIZER_ROOT_BOOKMARKS);"/>           
#endif
  </commandset>
#endif

  <broadcasterset id="mainBroadcasterSet">
#ifdef MOZ_PLACES_NEW_HISTORY_UI
    <broadcaster id="viewHistoryPlaces" autoCheck="false" label="&historyButton.label;"
                 oncommand="PlacesCommandHook.showPlacesPopup(event);"/>
#else
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
#endif

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
# Mac:  Ctrl+K (cross platform binding)
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
# Do *not* tamper with these values without talking to ben@mozilla.org
#
    <key id="key_search" key="&searchFocus.commandkey;" command="Tools:Search" modifiers="accel"/>
#ifdef XP_WIN
    <key id="key_search2" key="&searchFocus.commandkey2;" command="Tools:Search" modifiers="accel"/>
#endif
#ifdef XP_GNOME
    <key id="key_search2" key="&searchFocusUnix.commandkey;" command="Tools:Search" modifiers="accel"/>
    <key id="key_openDownloads" key="&downloadsUnix.commandkey;" command="Tools:Downloads" modifiers="accel"/>
#else
    <key id="key_openDownloads" key="&downloads.commandkey;" command="Tools:Downloads" modifiers="accel"/>
#endif
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
    <key keycode="VK_F5" command="Browser:ReloadSkipCache" modifiers="accel"/>
    <key id="key_fullScreen" keycode="VK_F11" command="View:FullScreen"/>
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

    <key id="addBookmarkAsKb" key="&addCurPageAsCmd.commandkey;" command="Browser:AddBookmarkAs" modifiers="accel"/>
# Accel+Shift+A-F are reserved on GTK2
#ifndef MOZ_WIDGET_GTK2
    <key id="bookmarkAllTabsKb" key="&addCurPageAsCmd.commandkey;" command="Browser:BookmarkAllTabs" modifiers="accel,shift"/>
#endif
#ifdef MOZ_PLACES_BOOKMARKS
    <key id="searchBookmarksKb" key="&searchBookmarksCmd.commandkey;" command="Browser:ShowBookmarks" modifiers="accel"/>
#else
    <key id="viewBookmarksSidebarKb" key="&bookmarksSidebarCmd.commandkey;" command="viewBookmarksSidebar" modifiers="accel"/>
#endif
#ifdef XP_WIN
# Cmd+I is conventially mapped to Info on MacOS X, thus it should not be 
# overridden for other purposes there. 
    <key id="viewBookmarksSidebarWinKb" key="&bookmarksSidebarWinCmd.commandkey;" command="viewBookmarksSidebar" modifiers="accel"/>
#endif

# don't use |command="Browser:Stop"|, ESC is being used to freeze animated gifs,
# even if the stop button and menuitem are disabled (see Bug 284140)
    <key id="key_stop" keycode="VK_ESCAPE" oncommand="BrowserStop();"/>
    
#ifdef XP_MACOSX
    <key id="key_stop_mac" modifiers="accel" key="&stopCmd.macCommandKey;" oncommand="BrowserStop();"/>
#endif

#ifdef MOZ_PLACES_NEW_HISTORY_UI
    <key id="searchHistoryKb"
         key="&historySidebarCmd.commandKey;"
#ifdef XP_MACOSX
         modifiers="accel,shift"
#else
         modifiers="accel"
#endif
         command="viewHistoryPlaces"/>
#else
    <key id="key_gotoHistory"
         key="&historySidebarCmd.commandKey;"
#ifdef XP_MACOSX
         modifiers="accel,shift"
#else
         modifiers="accel"
#endif
         command="viewHistorySidebar"/>
#endif

    <key id="key_textZoomReduce"  key="&textZoomReduceCmd.commandkey;"   command="cmd_textZoomReduce"  modifiers="accel"/>
    <key id="key_textZoomEnlarge" key="&textZoomEnlargeCmd.commandkey;"  command="cmd_textZoomEnlarge" modifiers="accel"/>
    <key                          key="&textZoomEnlargeCmd.commandkey2;" command="cmd_textZoomEnlarge" modifiers="accel"/>
    <key id="key_textZoomReset"   key="&textZoomResetCmd.commandkey;"    command="cmd_textZoomReset"   modifiers="accel"/>
    
    <key id="key_switchTextDirection" key="&bidiSwitchTextDirectionItem.commandkey;" command="cmd_switchTextDirection" modifiers="accel,shift" />

    <key id="key_sanitize" command="Tools:Sanitize" keycode="VK_DELETE" modifiers="accel,shift"/>
#ifdef XP_MACOSX
    <key id="key_sanitize_mac" command="Tools:Sanitize" keycode="VK_BACK" modifiers="accel,shift"/>
    <key id="key_quitApplicationCmdMac" key="&quitApplicationCmdMac.key;" modifiers="accel"/>
#endif
    <key id="key_undoCloseTab" command="History:UndoCloseTab" key="&tabCmd.commandkey;" modifiers="accel,shift"/>
  </keyset>

# Used by baseMenuOverlay
#ifdef XP_MACOSX
  <commandset id="baseMenuCommandSet" />
#endif
  <keyset id="baseMenuKeyset" />
