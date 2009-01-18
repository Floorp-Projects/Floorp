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
#   Ehsan Akhgari <ehsan.akhgari@gmail.com>
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

       <menubar id="main-menubar"
                style="border:0px;padding:0px;margin:0px;-moz-appearance:none">
            <menu id="file-menu" label="&fileMenu.label;"
                  accesskey="&fileMenu.accesskey;">
              <menupopup id="menu_FilePopup" onpopupshowing="getContentAreaFrameCount();">
                <menuitem id="menu_newNavigator"
                          label="&newNavigatorCmd.label;"
                          accesskey="&newNavigatorCmd.accesskey;"
                          key="key_newNavigator"
                          command="cmd_newNavigator"/>
                <menuitem id="menu_newNavigatorTab"
                          label="&tabCmd.label;"
                          command="cmd_newNavigatorTab"
                          key="key_newNavigatorTab"
                          accesskey="&tabCmd.accesskey;"/>
                <menuitem id="menu_openLocation"
                          label="&openLocationCmd.label;"
                          command="Browser:OpenLocation"
                          key="focusURLBar"
                          accesskey="&openLocationCmd.accesskey;"/>
                <menuitem id="menu_openFile"
                          label="&openFileCmd.label;"
                          command="Browser:OpenFile"
                          key="openFileKb"
                          accesskey="&openFileCmd.accesskey;"/>
                <menuitem id="menu_closeWindow"
                          hidden="true"
                          command="cmd_closeWindow"
                          key="key_closeWindow"
                          label="&closeWindow.label;"
                          accesskey="&closeWindow.accesskey;"/>
                <menuitem id="menu_close"
                          label="&closeCmd.label;"
                          key="key_close"
                          accesskey="&closeCmd.accesskey;"
                          command="cmd_close"/>
                <menuseparator/>
                <menuitem id="menu_savePage"
                          label="&savePageCmd.label;"
                          accesskey="&savePageCmd.accesskey;"
                          key="key_savePage"
                          command="Browser:SavePage"/>
                <menuitem id="menu_saveFrame"
                          label="&saveFrameCmd.label;"
                          accesskey="&saveFrameCmd.accesskey;"
                          command="Browser:SaveFrame"
                          hidden="true"/>
                <menuitem id="menu_sendLink"
                          label="&sendPageCmd.label;"
                          accesskey="&sendPageCmd.accesskey;"
                          command="Browser:SendLink"/>
                <menuseparator/>
                <menuitem id="menu_printSetup"
                          label="&printSetupCmd.label;"
                          accesskey="&printSetupCmd.accesskey;"
                          command="cmd_pageSetup"/>
#ifndef XP_MACOSX
                <menuitem id="menu_printPreview"
                          label="&printPreviewCmd.label;"
                          accesskey="&printPreviewCmd.accesskey;"
                          command="cmd_printPreview"/>
#endif
                <menuitem id="menu_print"
                          label="&printCmd.label;"
                          accesskey="&printCmd.accesskey;"
                          key="printKb"
                          command="cmd_print"/>
                <menuseparator/>
                <menuitem id="menu_import"
                          label="&import.label;"
                          accesskey="&import.accesskey;"
                          oncommand="BrowserImport();"/>
#ifndef XP_MACOSX
                <menuseparator/>
#endif
                <menuitem id="goOfflineMenuitem"
                          label="&goOfflineCmd.label;"
                          accesskey="&goOfflineCmd.accesskey;"
                          type="checkbox"
                          oncommand="BrowserOffline.toggleOfflineStatus();"/>
                <menuitem id="menu_FileQuitItem"
#ifdef XP_WIN
                          label="&quitApplicationCmdWin.label;"
                          accesskey="&quitApplicationCmdWin.accesskey;"
#else
#ifdef XP_MACOSX
                          label="&quitApplicationCmdMac.label;"
#else
                          label="&quitApplicationCmd.label;"
                          accesskey="&quitApplicationCmd.accesskey;"
#endif
#ifdef XP_UNIX
                          key="key_quitApplication"
#endif
#endif
                          command="cmd_quitApplication"/>
              </menupopup>
            </menu>

            <menu id="edit-menu" label="&editMenu.label;"
                  accesskey="&editMenu.accesskey;">
              <menupopup id="menu_EditPopup"
                         onpopupshowing="updateEditUIVisibility()"
                         onpopuphidden="updateEditUIVisibility()">
                <menuitem id="menu_undo"
                          label="&undoCmd.label;"
                          key="key_undo"
                          accesskey="&undoCmd.accesskey;"
                          command="cmd_undo"/>
                <menuitem id="menu_redo"
                          label="&redoCmd.label;"
                          key="key_redo"
                          accesskey="&redoCmd.accesskey;"
                          command="cmd_redo"/>
                <menuseparator/>
                <menuitem id="menu_cut"
                          label="&cutCmd.label;"
                          key="key_cut"
                          accesskey="&cutCmd.accesskey;"
                          command="cmd_cut"/>
                <menuitem id="menu_copy"
                          label="&copyCmd.label;"
                          key="key_copy"
                          accesskey="&copyCmd.accesskey;"
                          command="cmd_copy"/>
                <menuitem id="menu_paste"
                          label="&pasteCmd.label;"
                          key="key_paste"
                          accesskey="&pasteCmd.accesskey;"
                          command="cmd_paste"/>
                <menuitem id="menu_delete"
                          label="&deleteCmd.label;"
                          key="key_delete"
                          accesskey="&deleteCmd.accesskey;"
                          command="cmd_delete"/>
                <menuseparator/>
                <menuitem id="menu_selectAll"
                          label="&selectAllCmd.label;"
                          key="key_selectAll"
                          accesskey="&selectAllCmd.accesskey;"
                          command="cmd_selectAll"/>
                <menuseparator/>
                <menuitem id="menu_find"
                          label="&findOnCmd.label;"
                          accesskey="&findOnCmd.accesskey;"
                          key="key_find"
                          command="cmd_find"/>
                <menuitem id="menu_findAgain"
                          label="&findAgainCmd.label;"
                          accesskey="&findAgainCmd.accesskey;"
                          key="key_findAgain"
                          command="cmd_findAgain"/>
                <menuseparator hidden="true" id="textfieldDirection-separator"/>
                <menuitem id="textfieldDirection-swap"
                          command="cmd_switchTextDirection"
                          key="key_switchTextDirection"
                          label="&bidiSwitchTextDirectionItem.label;"
                          accesskey="&bidiSwitchTextDirectionItem.accesskey;"
                          hidden="true"/>
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

            <menu id="view-menu" label="&viewMenu.label;"
                  accesskey="&viewMenu.accesskey;">
              <menupopup id="menu_viewPopup">
                <menu id="viewToolbarsMenu"
                      label="&viewToolbarsMenu.label;"
                      accesskey="&viewToolbarsMenu.accesskey;"
                      onpopupshowing="onViewToolbarsPopupShowing(event);">
                  <menupopup>
                    <menuseparator/>
                    <menuitem id="menu_customizeToolbars"
                              label="&viewCustomizeToolbar.label;"
                              accesskey="&viewCustomizeToolbar.accesskey;"
                              command="cmd_CustomizeToolbars"/>
                  </menupopup>
                </menu>
                <menuitem id="toggle_taskbar"
                          label="&taskbarCmd.label;"
                          accesskey="&taskbarCmd.accesskey;"
                          type="checkbox"
                          command="cmd_toggleTaskbar"
                          checked="true" />
                <menu id="viewSidebarMenuMenu"
                      label="&viewSidebarMenu.label;"
                      accesskey="&viewSidebarMenu.accesskey;">
                  <menupopup id="viewSidebarMenu">
                    <menuitem id="menu_bookmarksSidebar"
                              key="viewBookmarksSidebarKb"
                              observes="viewBookmarksSidebar"
                              accesskey="&bookmarksSidebarCmd.accesskey;"/>
                    <menuitem id="menu_historySidebar"
                              key="key_gotoHistory"
                              observes="viewHistorySidebar"
                              label="&historyButton.label;"
                              accesskey="&historySidebarCmd.accesskey;"/>
                  </menupopup>
                </menu>
                <menuseparator/>
                <menuitem id="menu_stop"
                          label="&stopCmd.label;"
                          accesskey="&stopCmd.accesskey;"
                          command="Browser:Stop"
#ifdef XP_MACOSX
                          key="key_stop_mac"/>
#else
                          key="key_stop"/>
#endif
                <menuitem id="menu_reload"
                          label="&reloadCmd.label;"
                          accesskey="&reloadCmd.accesskey;"
                          key="key_reload"
                          command="Browser:ReloadOrDuplicate"
                          onclick="checkForMiddleClick(this, event);"/>
                <menuseparator/>
                <menu id="viewFullZoomMenu" label="&fullZoom.label;"
                      accesskey="&fullZoom.accesskey;"
                      onpopupshowing="FullZoom.updateMenu();">
                  <menupopup>
                    <menuitem id="menu_zoomEnlarge"
                              key="key_fullZoomEnlarge"
                              label="&fullZoomEnlargeCmd.label;"
                              accesskey="&fullZoomEnlargeCmd.accesskey;"
                              command="cmd_fullZoomEnlarge"/>
                    <menuitem id="menu_zoomReduce"
                              key="key_fullZoomReduce"
                              label="&fullZoomReduceCmd.label;"
                              accesskey="&fullZoomReduceCmd.accesskey;"
                              command="cmd_fullZoomReduce"/>
                    <menuseparator/>
                    <menuitem id="menu_zoomReset"
                              key="key_fullZoomReset"
                              label="&fullZoomResetCmd.label;"
                              accesskey="&fullZoomResetCmd.accesskey;"
                              command="cmd_fullZoomReset"/>
                    <menuseparator/>
                    <menuitem id="toggle_zoom"
                              label="&fullZoomToggleCmd.label;"
                              accesskey="&fullZoomToggleCmd.accesskey;"
                              type="checkbox"
                              command="cmd_fullZoomToggle"
                              checked="false"/>
                  </menupopup>
                </menu>
                <menu id="pageStyleMenu" label="&pageStyleMenu.label;"
                      accesskey="&pageStyleMenu.accesskey;" observes="isImage">
                  <menupopup onpopupshowing="stylesheetFillPopup(this);"
                             oncommand="stylesheetSwitchAll(window.content, event.target.getAttribute('data')); setStyleDisabled(false);">
                    <menuitem id="menu_pageStyleNoStyle"
                              label="&pageStyleNoStyle.label;"
                              accesskey="&pageStyleNoStyle.accesskey;"
                              oncommand="setStyleDisabled(true); event.stopPropagation();"
                              type="radio"/>
                    <menuitem id="menu_pageStylePersistentOnly"
                              label="&pageStylePersistentOnly.label;"
                              accesskey="&pageStylePersistentOnly.accesskey;"
                              type="radio"
                              checked="true"/>
                    <menuseparator/>
                  </menupopup>
                </menu>
                <menu id="charsetMenu"
                      label="&charsetMenu.label;"
                      accesskey="&charsetMenu.accesskey;"
                      datasources="rdf:charset-menu"
                      ref="NC:BrowserCharsetMenuRoot"
                      oncommand="MultiplexHandler(event)"
                      onpopupshowing="CreateMenu('browser');UpdateMenus(event)"
                      onpopupshown="CreateMenu('more-menu');"
                      observes="isImage">
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
                <menuitem id="menu_pageSource"
                          accesskey="&pageSourceCmd.accesskey;"
                          label="&pageSourceCmd.label;"
                          key="key_viewSource"
                          command="View:PageSource"/>
#ifndef XP_MACOSX
                <menuitem id="fullScreenItem"
                          accesskey="&fullScreenCmd.accesskey;"
                          label="&fullScreenCmd.label;"
                          key="key_fullScreen"
                          type="checkbox"
                          command="View:FullScreen"/>
#endif
                <menuseparator hidden="true" id="documentDirection-separator"/>
                <menuitem id="documentDirection-swap"
                          hidden="true"
                          label="&bidiSwitchPageDirectionItem.label;"
                          accesskey="&bidiSwitchPageDirectionItem.accesskey;"
                          oncommand="SwitchDocumentDirection(window.content)"/>
              </menupopup>
            </menu>

            <menu id="history-menu"
                  oncommand="var node = event.target.node; if (node) { PlacesUIUtils.markPageAsTyped(node.uri); openUILink(node.uri, event, false, true); }"
                  onclick="checkForMiddleClick(this, event);"
                  label="&historyMenu.label;"
                  accesskey="&historyMenu.accesskey;">
              <menupopup id="goPopup"
                         type="places"
                         onpopupshowing="HistoryMenu.onPopupShowing(this);"
                         place="place:type=0&amp;sort=4&amp;maxResults=10"
                         tooltip="btTooltip">
                <menuitem id="historyMenuBack"
                          label="&backCmd.label;"
#ifdef XP_MACOSX
                          key="goBackKb2"
#else
                          key="goBackKb"
#endif
                          chromedir="&locale.dir;"
                          command="Browser:BackOrBackDuplicate"
                          onclick="checkForMiddleClick(this, event);"/>
                <menuitem id="historyMenuForward"
                          label="&forwardCmd.label;"
#ifdef XP_MACOSX
                          key="goForwardKb2"
#else
                          key="goForwardKb"
#endif
                          chromedir="&locale.dir;"
                          command="Browser:ForwardOrForwardDuplicate"
                          onclick="checkForMiddleClick(this, event);"/>
                <menuitem id="historyMenuHome"
                          label="&historyHomeCmd.label;"
                          oncommand="BrowserGoHome(event);"
                          onclick="checkForMiddleClick(this, event);"
                          key="goHome"/>
                <menuitem id="menu_showAllHistory"
                          label="&showAllHistoryCmd2.label;"
#ifndef XP_MACOSX
                          key="showAllHistoryKb"
#endif
                          command="Browser:ShowAllHistory"/>
                <menuseparator id="startHistorySeparator"/>
                <menuseparator id="endHistorySeparator" builder="end"/>
                <menu id="historyUndoMenu"
                      label="&historyUndoMenu.label;"
                      disabled="true">
                  <menupopup id="historyUndoPopup"
                             onpopupshowing="HistoryMenu.populateUndoSubmenu();"/>
                </menu>
              </menupopup>
            </menu>

  <menu id="bookmarksMenu"
        label="&bookmarksMenu.label;"
        accesskey="&bookmarksMenu.accesskey;"
        ondragenter="PlacesMenuDNDController.onBookmarksMenuDragEnter(event);"
        ondrop="nsDragAndDrop.drop(event, BookmarksMenuDropHandler);"
        ondragover="nsDragAndDrop.dragOver(event, BookmarksMenuDropHandler);"
        ondragexit="nsDragAndDrop.dragExit(event, BookmarksMenuDropHandler);">
    <menupopup id="bookmarksMenuPopup"
               type="places"
               place="place:folder=BOOKMARKS_MENU"
               context="placesContext"
               openInTabs="children"
               oncommand="BookmarksEventHandler.onCommand(event);"
               onclick="BookmarksEventHandler.onClick(event);"
               onpopupshowing="BookmarksEventHandler.onPopupShowing(event);"
               tooltip="btTooltip">
      <menuitem id="menu_bookmarkThisPage"
                label="&bookmarkThisPageCmd.label;"
                command="Browser:AddBookmarkAs"
                key="addBookmarkAsKb"/>
      <menuitem id="subscribeToPageMenuitem"
                label="&subscribeToPageMenuitem.label;"
                oncommand="return FeedHandler.subscribeToFeed(null, event);"
                onclick="checkForMiddleClick(this, event);"
                disabled="true"/>
      <menu id="subscribeToPageMenupopup"
            label="&subscribeToPageMenupopup.label;"
            hidden="true">
        <menupopup id="subscribeToPageSubmenuMenupopup"
                   onpopupshowing="return FeedHandler.buildFeedList(event.target);"
                   oncommand="return FeedHandler.subscribeToFeed(null, event);"
                   onclick="checkForMiddleClick(this, event);"/>
      </menu>
      <menuitem id="menu_bookmarkAllTabs"
                label="&addCurPagesCmd.label;"
                command="Browser:BookmarkAllTabs"
                key="bookmarkAllTabsKb"/>
      <menuitem id="bookmarksShowAll"
                label="&organizeBookmarks.label;"
                command="Browser:ShowAllBookmarks"
                key="manBookmarkKb"/>
      <menuseparator id="organizeBookmarksSeparator"/>
      <menu id="bookmarksToolbarFolderMenu"
            class="menu-iconic bookmark-item"
            label="&personalbarCmd.label;"
            container="true">
        <menupopup id="bookmarksToolbarFolderPopup"
                   type="places"
                   place="place:folder=TOOLBAR"
                   context="placesContext"
                   onpopupshowing="BookmarksEventHandler.onPopupShowing(event);"/>
      </menu>
      <menuseparator/>
    </menupopup>
  </menu>

            <menu id="tools-menu"
                  label="&toolsMenu.label;"
                  accesskey="&toolsMenu.accesskey;">
              <menupopup id="menu_ToolsPopup">
              <menuitem id="menu_search"
                        label="&search.label;"
                        accesskey="&search.accesskey;"
                        key="key_search"
                        command="Tools:Search"/>
              <menuseparator id="browserToolsSeparator"/>
              <menuitem id="menu_openDownloads"
                        label="&downloads.label;"
                        accesskey="&downloads.accesskey;"
                        key="key_openDownloads"
                        command="Tools:Downloads"/>
              <menuitem id="menu_openAddons"
                        label="&addons.label;"
                        accesskey="&addons.accesskey;"
                        command="Tools:Addons"/>
              <menuseparator id="devToolsSeparator"/>
              <menuitem id="javascriptConsole"
                        label="&errorConsoleCmd.label;"
                        accesskey="&errorConsoleCmd.accesskey;"
                        key="key_errorConsole"
                        oncommand="toJavaScriptConsole();"/>
              <menuitem id="menu_pageInfo"
                        accesskey="&pageInfoCmd.accesskey;"
                        label="&pageInfoCmd.label;"
#ifndef XP_WIN
                        key="key_viewInfo"
#endif
                        command="View:PageInfo"/>
              <menuseparator id="sanitizeSeparator"/>
              <menuitem id="privateBrowsingItem"
                        label="&privateBrowsingCmd.start.label;"
                        accesskey="&privateBrowsingCmd.start.accesskey;"
                        startlabel="&privateBrowsingCmd.start.label;"
                        startaccesskey="&privateBrowsingCmd.start.accesskey;"
                        stoplabel="&privateBrowsingCmd.stop.label;"
                        stopaccesskey="&privateBrowsingCmd.stop.accesskey;"
                        key="key_privatebrowsing"
                        command="Tools:PrivateBrowsing"/>
              <menuitem id="sanitizeItem"
                        accesskey="&clearRecentHistoryCmd.accesskey;"
                        label="&clearRecentHistoryCmd.label;"
                        key="key_sanitize"
                        command="Tools:Sanitize"/>
#ifndef XP_UNIX
              <menuseparator id="prefSep"/>
              <menuitem id="menu_preferences"
                        label="&preferencesCmd.label;"
                        accesskey="&preferencesCmd.accesskey;"
                        oncommand="openPreferences();"/>
#endif
              </menupopup>
            </menu>

#ifdef XP_MACOSX
          <menu id="windowMenu" />
#endif
          <menu id="helpMenu" />
        </menubar>
