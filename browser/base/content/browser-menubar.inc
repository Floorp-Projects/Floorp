# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

       <menubar id="main-menubar"
# On macOS, we don't track whether activation of the native menubar happened
# with the keyboard.
#ifndef XP_MACOSX
                onpopupshowing="if (event.target.parentNode.parentNode == this)
                                  this.setAttribute('openedwithkey',
                                                    event.target.parentNode.openedWithKey);"
#endif
                >
            <menu id="file-menu" data-l10n-id="menu-file">
              <menupopup id="menu_FilePopup"
                         onpopupshowing="updateFileMenuUserContextUIVisibility('menu_newUserContext');
                                         updateImportCommandEnabledState();
                                         PrintUtils.updatePrintPreviewMenuHiddenState();">
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
#ifdef NIGHTLY_BUILD
                <menuitem id="menu_newFissionWindow"
                          command="Tools:FissionWindow"
                          accesskey="s" label="New Fission Window"/>
                <menuitem id="menu_newNonFissionWindow"
                          command="Tools:NonFissionWindow"
                          accesskey="s" label="New Non-Fission Window"/>
#endif
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
                          command="cmd_pageSetup" data-l10n-id="menu-file-print-setup" hidden="true"/>
#endif
#ifndef XP_MACOSX
                <menuitem id="menu_printPreview"
                          command="cmd_printPreview" data-l10n-id="menu-file-print-preview" hidden="true"/>
#endif
                <menuitem id="menu_print"
                          key="printKb"
                          command="cmd_print" data-l10n-id="menu-file-print"/>
                <menuseparator/>
                <menuitem id="menu_importFromAnotherBrowser"
                          command="cmd_file_importFromAnotherBrowser" data-l10n-id="menu-file-import-from-another-browser"/>
                <menuseparator/>
                <menuitem id="goOfflineMenuitem"
                          type="checkbox"
                          command="cmd_toggleOfflineStatus" data-l10n-id="menu-file-go-offline"/>
                <menuitem id="menu_FileQuitItem"
#ifdef XP_MACOSX
                          data-l10n-id="menu-quit-mac"
#else
                          data-l10n-id="menu-quit"
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
                          command="cmd_undo" data-l10n-id="text-action-undo"/>
                <menuitem id="menu_redo"
                          key="key_redo"
                          command="cmd_redo" data-l10n-id="text-action-redo"/>
                <menuseparator/>
                <menuitem id="menu_cut"
                          key="key_cut"
                          command="cmd_cut" data-l10n-id="text-action-cut"/>
                <menuitem id="menu_copy"
                          key="key_copy"
                          command="cmd_copy" data-l10n-id="text-action-copy"/>
                <menuitem id="menu_paste"
                          key="key_paste"
                          command="cmd_paste" data-l10n-id="text-action-paste"/>
                <menuitem id="menu_delete"
                          key="key_delete"
                          command="cmd_delete" data-l10n-id="text-action-delete"/>
                <menuseparator/>
                <menuitem id="menu_selectAll"
                          key="key_selectAll"
                          command="cmd_selectAll" data-l10n-id="text-action-select-all"/>
                <menuseparator/>
                <menuitem id="menu_find"
                          key="key_find"
                          command="cmd_find" data-l10n-id="menu-edit-find-in-page"/>
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
                          oncommand="openPreferences(undefined);"
#ifdef NIGHTLY_BUILD
                          data-l10n-id="menu-settings"
#else
                          data-l10n-id="menu-preferences"
#endif
                          />
#endif
#endif
              </menupopup>
            </menu>

            <menu id="view-menu" data-l10n-id="menu-view">
              <menupopup id="menu_viewPopup"
                         onpopupshowing="updateCharacterEncodingMenuState();">
                <menu id="viewToolbarsMenu" data-l10n-id="menu-view-toolbars-menu">
                  <menupopup id="view-menu-popup" onpopupshowing="onViewToolbarsPopupShowing(event);">
                    <menuseparator/>
                    <menuitem id="menu_customizeToolbars"
                              command="cmd_CustomizeToolbars" data-l10n-id="menu-view-customize-toolbar2"/>
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
                              command="cmd_fullZoomReset" data-l10n-id="menu-view-full-zoom-actual-size"/>
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
                              oncommand="gPageStyleMenu.switchStyleSheet(null);"
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
                </menuitem>
                <menuitem id="exitFullScreenItem"
                          key="key_fullScreen"
                          hidden="true" data-l10n-id="menu-view-exit-full-screen">
                  <observes element="View:FullScreen" attribute="oncommand"/>
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
                          oncommand="gBrowser.selectedBrowser.sendMessageToActor('SwitchDocumentDirection', {}, 'SwitchDocumentDirection', 'roots');" data-l10n-id="menu-view-bidi-switch-page-direction"/>
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
                          key="showAllHistoryKb"
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
                          oncommand="gTabsPanel.showHiddenTabsPanel(event);"
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
                             onpopupshowing="document.getElementById('history-menu')._placesView.populateUndoWindowSubmenu();">
#ifdef HIDDEN_WINDOW
# This entry is never visible. It's here to make the cmd-shift-n
# shortcut work in the hidden window when the last window is closed.
# If the menu is actually opened, we'll clear this out and replace
# it with a "real" entry.
# See bug 492320 for the nasty details.
                    <menuitem key="key_undoCloseWindow"
                              oncommand="undoCloseWindow(0)"
                              />
#endif
                  </menupopup>
                </menu>
                <menuseparator id="startHistorySeparator"
                               class="hide-if-empty-places-result"/>
              </menupopup>
            </menu>

  <menu id="bookmarksMenu"
        ondragenter="PlacesMenuDNDHandler.onDragEnter(event);"
        ondragover="PlacesMenuDNDHandler.onDragOver(event);"
        ondrop="PlacesMenuDNDHandler.onDrop(event);"
        data-l10n-id="menu-bookmarks-menu">
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
                key="manBookmarkKb"
                data-l10n-id="menu-bookmarks-show-all"/>
      <menuseparator id="organizeBookmarksSeparator"/>
      <menuitem id="menu_bookmarkThisPage"
                command="Browser:AddBookmarkAs"
                key="addBookmarkAsKb"
                data-l10n-id="menu-bookmark-current-tab"/>
      <menuitem id="menu_bookmarkAllTabs"
                class="show-only-for-keyboard"
                command="Browser:BookmarkAllTabs"
                key="bookmarkAllTabsKb"
                data-l10n-id="menu-bookmarks-all-tabs"/>
      <menuseparator id="bookmarksToolbarSeparator"/>
      <menu id="bookmarksToolbarFolderMenu"
            class="menu-iconic bookmark-item"
            container="true"
            data-l10n-id="menu-bookmarks-toolbar">
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
            container="true"
            data-l10n-id="menu-bookmarks-other">
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
            container="true"
            data-l10n-id="menu-bookmarks-mobile">
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
                        command="Tools:Addons" data-l10n-id="menu-tools-extensions"/>

              <!-- only one of sync-setup, sync-enable, sync-unverifieditem, sync-syncnowitem or sync-reauthitem will be showing at once -->
              <menuitem id="sync-setup"
                        class="sync-ui-item"
                        hidden="true"
                        oncommand="gSync.openPrefs('menubar')" data-l10n-id="menu-tools-fxa-sign-in2"/>
              <menuitem id="sync-enable"
                        class="sync-ui-item"
                        hidden="true"
                        oncommand="gSync.openPrefs('menubar')" data-l10n-id="menu-tools-turn-on-sync"/>
              <menuitem id="sync-unverifieditem"
                        class="sync-ui-item"
                        hidden="true"
                        oncommand="gSync.openPrefs('menubar')" data-l10n-id="menu-tools-fxa-sign-in2"/>
              <menuitem id="sync-syncnowitem"
                        class="sync-ui-item"
                        hidden="true"
                        oncommand="gSync.doSync(event);" data-l10n-id="menu-tools-sync-now"/>
              <menuitem id="sync-reauthitem"
                        class="sync-ui-item"
                        hidden="true"
                        oncommand="gSync.openSignInAgainPage('menubar');" data-l10n-id="menu-tools-fxa-re-auth"/>
              <menuseparator id="devToolsSeparator"/>
              <menu id="webDeveloperMenu" data-l10n-id="menu-tools-web-developer">
                <menupopup id="menuWebDeveloperPopup">
                  <menuitem id="menu_pageSource"
                            key="key_viewSource"
                            command="View:PageSource" data-l10n-id="menu-tools-page-source"/>
                </menupopup>
              </menu>
              <menuitem id="menu_pageInfo"
                        key="key_viewInfo"
                        command="View:PageInfo" data-l10n-id="menu-tools-page-info"/>
#ifndef XP_UNIX
              <menuseparator id="prefSep"/>
              <menuitem id="menu_preferences"
#ifdef NIGHTLY_BUILD
                        data-l10n-id="menu-settings"
#else
                        data-l10n-id="menu-preferences"
#endif
                        oncommand="openPreferences(undefined);"/>
#endif
#ifdef MOZ_DEBUG
              <menuitem id="menu_layout_debugger"
                        data-l10n-id="menu-tools-layout-debugger"
                        oncommand="toOpenWindowByType('mozapp:layoutdebug',
                                          'chrome://layoutdebug/content/layoutdebug.xhtml');"/>
#endif
#ifdef XP_MACOSX
<!-- nsMenuBarX hides these and uses them to build the Application menu. -->
              <menuitem id="menu_preferences"
#ifdef NIGHTLY_BUILD
                        data-l10n-id="menu-settings"
#else
                        data-l10n-id="menu-preferences"
#endif
                        key="key_preferencesCmdMac" oncommand="openPreferences(undefined);"/>
              <menuitem id="menu_mac_services" data-l10n-id="menu-application-services"/>
              <menuitem id="menu_mac_hide_app" data-l10n-id="menu-application-hide-this" key="key_hideThisAppCmdMac"/>
              <menuitem id="menu_mac_hide_others" data-l10n-id="menu-application-hide-other" key="key_hideOtherAppsCmdMac"/>
              <menuitem id="menu_mac_show_all" data-l10n-id="menu-application-show-all"/>
              <menuitem id="menu_mac_touch_bar" data-l10n-id="menu-application-touch-bar"/>
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
            <menu id="helpMenu" data-l10n-id="menu-help">
              <menupopup id="menu_HelpPopup" onpopupshowing="buildHelpMenu();">
<!-- Note: Items under here are cloned to the AppMenu Help submenu. The cloned items
     have their strings defined by appmenu-data-l10n-id. -->
                <menuitem id="menu_openHelp"
                          oncommand="openHelpLink('firefox-help')"
                          onclick="checkForMiddleClick(this, event);"
                          data-l10n-id="menu-get-help"
                          appmenu-data-l10n-id="appmenu-get-help"
#ifdef XP_MACOSX
                          key="key_openHelpMac"/>
#else
                          />
#endif
                <menuitem id="feedbackPage"
                          oncommand="openFeedbackPage()"
                          onclick="checkForMiddleClick(this, event);"
                          data-l10n-id="menu-help-feedback-page"
                          appmenu-data-l10n-id="appmenu-help-feedback-page"/>
                <menuitem id="helpSafeMode"
                          oncommand="safeModeRestart();"
                          data-l10n-id="menu-help-enter-troubleshoot-mode"
                          appmenu-data-l10n-id="appmenu-help-enter-troubleshoot-mode"/>
                <menuitem id="troubleShooting"
                          oncommand="openTroubleshootingPage()"
                          onclick="checkForMiddleClick(this, event);"
                          data-l10n-id="menu-help-more-troubleshooting-info"
                          appmenu-data-l10n-id="appmenu-help-more-troubleshooting-info"/>
                <menuitem id="help_taskManager"
                          oncommand="switchToTabHavingURI('about:performance', true)"
                          onclick="checkForMiddleClick(this, event);"
                          data-l10n-id="menu-help-taskmanager"
                          appmenu-data-l10n-id="appmenu-help-taskmanager"/>
                <menuitem id="help_reportSiteIssue"
                          oncommand="ReportSiteIssue();"
                          data-l10n-id="menu-help-report-site-issue"
                          appmenu-data-l10n-id="appmenu-help-report-site-issue"
                          hidden="true"/>
                <menuitem id="menu_HelpPopup_reportPhishingtoolmenu"
                          disabled="true"
                          oncommand="openUILink(gSafeBrowsing.getReportURL('Phish'), event, {triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({})});"
                          onclick="checkForMiddleClick(this, event);"
                          hidden="true"
                          data-l10n-id="menu-help-report-deceptive-site"
                          appmenu-data-l10n-id="appmenu-help-report-deceptive-site"/>
                <menuitem id="menu_HelpPopup_reportPhishingErrortoolmenu"
                          disabled="true"
                          oncommand="ReportFalseDeceptiveSite();"
                          onclick="checkForMiddleClick(this, event);"
                          data-l10n-id="menu-help-not-deceptive"
                          appmenu-data-l10n-id="appmenu-help-not-deceptive"
                          hidden="true"/>
                <menuseparator id="aboutSeparator"/>
                <menuitem id="aboutName"
                          oncommand="openAboutDialog();"
                          data-l10n-id="menu-about"
                          appmenu-data-l10n-id="appmenu-about"/>
                <menuseparator id="helpPolicySeparator"
                          hidden="true"/>
                <menuitem id="helpPolicySupport"
                          hidden="true"
                          oncommand="openUILinkIn(Services.policies.getSupportMenu().URL.href, 'tab', {triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({})});"
                          onclick="checkForMiddleClick(this, event);"/>
              </menupopup>
          </menu>
        </menubar>
