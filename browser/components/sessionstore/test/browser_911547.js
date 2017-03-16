/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests that session restore component does restore the right content
// security policy with the document.
// The policy being tested disallows inline scripts

add_task(function* test() {
  // create a tab that has a CSP
  let testURL = "http://mochi.test:8888/browser/browser/components/sessionstore/test/browser_911547_sample.html";
  let tab = gBrowser.selectedTab = gBrowser.addTab(testURL);
  gBrowser.selectedTab = tab;

  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // this is a baseline to ensure CSP is active
  // attempt to inject and run a script via inline (pre-restore, allowed)
  yield injectInlineScript(browser, `document.getElementById("test_id").value = "fail";`);

  let loadedPromise = promiseBrowserLoaded(browser);
  yield ContentTask.spawn(browser, null, function() {
    is(content.document.getElementById("test_id").value, "ok",
       "CSP should block the inline script that modifies test_id");

    // attempt to click a link to a data: URI (will inherit the CSP of the
    // origin document) and navigate to the data URI in the link.
    content.document.getElementById("test_data_link").click();
  });

  yield loadedPromise;

  yield ContentTask.spawn(browser, null, function() {
    is(content.document.getElementById("test_id2").value, "ok",
       "CSP should block the script loaded by the clicked data URI");
  });

  // close the tab
  yield promiseRemoveTab(tab);

  // open new tab and recover the state
  tab = ss.undoCloseTab(window, 0);
  yield promiseTabRestored(tab);
  browser = tab.linkedBrowser;

  yield ContentTask.spawn(browser, null, function() {
    is(content.document.getElementById("test_id2").value, "ok",
       "CSP should block the script loaded by the clicked data URI after restore");
  });

  // clean up
  gBrowser.removeTab(tab);
});

// injects an inline script element (with a text body)
function injectInlineScript(browser, scriptText) {
  return ContentTask.spawn(browser, scriptText, function(text) {
    let scriptElt = content.document.createElement("script");
    scriptElt.type = "text/javascript";
    scriptElt.text = text;
    content.document.body.appendChild(scriptElt);
  });
}
