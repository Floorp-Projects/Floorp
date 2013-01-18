function test() {
  waitForExplicitFinish();

  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  gBrowser.selectedTab = gBrowser.addTab(rootDir + "file_bug234628-6.html");
  gBrowser.selectedBrowser.addEventListener("load", afterOpen, true);
}

function afterOpen(event) {
  if (event.target != gBrowser.contentDocument) {
    return;
  }

  gBrowser.selectedBrowser.removeEventListener("load", afterOpen, true);
  gBrowser.selectedBrowser.addEventListener("load", afterChangeCharset, true);

  is(gBrowser.contentDocument.documentElement.textContent.indexOf('\u20AC'), 190, "Parent doc should be windows-1252 initially");

  is(gBrowser.contentDocument.getElementsByTagName("iframe")[0].contentDocument.documentElement.textContent.indexOf('\u20AC'), 109, "Child doc should be utf-16 initially");

  BrowserSetForcedCharacterSet("windows-1251");
}
  
function afterChangeCharset(event) {
  if (event.target != gBrowser.contentDocument) {
    return;
  }

  gBrowser.selectedBrowser.removeEventListener("load", afterChangeCharset, true);

  is(gBrowser.contentDocument.documentElement.textContent.indexOf('\u0402'), 190, "Parent doc should decode as windows-1251 subsequently");
  is(gBrowser.contentDocument.getElementsByTagName("iframe")[0].contentDocument.documentElement.textContent.indexOf('\u20AC'), 109, "Child doc should decode as utf-16 subsequently");  

  is(gBrowser.contentDocument.characterSet, "windows-1251", "Parent doc should report windows-1251 subsequently");
  is(gBrowser.contentDocument.getElementsByTagName("iframe")[0].contentDocument.characterSet, "UTF-16BE", "Child doc should report UTF-16 subsequently");

  gBrowser.removeCurrentTab();
  finish();
}
