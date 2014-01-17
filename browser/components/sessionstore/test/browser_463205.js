/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = ROOT + "browser_463205_sample.html";

/**
 * Bug 463205 - Check URLs before restoring form data to make sure a malicious
 * website can't modify frame URLs and make us inject form data into the wrong
 * web pages.
 */
add_task(function test_check_urls_before_restoring() {
  // Add a blank tab.
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Restore form data with a valid URL.
  ss.setTabState(tab, getState(URL));
  yield promiseTabRestored(tab);

  let value = yield getInputValue(browser, {id: "text"});
  is(value, "foobar", "value was restored");

  // Restore form data with an invalid URL.
  ss.setTabState(tab, getState("http://example.com/"));
  yield promiseTabRestored(tab);

  let value = yield getInputValue(browser, {id: "text"});
  is(value, "", "value was not restored");

  // Cleanup.
  gBrowser.removeTab(tab);
});

function getState(url) {
  return JSON.stringify({
    entries: [{url: URL}],
    formdata: {url: url, id: {text: "foobar"}}
  });
}
