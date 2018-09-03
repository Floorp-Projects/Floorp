/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test is two fold:
// a) if security.data_uri.unique_opaque_origin == false, then
//    this tests that session restore component does restore the right
//    content security policy with the document. (The policy being
//    tested disallows inline scripts).
// b) if security.data_uri.unique_opaque_origin == true, then
//    this tests that data: URIs do not inherit the CSP from
//    it's enclosing context.

add_task(async function test() {
  // allow top level data: URI navigations, otherwise clicking a data: link fails
  await SpecialPowers.pushPrefEnv({
    "set": [["security.data_uri.block_toplevel_data_uri_navigations", false]],
  });
  let dataURIPref = Services.prefs.getBoolPref("security.data_uri.unique_opaque_origin");
  // create a tab that has a CSP
  let testURL = "http://mochi.test:8888/browser/browser/components/sessionstore/test/browser_911547_sample.html";
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, testURL);
  gBrowser.selectedTab = tab;

  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // this is a baseline to ensure CSP is active
  // attempt to inject and run a script via inline (pre-restore, allowed)
  await injectInlineScript(browser, `document.getElementById("test_id1").value = "id1_modified";`);

  let loadedPromise = promiseBrowserLoaded(browser);
  await ContentTask.spawn(browser, null, function() {
    is(content.document.getElementById("test_id1").value, "id1_initial",
       "CSP should block the inline script that modifies test_id");


    // (a) if security.data_uri.unique_opaque_origin == false:
    //     attempt to click a link to a data: URI (will inherit the CSP of
    //     the origin document) and navigate to the data URI in the link.
    // (b) if security.data_uri.unique_opaque_origin == true:
    //     attempt to click a link to a data: URI (will *not* inherit the CSP of
    //     the origin document) and navigate to the data URI in the link.
    content.document.getElementById("test_data_link").click();
  });

  await loadedPromise;

  await ContentTask.spawn(browser, {dataURIPref}, function( {dataURIPref}) { // eslint-disable-line
    if (dataURIPref) {
      is(content.document.getElementById("test_id2").value, "id2_modified",
         "data: URI should *not* inherit the CSP of the enclosing context");
    } else {
      is(content.document.getElementById("test_id2").value, "id2_initial",
        "CSP should block the script loaded by the clicked data URI");
    }
  });

  // close the tab
  await promiseRemoveTabAndSessionState(tab);

  // open new tab and recover the state
  tab = ss.undoCloseTab(window, 0);
  await promiseTabRestored(tab);
  browser = tab.linkedBrowser;

  await ContentTask.spawn(browser, {dataURIPref}, function({dataURIPref}) { // eslint-disable-line
    if (dataURIPref) {
      is(content.document.getElementById("test_id2").value, "id2_modified",
         "data: URI should *not* inherit the CSP of the enclosing context");
    } else {
      is(content.document.getElementById("test_id2").value, "id2_initial",
        "CSP should block the script loaded by the clicked data URI after restore");
    }
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
