/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test ensures that form data collection respects the privacy level as
 * set by the user.
 */
add_task(function test_formdata() {
  const URL = "http://mochi.test:8888/browser/browser/components/" +
              "sessionstore/test/browser_formdata_sample.html";

  const OUTER_VALUE = "browser_formdata_" + Math.random();
  const INNER_VALUE = "browser_formdata_" + Math.random();

  // Creates a tab, loads a page with some form fields,
  // modifies their values and closes the tab.
  function createAndRemoveTab() {
    return Task.spawn(function () {
      // Create a new tab.
      let tab = gBrowser.addTab(URL);
      let browser = tab.linkedBrowser;
      yield promiseBrowserLoaded(browser);

      // Modify form data.
      yield setInputValue(browser, {id: "txt", value: OUTER_VALUE});
      yield setInputValue(browser, {id: "txt", value: INNER_VALUE, frame: 0});

      // Remove the tab.
      gBrowser.removeTab(tab);
    });
  }

  yield createAndRemoveTab();
  let [{state: {formdata}}] = JSON.parse(ss.getClosedTabData(window));
  is(formdata.id.txt, OUTER_VALUE, "outer value is correct");
  is(formdata.children[0].id.txt, INNER_VALUE, "inner value is correct");

  // Disable saving data for encrypted sites.
  Services.prefs.setIntPref("browser.sessionstore.privacy_level", 1);

  yield createAndRemoveTab();
  let [{state: {formdata}}] = JSON.parse(ss.getClosedTabData(window));
  is(formdata.id.txt, OUTER_VALUE, "outer value is correct");
  ok(!formdata.children, "inner value was *not* stored");

  // Disable saving data for any site.
  Services.prefs.setIntPref("browser.sessionstore.privacy_level", 2);

  yield createAndRemoveTab();
  let [{state: {formdata}}] = JSON.parse(ss.getClosedTabData(window));
  ok(!formdata, "form data has *not* been stored");

  // Restore the default privacy level.
  Services.prefs.clearUserPref("browser.sessionstore.privacy_level");
});

/**
 * This test ensures that we maintain backwards compatibility with the form
 * data format used pre Fx 29.
 */
add_task(function test_old_format() {
  const URL = "data:text/html;charset=utf-8,<input%20id=input>";
  const VALUE = "value-" + Math.random();

  // Create a tab with an iframe containing an input field.
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Check that the form value is restored.
  let state = {entries: [{url: URL, formdata: {id: {input: VALUE}}}]};
  ss.setTabState(tab, JSON.stringify(state));
  yield promiseTabRestored(tab);
  is((yield getInputValue(browser, "input")), VALUE, "form data restored");

  // Cleanup.
  gBrowser.removeTab(tab);
});

/**
 * This test ensures that we maintain backwards compatibility with the form
 * data form used pre Fx 29, esp. the .innerHTML property for editable docs.
 */
add_task(function test_old_format_inner_html() {
  const URL = "data:text/html;charset=utf-8,<h1>mozilla</h1>" +
              "<script>document.designMode='on'</script>";
  const VALUE = "<h1>value-" + Math.random() + "</h1>";

  // Create a tab with an iframe containing an input field.
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Restore the tab state.
  let state = {entries: [{url: URL, innerHTML: VALUE}]};
  ss.setTabState(tab, JSON.stringify(state));
  yield promiseTabRestored(tab);

  // Check that the innerHTML value was restored.
  let html = yield getInnerHTML(browser);
  is(html, VALUE, "editable document has been restored correctly");

  // Cleanup.
  gBrowser.removeTab(tab);
});

/**
 * This test ensures that a malicious website can't trick us into restoring
 * form data into a wrong website and that we always check the stored URL
 * before doing so.
 */
add_task(function test_url_check() {
  const URL = "data:text/html;charset=utf-8,<input%20id=input>";
  const VALUE = "value-" + Math.random();

  // Create a tab with an iframe containing an input field.
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Restore a tab state with a given form data url.
  function restoreStateWithURL(url) {
    let state = {entries: [{url: URL}], formdata: {id: {input: VALUE}}};

    if (url) {
      state.formdata.url = url;
    }

    ss.setTabState(tab, JSON.stringify(state));
    return promiseTabRestored(tab).then(() => getInputValue(browser, "input"));
  }

  // Check that the form value is restored with the correct URL.
  is((yield restoreStateWithURL(URL)), VALUE, "form data restored");

  // Check that the form value is *not* restored with the wrong URL.
  is((yield restoreStateWithURL(URL + "?")), "", "form data not restored");
  is((yield restoreStateWithURL()), "", "form data not restored");

  // Cleanup.
  gBrowser.removeTab(tab);
});

/**
 * This test ensures that collecting form data works as expected when having
 * nested frame sets.
 */
add_task(function test_nested() {
  const URL = "data:text/html;charset=utf-8," +
              "<iframe src='data:text/html;charset=utf-8," +
              "<input autofocus=true>'/>";

  const FORM_DATA = {
    children: [{
      xpath: {"/xhtml:html/xhtml:body/xhtml:input": "M"},
      url: "data:text/html;charset=utf-8,<input%20autofocus=true>"
    }]
  };

  // Create a tab with an iframe containing an input field.
  let tab = gBrowser.selectedTab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Modify the input field's value.
  yield sendMessage(browser, "ss-test:sendKeyEvent", {key: "m", frame: 0});

  // Remove the tab and check that we stored form data correctly.
  gBrowser.removeTab(tab);
  let [{state: {formdata}}] = JSON.parse(ss.getClosedTabData(window));
  is(JSON.stringify(formdata), JSON.stringify(FORM_DATA),
    "formdata for iframe stored correctly");

  // Restore the closed tab.
  let tab = ss.undoCloseTab(window, 0);
  let browser = tab.linkedBrowser;
  yield promiseTabRestored(tab);

  // Check that the input field has the right value.
  SyncHandlers.get(browser).flush();
  let {formdata} = JSON.parse(ss.getTabState(tab));
  is(JSON.stringify(formdata), JSON.stringify(FORM_DATA),
    "formdata for iframe restored correctly");

  // Cleanup.
  gBrowser.removeTab(tab);
});

/**
 * This test ensures that collecting form data for documents with
 * designMode=on works as expected.
 */
add_task(function test_design_mode() {
  const URL = "data:text/html;charset=utf-8,<h1>mozilla</h1>" +
              "<script>document.designMode='on'</script>";

  // Load a tab with an editable document.
  let tab = gBrowser.selectedTab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Modify the document content.
  yield sendMessage(browser, "ss-test:sendKeyEvent", {key: "m"});

  // Close and restore the tab.
  gBrowser.removeTab(tab);
  tab = ss.undoCloseTab(window, 0);
  browser = tab.linkedBrowser;
  yield promiseTabRestored(tab);

  // Check that the innerHTML value was restored.
  let html = yield getInnerHTML(browser);
  let expected = "<h1>Mmozilla</h1><script>document.designMode='on'</script>";
  is(html, expected, "editable document has been restored correctly");

  // Close and restore the tab.
  gBrowser.removeTab(tab);
  tab = ss.undoCloseTab(window, 0);
  browser = tab.linkedBrowser;
  yield promiseTabRestored(tab);

  // Check that the innerHTML value was restored.
  let html = yield getInnerHTML(browser);
  let expected = "<h1>Mmozilla</h1><script>document.designMode='on'</script>";
  is(html, expected, "editable document has been restored correctly");

  // Cleanup.
  gBrowser.removeTab(tab);
});

function getInputValue(browser, id) {
  return sendMessage(browser, "ss-test:getInputValue", {id: id});
}

function setInputValue(browser, data) {
  return sendMessage(browser, "ss-test:setInputValue", data);
}

function getInnerHTML(browser) {
  return sendMessage(browser, "ss-test:getInnerHTML", {selector: "body"});
}
