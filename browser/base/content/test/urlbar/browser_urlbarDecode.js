"use strict";

// This test makes sure (1) you can't break the urlbar by typing particular JSON
// or JS fragments into it, (2) urlbar.textValue shows URLs unescaped, and (3)
// the urlbar also shows the URLs embedded in action URIs unescaped.  See bug
// 1233672.

add_task(function* injectJSON() {
  let inputStrs = [
    'http://example.com/ ", "url": "bar',
    'http://example.com/\\',
    'http://example.com/"',
    'http://example.com/","url":"evil.com',
    'http://mozilla.org/\\u0020',
    'http://www.mozilla.org/","url":1e6,"some-key":"foo',
    'http://www.mozilla.org/","url":null,"some-key":"foo',
    'http://www.mozilla.org/","url":["foo","bar"],"some-key":"foo',
  ];
  for (let inputStr of inputStrs) {
    yield checkInput(inputStr);
  }
  gURLBar.value = "";
  gURLBar.handleRevert();
  gURLBar.blur();
});

add_task(function losslessDecode() {
  let urlNoScheme = "example.com/\u30a2\u30a4\u30a6\u30a8\u30aa";
  let url = "http://" + urlNoScheme;
  gURLBar.textValue = url;
  // Since this is directly setting textValue, it is expected to be trimmed.
  Assert.equal(gURLBar.inputField.value, urlNoScheme,
               "The string displayed in the textbox should not be escaped");
  gURLBar.value = "";
  gURLBar.handleRevert();
  gURLBar.blur();
});

add_task(function* actionURILosslessDecode() {
  let urlNoScheme = "example.com/\u30a2\u30a4\u30a6\u30a8\u30aa";
  let url = "http://" + urlNoScheme;
  yield promiseAutocompleteResultPopup(url);

  // At this point the heuristic result is selected but the urlbar's value is
  // simply `url`.  Key down and back around until the heuristic result is
  // selected again, and at that point the urlbar's value should be a visiturl
  // moz-action.

  do {
    gURLBar.controller.handleKeyNavigation(KeyEvent.DOM_VK_DOWN);
  } while (gURLBar.popup.selectedIndex != 0);

  let [, type, ] = gURLBar.value.match(/^moz-action:([^,]+),(.*)$/);
  Assert.equal(type, "visiturl",
               "visiturl action URI should be in the urlbar");

  Assert.equal(gURLBar.inputField.value, urlNoScheme,
               "The string displayed in the textbox should not be escaped");

  gURLBar.value = "";
  gURLBar.handleRevert();
  gURLBar.blur();
});

function* checkInput(inputStr) {
  yield promiseAutocompleteResultPopup(inputStr);

  let item = gURLBar.popup.richlistbox.firstChild;
  Assert.ok(item, "Should have a result");

  // visiturl matches have their param.urls fixed up.
  let fixupInfo = Services.uriFixup.getFixupURIInfo(inputStr,
    Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
    Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP
  );
  let expectedVisitURL = fixupInfo.fixedURI.spec;

  let type = "visiturl";
  let params = {
    url: expectedVisitURL,
    input: inputStr,
  };
  for (let key in params) {
    params[key] = encodeURIComponent(params[key]);
  }
  let expectedURL = "moz-action:" + type + "," + JSON.stringify(params);
  Assert.equal(item.getAttribute("url"), expectedURL, "url");

  Assert.equal(item.getAttribute("title"), inputStr.replace("\\", "/"), "title");
  Assert.equal(item.getAttribute("text"), inputStr, "text");

  let itemType = item.getAttribute("type");
  Assert.equal(itemType, "visiturl");

  Assert.equal(item._titleText.textContent, inputStr.replace("\\", "/"), "Visible title");
  Assert.equal(item._actionText.textContent, "Visit", "Visible action");
}
