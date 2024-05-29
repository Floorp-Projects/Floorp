/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const COMMANDS = {
  // <commandset id="mainCommandSet"> defined in browser-sets.inc
  cmd_newNavigator() {
    OpenBrowserWindow();
  },
  cmd_handleBackspace() {
    BrowserCommands.handleBackspace();
  },
  cmd_handleShiftBackspace() {
    BrowserCommands.handleShiftBackspace();
  },
  cmd_newNavigatorTab(event) {
    BrowserCommands.openTab({ event });
  },
  cmd_newNavigatorTabNoEvent() {
    BrowserCommands.openTab();
  },
  "Browser:OpenFile": function () {
    BrowserCommands.openFileWindow();
  },
  "Browser:SavePage": function () {
    saveBrowser(gBrowser.selectedBrowser);
  },
  "Browser:SendLink": function () {
    MailIntegration.sendLinkForBrowser(gBrowser.selectedBrowser);
  },
  cmd_pageSetup() {
    PrintUtils.showPageSetup();
  },
  cmd_print() {
    PrintUtils.startPrintWindow(gBrowser.selectedBrowser.browsingContext);
  },
  cmd_printPreviewToggle() {
    PrintUtils.togglePrintPreview(gBrowser.selectedBrowser.browsingContext);
  },
  cmd_file_importFromAnotherBrowser() {
    MigrationUtils.showMigrationWizard(window, {
      entrypoint: MigrationUtils.MIGRATION_ENTRYPOINTS.FILE_MENU,
    });
  },
  cmd_help_importFromAnotherBrowser() {
    MigrationUtils.showMigrationWizard(window, {
      entrypoint: MigrationUtils.MIGRATION_ENTRYPOINTS.HELP_MENU,
    });
  },
  cmd_close(event) {
    BrowserCommands.closeTabOrWindow(event);
  },
  cmd_closeWindow(event) {
    BrowserCommands.tryToCloseWindow(event);
  },
  cmd_toggleMute() {
    gBrowser.toggleMuteAudioOnMultiSelectedTabs(gBrowser.selectedTab);
  },
  // TODO: Used with <observes>
  // cmd_CustomizeToolbars() {
  //   gCustomizeMode.enter();
  // },
  cmd_toggleOfflineStatus() {
    BrowserOffline.toggleOfflineStatus();
  },
  cmd_quitApplication(event) {
    goQuitApplication(event);
  },
  "View:AboutProcesses": function () {
    switchToTabHavingURI("about:processes", true);
  },
  "View:PageSource": function () {
    BrowserCommands.viewSource(window.gBrowser.selectedBrowser);
  },
  "View:PageInfo": function () {
    BrowserCommands.pageInfo();
  },
  // TODO: Used with <observes>
  // "View:FullScreen": function () {
  //   BrowserCommands.fullScreen();
  // },
  // "View:ReaderView": function (event) {
  //   AboutReaderParent.toggleReaderMode(event);
  // },
  "View:PictureInPicture": function (event) {
    PictureInPicture.onCommand(event);
  },
  cmd_find() {
    gLazyFindCommand("onFindCommand");
  },
  cmd_findAgain() {
    gLazyFindCommand("onFindAgainCommand", false);
  },
  cmd_findPrevious() {
    gLazyFindCommand("onFindAgainCommand", true);
  },
  cmd_findSelection() {
    gLazyFindCommand("onFindSelectionCommand");
  },
  // TODO: <observes>
  // cmd_reportBrokenSite(event) {
  //   ReportBrokenSite.open(event);
  // },
  cmd_translate(event) {
    FullPageTranslationsPanel.open(event);
  },
  "Browser:AddBookmarkAs": function () {
    PlacesCommandHook.bookmarkPage();
  },
  "Browser:SearchBookmarks": function () {
    PlacesCommandHook.searchBookmarks();
  },
  "Browser:BookmarkAllTabs": function () {
    PlacesCommandHook.bookmarkTabs();
  },
  "Browser:Back": function () {
    BrowserCommands.back();
  },
  "Browser:BackOrBackDuplicate": function (event) {
    BrowserCommands.back(event);
  },
  "Browser:Forward": function () {
    BrowserCommands.forward();
  },
  "Browser:ForwardOrForwardDuplicate": function (event) {
    BrowserCommands.forward(event);
  },
  "Browser:Stop": function () {
    BrowserCommands.stop();
  },
  "Browser:Reload": function (event) {
    if (event.shiftKey) {
      BrowserCommands.reloadSkipCache();
    } else {
      BrowserCommands.reload();
    }
  },
  "Browser:ReloadOrDuplicate": function (event) {
    BrowserCommands.reloadOrDuplicate(event);
  },
  "Browser:ReloadSkipCache": function () {
    BrowserCommands.reloadSkipCache();
  },
  "Browser:NextTab": function () {
    gBrowser.tabContainer.advanceSelectedTab(1, true);
  },
  "Browser:PrevTab": function () {
    gBrowser.tabContainer.advanceSelectedTab(-1, true);
  },
  "Browser:ShowAllTabs": function () {
    gTabsPanel.showAllTabsPanel();
  },
  cmd_fullZoomReduce() {
    FullZoom.reduce();
  },
  cmd_fullZoomEnlarge() {
    FullZoom.enlarge();
  },
  cmd_fullZoomReset() {
    FullZoom.reset();
    FullZoom.resetScalingZoom();
  },
  cmd_fullZoomToggle() {
    ZoomManager.toggleZoom();
  },
  cmd_gestureRotateLeft(event) {
    gGestureSupport.rotate(event.sourceEvent);
  },
  cmd_gestureRotateRight(event) {
    gGestureSupport.rotate(event.sourceEvent);
  },
  cmd_gestureRotateEnd() {
    gGestureSupport.rotateEnd();
  },
  "Browser:OpenLocation": function (event) {
    openLocation(event);
  },
  "Browser:RestoreLastSession": function () {
    SessionStore.restoreLastSession();
  },
  "Browser:NewUserContextTab": function (event) {
    openNewUserContextTab(event.sourceEvent);
  },
  "Browser:OpenAboutContainers": function () {
    openPreferences("paneContainers");
  },
  "Tools:Search": function () {
    BrowserSearch.webSearch();
  },
  "Tools:Downloads": function () {
    BrowserCommands.downloadsUI();
  },
  "Tools:Addons": function () {
    BrowserAddonUI.openAddonsMgr();
  },
  "Tools:Sanitize": function () {
    Sanitizer.showUI(window);
  },
  "Tools:PrivateBrowsing": function () {
    OpenBrowserWindow({ private: true });
  },
  "Browser:Screenshot": function () {
    ScreenshotsUtils.notify(window, "shortcut");
  },
  // TODO: <observes>
  // "History:UndoCloseTab": function () {
  //   undoCloseTab();
  // },
  "History:UndoCloseWindow": function () {
    undoCloseWindow();
  },
  "History:RestoreLastClosedTabOrWindowOrSession": function () {
    restoreLastClosedTabOrWindowOrSession();
  },
  "History:SearchHistory": function () {
    PlacesCommandHook.searchHistory();
  },
  wrCaptureCmd() {
    gGfxUtils.webrenderCapture();
  },
  wrToggleCaptureSequenceCmd() {
    gGfxUtils.toggleWebrenderCaptureSequence();
  },
  windowRecordingCmd() {
    gGfxUtils.toggleWindowRecording();
  },
  minimizeWindow() {
    window.minimize();
  },
  zoomWindow() {
    zoomWindow();
  },
};

document.addEventListener(
  "MozBeforeInitialXULLayout",
  () => {
    for (let [command, listener] of Object.entries(COMMANDS)) {
      document.getElementById(command)?.addEventListener("command", listener);
    }
  },
  { once: true }
);
