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
                <menuitem label="&openLocationCmd.label;" accesskey="&openLocationCmd.accesskey;" key="focusURLBar" command="Browser:OpenLocation"/>
                <menuitem label="&openFileCmd.label;" accesskey="&openFileCmd.accesskey;" key="openFileKb" command="Browser:OpenFile"/>
                <menuitem id="menu_close" label="&closeCmd.label;" key="key_close" accesskey="&closeCmd.accesskey;" command="cmd_close"/>
                <menuitem id="menu_closeWindow" hidden="true" command="cmd_closeWindow" key="key_closeWindow" label="&closeWindow.label;"/>
                <menuseparator/>
                <menuitem label="&savePageCmd.label;" accesskey="&savePageCmd.accesskey;" key="key_savePage" command="Browser:SavePage"/>
                <menuitem id="menu_saveFrame" label="&saveFrameCmd.label;" accesskey="&saveFrameCmd.accesskey;" oncommand="saveFrameDocument();" hidden="true"/>
                <menuitem id="menu_sendLink"  label="&sendPageCmd.label;"  accesskey="&sendPageCmd.accesskey;"  command="Browser:SendLink"/>
                <menuseparator/>
                <menuitem label="&printSetupCmd.label;" accesskey="&printSetupCmd.accesskey;" oncommand="PrintUtils.showPageSetup();"/>
#ifndef XP_MACOSX
                <menuitem label="&printPreviewCmd.label;" accesskey="&printPreviewCmd.accesskey;" oncommand="PrintUtils.printPreview(onEnterPrintPreview, onExitPrintPreview);"/>
#endif
                <menuitem label="&printCmd.label;" accesskey="&printCmd.accesskey;" key="printKb" command="cmd_print"/>
                <menuseparator/>
                <menuitem label="&import.label;" accesskey="&import.accesskey;" oncommand="BrowserImport();"/>
#ifndef XP_MACOSX
                <menuseparator/>
#endif
                <menuitem id="goOfflineMenuitem"
                          label="&goOfflineCmd.label;" accesskey="&goOfflineCmd.accesskey;" 
                          type="checkbox" oncommand="BrowserOffline.toggleOfflineStatus();"/>
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
                <menuitem label="&findOnCmd.label;" accesskey="&findOnCmd.accesskey;" key="key_find" command="cmd_find"/>
                <menuitem label="&findAgainCmd.label;" accesskey="&findAgainCmd.accesskey;" key="key_findAgain" command="cmd_findAgain"/>
#ifdef XP_UNIX
#ifndef XP_MACOSX
                <menuseparator/>
                <menuitem id="menu_preferences"
                          label="&preferencesCmdUnix.label;"
                          accesskey="&preferencesCmdUnix.accesskey;"
                          oncommand="openPreferences();"/>
#endif
#endif
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
                    <menuitem key="viewBookmarksSidebarKb" observes="viewBookmarksSidebar" accesskey="&bookmarksSidebarCmd.accesskey;"/>
                    <menuitem key="key_gotoHistory" observes="viewHistorySidebar" accesskey="&historySidebarCmd.accesskey;"/>
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
                            <menu label="&charsetMenuUnicode.label;" accesskey="&charsetMenuUnicode.accesskey;" datasources="rdf:charset-menu" ref="NC:BrowserUnicodeCharsetMenuRoot">
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
  
            <menu label="&goMenu.label;" accesskey="&goMenu.accesskey;" 
                  oncommand="var url = event.target.getAttribute('statustext'); if (url) openUILink(url, event, false, true);"
                  onclick="checkForMiddleClick(this, event);">

              <menupopup id="goPopup" onpopupshowing="updateGoMenu(this);" onpopuphiding="onGoMenuHidden();">
                <menuitem label="&backCmd.label;"
                          accesskey="&backCmd.accesskey;"
                          key="goBackKb"
                          oncommand="BrowserBack(event, true)"
                          onclick="checkForMiddleClick(this, event);">
                  <observes element="Browser:Back" attribute="disabled" />
                </menuitem>
                <menuitem label="&forwardCmd.label;" 
                          accesskey="&forwardCmd.accesskey;"
                          key="goForwardKb" 
                          oncommand="BrowserForward(event, true)"
                          onclick="checkForMiddleClick(this, event);">
                  <observes element="Browser:Forward" attribute="disabled" />
                </menuitem>

                          
                          
                <menuitem label="&goHomeCmd.label;"    accesskey="&goHomeCmd.accesskey;"    command="Browser:Home" key="goHome"/>
                <menuseparator id="startHistorySeparator"/>
                <menuseparator hidden="true" id="endHistorySeparator"/>
                <menuitem accesskey="&historySidebarCmd.accesskey;" command="viewHistorySidebar" key="key_gotoHistory"/>
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
              <menuitem label="&search.label;" accesskey="&search.accesskey;" 
                        key="key_search" command="Tools:Search"/>
#ifdef XP_WIN
              <menu label="&mailMenu.label;"
                           accesskey="&mailMenu.accesskey;">
                <menupopup id="viewMailMenu">
                  <menuitem label="&mailButton.readMail.label;" 
                                   id="readMailItem"
                                   accesskey="&mailButton.readMail.accesskey;"
                                   command="Browser:ReadMail"/>
                  <menuitem label="&mailButton.newMessage.label;" 
                                   accesskey="&mailButton.newMessage.accesskey;"
                                   key="key_newMessage" command="Browser:NewMessage"/>
                  <menuitem label="&mailButton.sendLink.label;" 
                                   accesskey="&mailButton.sendLink.accesskey;"
                                   command="Browser:SendLink"/>
                  <menuseparator/>
                  <menuitem label="&mailButton.readNews.label;" 
                                   accesskey="&mailButton.readNews.accesskey;"
                                   command="Browser:ReadNews"/>
                </menupopup>
              </menu>
#endif
              <menuseparator/>
              <menuitem label="&downloads.label;" accesskey="&downloads.accesskey;" 
                        key="key_openDownloads" command="Tools:Downloads"/>
              <menuitem label="&extensions.label;" accesskey="&extensions.accesskey;" 
                        command="Tools:Extensions"/>
              <menuitem label="&themes.label;" accesskey="&themes.accesskey;" 
                        command="Tools:Themes"/>
              <menuseparator id="devToolsSeparator"/>
              <menuitem id="javascriptConsole" 
                        label="&javaScriptConsoleCmd.label;" accesskey="&javaScriptConsoleCmd.accesskey;" 
                        oncommand="toJavaScriptConsole();"/>
              <menuitem accesskey="&pageInfoCmd.accesskey;" label="&pageInfoCmd.label;"   
                        key="key_viewInfo" command="View:PageInfo"/>
#ifndef XP_UNIX
              <menuseparator id="prefSep"/>
              <menuitem id="menu_preferences"
                        label="&preferencesCmd.label;"
                        accesskey="&preferencesCmd.accesskey;"
                        oncommand="openPreferences();"/>
#else
#ifdef XP_MACOSX
              <menuitem id="menu_preferences" oncommand="openPreferences();"/>
#endif
#endif
            </menupopup>
          </menu>
    
#ifdef XP_MACOSX
        <menu id="windowMenu" label="&windowMenu.label;"
            datasources="rdf:window-mediator" ref="NC:WindowMediatorRoot"
            onpopupshown="checkFocusedWindow();">
            <template>
            <rule>
                <menupopup>
                <menuitem uri="rdf:*" label="rdf:http://home.netscape.com/NC-rdf#Name" type="radio"
                            name="windowList" oncommand="ShowWindowFromResource(event.target)"/>
                </menupopup>
            </rule>
            </template>
            <menupopup id="windowPopup">
              <menuitem command="minimizeWindow" key="key_minimizeWindow"/>
              <menuitem label="&zoomWindow.label;" disabled="true"/>
              <menuseparator/>
              <menuitem label="&bringAllToFront.label;" disabled="true"/>
              <menuseparator id="sep-window-list"/>
            </menupopup>
        </menu>
#endif

#ifdef XP_WIN
          <menu label="&helpMenuWin.label;"
                accesskey="&helpMenuWin.accesskey;">
#else
          <menu label="&helpMenu.label;"
                accesskey="&helpMenu.accesskey;">
#endif
            <menupopup id="menu_HelpPopup">
              <menuitem accesskey="&releaseCmd.accesskey;"
                        label="&releaseCmd.label;"
                        oncommand="openUILink('&releaseURL;', event, false, true);"
                        onclick="checkForMiddleClick(this, event);"/>
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
    <rule rdf:type="http://home.netscape.com/NC-rdf#BookmarkSeparator">
      <menupopup> 
        <menuseparator uri="rdf:*" />
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
