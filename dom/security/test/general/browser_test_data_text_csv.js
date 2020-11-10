"use strict";

const kTestPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const kTestURI = kTestPath + "file_data_text_csv.html";

function addWindowListener(aURL, aCallback) {
  return new Promise(resolve => {
    Services.wm.addListener({
      onOpenWindow(aXULWindow) {
        info("window opened, waiting for focus");
        Services.wm.removeListener(this);
        var domwindow = aXULWindow.docShell.domWindow;
        waitForFocus(function() {
          is(
            domwindow.document.location.href,
            aURL,
            "should have seen the right window open"
          );
          resolve(domwindow);
        }, domwindow);
      },
      onCloseWindow(aXULWindow) {},
    });
  });
}

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.data_uri.block_toplevel_data_uri_navigations", true]],
  });
  let windowPromise = addWindowListener(
    "chrome://mozapps/content/downloads/unknownContentType.xhtml"
  );
  BrowserTestUtils.loadURI(gBrowser, kTestURI);
  let win = await windowPromise;

  let expectedValue = "text/csv;foo,bar,foobar";
  is(
    win.document.getElementById("location").value,
    expectedValue,
    "file name of download should match"
  );
  win.close();
});
