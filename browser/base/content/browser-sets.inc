  <stringbundleset id="stringbundleset">
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
    <command id="Browser:SendLink" oncommand="sendLink(Components.lookupMethod(window._content, 'location').call(window._content).href,
                                              Components.lookupMethod(window._content.document, 'title').call(window._content.document));"/>
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
    <command id="Browser:Find" oncommand="BrowserFind();"/>
    <command id="Browser:FindAgain" oncommand="BrowserFindAgain(false);"/>
    <command id="Browser:AddBookmarkAs" oncommand="addBookmarkAs(document.getElementById('content'));"/>
    <command id="Browser:Home"    oncommand="BrowserHome();"/>
    <command id="Browser:Back"    oncommand="BrowserBack();" disabled="true"/>
    <command id="Browser:Forward" oncommand="BrowserForward();" disabled="true"/>
    <command id="Browser:Stop"    oncommand="BrowserStop();" disabled="true"/>
    <command id="cmd_textZoomReduce"  oncommand="ZoomManager.prototype.getInstance().reduce();"/>
    <command id="cmd_textZoomEnlarge" oncommand="ZoomManager.prototype.getInstance().enlarge();"/>
    <command id="Browser:OpenLocation" oncommand="openLocation();"/>
    
#ifdef XP_MACOSX
    <command id="minimizeWindow" label="&minimizeWindow.label;" disabled="true"/>
#endif
  </commandset>

  <broadcasterset id="mainBroadcasterSet">
    <broadcaster id="viewDownloadsSidebar" autoCheck="false" label="&downloadsButton.label;"
                 type="checkbox" group="sidebar" sidebarurl="chrome://browser/content/downloads/downloadPanel.xul"
                 sidebartitle="&currentDownloads.label;"
                 oncommand="toggleSidebar('viewDownloadsSidebar');"/>

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
    <key id="focusURLBar"      key="&openCmd.commandkey;" command="Browser:OpenLocation"
         modifiers="accel"/>
    <key id="focusSearchBar"     key="&searchFocus.commandkey;" oncommand="focusSearchBar();" modifiers="accel"/>
    <key id="openFileKb"         key="&openFileCmd.commandkey;" command="Browser:OpenFile"  modifiers="accel"/>
    <key id="key_savePage"       key="&savePageCmd.commandkey;" command="Browser:SavePage" modifiers="accel"/>
    <key id="printKb"            key="&printCmd.commandkey;" command="cmd_print"  modifiers="accel"/>
    <key id="key_close"  key="&closeCmd.key;" command="cmd_close" modifiers="accel"/>
    <key id="key_closeWindow"  key="&closeCmd.key;" command="cmd_closeWindow" modifiers="accel,shift"/>
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
    <key keycode="&findAgainCmd.commandkey2;" command="Browser:FindAgain"/>
    <key keycode="&findPrevCmd.commandkey2;"  oncommand="BrowserFindAgain(true);" modifiers="shift"/>

#ifdef XP_WIN
    <key id="key_selectAll" key="&selectAllCmd.key;" modifiers="accel"/>
    <key id="goBackKb"  keycode="VK_LEFT" command="Browser:Back" modifiers="alt"/>
    <key id="goForwardKb"  keycode="VK_RIGHT" command="Browser:Forward" modifiers="alt"/>
    <key keycode="VK_BACK" command="cmd_handleBackspace"/>
    <key keycode="VK_BACK" command="Browser:Forward" modifiers="shift"/>

    <key id="goHome" keycode="VK_HOME" command="Browser:Home" modifiers="alt"/>

    <key id="key_fullScreen" keycode="VK_F11" command="View:FullScreen"/>
#else
    <key id="key_selectAll" key="&selectAllCmd.key;" modifiers="alt"/>
    
#ifdef XP_MACOSX
    <key id="goBackKb"  keycode="VK_LEFT" command="Browser:Back" modifiers="accel"/>
    <key id="goForwardKb"  keycode="VK_RIGHT" command="Browser:Forward" modifiers="accel"/>
#else
    <key id="goBackKb"  keycode="VK_LEFT" command="Browser:Back" modifiers="alt"/>
    <key id="goForwardKb"  keycode="VK_RIGHT" command="Browser:Forward" modifiers="alt"/>
#endif

    <key key="&goBackCmd.commandKey;" command="Browser:Back" modifiers="accel"/>
    <key key="&goForwardCmd.commandKey;" command="Browser:Forward" modifiers="accel"/>

    <key id="goHome" keycode="VK_HOME" command="Browser:Home" modifiers="alt"/>
    <key id="key_fullScreen" keycode="VK_F11" command="View:FullScreen"/>
#endif
    <key keycode="VK_F5" oncommand="BrowserReload();"/>
    <key keycode="VK_F5" modifiers="control" oncommand="BrowserReloadSkipCache();"/>
    <key id="key_reload"  key="&reloadCmd.commandkey;" oncommand="BrowserReload();" modifiers="accel"/>
    <key key="&reloadCmd.commandkey;" oncommand="BrowserReloadSkipCache();" modifiers="accel,shift"/>
    <key id="key_viewSource" key="&pageSourceCmd.commandkey;" command="View:PageSource" modifiers="accel"/>
    <key id="key_viewInfo"   key="&pageInfoCmd.commandkey;"   command="View:PageInfo"   modifiers="accel"/>
    <key id="key_find"              key="&findOnCmd.commandkey;" command="Browser:Find" modifiers="accel"/>
    <key id="key_findAgain"         key="&findAgainCmd.commandkey;" command="Browser:FindAgain" modifiers="accel"/>
    <key id="addBookmarkAsKb" key="&addCurPageAsCmd.commandkey;" command="Browser:AddBookmarkAs" modifiers="accel"/>
    <key id="viewBookmarksSidebarKb" key="&manBookmarksCmd.commandkey;" command="viewBookmarksSidebar" modifiers="accel"/>
    <key id="viewBookmarksSidebarKb" key="&manBookmarksCmd2.commandkey;" command="viewBookmarksSidebar" modifiers="accel"/>
    <key id="key_stop" keycode="VK_ESCAPE" command="Browser:Stop"/>
#ifdef XP_MACOSX
    <key id="key_gotoHistory"  key="&history.commandKey;"  command="viewHistorySidebar" modifiers="accel,shift"/>
#else
    <key id="key_gotoHistory"  key="&history.commandKey;"  command="viewHistorySidebar" modifiers="accel"/>
#endif

    <key id="key_textZoomReduce"  key="&textZoomReduceCmd.commandkey;"   command="cmd_textZoomReduce"  modifiers="accel"/>
    <key id="key_textZoomEnlarge" key="&textZoomEnlargeCmd.commandkey;"  command="cmd_textZoomEnlarge" modifiers="accel"/>
    <key                          key="&textZoomEnlargeCmd.commandkey2;" command="cmd_textZoomEnlarge" modifiers="accel"/>
    <key                          key="&textZoomResetCmd.commandkey;"    oncommand="ZoomManager.prototype.getInstance().reset();"   modifiers="accel"/>
    <key id="key_openDownloadsSidebar" key="&openDownloadsSidebar.commandkey;" command="viewDownloadsSidebar" modifiers="accel"/>
    
#ifdef XP_MACOSX
    <key id="key_minimizeWindow" command="minimizeWindow" key="&minimizeWindow.key;" modifiers="accel"/>
#endif

  </keyset>

