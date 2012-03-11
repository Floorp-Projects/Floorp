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
#   Rob Campbell <rcampbell@mozilla.com>
#   Steffen Wilberg <steffen.wilberg@web.de>
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
                onpopupshowing="if (event.target.parentNode.parentNode == this &amp;&amp;
                                    !('@mozilla.org/widget/nativemenuservice;1' in Cc))
                                  this.setAttribute('openedwithkey',
                                                    event.target.parentNode.openedWithKey);"
                style="border:0px;padding:0px;margin:0px;-moz-appearance:none">
            <menu id="file-menu" label="&fileMenu.label;"
                  accesskey="&fileMenu.accesskey;">
              <menupopup id="menu_FilePopup">
                <menuitem id="menu_newNavigatorTab"
                          label="&tabCmd.label;"
                          command="cmd_newNavigatorTab"
                          key="key_newNavigatorTab"
                          accesskey="&tabCmd.accesskey;"/>
                <menuitem id="menu_newNavigator"
                          label="&newNavigatorCmd.label;"
                          accesskey="&newNavigatorCmd.accesskey;"
                          key="key_newNavigator"
                          command="cmd_newNavigator"/>
                <menuitem id="menu_openLocation"
                          class="show-only-for-keyboard"
                          label="&openLocationCmd.label;"
                          command="Browser:OpenLocation"
                          key="focusURLBar"
                          accesskey="&openLocationCmd.accesskey;"/>
                <menuitem id="menu_openFile"
                          label="&openFileCmd.label;"
                          command="Browser:OpenFile"
                          key="openFileKb"
                          accesskey="&openFileCmd.accesskey;"/>
                <menuitem id="menu_close"
                          class="show-only-for-keyboard"
                          label="&closeCmd.label;"
                          key="key_close"
                          accesskey="&closeCmd.accesskey;"
                          command="cmd_close"/>
                <menuitem id="menu_closeWindow"
                          class="show-only-for-keyboard"
                          hidden="true"
                          command="cmd_closeWindow"
                          key="key_closeWindow"
                          label="&closeWindow.label;"
                          accesskey="&closeWindow.accesskey;"/>
                <menuseparator/>
                <menuitem id="menu_savePage"
                          label="&savePageCmd.label;"
                          accesskey="&savePageCmd.accesskey;"
                          key="key_savePage"
                          command="Browser:SavePage"/>
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
                <menuitem id="goOfflineMenuitem"
                          label="&goOfflineCmd.label;"
                          accesskey="&goOfflineCmd.accesskey;"
                          type="checkbox"
                          observes="workOfflineMenuitemState"
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
                          class="show-only-for-keyboard"
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
                      accesskey="&viewToolbarsMenu.accesskey;">
                  <menupopup onpopupshowing="onViewToolbarsPopupShowing(event);">
                    <menuseparator/>
                    <menuitem id="menu_tabsOnTop"
                              command="cmd_ToggleTabsOnTop"
                              type="checkbox"
                              label="&viewTabsOnTop.label;"
                              accesskey="&viewTabsOnTop.accesskey;"/>
                    <menuseparator/>
                    <menuitem id="menu_customizeToolbars"
                              label="&viewCustomizeToolbar.label;"
                              accesskey="&viewCustomizeToolbar.accesskey;"
                              command="cmd_CustomizeToolbars"/>
                  </menupopup>
                </menu>
                <menu id="viewSidebarMenuMenu"
                      label="&viewSidebarMenu.label;"
                      accesskey="&viewSidebarMenu.accesskey;">
                  <menupopup id="viewSidebarMenu">
                    <menuitem id="menu_bookmarksSidebar"
                              key="viewBookmarksSidebarKb"
                              observes="viewBookmarksSidebar"
                              accesskey="&bookmarksButton.accesskey;"/>
                    <menuitem id="menu_historySidebar"
                              key="key_gotoHistory"
                              observes="viewHistorySidebar"
                              label="&historyButton.label;"
                              accesskey="&historySidebarCmd.accesskey;"/>
                  </menupopup>
                </menu>
                <menuseparator/>
                <menuitem id="menu_stop"
                          class="show-only-for-keyboard"
                          label="&stopCmd.label;"
                          accesskey="&stopCmd.accesskey;"
                          command="Browser:Stop"
#ifdef XP_MACOSX
                          key="key_stop_mac"/>
#else
                          key="key_stop"/>
#endif
                <menuitem id="menu_reload"
                          class="show-only-for-keyboard"
                          label="&reloadCmd.label;"
                          accesskey="&reloadCmd.accesskey;"
                          key="key_reload"
                          command="Browser:ReloadOrDuplicate"
                          onclick="checkForMiddleClick(this, event);"/>
                <menuseparator class="show-only-for-keyboard"/>
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
                  <menupopup onpopupshowing="gPageStyleMenu.fillPopup(this);"
                             oncommand="gPageStyleMenu.switchStyleSheet(event.target.getAttribute('data'));">
                    <menuitem id="menu_pageStyleNoStyle"
                              label="&pageStyleNoStyle.label;"
                              accesskey="&pageStyleNoStyle.accesskey;"
                              oncommand="gPageStyleMenu.disableStyle(); event.stopPropagation();"
                              type="radio"/>
                    <menuitem id="menu_pageStylePersistentOnly"
                              label="&pageStylePersistentOnly.label;"
                              accesskey="&pageStylePersistentOnly.accesskey;"
                              type="radio"
                              checked="true"/>
                    <menuseparator/>
                  </menupopup>
                </menu>
#include browser-charsetmenu.inc
                <menuseparator/>
                <menuitem id="fullScreenItem"
                          accesskey="&fullScreenCmd.accesskey;"
                          label="&fullScreenCmd.label;"
                          key="key_fullScreen"
                          type="checkbox"
                          observes="View:FullScreen"/>
                <menuitem id="menu_showAllTabs"
                          hidden="true"
                          accesskey="&showAllTabsCmd.accesskey;"
                          label="&showAllTabsCmd.label;"
                          command="Browser:ShowAllTabs"
                          key="key_showAllTabs"/>
                <menuseparator hidden="true" id="documentDirection-separator"/>
                <menuitem id="documentDirection-swap"
                          hidden="true"
                          label="&bidiSwitchPageDirectionItem.label;"
                          accesskey="&bidiSwitchPageDirectionItem.accesskey;"
                          oncommand="SwitchDocumentDirection(window.content)"/>
              </menupopup>
            </menu>

            <menu id="history-menu"
                  label="&historyMenu.label;"
                  accesskey="&historyMenu.accesskey;">
              <menupopup id="goPopup"
#ifndef XP_MACOSX
                         placespopup="true"
#endif
                         oncommand="this.parentNode._placesView._onCommand(event);"
                         onclick="checkForMiddleClick(this, event);"
                         onpopupshowing="if (!this.parentNode._placesView)
                                           new HistoryMenu(event);"
                         tooltip="bhTooltip"
                         popupsinherittooltip="true">
                <menuitem id="historyMenuBack"
                          class="show-only-for-keyboard"
                          label="&backCmd.label;"
#ifdef XP_MACOSX
                          key="goBackKb2"
#else
                          key="goBackKb"
#endif
                          command="Browser:BackOrBackDuplicate"
                          onclick="checkForMiddleClick(this, event);"/>
                <menuitem id="historyMenuForward"
                          class="show-only-for-keyboard"
                          label="&forwardCmd.label;"
#ifdef XP_MACOSX
                          key="goForwardKb2"
#else
                          key="goForwardKb"
#endif
                          command="Browser:ForwardOrForwardDuplicate"
                          onclick="checkForMiddleClick(this, event);"/>
                <menuitem id="historyMenuHome"
                          class="show-only-for-keyboard"
                          label="&historyHomeCmd.label;"
                          oncommand="BrowserGoHome(event);"
                          onclick="checkForMiddleClick(this, event);"
                          key="goHome"/>
                <menuseparator id="historyMenuHomeSeparator"
                               class="show-only-for-keyboard"/>
                <menuitem id="menu_showAllHistory"
                          label="&showAllHistoryCmd2.label;"
#ifndef XP_MACOSX
                          key="showAllHistoryKb"
#endif
                          command="Browser:ShowAllHistory"/>
                <menuseparator id="showAllHistorySeparator"/>
#ifdef MOZ_SERVICES_SYNC
                <menuitem id="sync-tabs-menuitem"
                          class="syncTabsMenuItem"
                          label="&syncTabsMenu.label;"
                          oncommand="BrowserOpenSyncTabs();"
                          disabled="true"/>
#endif
                <menuitem id="historyRestoreLastSession"
                          class="restoreLastSession"
                          label="&historyRestoreLastSession.label;"
                          oncommand="restoreLastSession();"
                          disabled="true"/>
                <menu id="historyUndoMenu"
                      class="recentlyClosedTabsMenu"
                      label="&historyUndoMenu.label;"
                      disabled="true">
                  <menupopup id="historyUndoPopup"
#ifndef XP_MACOSX
                             placespopup="true"
#endif
                             onpopupshowing="document.getElementById('history-menu')._placesView.populateUndoSubmenu();"/>
                </menu>
                <menu id="historyUndoWindowMenu"
                      class="recentlyClosedWindowsMenu"
                      label="&historyUndoWindowMenu.label;"
                      disabled="true">
                  <menupopup id="historyUndoWindowPopup"
#ifndef XP_MACOSX
                             placespopup="true"
#endif
                             onpopupshowing="document.getElementById('history-menu')._placesView.populateUndoWindowSubmenu();"/>
                </menu>
                <menuseparator id="startHistorySeparator"
                               class="hide-if-empty-places-result"/>
              </menupopup>
            </menu>

  <menu id="bookmarksMenu"
        label="&bookmarksMenu.label;"
        accesskey="&bookmarksMenu.accesskey;"
        ondragenter="PlacesMenuDNDHandler.onDragEnter(event);"
        ondragover="PlacesMenuDNDHandler.onDragOver(event);"
        ondrop="PlacesMenuDNDHandler.onDrop(event);">
    <menupopup id="bookmarksMenuPopup"
#ifndef XP_MACOSX
               placespopup="true"
#endif
               context="placesContext"
               openInTabs="children"
               oncommand="BookmarksEventHandler.onCommand(event, this.parentNode._placesView);"
               onclick="BookmarksEventHandler.onClick(event, this.parentNode._placesView);"
               onpopupshowing="PlacesCommandHook.updateBookmarkAllTabsCommand();
                               if (!this.parentNode._placesView)
                                 new PlacesMenu(event, 'place:folder=BOOKMARKS_MENU');"
               tooltip="bhTooltip" popupsinherittooltip="true">
      <menuitem id="bookmarksShowAll"
                label="&showAllBookmarks2.label;"
                command="Browser:ShowAllBookmarks"
                key="manBookmarkKb"/>
      <menuseparator id="organizeBookmarksSeparator"/>
      <menuitem id="menu_bookmarkThisPage"
                label="&bookmarkThisPageCmd.label;"
                command="Browser:AddBookmarkAs"
                key="addBookmarkAsKb"/>
      <menuitem id="subscribeToPageMenuitem"
#ifndef XP_MACOSX
                class="menuitem-iconic"
#endif
                label="&subscribeToPageMenuitem.label;"
                oncommand="return FeedHandler.subscribeToFeed(null, event);"
                onclick="checkForMiddleClick(this, event);"
                observes="singleFeedMenuitemState"/>
      <menu id="subscribeToPageMenupopup"
#ifndef XP_MACOSX
            class="menu-iconic"
#endif
            label="&subscribeToPageMenupopup.label;"
            observes="multipleFeedsMenuState">
        <menupopup id="subscribeToPageSubmenuMenupopup"
                   onpopupshowing="return FeedHandler.buildFeedList(event.target);"
                   oncommand="return FeedHandler.subscribeToFeed(null, event);"
                   onclick="checkForMiddleClick(this, event);"/>
      </menu>
      <menuitem id="menu_bookmarkAllTabs"
                label="&addCurPagesCmd.label;"
                class="show-only-for-keyboard"
                command="Browser:BookmarkAllTabs"
                key="bookmarkAllTabsKb"/>
      <menuseparator id="bookmarksToolbarSeparator"/>
      <menu id="bookmarksToolbarFolderMenu"
            class="menu-iconic bookmark-item"
            label="&personalbarCmd.label;"
            container="true">
        <menupopup id="bookmarksToolbarFolderPopup"
#ifndef XP_MACOSX
                   placespopup="true"
#endif
                   context="placesContext"
                   onpopupshowing="if (!this.parentNode._placesView)
                                     new PlacesMenu(event, 'place:folder=TOOLBAR');"/>
      </menu>
      <menuseparator id="bookmarksMenuItemsSeparator"/>
      <!-- Bookmarks menu items -->
      <menuseparator builder="end"
                     class="hide-if-empty-places-result"/>
      <menuitem id="menu_unsortedBookmarks"
                label="&unsortedBookmarksCmd.label;"
                oncommand="PlacesCommandHook.showPlacesOrganizer('UnfiledBookmarks');"/>
    </menupopup>
  </menu>

            <menu id="tools-menu"
                  label="&toolsMenu.label;"
                  accesskey="&toolsMenu.accesskey;">
              <menupopup id="menu_ToolsPopup"
#ifdef MOZ_SERVICES_SYNC
                         onpopupshowing="gSyncUI.updateUI();"
#endif
                         >
              <menuitem id="menu_search"
                        class="show-only-for-keyboard"
                        label="&search.label;"
                        accesskey="&search.accesskey;"
                        key="key_search"
                        command="Tools:Search"/>
              <menuseparator id="browserToolsSeparator"
                             class="show-only-for-keyboard"/>
              <menuitem id="menu_openDownloads"
                        label="&downloads.label;"
                        accesskey="&downloads.accesskey;"
                        key="key_openDownloads"
                        command="Tools:Downloads"/>
              <menuitem id="menu_openAddons"
                        label="&addons.label;"
                        accesskey="&addons.accesskey;"
                        key="key_openAddons"
                        command="Tools:Addons"/>
#ifdef MOZ_SERVICES_SYNC
              <!-- only one of sync-setup or sync-menu will be showing at once -->
              <menuitem id="sync-setup"
                        label="&syncSetup.label;"
                        accesskey="&syncSetup.accesskey;"
                        observes="sync-setup-state"
                        oncommand="gSyncUI.openSetup()"/>
              <menuitem id="sync-syncnowitem"
                        label="&syncSyncNowItem.label;"
                        accesskey="&syncSyncNowItem.accesskey;"
                        observes="sync-syncnow-state"
                        oncommand="gSyncUI.doSync(event);"/>
#endif
              <menuseparator id="devToolsSeparator"/>
              <menu id="webDeveloperMenu"
                    label="&webDeveloperMenu.label;"
                    accesskey="&webDeveloperMenu.accesskey;">
                <menupopup id="menuWebDeveloperPopup"
                           onpopupshowing="onWebDeveloperMenuShowing();">
                  <menuitem id="webConsole"
                            type="checkbox"
                            label="&webConsoleCmd.label;"
                            accesskey="&webConsoleCmd.accesskey;"
                            key="key_webConsole"
                            command="Tools:WebConsole"/>
                  <menuitem id="menu_pageinspect"
                            type="checkbox"
                            hidden="true"
                            label="&inspectMenu.label;"
                            accesskey="&inspectMenu.accesskey;"
                            key="key_inspect"
                            command="Tools:Inspect"/>
                  <menuitem id="menu_debugger"
                            hidden="true"
                            label="&debuggerMenu.label;"
                            key="key_debugger"
                            command="Tools:Debugger"/>
                  <menuitem id="menu_scratchpad"
                            hidden="true"
                            label="&scratchpad.label;"
                            accesskey="&scratchpad.accesskey;"
                            key="key_scratchpad"
                            command="Tools:Scratchpad"/>
                  <menuitem id="menu_styleeditor"
                            hidden="true"
                            label="&styleeditor.label;"
                            accesskey="&styleeditor.accesskey;"
                            key="key_styleeditor"
                            command="Tools:StyleEditor"/>
                  <menuitem id="menu_pageSource"
                            accesskey="&pageSourceCmd.accesskey;"
                            label="&pageSourceCmd.label;"
                            key="key_viewSource"
                            command="View:PageSource"/>
                  <menuitem id="javascriptConsole"
                            hidden="true"
                            label="&errorConsoleCmd.label;"
                            accesskey="&errorConsoleCmd.accesskey;"
                            key="key_errorConsole"
                            oncommand="toJavaScriptConsole();"/>
                  <menuseparator id="devToolsEndSeparator"/>
                  <menuitem id="getMoreDevtools"
                            label="&getMoreDevtoolsCmd.label;"
                            accesskey="&getMoreDevtoolsCmd.accesskey;"
                            oncommand="openUILinkIn('https://addons.mozilla.org/firefox/collections/mozilla/webdeveloper/', 'tab');"/>
                </menupopup>
              </menu>
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
                        accesskey="&clearRecentHistory.accesskey;"
                        label="&clearRecentHistory.label;"
                        key="key_sanitize"
                        command="Tools:Sanitize"/>
#ifndef XP_UNIX
              <menuseparator id="prefSep"/>
              <menuitem id="menu_preferences"
                        label="&preferencesCmd2.label;"
                        accesskey="&preferencesCmd2.accesskey;"
                        oncommand="openPreferences();"/>
#endif
              </menupopup>
            </menu>

#ifdef XP_MACOSX
          <menu id="windowMenu" />
#endif
          <menu id="helpMenu" />
        </menubar>
