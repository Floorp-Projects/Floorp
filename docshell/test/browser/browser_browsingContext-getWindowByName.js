/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function addWindow(name) {
  var blank = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString
  );
  blank.data = "about:blank";
  let promise = BrowserTestUtils.waitForNewWindow({
    anyWindow: true,
    url: "about:blank",
  });
  Services.ww.openWindow(
    null,
    AppConstants.BROWSER_CHROME_URL,
    name,
    "chrome,dialog=no",
    blank
  );
  return promise;
}

add_task(async function() {
  let windows = [await addWindow("first"), await addWindow("second")];

  for (let w of windows) {
    isnot(w, null);
    is(Services.ww.getWindowByName(w.name, null), w, `Found ${w.name}`);
  }

  await Promise.all(windows.map(BrowserTestUtils.closeWindow));
});
