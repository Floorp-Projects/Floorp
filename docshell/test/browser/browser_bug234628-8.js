function test() {
  waitForExplicitFinish();

  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  gBrowser.selectedTab = gBrowser.addTab(rootDir + "file_bug234628-8.html");
  gBrowser.selectedBrowser.addEventListener("load", afterOpen, true);
}

function afterOpen(event) {
  if (event.target != gBrowser.contentDocument) {
    return;
  }

  gBrowser.selectedBrowser.removeEventListener("load", afterOpen, true);

  is(gBrowser.contentDocument.documentElement.textContent.indexOf('\u0402'), 156, "Parent doc should be windows-1251");

  is(gBrowser.contentDocument.getElementsByTagName("iframe")[0].contentDocument.documentElement.textContent.indexOf('\u0402'), 99, "Child doc should be windows-1251");

  gBrowser.removeCurrentTab();
  finish();
}

