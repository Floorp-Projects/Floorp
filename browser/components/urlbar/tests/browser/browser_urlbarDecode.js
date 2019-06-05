/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test makes sure (1) you can't break the urlbar by typing particular JSON
// or JS fragments into it, (2) urlbar.textValue shows URLs unescaped, and (3)
// the urlbar also shows the URLs embedded in action URIs unescaped.  See bug
// 1233672.

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

add_task(async function injectJSON() {
  let inputStrs = [
    'http://example.com/ ", "url": "bar',
    "http://example.com/\\",
    'http://example.com/"',
    'http://example.com/","url":"evil.com',
    "http://mozilla.org/\\u0020",
    'http://www.mozilla.org/","url":1e6,"some-key":"foo',
    'http://www.mozilla.org/","url":null,"some-key":"foo',
    'http://www.mozilla.org/","url":["foo","bar"],"some-key":"foo',
  ];
  for (let inputStr of inputStrs) {
    await checkInput(inputStr);
  }
  gURLBar.value = "";
  gURLBar.handleRevert();
  gURLBar.blur();
});

add_task(function losslessDecode() {
  let urlNoScheme = "example.com/\u30a2\u30a4\u30a6\u30a8\u30aa";
  let url = "http://" + urlNoScheme;
  if (Services.prefs.getBoolPref("browser.urlbar.quantumbar", true)) {
    const result = new UrlbarResult(UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
                                    UrlbarUtils.RESULT_SOURCE.TABS,
                                    { url });
    gURLBar.setValueFromResult(result);
  } else {
    gURLBar.textValue = url;
  }
  // Since this is directly setting textValue, it is expected to be trimmed.
  Assert.equal(gURLBar.inputField.value, urlNoScheme,
               "The string displayed in the textbox should not be escaped");
  gURLBar.value = "";
  gURLBar.handleRevert();
  gURLBar.blur();
});

add_task(async function actionURILosslessDecode() {
  let urlNoScheme = "example.com/\u30a2\u30a4\u30a6\u30a8\u30aa";
  let url = "http://" + urlNoScheme;
  await promiseAutocompleteResultPopup(url);

  // At this point the heuristic result is selected but the urlbar's value is
  // simply `url`.  Key down and back around until the heuristic result is
  // selected again, and at that point the urlbar's value should be a visiturl
  // moz-action.
  do {
    EventUtils.synthesizeKey("KEY_ArrowDown");
  } while (UrlbarTestUtils.getSelectedIndex(window) != 0);

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);

  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL,
    "Should have selected a result of URL type");

  Assert.equal(gURLBar.inputField.value, urlNoScheme,
               "The string displayed in the textbox should not be escaped");

  gURLBar.value = "";
  gURLBar.handleRevert();
  gURLBar.blur();
});

add_task(async function test_resultsDisplayDecoded() {
  await PlacesUtils.history.clear();

  await PlacesTestUtils.addVisits("http://example.com/%E9%A1%B5");

  await promiseAutocompleteResultPopup("example");

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.displayed.url, "example.com/\u9875",
    "Should be displayed the correctly unescaped URL");
});

async function checkInput(inputStr) {
  await promiseAutocompleteResultPopup(inputStr);

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);

  // URL matches have their param.urls fixed up.
  let fixupInfo = Services.uriFixup.getFixupURIInfo(inputStr,
    Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
    Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP
  );
  let expectedVisitURL = fixupInfo.fixedURI.spec;

  if (!UrlbarPrefs.get("quantumbar")) {
    let type = "visiturl";
    let params = {
      url: expectedVisitURL,
      input: inputStr,
    };
    for (let key in params) {
      params[key] = encodeURIComponent(params[key]);
    }
    expectedVisitURL = "moz-action:" + type + "," + JSON.stringify(params);
  }

  Assert.equal(result.url, expectedVisitURL, "Should have the correct URL");
  Assert.equal(result.title, inputStr.replace("\\", "/"),
    "Should have the correct title");
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL,
    "Should have be a result of type URL");

  Assert.equal(result.displayed.title, inputStr.replace("\\", "/"),
    "Should be displaying the correct text");
  Assert.equal(result.displayed.action,
    Services.strings.createBundle("chrome://global/locale/autocomplete.properties")
            .GetStringFromName("visit"),
    "Should be displaying the correct action text");
}
