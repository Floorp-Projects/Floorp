function test() {
  waitForExplicitFinish();

  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  gBrowser.selectedTab = gBrowser.addTab(rootDir + "file_bug234628-9.html");
  gBrowser.selectedBrowser.addEventListener("load", afterOpen, true);
}

function afterOpen(event) {
  if (event.target != gBrowser.contentDocument) {
    return;
  }

  gBrowser.selectedBrowser.removeEventListener("load", afterOpen, true);

  is(gBrowser.contentDocument.documentElement.textContent.indexOf('\u20AC'), 145, "Parent doc should be UTF-16");

  is(gBrowser.contentDocument.getElementsByTagName("iframe")[0].contentDocument.documentElement.textContent.indexOf('\u20AC'), 96, "Child doc should be windows-1252");

  gBrowser.removeCurrentTab();
  finish();
}

