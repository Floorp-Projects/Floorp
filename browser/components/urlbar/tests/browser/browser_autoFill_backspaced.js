/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test ensures that backspacing autoFilled values still allows to
 * confirm the remaining value.
 */

"use strict";

async function test_autocomplete(data) {
  let {desc, typed, autofilled, modified, keys, type, onAutoFill} = data;
  info(desc);

  await promiseAutocompleteResultPopup(typed);
  Assert.equal(gURLBar.textValue, autofilled, "autofilled value is as expected");
  if (onAutoFill)
    onAutoFill();

  info("Synthesizing keys");
  keys.forEach(key => EventUtils.synthesizeKey(key));

  Assert.equal(gURLBar.textValue, modified, "backspaced value is as expected");

  await promiseSearchComplete();

  Assert.greater(UrlbarTestUtils.getResultCount(window), 0,
    "Should get at least 1 result");

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);

  Assert.equal(result.type, type,
    "Should have the correct result type");

  await UrlbarTestUtils.promisePopupClose(window);
  gURLBar.blur();
}

add_task(async function() {
  registerCleanupFunction(async function() {
    Services.prefs.clearUserPref("browser.urlbar.autoFill");
    gURLBar.handleRevert();
    await PlacesUtils.history.clear();
  });
  Services.prefs.setBoolPref("browser.urlbar.autoFill", true);

  // Add a typed visit, so it will be autofilled.
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://example.com/"),
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
  });

  await test_autocomplete({ desc: "DELETE the autofilled part should search",
                            typed: "exam",
                            autofilled: "example.com/",
                            modified: "exam",
                            keys: ["KEY_Delete"],
                            type: UrlbarUtils.RESULT_TYPE.SEARCH,
                          });
  await test_autocomplete({ desc: "DELETE the final slash should visit",
                            typed: "example.com",
                            autofilled: "example.com/",
                            modified: "example.com",
                            keys: ["KEY_Delete"],
                            type: UrlbarUtils.RESULT_TYPE.URL,
                          });

  await test_autocomplete({ desc: "BACK_SPACE the autofilled part should search",
                            typed: "exam",
                            autofilled: "example.com/",
                            modified: "exam",
                            keys: ["KEY_Backspace"],
                            type: UrlbarUtils.RESULT_TYPE.SEARCH,
                          });
  await test_autocomplete({ desc: "BACK_SPACE the final slash should visit",
                            typed: "example.com",
                            autofilled: "example.com/",
                            modified: "example.com",
                            keys: ["KEY_Backspace"],
                            type: UrlbarUtils.RESULT_TYPE.URL,
                          });

  await test_autocomplete({ desc: "DELETE the autofilled part, then BACK_SPACE, should search",
                            typed: "exam",
                            autofilled: "example.com/",
                            modified: "exa",
                            keys: ["KEY_Delete", "KEY_Backspace"],
                            type: UrlbarUtils.RESULT_TYPE.SEARCH,
                          });
  await test_autocomplete({ desc: "DELETE the final slash, then BACK_SPACE, should search",
                            typed: "example.com",
                            autofilled: "example.com/",
                            modified: "example.co",
                            keys: ["KEY_Delete", "KEY_Backspace"],
                            type: UrlbarUtils.RESULT_TYPE.URL,
                          });

  await test_autocomplete({ desc: "BACK_SPACE the autofilled part, then BACK_SPACE, should search",
                            typed: "exam",
                            autofilled: "example.com/",
                            modified: "exa",
                            keys: ["KEY_Backspace", "KEY_Backspace"],
                            type: UrlbarUtils.RESULT_TYPE.SEARCH,
                          });
  await test_autocomplete({ desc: "BACK_SPACE the final slash, then BACK_SPACE, should search",
                            typed: "example.com",
                            autofilled: "example.com/",
                            modified: "example.co",
                            keys: ["KEY_Backspace", "KEY_Backspace"],
                            type: UrlbarUtils.RESULT_TYPE.URL,
                          });

  await test_autocomplete({ desc: "BACK_SPACE after blur should search",
                            typed: "ex",
                            autofilled: "example.com/",
                            modified: "e",
                            keys: ["KEY_Backspace"],
                            type: UrlbarUtils.RESULT_TYPE.SEARCH,
                            onAutoFill: () => {
                              gURLBar.blur();
                              gURLBar.focus();
                              gURLBar.selectionStart = 1;
                              gURLBar.selectionEnd = 12;
                            },
                         });
  await test_autocomplete({ desc: "DELETE after blur should search",
                            typed: "ex",
                            autofilled: "example.com/",
                            modified: "e",
                            keys: ["KEY_Delete"],
                            type: UrlbarUtils.RESULT_TYPE.SEARCH,
                            onAutoFill: () => {
                              gURLBar.blur();
                              gURLBar.focus();
                              gURLBar.selectionStart = 1;
                              gURLBar.selectionEnd = 12;
                            },
                          });
  await test_autocomplete({ desc: "double BACK_SPACE after blur should search",
                            typed: "ex",
                            autofilled: "example.com/",
                            modified: "e",
                            keys: ["KEY_Backspace", "KEY_Backspace"],
                            type: UrlbarUtils.RESULT_TYPE.SEARCH,
                            onAutoFill: () => {
                              gURLBar.blur();
                              gURLBar.focus();
                              gURLBar.selectionStart = 2;
                              gURLBar.selectionEnd = 12;
                            },
                         });

  await PlacesUtils.history.clear();
});
