function test() {
  waitForExplicitFinish();

  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  gBrowser.selectedTab = gBrowser.addTab(rootDir + "file_bug234628-2.html");
  gBrowser.selectedBrowser.addEventListener("load", afterOpen, true);
}

function afterOpen(event) {
  if (event.target != gBrowser.contentDocument) {
    return;
  }

  gBrowser.selectedBrowser.removeEventListener("load", afterOpen, true);
  gBrowser.selectedBrowser.addEventListener("load", afterChangeCharset, true);

  is(gBrowser.contentDocument.documentElement.textContent.indexOf('\u20AC'), 129, "Parent doc should be windows-1252 initially");

  is(gBrowser.contentDocument.getElementsByTagName("iframe")[0].contentDocument.documentElement.textContent.indexOf('\u00E2\u201A\u00AC'), 78, "Child doc should be windows-1252 initially");  

  BrowserSetForcedCharacterSet("windows-1251");
}
  
function afterChangeCharset(event) {
  if (event.target != gBrowser.contentDocument) {
    return;
  }

  gBrowser.selectedBrowser.removeEventListener("load", afterChangeCharset, true);

  is(gBrowser.contentDocument.documentElement.textContent.indexOf('\u0402'), 129, "Parent doc should decode as windows-1251 subsequently");
  is(gBrowser.contentDocument.getElementsByTagName("iframe")[0].contentDocument.documentElement.textContent.indexOf('\u0432\u201A\u00AC'), 78, "Child doc should decode as windows-1251 subsequently");  

  is(gBrowser.contentDocument.characterSet, "windows-1251", "Parent doc should report windows-1251 subsequently");
  is(gBrowser.contentDocument.getElementsByTagName("iframe")[0].contentDocument.characterSet, "windows-1251", "Child doc should report windows-1251 subsequently");

  gBrowser.removeCurrentTab();
  finish();
}
