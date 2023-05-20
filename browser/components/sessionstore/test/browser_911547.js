/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test tests that session restore component does restore the right
// content security policy with the document. (The policy being tested
// disallows inline scripts).

add_task(async function test() {
  // allow top level data: URI navigations, otherwise clicking a data: link fails
  await SpecialPowers.pushPrefEnv({
    set: [["security.data_uri.block_toplevel_data_uri_navigations", false]],
  });
  // create a tab that has a CSP
  let testURL =
    "http://mochi.test:8888/browser/browser/components/sessionstore/test/browser_911547_sample.html";
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, testURL));
  gBrowser.selectedTab = tab;

  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // this is a baseline to ensure CSP is active
  // attempt to inject and run a script via inline (pre-restore, allowed)
  await injectInlineScript(
    browser,
    `document.getElementById("test_id1").value = "id1_modified";`
  );

  let loadedPromise = promiseBrowserLoaded(browser);
  await SpecialPowers.spawn(browser, [], function () {
    is(
      content.document.getElementById("test_id1").value,
      "id1_initial",
      "CSP should block the inline script that modifies test_id"
    );
    content.document.getElementById("test_data_link").click();
  });

  await loadedPromise;

  await SpecialPowers.spawn(browser, [], function () {
    // eslint-disable-line
    // the data: URI inherits the CSP and the inline script needs to be blocked
    is(
      content.document.getElementById("test_id2").value,
      "id2_initial",
      "CSP should block the script loaded by the clicked data URI"
    );
  });

  // close the tab
  await promiseRemoveTabAndSessionState(tab);

  // open new tab and recover the state
  tab = ss.undoCloseTab(window, 0);
  await promiseTabRestored(tab);
  browser = tab.linkedBrowser;

  await SpecialPowers.spawn(browser, [], function () {
    // eslint-disable-line
    // the data: URI should be restored including the inherited CSP and the
    // inline script should be blocked.
    is(
      content.document.getElementById("test_id2").value,
      "id2_initial",
      "CSP should block the script loaded by the clicked data URI after restore"
    );
  });

  // clean up
  gBrowser.removeTab(tab);
});

// injects an inline script element (with a text body)
function injectInlineScript(browser, scriptText) {
  return SpecialPowers.spawn(browser, [scriptText], function (text) {
    let scriptElt = content.document.createElement("script");
    scriptElt.type = "text/javascript";
    scriptElt.text = text;
    content.document.body.appendChild(scriptElt);
  });
}
