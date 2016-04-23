/* This test ensures that backspacing autoFilled values still allows to
 * confirm the remaining value.
 */

function* test_autocomplete(data) {
  let {desc, typed, autofilled, modified, keys, action, onAutoFill} = data;
  info(desc);

  yield promiseAutocompleteResultPopup(typed);
  is(gURLBar.value, autofilled, "autofilled value is as expected");
  if (onAutoFill)
    onAutoFill()

  keys.forEach(key => EventUtils.synthesizeKey(key, {}));

  is(gURLBar.value, modified, "backspaced value is as expected");

  yield promiseSearchComplete();

  ok(gURLBar.popup.richlistbox.children.length > 0, "Should get at least 1 result");
  let result = gURLBar.popup.richlistbox.children[0];
  let type = result.getAttribute("type");
  let types = type.split(/\s+/);
  ok(types.indexOf(action) >= 0, `The type attribute "${type}" includes the expected action "${action}"`);

  gURLBar.popup.hidePopup();
  yield promisePopupHidden(gURLBar.popup);
  gURLBar.blur();
};

add_task(function* () {
  registerCleanupFunction(function* () {
    Services.prefs.clearUserPref("browser.urlbar.unifiedcomplete");
    Services.prefs.clearUserPref("browser.urlbar.autoFill");
    gURLBar.handleRevert();
    yield PlacesTestUtils.clearHistory();
  });
  Services.prefs.setBoolPref("browser.urlbar.unifiedcomplete", true);
  Services.prefs.setBoolPref("browser.urlbar.autoFill", true);

  // Add a typed visit, so it will be autofilled.
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://example.com/"),
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED
  });

  yield test_autocomplete({ desc: "DELETE the autofilled part should search",
                            typed: "exam",
                            autofilled: "example.com/",
                            modified: "exam",
                            keys: ["VK_DELETE"],
                            action: "searchengine"
                          });
  yield test_autocomplete({ desc: "DELETE the final slash should visit",
                            typed: "example.com",
                            autofilled: "example.com/",
                            modified: "example.com",
                            keys: ["VK_DELETE"],
                            action: "visiturl"
                          });

  yield test_autocomplete({ desc: "BACK_SPACE the autofilled part should search",
                            typed: "exam",
                            autofilled: "example.com/",
                            modified: "exam",
                            keys: ["VK_BACK_SPACE"],
                            action: "searchengine"
                          });
  yield test_autocomplete({ desc: "BACK_SPACE the final slash should visit",
                            typed: "example.com",
                            autofilled: "example.com/",
                            modified: "example.com",
                            keys: ["VK_BACK_SPACE"],
                            action: "visiturl"
                          });

  yield test_autocomplete({ desc: "DELETE the autofilled part, then BACK_SPACE, should search",
                            typed: "exam",
                            autofilled: "example.com/",
                            modified: "exa",
                            keys: ["VK_DELETE", "VK_BACK_SPACE"],
                            action: "searchengine"
                          });
  yield test_autocomplete({ desc: "DELETE the final slash, then BACK_SPACE, should search",
                            typed: "example.com",
                            autofilled: "example.com/",
                            modified: "example.co",
                            keys: ["VK_DELETE", "VK_BACK_SPACE"],
                            action: "visiturl"
                          });

  yield test_autocomplete({ desc: "BACK_SPACE the autofilled part, then BACK_SPACE, should search",
                            typed: "exam",
                            autofilled: "example.com/",
                            modified: "exa",
                            keys: ["VK_BACK_SPACE", "VK_BACK_SPACE"],
                            action: "searchengine"
                          });
  yield test_autocomplete({ desc: "BACK_SPACE the final slash, then BACK_SPACE, should search",
                            typed: "example.com",
                            autofilled: "example.com/",
                            modified: "example.co",
                            keys: ["VK_BACK_SPACE", "VK_BACK_SPACE"],
                            action: "visiturl"
                          });

  yield test_autocomplete({ desc: "BACK_SPACE after blur should search",
                            typed: "ex",
                            autofilled: "example.com/",
                            modified: "e",
                            keys: ["VK_BACK_SPACE"],
                            action: "searchengine",
                            onAutoFill: () => {
                              gURLBar.blur();
                              gURLBar.focus();
                              gURLBar.selectionStart = 1;
                              gURLBar.selectionEnd = 12;
                            }
                         });
  yield test_autocomplete({ desc: "DELETE after blur should search",
                            typed: "ex",
                            autofilled: "example.com/",
                            modified: "e",
                            keys: ["VK_DELETE"],
                            action: "searchengine",
                            onAutoFill: () => {
                              gURLBar.blur();
                              gURLBar.focus();
                              gURLBar.selectionStart = 1;
                              gURLBar.selectionEnd = 12;
                            }
                          });
  yield test_autocomplete({ desc: "double BACK_SPACE after blur should search",
                            typed: "ex",
                            autofilled: "example.com/",
                            modified: "e",
                            keys: ["VK_BACK_SPACE", "VK_BACK_SPACE"],
                            action: "searchengine",
                            onAutoFill: () => {
                              gURLBar.blur();
                              gURLBar.focus();
                              gURLBar.selectionStart = 2;
                              gURLBar.selectionEnd = 12;
                            }
                         });

  yield PlacesTestUtils.clearHistory();
});
