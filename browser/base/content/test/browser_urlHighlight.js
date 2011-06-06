function testVal(aExpected) {
  gURLBar.value = aExpected.replace(/[<>]/g, "");

  let selectionController = gURLBar.editor.selectionController;
  let selection = selectionController.getSelection(selectionController.SELECTION_URLSECONDARY);
  let value = gURLBar.editor.rootElement.textContent;
  let result = "";
  for (let i = 0; i < selection.rangeCount; i++) {
    let range = selection.getRangeAt(i).toString();
    let pos = value.indexOf(range);
    result += value.substring(0, pos) + "<" + range + ">";
    value = value.substring(pos + range.length);
  }
  result += value;
  is(result, aExpected);
}

function test() {
  const prefname = "browser.urlbar.formatting.enabled";

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref(prefname);
    URLBarSetURI();
  });

  Services.prefs.setBoolPref(prefname, true);

  gURLBar.focus();

  testVal("http://mozilla.org/");

  gBrowser.selectedBrowser.focus();

  testVal("<http://>mozilla.org");
  testVal("<http://>mozilla.org</>");
  testVal("<http://>mözilla.org</>");
  testVal("<http://>mozilla.imaginatory</>");
  testVal("<http://www.>mozilla.org</>");
  testVal("<http://sub.>mozilla.org</>");
  testVal("<http://sub1.sub2.sub3.>mozilla.org</>");
  testVal("<http://ftp.>mozilla.org</>");
  testVal("<ftp://ftp.>mozilla.org</>");

  testVal("<https://sub.>mozilla.org</>");
  testVal("<https://sub1.sub2.sub3.>mozilla.org</>");
  testVal("<https://user:pass@sub1.sub2.sub3.>mozilla.org</>");
  testVal("<https://user:pass@>mozilla.org</>");

  testVal("<http://>mozilla.org</file.ext>");
  testVal("<http://>mozilla.org</sub/file.ext>");
  testVal("<http://>mozilla.org</sub/file.ext?foo>");
  testVal("<http://>mozilla.org</sub/file.ext?foo&bar>");
  testVal("<http://>mozilla.org</sub/file.ext?foo&bar#top>");

  testVal("<http://sub.>mozilla.org<:666/file.ext>");

  testVal("<http://>[fe80::222:19ff:fe11:8c76]</file.ext>");
  testVal("<http://user:pass@>[fe80::222:19ff:fe11:8c76]<:666/file.ext>");

  testVal("mailto:admin@mozilla.org");
  testVal("gopher://mozilla.org/");
  testVal("about:config");

  Services.prefs.setBoolPref(prefname, false);

  testVal("http://mozilla.org/");
}
