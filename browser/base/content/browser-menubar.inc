       <menubar id="main-menubar" statusbar="statusbar-display" 
                style="border:0px;padding:0px;margin:0px;-moz-appearance:none">
            <menu label="&fileMenu.label;"
                  accesskey="&fileMenu.accesskey;">
              <menupopup id="menu_FilePopup" onpopupshowing="getContentAreaFrameCount();">
                <menuitem label="&newNavigatorCmd.label;"
                          accesskey="&newNavigatorCmd.accesskey;"
                          key="key_newNavigator"
                          command="cmd_newNavigator"/>
                <menuitem command="cmd_newNavigatorTab" key="key_newNavigatorTab"
                          label="&tabCmd.label;" accesskey="&tabCmd.accesskey;"/>
                <menuitem label="&openFileCmd.label;" accesskey="&openFileCmd.accesskey;" key="openFileKb" command="Browser:OpenFile"/>
                <menuitem id="menu_close" label="&closeCmd.label;" key="key_close" accesskey="&closeCmd.accesskey;" command="cmd_close"/>
                <menuitem id="menu_closeWindow" hidden="true" command="cmd_closeWindow" key="key_closeWindow" label="&closeWindow.label;"/>
                <menuseparator/>
                <menuitem label="&savePageCmd.label;" accesskey="&savePageCmd.accesskey;" key="key_savePage" command="Browser:SavePage"/>
                <menuitem id="menu_saveFrame" label="&saveFrameCmd.label;" accesskey="&saveFrameCmd.accesskey;" oncommand="saveFrameDocument();" hidden="true"/>
#ifndef XP_UNIX
                <menuitem id="menu_sendLink"  label="&sendPageCmd.label;"  accesskey="&sendPageCmd.accesskey;"  command="Browser:SendLink"/>
#endif
                <menuseparator/>
                <menuitem label="&printSetupCmd.label;" accesskey="&printSetupCmd.accesskey;" oncommand="PrintUtils.showPageSetup();"/>
                <menuitem label="&printPreviewCmd.label;" accesskey="&printPreviewCmd.accesskey;" oncommand="PrintUtils.printPreview(onEnterPrintPreview, onExitPrintPreview);"/>
                <menuitem label="&printCmd.label;" accesskey="&printCmd.accesskey;" key="printKb" command="cmd_print"/>
#ifndef XP_MACOSX
                <menuseparator/>
#endif
                <menuitem id="menu_FileQuitItem" 
#ifdef XP_WIN
			  label="&quitApplicationCmdWin.label;"
			  accesskey="&quitApplicationCmdWin.accesskey;"
#else
			  label="&quitApplicationCmd.label;" 
			  accesskey="&quitApplicationCmd.accesskey;" 
#endif
			  oncommand="goQuitApplication();"/>
              </menupopup>
            </menu>
  
            <menu label="&editMenu.label;"
                  accesskey="&editMenu.accesskey;">
              <menupopup id="menu_EditPopup">
                <menuitem label="&undoCmd.label;"
                          key="key_undo"
                          accesskey="&undoCmd.accesskey;"
                          command="cmd_undo"/>
                <menuitem label="&redoCmd.label;"
                          key="key_redo"
                          accesskey="&redoCmd.accesskey;"
                          command="cmd_redo"/>
                <menuseparator/>
                <menuitem label="&cutCmd.label;"
                          key="key_cut"
                          accesskey="&cutCmd.accesskey;"
                          command="cmd_cut"/>
                <menuitem label="&copyCmd.label;"
                          key="key_copy"
                          accesskey="&copyCmd.accesskey;"
                          command="cmd_copy"/>
                <menuitem label="&pasteCmd.label;"
                          key="key_paste"
                          accesskey="&pasteCmd.accesskey;"
                          command="cmd_paste"/>
                <menuitem label="&deleteCmd.label;"
                          key="key_delete"
                          accesskey="&deleteCmd.accesskey;"
                          command="cmd_delete"/>
                <menuseparator/>
                <menuitem label="&selectAllCmd.label;"
                          key="key_selectAll"
                          accesskey="&selectAllCmd.accesskey;"
                          command="cmd_selectAll"/>
                <menuseparator/>
                <menuitem label="&findOnCmd.label;" accesskey="&findOnCmd.accesskey;" key="key_find" command="Browser:Find"/>
                <menuitem label="&findAgainCmd.label;" accesskey="&findAgainCmd.accesskey;" key="key_findAgain" command="Browser:FindAgain"/>
              </menupopup>
            </menu>
  
            <menu label="&viewMenu.label;" accesskey="&viewMenu.accesskey;">
              <menupopup id="menu_viewPopup">
                <menu label="&viewToolbarsMenu.label;" accesskey="&viewToolbarsMenu.accesskey;"
                      onpopupshowing="onViewToolbarsPopupShowing(event);">
                  <menupopup onpopupshowing="updateToolbarStates(this);"> 
                    <menuseparator/>
                      <menuitem label="&viewCustomizeToolbar.label;" accesskey="&viewCustomizeToolbar.accesskey;"
                                command="cmd_CustomizeToolbars"/>             
                  </menupopup>
                </menu>
                <menuitem id="toggle_taskbar" label="&taskbarCmd.label;" accesskey="&taskbarCmd.accesskey;" type="checkbox" oncommand="goToggleToolbar('status-bar','toggle_taskbar');" checked="true" />
                <menu label="&viewSidebarMenu.label;"
                      accesskey="&viewSidebarMenu.accesskey;">
                  <menupopup id="viewSidebarMenu">
                    <menuitem key="key_openDownloadsSidebar" observes="viewDownloadsSidebar"/>
                    <menuitem key="viewBookmarksSidebarKb" observes="viewBookmarksSidebar"/>
                    <menuitem key="key_gotoHistory" observes="viewHistorySidebar"/>
                  </menupopup>
                </menu>            
                <menuseparator/>
                <menuitem label="&stopCmd.label;" accesskey="&stopCmd.accesskey;" command="Browser:Stop" key="key_stop"/>
                <menuitem accesskey="&reloadCmd.accesskey;" key="key_reload" label="&reloadCmd.label;" oncommand="BrowserReload();"/>
                <menuseparator/>
                <menuitem key="key_textZoomEnlarge" label="&textZoomEnlargeCmd.label;" accesskey="&textZoomEnlargeCmd.accesskey;"
                          command="cmd_textZoomEnlarge"/>
                <menuitem key="key_textZoomReduce"  label="&textZoomReduceCmd.label;"  accesskey="&textZoomReduceCmd.accesskey;"
                          command="cmd_textZoomReduce"/>
                <menu label="&charsetMenu.label;" accesskey="&charsetMenu.accesskey;"
                      datasources="rdf:charset-menu" ref="NC:BrowserCharsetMenuRoot"
                      oncommand="MultiplexHandler(event)" onpopupshowing="CreateMenu('browser');UpdateMenus(event)" onpopupshown="CreateMenu('more-menu');">
                    <template>
                        <rule rdf:type="http://home.netscape.com/NC-rdf#BookmarkSeparator">
                            <menupopup>
                            <menuseparator uri="..." />
                            </menupopup>
                        </rule>
                        <rule>
                            <menupopup>
                            <menuitem type="radio" name="charsetGroup" checked="rdf:http://home.netscape.com/NC-rdf#Checked" uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
                            </menupopup>
                        </rule>
                    </template>
      
                    <menupopup>
                    <menu label="&charsetMenuAutodet.label;" accesskey="&charsetMenuAutodet.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserAutodetMenuRoot">
                        <template>
                            <rule rdf:type="http://home.netscape.com/NC-rdf#CharsetDetector">
                                <menupopup>
                                <menuitem type="radio" name="detectorGroup" checked="rdf:http://home.netscape.com/NC-rdf#Checked" uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
                                </menupopup>
                            </rule>
                        </template>
                        <menupopup>
                        </menupopup>
                    </menu>
                    <menu label="&charsetMenuMore.label;" accesskey="&charsetMenuMore.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserMoreCharsetMenuRoot">
                        <template>
                            <rule>
                                <menupopup>
                                <menuitem uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
                                </menupopup>
                            </rule>
                        </template>
                        <menupopup>
                            <menu label="&charsetMenuMore1.label;" accesskey="&charsetMenuMore1.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserMore1CharsetMenuRoot">
                                <template>
                                    <rule>
                                        <menupopup>
                                        <menuitem uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
                                        </menupopup>
                                    </rule>
                                </template>
                                <menupopup>
                                </menupopup>
                            </menu>
                            <menu label="&charsetMenuMore2.label;" accesskey="&charsetMenuMore2.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserMore2CharsetMenuRoot">
                                <template>
                                    <rule>
                                        <menupopup>
                                        <menuitem uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
                                        </menupopup>
                                    </rule>
                                </template>
                                <menupopup>
                                </menupopup>
                            </menu>
                            <menu label="&charsetMenuMore3.label;" accesskey="&charsetMenuMore3.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserMore3CharsetMenuRoot">
                                <template>
                                    <rule>
                                        <menupopup>
                                        <menuitem uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
                                        </menupopup>
                                    </rule>
                                </template>
                                <menupopup>
                                </menupopup>
                            </menu>
                            <menu label="&charsetMenuMore4.label;" accesskey="&charsetMenuMore4.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserMore4CharsetMenuRoot">
                                <template>
                                    <rule>
                                        <menupopup>
                                        <menuitem uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
                                        </menupopup>
                                    </rule>
                                </template>
                                <menupopup>
                                </menupopup>
                            </menu>
                            <menu label="&charsetMenuMore5.label;" accesskey="&charsetMenuMore5.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserMore5CharsetMenuRoot">
                                <template>
                                    <rule>
                                        <menupopup>
                                        <menuitem uri="..." label="rdf:http://home.netscape.com/NC-rdf#Name"/>
                                        </menupopup>
                                    </rule>
                                </template>
                                <menupopup>
                                </menupopup>
                            </menu>
                            <menuseparator />
                        </menupopup>
                    </menu>
                    <menuitem name="charsetCustomize" accesskey="&charsetCustomize.accesskey;" label="&charsetCustomize.label;" oncommand="window.openDialog('chrome://global/content/customizeCharset.xul','PrefWindow', 'chrome,modal=yes,resizable=yes', 'browser')"/>
                    </menupopup>
                </menu>
                <menuseparator/>
                <menuitem accesskey="&pageSourceCmd.accesskey;" label="&pageSourceCmd.label;" key="key_viewSource" command="View:PageSource"/>
                  <menuitem accesskey="&fullScreenCmd.accesskey;"
                            label="&fullScreenCmd.label;" key="key_fullScreen"
                            command="View:FullScreen"/>
              </menupopup>
            </menu>
  
            <menu label="&goMenu.label;" accesskey="&goMenu.accesskey;" oncommand="var url = event.target.getAttribute('statustext'); if (url) openTopWin(url);">
              <menupopup id="goPopup" onpopupshowing="updateGoMenu(this);" onpopuphiding="onGoMenuHidden();">
                <menuitem label="&backCmd.label;"    accesskey="&backCmd.accesskey;"    key="goBackKb"    command="Browser:Back"/>
                <menuitem label="&forwardCmd.label;" accesskey="&forwardCmd.accesskey;" key="goForwardKb" command="Browser:Forward"/>
                <menuitem label="&goHomeCmd.label;"    accesskey="&goHomeCmd.accesskey;"    command="Browser:Home" key="goHome"/>
                <menuseparator id="startHistorySeparator"/>
                <menuseparator hidden="true" id="endHistorySeparator"/>
                <menuitem accesskey="&historyCmd.accesskey;" command="viewHistorySidebar" key="key_gotoHistory"/>
              </menupopup>
            </menu>

            <menu id="bookmarks-menu" label="&bookmarksMenu.label;" accesskey="&bookmarksMenu.accesskey;"
                  datasources="rdf:bookmarks rdf:files rdf:localsearch" 
                  ref="NC:BookmarksRoot" flags="dont-test-empty"
                  template='bookmarks-template'
                  onpopupshowing="BookmarksMenu.showOpenInTabsMenuItem(event.target)"
                  onpopuphidden="BookmarksMenu.hideOpenInTabsMenuItem(event.target)"
                  oncommand="BookmarksMenu.loadBookmark(event, event.target, this.database)"
                  onclick="BookmarksMenu.loadBookmarkMiddleClick(event, this.database)"
                  ondraggesture="nsDragAndDrop.startDrag(event, BookmarksMenuDNDObserver)"
                  ondragdrop="nsDragAndDrop.drop(event, BookmarksMenuDNDObserver); event.preventBubble()"
                  ondragenter="nsDragAndDrop.dragEnter(event, BookmarksMenuDNDObserver); event.preventBubble()"
                  ondragexit="nsDragAndDrop.dragExit(event, BookmarksMenuDNDObserver); event.preventBubble()"
                  ondragover="nsDragAndDrop.dragOver(event, BookmarksMenuDNDObserver); event.preventBubble()">
              <menupopup contextmenu='bookmarks-context-menu'>
                <menuitem key="addBookmarkAsKb"
                          label="&addCurPageAsCmd.label;"
                          accesskey="&addCurPageAsCmd.accesskey;"
                          command="Browser:AddBookmarkAs"/>
                <menuitem key="manBookmarkKb" 
                          label="&manBookmarksCmd.label;"
                          accesskey="&manBookmarksCmd.accesskey;"    
                          oncommand="toOpenWindowByType('bookmarks:manager', 'chrome://browser/content/bookmarks/bookmarksManager.xul');"/>
                <menuseparator/>
              </menupopup>
            </menu>
        
          <menu label="&toolsMenu.label;" accesskey="&toolsMenu.accesskey;">
            <menupopup id="menu_ToolsPopup">
              <menuitem id="javascriptConsole" label="&javaScriptConsoleCmd.label;" accesskey="&javaScriptConsoleCmd.accesskey;" oncommand="toJavaScriptConsole();"/>
              <menuitem accesskey="&pageInfoCmd.accesskey;"   label="&pageInfoCmd.label;"   key="key_viewInfo"   command="View:PageInfo"/>
#ifndef XP_MACOSX
              <menuseparator id="prefSep"/>
#endif
              <menuitem id="menu_preferences"
                        label="&preferencesCmd.label;"
                        accesskey="&preferencesCmd.accesskey;"
                        oncommand="openPreferences();"/>
            </menupopup>
          </menu>
    
#ifdef XP_MACOSX
<!-- XXXnoririty localize! -->
        <menu id="windowMenu" label="Window"
            datasources="rdf:window-mediator" ref="NC:WindowMediatorRoot"
            onpopupshown="checkFocusedWindow();">
            <template>
            <rule>
                <menupopup>
                <menuitem uri="rdf:*" label="rdf:http://home.netscape.com/NC-rdf#Name" type="radio"
                            oncommand="ShowWindowFromResource(event.target)"/>
                </menupopup>
            </rule>
            </template>
            <menupopup id="windowPopup">
              <menuitem label="&zoomWindow.label;" disabled="true"/>
              <menuitem command="minimizeWindow" key="key_minimizeWindow"/>
              <menuseparator/>
              <menuitem label="&bringAllToFront.label;" disabled="true"/>
              <menuseparator id="sep-window-list"/>
            </menupopup>
        </menu>
#endif

          <menu label="&helpMenu.label;"
                accesskey="&helpMenu.accesskey;">
            <menupopup id="menu_HelpPopup">
              <menuitem accesskey="&releaseCmd.accesskey;"
                        label="&releaseCmd.label;"
                        oncommand="openTopWin('&releaseURL;');"/>
              <menuseparator/>
              <menuitem id="aboutName"
                        accesskey="&aboutCmd.accesskey;"
                        label="&aboutCmd.label;"
                        oncommand="openAboutDialog();"/>
            </menupopup>
          </menu>
        </menubar>


<template id='bookmarks-template' xmlns:nc="http://home.netscape.com/NC-rdf#">
    <rule parent="hbox" rdf:type="http://home.netscape.com/NC-rdf#BookmarkSeparator">
      <toolbarseparator uri="rdf:*"
                        rdf:type="rdf:http://www.w3.org/1999/02/22-rdf-syntax-ns#type"/>
    </rule>
    <!-- yes, the type attribute is necessary otherwise all leaves are 
         treated as containers for some reason -->
    <rule parent="hbox" iscontainer="true" isempty="true"
          rdf:type="http://home.netscape.com/NC-rdf#Folder">
      <toolbarbutton type="menu" editable="true" class="bookmark-item"
                     rdf:type="http://home.netscape.com/NC-rdf#Folder"
                     uri="rdf:*" label="rdf:http://home.netscape.com/NC-rdf#Name">
        <menupopup>
          <menuitem label="&emptyItem.label;" disabled="true"/>
        </menupopup>
      </toolbarbutton>
    </rule>
    <rule parent="hbox" iscontainer="true">
      <toolbarbutton type="menu" class="bookmark-item" uri="rdf:*" editable="true"
                     rdf:type="http://home.netscape.com/NC-rdf#Folder"
                     label="rdf:http://home.netscape.com/NC-rdf#Name">
        <menupopup contextmenu="bookmarks-context-menu"/>
      </toolbarbutton>
    </rule>
    <rule parent="hbox">
      <toolbarbutton class="bookmark-item" uri="rdf:*" editable="true" 
                     status="rdf:http://home.netscape.com/WEB-rdf#status"
                     rdf:type="http://home.netscape.com/NC-rdf#Bookmark"
                     image="rdf:http://home.netscape.com/NC-rdf#Icon"
                     statustext="rdf:http://home.netscape.com/NC-rdf#URL"
                     tooltip="btTooltip"
                     label="rdf:http://home.netscape.com/NC-rdf#Name"/>
    </rule>
    <rule iscontainer="true" isempty="true"
          rdf:type="http://home.netscape.com/NC-rdf#Folder">
      <menupopup>
        <menu class="menu-iconic bookmark-item" uri="rdf:*" 
              type="rdf:http://www.w3.org/1999/02/22-rdf-syntax-ns#type"
              label="rdf:http://home.netscape.com/NC-rdf#Name">
          <menupopup>
            <menuitem label="&emptyItem.label;" disabled="true"/>
          </menupopup>
        </menu>
      </menupopup>
    </rule>
    <rule iscontainer="true">
      <menupopup>
        <menu class="menu-iconic bookmark-item" uri="rdf:*"
              label="rdf:http://home.netscape.com/NC-rdf#Name"
              type="rdf:http://www.w3.org/1999/02/22-rdf-syntax-ns#type">
          <menupopup contextmenu="bookmarks-context-menu"/>
        </menu>
      </menupopup>
    </rule>
    <rule rdf:type="http://home.netscape.com/NC-rdf#BookmarkSeparator">
      <menupopup> 
        <menuseparator uri="rdf:*" />
      </menupopup>
    </rule>
    <rule>
      <menupopup>
        <menuitem class="menuitem-iconic bookmark-item" uri="rdf:*"
                  label="rdf:http://home.netscape.com/NC-rdf#Name" 
                  image="rdf:http://home.netscape.com/NC-rdf#Icon"
                  status="rdf:http://home.netscape.com/WEB-rdf#status"
                  statustext="rdf:http://home.netscape.com/NC-rdf#URL"/>
      </menupopup>
    </rule>
  </template>
