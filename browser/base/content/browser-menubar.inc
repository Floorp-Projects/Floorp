# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

       <menubar id="main-menubar"
                onpopupshowing="if (event.target.parentNode.parentNode == this &amp;&amp;
                                    !('@mozilla.org/widget/nativemenuservice;1' in Cc))
                                  this.setAttribute('openedwithkey',
                                                    event.target.parentNode.openedWithKey);">
            <menu id="file-menu" data-l10n-id="menu-file">
              <menupopup id="menu_FilePopup"
                         onpopupshowing="updateFileMenuUserContextUIVisibility('menu_newUserContext');
                                         updateFileMenuImportUIVisibility('cmd_importFromAnotherBrowser');">
                <menuitem id="menu_newNavigatorTab"
                          command="cmd_newNavigatorTab"
                          key="key_newNavigatorTab" data-l10n-id="menu-file-new-tab"/>
                <menu id="menu_newUserContext"
                      hidden="true" data-l10n-id="menu-file-new-container-tab">
                  <menupopup onpopupshowing="return createUserContextMenu(event);" />
                </menu>
                <menuitem id="menu_newNavigator"
                          key="key_newNavigator"
                          command="cmd_newNavigator" data-l10n-id="menu-file-new-window"/>
                <menuitem id="menu_newPrivateWindow"
                          command="Tools:PrivateBrowsing"
                          key="key_privatebrowsing" data-l10n-id="menu-file-new-private-window"/>
                <menuitem id="menu_openLocation"
                          hidden="true"
                          command="Browser:OpenLocation"
                          key="focusURLBar" data-l10n-id="menu-file-open-location"/>
                <menuitem id="menu_openFile"
                          command="Browser:OpenFile"
                          key="openFileKb" data-l10n-id="menu-file-open-file"/>
                <menuitem id="menu_close"
                          class="show-only-for-keyboard"
                          key="key_close"
                          command="cmd_close" data-l10n-id="menu-file-close"/>
                <menuitem id="menu_closeWindow"
                          class="show-only-for-keyboard"
                          hidden="true"
                          command="cmd_closeWindow"
                          key="key_closeWindow" data-l10n-id="menu-file-close-window"/>
                <menuseparator/>
                <menuitem id="menu_savePage"
                          key="key_savePage"
                          command="Browser:SavePage" data-l10n-id="menu-file-save-page"/>
                <menuitem id="menu_sendLink"
                          command="Browser:SendLink" data-l10n-id="menu-file-email-link"/>
                <menuseparator/>
#if !defined(MOZ_WIDGET_GTK)
                <menuitem id="menu_printSetup"
                          command="cmd_pageSetup" data-l10n-id="menu-file-print-setup"/>
#endif
#ifndef XP_MACOSX
                <menuitem id="menu_printPreview"
                          command="cmd_printPreview" data-l10n-id="menu-file-print-preview"/>
#endif
                <menuitem id="menu_print"
                          key="printKb"
                          command="cmd_print" data-l10n-id="menu-file-print"/>
                <menuseparator/>
                <menuitem id="menu_importFromAnotherBrowser"
                          command="cmd_importFromAnotherBrowser" data-l10n-id="menu-file-import-from-another-browser"/>
                <menuseparator/>
                <menuitem id="goOfflineMenuitem"
                          type="checkbox"
                          command="cmd_toggleOfflineStatus" data-l10n-id="menu-file-go-offline"/>
<!-- We need to leave those strings in DTD until bug 1568133 is fixed. -->
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

            <menu id="edit-menu" data-l10n-id="menu-edit">
              <menupopup id="menu_EditPopup"
                         onpopupshowing="updateEditUIVisibility()"
                         onpopuphidden="updateEditUIVisibility()">
                <menuitem id="menu_undo"
                          key="key_undo"
                          command="cmd_undo" data-l10n-id="menu-edit-undo"/>
                <menuitem id="menu_redo"
                          key="key_redo"
                          command="cmd_redo" data-l10n-id="menu-edit-redo"/>
                <menuseparator/>
                <menuitem id="menu_cut"
                          key="key_cut"
                          command="cmd_cut" data-l10n-id="menu-edit-cut"/>
                <menuitem id="menu_copy"
                          key="key_copy"
                          command="cmd_copy" data-l10n-id="menu-edit-copy"/>
                <menuitem id="menu_paste"
                          key="key_paste"
                          command="cmd_paste" data-l10n-id="menu-edit-paste"/>
                <menuitem id="menu_delete"
                          key="key_delete"
                          command="cmd_delete" data-l10n-id="menu-edit-delete"/>
                <menuseparator/>
                <menuitem id="menu_selectAll"
                          key="key_selectAll"
                          command="cmd_selectAll" data-l10n-id="menu-edit-select-all"/>
                <menuseparator/>
                <menuitem id="menu_find"
                          key="key_find"
                          command="cmd_find" data-l10n-id="menu-edit-find-on"/>
                <menuitem id="menu_findAgain"
                          class="show-only-for-keyboard"
                          key="key_findAgain"
                          command="cmd_findAgain" data-l10n-id="menu-edit-find-again"/>
                <menuseparator hidden="true" id="textfieldDirection-separator"/>
                <menuitem id="textfieldDirection-swap"
                          command="cmd_switchTextDirection"
                          key="key_switchTextDirection"
                          hidden="true" data-l10n-id="menu-edit-bidi-switch-text-direction"/>
#ifdef XP_UNIX
#ifndef XP_MACOSX
                <menuseparator/>
                <menuitem id="menu_preferences"
                          oncommand="openPreferences(undefined);" data-l10n-id="menu-preferences"/>
#endif
#endif
              </menupopup>
            </menu>

            <menu id="view-menu" data-l10n-id="menu-view">
              <menupopup id="menu_viewPopup"
                         onpopupshowing="updateCharacterEncodingMenuState();">
                <menu id="viewToolbarsMenu" data-l10n-id="menu-view-toolbars-menu">
                  <menupopup onpopupshowing="onViewToolbarsPopupShowing(event);">
                    <menuseparator/>
                    <menuitem id="menu_customizeToolbars"
                              command="cmd_CustomizeToolbars" data-l10n-id="menu-view-customize-toolbar"/>
                  </menupopup>
                </menu>
                <menu id="viewSidebarMenuMenu" data-l10n-id="menu-view-sidebar">
                  <menupopup id="viewSidebarMenu">
                    <menuitem id="menu_bookmarksSidebar"
                              type="checkbox"
                              key="viewBookmarksSidebarKb"
                              oncommand="SidebarUI.toggle('viewBookmarksSidebar');" data-l10n-id="menu-view-bookmarks"/>
                    <menuitem id="menu_historySidebar"
                              type="checkbox"
                              key="key_gotoHistory"
                              oncommand="SidebarUI.toggle('viewHistorySidebar');" data-l10n-id="menu-view-history-button"/>
                    <menuitem id="menu_tabsSidebar"
                              type="checkbox"
                              class="sync-ui-item"
                              oncommand="SidebarUI.toggle('viewTabsSidebar');" data-l10n-id="menu-view-synced-tabs-sidebar"/>
                  </menupopup>
                </menu>
                <menuseparator/>
                <menu id="viewFullZoomMenu"
                      onpopupshowing="FullZoom.updateMenu();" data-l10n-id="menu-view-full-zoom">
                  <menupopup>
                    <menuitem id="menu_zoomEnlarge"
                              key="key_fullZoomEnlarge"
                              command="cmd_fullZoomEnlarge" data-l10n-id="menu-view-full-zoom-enlarge"/>
                    <menuitem id="menu_zoomReduce"
                              key="key_fullZoomReduce"
                              command="cmd_fullZoomReduce" data-l10n-id="menu-view-full-zoom-reduce"/>
                    <menuseparator/>
                    <menuitem id="menu_zoomReset"
                              key="key_fullZoomReset"
                              command="cmd_fullZoomReset" data-l10n-id="menu-view-full-zoom-reset"/>
                    <menuseparator/>
                    <menuitem id="toggle_zoom"
                              type="checkbox"
                              command="cmd_fullZoomToggle"
                              checked="false" data-l10n-id="menu-view-full-zoom-toggle"/>
                  </menupopup>
                </menu>
                <menu id="pageStyleMenu" data-l10n-id="menu-view-page-style-menu">
                  <menupopup onpopupshowing="gPageStyleMenu.fillPopup(this);">
                    <menuitem id="menu_pageStyleNoStyle"
                              oncommand="gPageStyleMenu.disableStyle();"
                              type="radio" data-l10n-id="menu-view-page-style-no-style"/>
                    <menuitem id="menu_pageStylePersistentOnly"
                              oncommand="gPageStyleMenu.switchStyleSheet('');"
                              type="radio"
                              checked="true" data-l10n-id="menu-view-page-basic-style"/>
                    <menuseparator/>
                  </menupopup>
                </menu>
                <menu id="charsetMenu"
                    oncommand="BrowserSetForcedCharacterSet(event.target.getAttribute('charset'));"
                    onpopupshowing="CharsetMenu.build(event.target); UpdateCurrentCharset(this);" data-l10n-id="menu-view-charset">
                  <menupopup>
                  </menupopup>
                </menu>
                <menuseparator/>
#ifdef XP_MACOSX
                <menuitem id="enterFullScreenItem"
                          key="key_fullScreen" data-l10n-id="menu-view-enter-full-screen">
                  <observes element="View:FullScreen" attribute="oncommand"/>
                  <observes element="View:FullScreen" attribute="disabled"/>
                </menuitem>
                <menuitem id="exitFullScreenItem"
                          key="key_fullScreen"
                          hidden="true" data-l10n-id="menu-view-exit-full-screen">
                  <observes element="View:FullScreen" attribute="oncommand"/>
                  <observes element="View:FullScreen" attribute="disabled"/>
                </menuitem>
#else
                <menuitem id="fullScreenItem"
                          key="key_fullScreen"
                          type="checkbox"
                          observes="View:FullScreen" data-l10n-id="menu-view-full-screen"/>
#endif
                <menuitem id="menu_readerModeItem"
                          observes="View:ReaderView"
                          key="key_toggleReaderMode"
                          hidden="true"/>
                <menuitem id="menu_showAllTabs"
                          hidden="true"
                          command="Browser:ShowAllTabs"
                          key="key_showAllTabs" data-l10n-id="menu-view-show-all-tabs"/>
                <menuseparator hidden="true" id="documentDirection-separator"/>
                <menuitem id="documentDirection-swap"
                          hidden="true"
                          oncommand="gBrowser.selectedBrowser.sendMessageToActor('SwitchDocumentDirection', {}, 'SwitchDocumentDirection', true);" data-l10n-id="menu-view-bidi-switch-page-direction"/>
              </menupopup>
            </menu>

            <menu id="history-menu" data-l10n-id="menu-history">
              <menupopup id="goPopup"
#ifndef XP_MACOSX
                         placespopup="true"
                         is="places-popup"
#endif
                         oncommand="this.parentNode._placesView._onCommand(event);"
                         onclick="checkForMiddleClick(this, event);"
                         onpopupshowing="if (!this.parentNode._placesView)
                                           new HistoryMenu(event);"
                         tooltip="bhTooltip"
                         popupsinherittooltip="true">
                <menuitem id="menu_showAllHistory"
#ifndef XP_MACOSX
                          key="showAllHistoryKb"
#endif
                          command="Browser:ShowAllHistory" data-l10n-id="menu-history-show-all-history"/>
                <menuitem id="sanitizeItem"
                          key="key_sanitize"
                          command="Tools:Sanitize" data-l10n-id="menu-history-clear-recent-history"/>
                <menuseparator id="sanitizeSeparator"/>
                <menuitem id="sync-tabs-menuitem"
                          oncommand="gSync.openSyncedTabsPanel();"
                          hidden="true" data-l10n-id="menu-history-synced-tabs"/>
                <menuitem id="historyRestoreLastSession"
                          command="Browser:RestoreLastSession" data-l10n-id="menu-history-restore-last-session"/>
                <menuitem id="hiddenTabsMenu"
                          oncommand="gTabsPanel.showHiddenTabsPanel();"
                          hidden="true" data-l10n-id="menu-history-hidden-tabs"/>
                <menu id="historyUndoMenu"
                      disabled="true" data-l10n-id="menu-history-undo-menu">
                  <menupopup id="historyUndoPopup"
#ifndef XP_MACOSX
                             placespopup="true"
                             is="places-popup"
#endif
                             onpopupshowing="document.getElementById('history-menu')._placesView.populateUndoSubmenu();"/>
                </menu>
                <menu id="historyUndoWindowMenu"
                      disabled="true" data-l10n-id="menu-history-undo-window-menu">
                  <menupopup id="historyUndoWindowPopup"
#ifndef XP_MACOSX
                             placespopup="true"
                             is="places-popup"
#endif
                             onpopupshowing="document.getElementById('history-menu')._placesView.populateUndoWindowSubmenu();"/>
                </menu>
                <menuseparator id="startHistorySeparator"
                               class="hide-if-empty-places-result"/>
              </menupopup>
            </menu>

  <menu id="bookmarksMenu"
        ondragenter="PlacesMenuDNDHandler.onDragEnter(event);"
        ondragover="PlacesMenuDNDHandler.onDragOver(event);"
        ondrop="PlacesMenuDNDHandler.onDrop(event);" data-l10n-id="menu-bookmarks-menu">
    <menupopup id="bookmarksMenuPopup"
#ifndef XP_MACOSX
               placespopup="true"
               is="places-popup"
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
                command="Browser:ShowAllBookmarks"
                key="manBookmarkKb" data-l10n-id="menu-bookmarks-show-all"/>
      <menuseparator id="organizeBookmarksSeparator"/>
      <menuitem id="menu_bookmarkThisPage"
                command="Browser:AddBookmarkAs"
                key="addBookmarkAsKb"/>
      <menuitem id="menu_bookmarkAllTabs"
                class="show-only-for-keyboard"
                command="Browser:BookmarkAllTabs"
                key="bookmarkAllTabsKb" data-l10n-id="menu-bookmarks-all-tabs"/>
      <menuseparator id="bookmarksToolbarSeparator"/>
      <menu id="bookmarksToolbarFolderMenu"
            class="menu-iconic bookmark-item"
            container="true" data-l10n-id="menu-bookmarks-toolbar">
        <menupopup id="bookmarksToolbarFolderPopup"
#ifndef XP_MACOSX
                   placespopup="true"
                   is="places-popup"
#endif
                   context="placesContext"
                   onpopupshowing="if (!this.parentNode._placesView)
                                     new PlacesMenu(event, `place:parent=${PlacesUtils.bookmarks.toolbarGuid}`);"/>
      </menu>
      <menu id="menu_unsortedBookmarks"
            class="menu-iconic bookmark-item"
            container="true" data-l10n-id="menu-bookmarks-other">
        <menupopup id="otherBookmarksFolderPopup"
#ifndef XP_MACOSX
                   placespopup="true"
                   is="places-popup"
#endif
                   context="placesContext"
                   onpopupshowing="if (!this.parentNode._placesView)
                                     new PlacesMenu(event, `place:parent=${PlacesUtils.bookmarks.unfiledGuid}`);"/>
      </menu>
      <menu id="menu_mobileBookmarks"
            class="menu-iconic bookmark-item"
            hidden="true"
            container="true" data-l10n-id="menu-bookmarks-mobile">
        <menupopup id="mobileBookmarksFolderPopup"
#ifndef XP_MACOSX
                   placespopup="true"
                   is="places-popup"
#endif
                   context="placesContext"
                   onpopupshowing="if (!this.parentNode._placesView)
                                     new PlacesMenu(event, `place:parent=${PlacesUtils.bookmarks.mobileGuid}`);"/>
      </menu>
      <menuseparator id="bookmarksMenuItemsSeparator"/>
      <!-- Bookmarks menu items -->
    </menupopup>
  </menu>

            <menu id="tools-menu" data-l10n-id="menu-tools">
              <menupopup id="menu_ToolsPopup">
              <menuitem id="menu_openDownloads"
                        key="key_openDownloads"
                        command="Tools:Downloads" data-l10n-id="menu-tools-downloads"/>
              <menuitem id="menu_openAddons"
                        key="key_openAddons"
                        command="Tools:Addons" data-l10n-id="menu-tools-addons"/>

              <!-- only one of sync-setup, sync-enable, sync-unverifieditem, sync-syncnowitem or sync-reauthitem will be showing at once -->
              <menuitem id="sync-setup"
                        class="sync-ui-item"
                        hidden="true"
                        oncommand="gSync.openPrefs('menubar')" data-l10n-id="menu-tools-sync-sign-in"/>
              <menuitem id="sync-enable"
                        class="sync-ui-item"
                        hidden="true"
                        oncommand="gSync.openPrefs('menubar')" data-l10n-id="menu-tools-turn-on-sync"/>
              <menuitem id="sync-unverifieditem"
                        class="sync-ui-item"
                        hidden="true"
                        oncommand="gSync.openPrefs('menubar')" data-l10n-id="menu-tools-sync-sign-in"/>
              <menuitem id="sync-syncnowitem"
                        class="sync-ui-item"
                        hidden="true"
                        oncommand="gSync.doSync(event);" data-l10n-id="menu-tools-sync-now"/>
              <menuitem id="sync-reauthitem"
                        class="sync-ui-item"
                        hidden="true"
                        oncommand="gSync.openSignInAgainPage('menubar');" data-l10n-id="menu-tools-sync-re-auth"/>
              <menuseparator id="devToolsSeparator"/>
              <menu id="webDeveloperMenu" data-l10n-id="menu-tools-web-developer">
                <menupopup id="menuWebDeveloperPopup">
                  <menuitem id="menu_pageSource"
                            key="key_viewSource"
                            command="View:PageSource" data-l10n-id="menu-tools-page-source"/>
                </menupopup>
              </menu>
              <menuitem id="menu_pageInfo"
#ifndef XP_WIN
                        key="key_viewInfo"
#endif
                        command="View:PageInfo" data-l10n-id="menu-tools-page-info"/>
#ifndef XP_UNIX
              <menuseparator id="prefSep"/>
              <menuitem id="menu_preferences"
                        oncommand="openPreferences(undefined);" data-l10n-id="menu-preferences"/>
#endif
#ifdef MOZ_DEBUG
              <menuitem id="menu_layout_debugger"
                        data-l10n-id="menu-tools-layout-debugger"
                        oncommand="toOpenWindowByType('mozapp:layoutdebug',
                                          'chrome://layoutdebug/content/');"/>
#endif
#ifdef XP_MACOSX
<!-- nsMenuBarX hides these and uses them to build the Application menu. -->
<!-- We need to leave those strings in DTD until bug 1568133 is fixed. -->
              <menuitem id="menu_preferences" label="&preferencesCmdMac.label;" key="key_preferencesCmdMac" oncommand="openPreferences(undefined);"/>
              <menuitem id="menu_mac_services" label="&servicesMenuMac.label;"/>
              <menuitem id="menu_mac_hide_app" label="&hideThisAppCmdMac2.label;" key="key_hideThisAppCmdMac"/>
              <menuitem id="menu_mac_hide_others" label="&hideOtherAppsCmdMac.label;" key="key_hideOtherAppsCmdMac"/>
              <menuitem id="menu_mac_show_all" label="&showAllAppsCmdMac.label;"/>
              <menuitem id="menu_mac_touch_bar" label="&touchBarCmdMac.label;"/>
#endif
              </menupopup>
            </menu>
#ifdef XP_MACOSX
            <menu id="windowMenu"
                  onpopupshowing="macWindowMenuDidShow();"
                  onpopuphidden="macWindowMenuDidHide();"
                   data-l10n-id="menu-window-menu">
                <menupopup id="windowPopup">
                    <menuitem command="minimizeWindow" key="key_minimizeWindow"/>
                    <menuitem command="zoomWindow"/>
                    <!-- decomment when "BringAllToFront" is implemented
                        <menuseparator/>
                        <menuitem disabled="true" data-l10n-id="menu-window-bring-all-to-front"/> -->
                    <menuseparator id="sep-window-list"/>
                </menupopup>
            </menu>
#endif
            <menu id="helpMenu"
#ifdef XP_WIN
#else
#endif
                   data-l10n-id="menu-help">
              <menupopup id="menu_HelpPopup" onpopupshowing="buildHelpMenu();">
                <menuitem id="menu_openHelp"
                          oncommand="openHelpLink('firefox-help')"
                          onclick="checkForMiddleClick(this, event);"
                          data-l10n-id="menu-help-product"
#ifdef XP_MACOSX
                          key="key_openHelpMac"/>
#else
                          />
#endif
                <menuitem id="menu_openTour"
                          oncommand="openTourPage();" data-l10n-id="menu-help-show-tour"/>
                <menuitem id="menu_keyboardShortcuts"
                          oncommand="openHelpLink('keyboard-shortcuts')"
                          onclick="checkForMiddleClick(this, event);" data-l10n-id="menu-help-keyboard-shortcuts"/>
                <menuitem id="troubleShooting"
                          oncommand="openTroubleshootingPage()"
                          onclick="checkForMiddleClick(this, event);" data-l10n-id="menu-help-troubleshooting-info"/>
                <menuitem id="feedbackPage"
                          oncommand="openFeedbackPage()"
                          onclick="checkForMiddleClick(this, event);" data-l10n-id="menu-help-feedback-page"/>
                <menuitem id="helpSafeMode"
                          oncommand="safeModeRestart();" data-l10n-id="menu-help-safe-mode-without-addons"/>
                <menuitem id="menu_HelpPopup_reportPhishingtoolmenu"
                          disabled="true"
                          oncommand="openUILink(gSafeBrowsing.getReportURL('Phish'), event, {triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({})});"
                          onclick="checkForMiddleClick(this, event);"
                          hidden="true" data-l10n-id="menu-help-report-deceptive-site"/>
                <menuitem id="menu_HelpPopup_reportPhishingErrortoolmenu"
                          disabled="true"
                          oncommand="ReportFalseDeceptiveSite();"
                          onclick="checkForMiddleClick(this, event);"
                          hidden="true"/>
                <menuseparator id="helpPolicySeparator"
                          hidden="true"/>
                <menuitem id="helpPolicySupport"
                          hidden="true"
                          oncommand="openUILinkIn(Services.policies.getSupportMenu().URL.href, 'tab', {triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({})});"
                          onclick="checkForMiddleClick(this, event);"/>
                <menuseparator id="aboutSeparator"/>
                <!-- We need to leave those strings in DTD until bug 1568133 is fixed. -->
                <menuitem id="aboutName"
                          accesskey="&aboutProduct2.accesskey;"
                          label="&aboutProduct2.label;"
                          oncommand="openAboutDialog();"/>
              </menupopup>
          </menu>
        </menubar>
