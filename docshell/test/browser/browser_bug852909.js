var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab(rootDir + "file_bug852909.png");
  gBrowser.selectedBrowser.addEventListener("load", image, true);
}

function image(event) {
  if (event.target != gBrowser.contentDocument) {
    return;
  }
  gBrowser.selectedBrowser.removeEventListener("load", image, true);

  ok(!gBrowser.docShell.mayEnableCharacterEncodingMenu, "Docshell should say the menu should be disabled for images.");

  gBrowser.removeCurrentTab();
  gBrowser.selectedTab = gBrowser.addTab(rootDir + "file_bug852909.pdf");
  gBrowser.selectedBrowser.addEventListener("load", pdf, true);
}

function pdf(event) {
  if (event.target != gBrowser.contentDocument) {
    return;
  }
  gBrowser.selectedBrowser.removeEventListener("load", pdf, true);

  ok(!gBrowser.docShell.mayEnableCharacterEncodingMenu, "Docshell should say the menu should be disabled for PDF.js.");

  gBrowser.removeCurrentTab();
  finish();
}
