/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test ensures that pressing ctrl/shift+enter bypasses the autoFilled
 * value, and only considers what the user typed (but not just enter).
 */

async function test_autocomplete(data) {
  let {desc, typed, autofilled, modified, waitForUrl, keys} = data;
  info(desc);

  await promiseAutocompleteResultPopup(typed);
  is(gURLBar.textValue, autofilled, "autofilled value is as expected");

  let promiseLoad = waitForDocLoadAndStopIt(waitForUrl);

  keys.forEach(([key, mods]) => EventUtils.synthesizeKey(key, mods || {}));

  is(gURLBar.textValue, modified, "value is as expected");

  await promiseLoad;
  gURLBar.blur();
}

add_task(async function() {
  registerCleanupFunction(async function() {
    Services.prefs.clearUserPref("browser.urlbar.autoFill");
    gURLBar.handleRevert();
    await PlacesTestUtils.clearHistory();
  });
  Services.prefs.setBoolPref("browser.urlbar.autoFill", true);

  // Add a typed visit, so it will be autofilled.
  await PlacesTestUtils.addVisits({
    uri: "http://example.com/",
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED
  });

  await test_autocomplete({ desc: "CTRL+ENTER on the autofilled part should use autofill",
                            typed: "exam",
                            autofilled: "example.com/",
                            modified: "example.com/",
                            waitForUrl: "http://example.com/",
                            keys: [["VK_RETURN", {}]]
                          });

  await test_autocomplete({ desc: "CTRL+ENTER on the autofilled part should bypass autofill",
                            typed: "exam",
                            autofilled: "example.com/",
                            modified: "www.exam.com",
                            waitForUrl: "http://www.exam.com/",
                            keys: [["VK_RETURN", AppConstants.platform === "macosx" ?
                                                 { metaKey: true } :
                                                 { ctrlKey: true }]]
                          });

  await test_autocomplete({ desc: "SHIFT+ENTER on the autofilled part should bypass autofill",
                            typed: "exam",
                            autofilled: "example.com/",
                            modified: "www.exam.net",
                            waitForUrl: "http://www.exam.net/",
                            keys: [["VK_RETURN", { shiftKey: true }]]
                          });
});
