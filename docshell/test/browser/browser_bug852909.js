var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab(rootDir + "file_bug852909.png");
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(image);
}

function image(event) {
  ok(!gBrowser.selectedTab.mayEnableCharacterEncodingMenu, "Docshell should say the menu should be disabled for images.");

  gBrowser.removeCurrentTab();
  gBrowser.selectedTab = gBrowser.addTab(rootDir + "file_bug852909.pdf");
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(pdf);
}

function pdf(event) {
  ok(!gBrowser.selectedTab.mayEnableCharacterEncodingMenu, "Docshell should say the menu should be disabled for PDF.js.");

  gBrowser.removeCurrentTab();
  finish();
}
