# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
# The Initial Developer of the Original Code is.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@bengoodger.com> (v2.0) 
#   Blake Ross <blakeross@telocity.com>
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

  <stringbundleset id="stringbundleset"> 
    <stringbundle id="bundle_brand" src="chrome://global/locale/brand.properties"/>
    <stringbundle id="bundle_shell" src="chrome://browser/locale/shellservice.properties"/>
    <stringbundle id="bundle_browser" src="chrome://browser/locale/browser.properties"/>
    <stringbundle id="bundle_browser_region" src="chrome://browser-region/locale/region.properties"/>
    <stringbundle id="findBundle" src="chrome://global/locale/finddialog.properties"/>
  </stringbundleset>

  <commandset commandupdater="true"
              events="focus"
              oncommandupdate="goUpdateGlobalEditMenuItems()"/>
  <commandset commandupdater="true"
              events="select"
              oncommandupdate="goUpdateSelectEditMenuItems()"/>
  <commandset commandupdater="true"
              events="undo"
              oncommandupdate="goUpdateUndoEditMenuItems()"/>
  <commandset commandupdater="true"
              events="clipboard"
              oncommandupdate="goUpdatePasteMenuItems()"/>
  
  <commandset id="mainCommandSet">
    <command id="cmd_newNavigator"                 oncommand="OpenBrowserWindow()"/>
    <command id="cmd_bm_open"                      oncommand="goDoCommand('cmd_bm_open');"/>
    <command id="cmd_bm_openinnewwindow"           oncommand="goDoCommand('cmd_bm_openinnewwindow');"/>
    <command id="cmd_bm_openinnewtab"              oncommand="goDoCommand('cmd_bm_openinnewtab');"/>
    <command id="cmd_bm_expandfolder"              oncommand="goDoCommand('cmd_bm_expandfolder');"/>
    <command id="cmd_bm_openfolder"                oncommand="goDoCommand('cmd_bm_openfolder');"/>
    <command id="cmd_bm_managefolder"              oncommand="goDoCommand('cmd_bm_managefolder');"/>
    <command id="cmd_bm_newfolder"                 oncommand="goDoCommand('cmd_bm_newfolder');"/>
    <command id="cmd_bm_newbookmark"               oncommand="goDoCommand('cmd_bm_newbookmark');"/>
    <command id="cmd_bm_newseparator"              oncommand="goDoCommand('cmd_bm_newseparator');"/>
    <command id="cmd_bm_properties"                oncommand="goDoCommand('cmd_bm_properties');"/>
    <command id="cmd_bm_rename"                    oncommand="goDoCommand('cmd_bm_rename');"/>
    <command id="cmd_bm_moveBookmark"              oncommand="goDoCommand('cmd_bm_moveBookmark');"/>

#ifdef XP_WIN
    <command id="cmd_handleBackspace"  oncommand="BrowserHandleBackspace();" />
#endif

    <command id="cmd_newNavigatorTab" oncommand="BrowserOpenTab();"/>
    <command id="Browser:OpenFile"  oncommand="BrowserOpenFileWindow();"/>
    <command id="Browser:SavePage" oncommand="saveDocument(window._content.document);"/>

    <command id="Browser:SendLink" oncommand="MailIntegration.sendLinkForContent();"/>
    <command id="Browser:NewMessage" oncommand="MailIntegration.sendMessage('', '');"/>
#ifdef XP_WIN
    <command id="Browser:ReadMail" oncommand="MailIntegration.readMail();"/>
    <command id="Browser:ReadNews" oncommand="MailIntegration.readNews();"/>
#endif

    <command id="cmd_print" oncommand="PrintUtils.print();"/>
    <command id="cmd_close" oncommand="BrowserCloseTabOrWindow()"/>
    <command id="cmd_closeWindow" oncommand="BrowserCloseWindow()"/>
    <command id="cmd_CustomizeToolbars" oncommand="BrowserCustomizeToolbar()"/>
  
    <command id="cmd_copyLink"
             oncommand="goDoCommand('cmd_copyLink')"
             disabled="false"/>
    <command id="cmd_copyImageLocation"
             oncommand="goDoCommand('cmd_copyImageLocation')"
             disabled="false"/>
    <command id="cmd_copyImageContents"
             oncommand="goDoCommand('cmd_copyImageContents')"
             disabled="false"/>
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

    <command id="View:PageSource" oncommand="BrowserViewSourceOfDocument(_content.document);"/>
    <command id="View:PageInfo" oncommand="BrowserPageInfo();"/>
    <command id="View:FullScreen" oncommand="BrowserFullScreen();"/>
    <command id="cmd_find" oncommand="getBrowser().find();"/>
    <command id="cmd_findAgain" oncommand="getBrowser().findAgain();"/>
    <command id="cmd_findPrevious" oncommand="getBrowser().findPrevious();"/>
    <command id="Browser:AddBookmarkAs" oncommand="addBookmarkAs(document.getElementById('content'));"/>
    <command id="Browser:Home"    oncommand="BrowserHome();"/>
    <command id="Browser:Back"    oncommand="BrowserBack();" disabled="true"/>
    <command id="Browser:Forward" oncommand="BrowserForward();" disabled="true"/>
    <command id="Browser:Stop"    oncommand="BrowserStop();" disabled="true"/>
    <command id="cmd_textZoomReduce"  oncommand="ZoomManager.prototype.getInstance().reduce();"/>
    <command id="cmd_textZoomEnlarge" oncommand="ZoomManager.prototype.getInstance().enlarge();"/>
    <command id="Browser:OpenLocation" oncommand="openLocation();"/>

    <command id="Tools:Search" oncommand="focusSearchBar();"/>    
    <command id="Tools:Downloads" oncommand="toOpenWindowByType('Download:Manager', 
                                                                'chrome://mozapps/content/downloads/downloads.xul',
                                                                'chrome,dialog=no,resizable');"/>
    <command id="Tools:Extensions" oncommand="BrowserOpenExtensions('extensions');"/>
    <command id="Tools:Themes" oncommand="BrowserOpenExtensions('themes');"/>
    
#ifdef XP_MACOSX
    <command id="minimizeWindow" label="&minimizeWindow.label;" disabled="true"/>
#endif
  </commandset>

  <broadcasterset id="mainBroadcasterSet">
    <broadcaster id="viewBookmarksSidebar" autoCheck="false" label="&bookmarksButton.label;"
                 type="checkbox" group="sidebar" sidebarurl="chrome://browser/content/bookmarks/bookmarksPanel.xul"
                 oncommand="toggleSidebar('viewBookmarksSidebar');"/>

    <broadcaster id="viewHistorySidebar" autoCheck="false" label="&historyButton.label;"
                 type="checkbox" group="sidebar" sidebarurl="chrome://browser/content/history/history-panel.xul"
                 oncommand="toggleSidebar('viewHistorySidebar');"/>
                 
    <broadcaster id="viewWebPanelsSidebar" autoCheck="false" 
                 type="checkbox" group="sidebar" sidebarurl="chrome://browser/content/web-panels.xul"
                 oncommand="toggleSidebar('viewWebPanelsSidebar');"/>

  </broadcasterset>

  <keyset id="mainKeyset">
    <key id="key_newNavigator"
         key="&newNavigatorCmd.key;"
         command="cmd_newNavigator"
         modifiers="accel"/>
    <key id="key_newNavigatorTab" key="&tabCmd.commandkey;" modifiers="accel" command="cmd_newNavigatorTab"/>
    <key id="focusURLBar" key="&openCmd.commandkey;" command="Browser:OpenLocation"
         modifiers="accel"/>
    <key id="focusURLBar2" key="&urlbar.accesskey;" command="Browser:OpenLocation"
         modifiers="alt"/>
#ifdef XP_UNIX
#ifndef XP_MACOSX
    <key id="key_search" key="&searchFocusUnix.commandkey;" command="Tools:Search" modifiers="accel"/>
#else
    <key id="key_search" key="&searchFocus.commandkey;" command="Tools:Search" modifiers="accel"/>
#endif
#else
    <key id="key_search" key="&searchFocus.commandkey;" command="Tools:Search" modifiers="accel"/>
#endif
    <key id="openFileKb" key="&openFileCmd.commandkey;" command="Browser:OpenFile"  modifiers="accel"/>
    <key id="key_savePage" key="&savePageCmd.commandkey;" command="Browser:SavePage" modifiers="accel"/>
    <key id="printKb" key="&printCmd.commandkey;" command="cmd_print"  modifiers="accel"/>
    <key id="key_close" key="&closeCmd.key;" command="cmd_close" modifiers="accel"/>
    <key id="key_closeWindow" key="&closeCmd.key;" command="cmd_closeWindow" modifiers="accel,shift"/>
    <key id="key_undo"
         key="&undoCmd.key;"
         modifiers="accel"/>
    <key id="key_redo"
         key="&redoCmd.key;"
         modifiers="accel"/>
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

#ifdef XP_WIN
    <key keycode="VK_BACK" command="cmd_handleBackspace"/>
    <key keycode="VK_BACK" command="Browser:Forward" modifiers="shift"/>
#endif
#ifdef XP_UNIX
    <key key="&goBackCmd.commandKey;" command="Browser:Back" modifiers="accel"/>
    <key key="&goForwardCmd.commandKey;" command="Browser:Forward" modifiers="accel"/>
#ifdef XP_MACOSX
    <key id="key_selectAll" key="&selectAllCmd.key;" modifiers="accel"/>
#else
    <key id="key_selectAll" key="&selectAllCmd.key;" modifiers="alt"/>
#endif
#else
    <key id="key_selectAll" key="&selectAllCmd.key;" modifiers="accel"/>
#endif
#ifdef XP_MACOSX
    <key id="goBackKb"  keycode="VK_LEFT" command="Browser:Back" modifiers="accel"/>
    <key id="goForwardKb"  keycode="VK_RIGHT" command="Browser:Forward" modifiers="accel"/>
#else
    <key id="goBackKb"  keycode="VK_LEFT" command="Browser:Back" modifiers="alt"/>
    <key id="goForwardKb"  keycode="VK_RIGHT" command="Browser:Forward" modifiers="alt"/>
#endif
    <key id="goHome" keycode="VK_HOME" command="Browser:Home" modifiers="alt"/>
    <key id="key_fullScreen" keycode="VK_F11" command="View:FullScreen"/>
    <key keycode="VK_F5" oncommand="BrowserReload();"/>
    <key keycode="VK_F5" modifiers="control" oncommand="BrowserReloadSkipCache();"/>
    <key id="key_reload"  key="&reloadCmd.commandkey;" oncommand="BrowserReload();" modifiers="accel"/>
    <key key="&reloadCmd.commandkey;" oncommand="BrowserReloadSkipCache();" modifiers="accel,shift"/>
    <key id="key_viewSource" key="&pageSourceCmd.commandkey;" command="View:PageSource" modifiers="accel"/>
#ifdef XP_WIN
    <key id="key_viewInfo"   key="&pageInfoWinCmd.commandkey;"   command="View:PageInfo"   modifiers="accel"/>
#else
    <key id="key_viewInfo"   key="&pageInfoCmd.commandkey;"   command="View:PageInfo"   modifiers="accel"/>
#endif
    <key id="key_find" key="&findOnCmd.commandkey;" command="cmd_find" modifiers="accel"/>
    <key id="key_findAgain" key="&findAgainCmd.commandkey;" command="cmd_findAgain" modifiers="accel"/>
    <key id="key_findPrevious" key="&findAgainCmd.commandkey;" command="cmd_findPrevious" modifiers="accel,shift"/>
    <key keycode="&findAgainCmd.commandkey2;" command="cmd_findAgain"/>
    <key keycode="&findAgainCmd.commandkey2;"  command="cmd_findPrevious" modifiers="shift"/>

    <key id="addBookmarkAsKb" key="&addCurPageAsCmd.commandkey;" command="Browser:AddBookmarkAs" modifiers="accel"/>
    <key id="viewBookmarksSidebarKb" key="&manBookmarksCmd.commandkey;" command="viewBookmarksSidebar" modifiers="accel"/>
#ifdef XP_WIN
# Cmd+I is conventially mapped to Info on MacOS X, 
# thus it should not be overridden for other purposes there.
    <key id="viewBookmarksSidebarKb" key="&manBookmarksWinCmd.commandkey;" command="viewBookmarksSidebar" modifiers="accel"/>
#endif

    <key id="key_stop" keycode="VK_ESCAPE" command="Browser:Stop"/>
#ifdef XP_MACOSX
    <key id="key_gotoHistory"  key="&history.commandKey;"  command="viewHistorySidebar" modifiers="accel,shift"/>
#else
    <key id="key_gotoHistory"  key="&history.commandKey;"  command="viewHistorySidebar" modifiers="accel"/>
#endif

    <key id="key_openDownloads"   key="&downloads.commandkey;"           command="Tools:Downloads" modifiers="accel"/>

    <key id="key_newMessage" key="&sendMessage.commandkey;" command="Browser:NewMessage" modifiers="accel"/>

    <key id="key_textZoomReduce"  key="&textZoomReduceCmd.commandkey;"   command="cmd_textZoomReduce"  modifiers="accel"/>
    <key id="key_textZoomEnlarge" key="&textZoomEnlargeCmd.commandkey;"  command="cmd_textZoomEnlarge" modifiers="accel"/>
    <key                          key="&textZoomEnlargeCmd.commandkey2;" command="cmd_textZoomEnlarge" modifiers="accel"/>
    <key                          key="&textZoomResetCmd.commandkey;"    oncommand="ZoomManager.prototype.getInstance().reset();"   modifiers="accel"/>
    
#ifdef XP_MACOSX
    <key id="key_minimizeWindow" command="minimizeWindow" key="&minimizeWindow.key;" modifiers="accel"/>
#endif

  </keyset>

