# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

       <menubar id="main-menubar"
                onpopupshowing="if (event.target.parentNode.parentNode == this &amp;&amp;
                                    !('@mozilla.org/widget/nativemenuservice;1' in Cc))
                                  this.setAttribute('openedwithkey',
                                                    event.target.parentNode.openedWithKey);">
            <menu id="file-menu" label="&fileMenu.label;"
                  accesskey="&fileMenu.accesskey;">
              <menupopup id="menu_FilePopup"
                         onpopupshowing="updateUserContextUIVisibility();">
                <menuitem id="menu_newNavigatorTab"
                          label="&tabCmd.label;"
                          command="cmd_newNavigatorTab"
                          key="key_newNavigatorTab"
                          accesskey="&tabCmd.accesskey;"/>
                <menu id="menu_newUserContext"
                      label="&newUserContext.label;"
                      accesskey="&newUserContext.accesskey;"
                      hidden="true">
                  <menupopup onpopupshowing="return createUserContextMenu(event);" />
                </menu>
                <menuitem id="menu_newNavigator"
                          label="&newNavigatorCmd.label;"
                          accesskey="&newNavigatorCmd.accesskey;"
                          key="key_newNavigator"
                          command="cmd_newNavigator"/>
                <menuitem id="menu_newPrivateWindow"
                          label="&newPrivateWindow.label;"
                          accesskey="&newPrivateWindow.accesskey;"
                          command="Tools:PrivateBrowsing"
                          key="key_privatebrowsing"/>
#ifdef MAC_NON_BROWSER_WINDOW
                <menuitem id="menu_openLocation"
                          label="&openLocationCmd.label;"
                          command="Browser:OpenLocation"
                          key="focusURLBar"/>
#endif
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
                          label="&emailPageCmd.label;"
                          accesskey="&emailPageCmd.accesskey;"
                          command="Browser:SendLink"/>
                <menuseparator/>
#if !defined(MOZ_WIDGET_GTK)
                <menuitem id="menu_printSetup"
                          label="&printSetupCmd.label;"
                          accesskey="&printSetupCmd.accesskey;"
                          command="cmd_pageSetup"/>
#endif
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
                          label="&quitApplicationCmdWin2.label;"
                          accesskey="&quitApplicationCmdWin2.accesskey;"
#elifdef XP_MACOSX
                          label="&quitApplicationCmdMac2.label;"
#else
                          label="&quitApplicationCmd.label;"
                          accesskey="&quitApplicationCmd.accesskey;"
#endif
                          key="key_quitApplication"
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
                          oncommand="openPreferences(undefined, {origin: 'menubar'});"/>
#endif
#endif
              </menupopup>
            </menu>

            <menu id="view-menu" label="&viewMenu.label;"
                  accesskey="&viewMenu.accesskey;">
              <menupopup id="menu_viewPopup"
                         onpopupshowing="updateCharacterEncodingMenuState();">
                <menu id="viewToolbarsMenu"
                      label="&viewToolbarsMenu.label;"
                      accesskey="&viewToolbarsMenu.accesskey;">
                  <menupopup onpopupshowing="onViewToolbarsPopupShowing(event);">
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
                              observes="viewBookmarksSidebar"/>
                    <menuitem id="menu_historySidebar"
                              key="key_gotoHistory"
                              observes="viewHistorySidebar"
                              label="&historyButton.label;"/>
                    <menuitem id="menu_tabsSidebar"
                              class="sync-ui-item"
                              observes="viewTabsSidebar"
                              label="&syncedTabs.sidebar.label;"/>
                  </menupopup>
                </menu>
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
                  <menupopup onpopupshowing="gPageStyleMenu.fillPopup(this);">
                    <menuitem id="menu_pageStyleNoStyle"
                              label="&pageStyleNoStyle.label;"
                              accesskey="&pageStyleNoStyle.accesskey;"
                              oncommand="gPageStyleMenu.disableStyle();"
                              type="radio"/>
                    <menuitem id="menu_pageStylePersistentOnly"
                              label="&pageStylePersistentOnly.label;"
                              accesskey="&pageStylePersistentOnly.accesskey;"
                              oncommand="gPageStyleMenu.switchStyleSheet('');"
                              type="radio"
                              checked="true"/>
                    <menuseparator/>
                  </menupopup>
                </menu>
                <menu id="charsetMenu"
                    label="&charsetMenu2.label;"
                    accesskey="&charsetMenu2.accesskey;"
                    oncommand="BrowserSetForcedCharacterSet(event.target.getAttribute('charset'));"
                    onpopupshowing="CharsetMenu.build(event.target); UpdateCurrentCharset(this);">
                  <menupopup>
                  </menupopup>
                </menu>
                <menuseparator/>
#ifdef XP_MACOSX
                <menuitem id="enterFullScreenItem"
                          accesskey="&enterFullScreenCmd.accesskey;"
                          label="&enterFullScreenCmd.label;"
                          key="key_fullScreen">
                  <observes element="View:FullScreen" attribute="oncommand"/>
                  <observes element="View:FullScreen" attribute="disabled"/>
                </menuitem>
                <menuitem id="exitFullScreenItem"
                          accesskey="&exitFullScreenCmd.accesskey;"
                          label="&exitFullScreenCmd.label;"
                          key="key_fullScreen"
                          hidden="true">
                  <observes element="View:FullScreen" attribute="oncommand"/>
                  <observes element="View:FullScreen" attribute="disabled"/>
                </menuitem>
#else
                <menuitem id="fullScreenItem"
                          accesskey="&fullScreenCmd.accesskey;"
                          label="&fullScreenCmd.label;"
                          key="key_fullScreen"
                          type="checkbox"
                          observes="View:FullScreen"/>
#endif
                <menuitem id="menu_readerModeItem"
                          observes="View:ReaderView"
                          key="key_toggleReaderMode"
                          hidden="true"/>
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
                          oncommand="gBrowser.selectedBrowser
                                             .messageManager
                                             .sendAsyncMessage('SwitchDocumentDirection');"/>
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
                <menuitem id="menu_showAllHistory"
                          label="&showAllHistoryCmd2.label;"
#ifndef XP_MACOSX
                          key="showAllHistoryKb"
#endif
                          command="Browser:ShowAllHistory"/>
                <menuitem id="sanitizeItem"
                          label="&clearRecentHistory.label;"
                          key="key_sanitize"
                          command="Tools:Sanitize"/>
                <menuseparator id="sanitizeSeparator"/>
                <menuitem id="sync-tabs-menuitem"
                          label="&syncTabsMenu3.label;"
                          oncommand="BrowserOpenSyncTabs();"
                          hidden="true"/>
                <menuitem id="historyRestoreLastSession"
                          label="&historyRestoreLastSession.label;"
                          command="Browser:RestoreLastSession"/>
                <menu id="hiddenTabsMenu"
                      label="&hiddenTabs.label;"
                      hidden="true">
                  <menupopup
#ifndef XP_MACOSX
                             placespopup="true"
#endif
                             />
                </menu>
                <menu id="historyUndoMenu"
                      label="&historyUndoMenu.label;"
                      disabled="true">
                  <menupopup id="historyUndoPopup"
#ifndef XP_MACOSX
                             placespopup="true"
#endif
                             onpopupshowing="document.getElementById('history-menu')._placesView.populateUndoSubmenu();"/>
                </menu>
                <menu id="historyUndoWindowMenu"
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
               onmouseup="BookmarksEventHandler.onMouseUp(event);"
               oncommand="BookmarksEventHandler.onCommand(event);"
               onclick="BookmarksEventHandler.onClick(event, this.parentNode._placesView);"
               onpopupshowing="BookmarkingUI.onMainMenuPopupShowing(event);
                               if (!this.parentNode._placesView)
                                 new PlacesMenu(event, `place:parent=${PlacesUtils.bookmarks.menuGuid}`);"
               tooltip="bhTooltip" popupsinherittooltip="true">
      <menuitem id="bookmarksShowAll"
                label="&showAllBookmarks2.label;"
                command="Browser:ShowAllBookmarks"
                key="manBookmarkKb"/>
      <menuseparator id="organizeBookmarksSeparator"/>
      <menuitem id="menu_bookmarkThisPage"
                command="Browser:AddBookmarkAs"
                observes="bookmarkThisPageBroadcaster"
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
                                     new PlacesMenu(event, `place:parent=${PlacesUtils.bookmarks.toolbarGuid}`);"/>
      </menu>
      <menu id="menu_unsortedBookmarks"
            class="menu-iconic bookmark-item"
            label="&otherBookmarksCmd.label;"
            container="true">
        <menupopup id="otherBookmarksFolderPopup"
#ifndef XP_MACOSX
                   placespopup="true"
#endif
                   context="placesContext"
                   onpopupshowing="if (!this.parentNode._placesView)
                                     new PlacesMenu(event, `place:parent=${PlacesUtils.bookmarks.unfiledGuid}`);"/>
      </menu>
      <menu id="menu_mobileBookmarks"
            class="menu-iconic bookmark-item"
            label="&mobileBookmarksCmd.label;"
            hidden="true"
            container="true">
        <menupopup id="mobileBookmarksFolderPopup"
#ifndef XP_MACOSX
                   placespopup="true"
#endif
                   context="placesContext"
                   onpopupshowing="if (!this.parentNode._placesView)
                                     new PlacesMenu(event, `place:parent=${PlacesUtils.bookmarks.mobileGuid}`);"/>
      </menu>
      <menuseparator id="bookmarksMenuItemsSeparator"/>
      <!-- Bookmarks menu items -->
    </menupopup>
  </menu>

            <menu id="tools-menu"
                  label="&toolsMenu.label;"
                  accesskey="&toolsMenu.accesskey;">
              <menupopup id="menu_ToolsPopup">
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

              <!-- only one of sync-setup, sync-unverifieditem, sync-syncnowitem or sync-reauthitem will be showing at once -->
              <menuitem id="sync-setup"
                        class="sync-ui-item"
                        label="&syncSignIn.label;"
                        accesskey="&syncSignIn.accesskey;"
                        observes="sync-setup-state"
                        oncommand="gSync.openPrefs('menubar')"/>
              <menuitem id="sync-unverifieditem"
                        class="sync-ui-item"
                        label="&syncSignIn.label;"
                        accesskey="&syncSignIn.accesskey;"
                        observes="sync-unverified-state"
                        oncommand="gSync.openPrefs('menubar')"/>
              <menuitem id="sync-syncnowitem"
                        class="sync-ui-item"
                        label="&syncSyncNowItem.label;"
                        accesskey="&syncSyncNowItem.accesskey;"
                        observes="sync-syncnow-state"
                        oncommand="gSync.doSync(event);"/>
              <menuitem id="sync-reauthitem"
                        class="sync-ui-item"
                        label="&syncReAuthItem.label;"
                        accesskey="&syncReAuthItem.accesskey;"
                        observes="sync-reauth-state"
                        oncommand="gSync.openSignInAgainPage('menubar');"/>
              <menuseparator id="devToolsSeparator"/>
              <menu id="webDeveloperMenu"
                    label="&webDeveloperMenu.label;"
                    accesskey="&webDeveloperMenu.accesskey;">
                <menupopup id="menuWebDeveloperPopup">
                  <menuitem id="menu_pageSource"
                            observes="devtoolsMenuBroadcaster_PageSource"
                            accesskey="&pageSourceCmd.accesskey;"/>
                </menupopup>
              </menu>
              <menuitem id="menu_pageInfo"
                        accesskey="&pageInfoCmd.accesskey;"
                        label="&pageInfoCmd.label;"
#ifndef XP_WIN
                        key="key_viewInfo"
#endif
                        command="View:PageInfo"/>
#ifndef XP_UNIX
              <menuseparator id="prefSep"/>
              <menuitem id="menu_preferences"
                        label="&preferencesCmd2.label;"
                        accesskey="&preferencesCmd2.accesskey;"
                        oncommand="openPreferences(undefined, {origin: 'menubar'});"/>
#endif
#ifdef MOZ_DEBUG
              <menuitem label="&ldbCmd.label;"
                        accesskey="&ldbCmd.accesskey;"
                        oncommand="toOpenWindowByType('mozapp:layoutdebug',
                                          'chrome://layoutdebug/content/');"/>
#endif
#ifdef XP_MACOSX
<!-- nsMenuBarX hides these and uses them to build the Application menu. -->
              <menuitem id="menu_preferences" label="&preferencesCmdMac.label;" key="key_preferencesCmdMac" oncommand="openPreferences(undefined, {origin: 'commandLineLegacy'});"/>
              <menuitem id="menu_mac_services" label="&servicesMenuMac.label;"/>
              <menuitem id="menu_mac_hide_app" label="&hideThisAppCmdMac2.label;" key="key_hideThisAppCmdMac"/>
              <menuitem id="menu_mac_hide_others" label="&hideOtherAppsCmdMac.label;" key="key_hideOtherAppsCmdMac"/>
              <menuitem id="menu_mac_show_all" label="&showAllAppsCmdMac.label;"/>
#endif
              </menupopup>
            </menu>
#ifdef XP_MACOSX
            <menu id="windowMenu"
                  label="&windowMenu.label;"
                  onpopupshowing="macWindowMenuDidShow();"
                  onpopuphidden="macWindowMenuDidHide();"
                  >
                <menupopup id="windowPopup">
                    <menuitem command="minimizeWindow" key="key_minimizeWindow"/>
                    <menuitem command="zoomWindow"/>
                    <!-- decomment when "BringAllToFront" is implemented
                        <menuseparator/>
                        <menuitem label="&bringAllToFront.label;" disabled="true"/> -->
                    <menuseparator id="sep-window-list"/>
                </menupopup>
            </menu>
#endif
            <menu id="helpMenu"
#ifdef XP_WIN
                  label="&helpMenuWin.label;"
                  accesskey="&helpMenuWin.accesskey;"
#else
                  label="&helpMenu.label;"
                  accesskey="&helpMenu.accesskey;"
#endif
                  >
              <menupopup id="menu_HelpPopup" onpopupshowing="buildHelpMenu();">
                <menuitem id="menu_openHelp"
                          oncommand="openHelpLink('firefox-help')"
                          onclick="checkForMiddleClick(this, event);"
                          label="&productHelp2.label;"
                          accesskey="&productHelp2.accesskey;"
#ifdef XP_MACOSX
                          key="key_openHelpMac"/>
#else
                          />
#endif
                <menuitem id="menu_openTour"
                          oncommand="openTourPage();"
                          label="&helpShowTour2.label;"
                          accesskey="&helpShowTour2.accesskey;"/>
                <menuitem id="menu_keyboardShortcuts"
                          oncommand="openHelpLink('keyboard-shortcuts')"
                          onclick="checkForMiddleClick(this, event);"
                          label="&helpKeyboardShortcuts.label;"
                          accesskey="&helpKeyboardShortcuts.accesskey;"/>
                <menuitem id="troubleShooting"
                          accesskey="&helpTroubleshootingInfo.accesskey;"
                          label="&helpTroubleshootingInfo.label;"
                          oncommand="openTroubleshootingPage()"
                          onclick="checkForMiddleClick(this, event);"/>
                <menuitem id="feedbackPage"
                          accesskey="&helpFeedbackPage.accesskey;"
                          label="&helpFeedbackPage.label;"
                          oncommand="openFeedbackPage()"
                          onclick="checkForMiddleClick(this, event);"/>
                <menuitem id="helpSafeMode"
                          accesskey="&helpSafeMode.accesskey;"
                          label="&helpSafeMode.label;"
                          stopaccesskey="&helpSafeMode.stop.accesskey;"
                          stoplabel="&helpSafeMode.stop.label;"
                          oncommand="safeModeRestart();"/>
                <menuitem id="menu_HelpPopup_reportPhishingtoolmenu"
                          label="&reportDeceptiveSiteMenu.title;"
                          accesskey="&reportDeceptiveSiteMenu.accesskey;"
                          observes="reportPhishingBroadcaster"
                          oncommand="openUILink(gSafeBrowsing.getReportURL('Phish'), event, {triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({})});"
                          onclick="checkForMiddleClick(this, event);"
                          hidden="true"/>
                <menuitem id="menu_HelpPopup_reportPhishingErrortoolmenu"
                          label="&safeb.palm.notdeceptive.label;"
                          accesskey="&safeb.palm.notdeceptive.accesskey;"
                          observes="reportPhishingErrorBroadcaster"
                          oncommand="ReportFalseDeceptiveSite();"
                          onclick="checkForMiddleClick(this, event);"
                          hidden="true"/>
                <menuseparator id="aboutSeparator"/>
                <menuitem id="aboutName"
                          accesskey="&aboutProduct2.accesskey;"
                          label="&aboutProduct2.label;"
                          oncommand="openAboutDialog();"/>
              </menupopup>
          </menu>
        </menubar>
