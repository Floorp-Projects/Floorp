/* This test ensures that backspacing autoFilled values still allows to
 * confirm the remaining value.
 */

async function test_autocomplete(data) {
  let {desc, typed, autofilled, modified, keys, action, onAutoFill} = data;
  info(desc);

  await promiseAutocompleteResultPopup(typed);
  is(gURLBar.textValue, autofilled, "autofilled value is as expected");
  if (onAutoFill)
    onAutoFill();

  keys.forEach(key => EventUtils.synthesizeKey(key, {}));

  is(gURLBar.textValue, modified, "backspaced value is as expected");

  await promiseSearchComplete();

  ok(gURLBar.popup.richlistbox.children.length > 0, "Should get at least 1 result");
  let result = gURLBar.popup.richlistbox.children[0];
  let type = result.getAttribute("type");
  let types = type.split(/\s+/);
  ok(types.indexOf(action) >= 0, `The type attribute "${type}" includes the expected action "${action}"`);

  gURLBar.popup.hidePopup();
  await promisePopupHidden(gURLBar.popup);
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
    uri: NetUtil.newURI("http://example.com/"),
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED
  });

  await test_autocomplete({ desc: "DELETE the autofilled part should search",
                            typed: "exam",
                            autofilled: "example.com/",
                            modified: "exam",
                            keys: ["VK_DELETE"],
                            action: "searchengine"
                          });
  await test_autocomplete({ desc: "DELETE the final slash should visit",
                            typed: "example.com",
                            autofilled: "example.com/",
                            modified: "example.com",
                            keys: ["VK_DELETE"],
                            action: "visiturl"
                          });

  await test_autocomplete({ desc: "BACK_SPACE the autofilled part should search",
                            typed: "exam",
                            autofilled: "example.com/",
                            modified: "exam",
                            keys: ["VK_BACK_SPACE"],
                            action: "searchengine"
                          });
  await test_autocomplete({ desc: "BACK_SPACE the final slash should visit",
                            typed: "example.com",
                            autofilled: "example.com/",
                            modified: "example.com",
                            keys: ["VK_BACK_SPACE"],
                            action: "visiturl"
                          });

  await test_autocomplete({ desc: "DELETE the autofilled part, then BACK_SPACE, should search",
                            typed: "exam",
                            autofilled: "example.com/",
                            modified: "exa",
                            keys: ["VK_DELETE", "VK_BACK_SPACE"],
                            action: "searchengine"
                          });
  await test_autocomplete({ desc: "DELETE the final slash, then BACK_SPACE, should search",
                            typed: "example.com",
                            autofilled: "example.com/",
                            modified: "example.co",
                            keys: ["VK_DELETE", "VK_BACK_SPACE"],
                            action: "visiturl"
                          });

  await test_autocomplete({ desc: "BACK_SPACE the autofilled part, then BACK_SPACE, should search",
                            typed: "exam",
                            autofilled: "example.com/",
                            modified: "exa",
                            keys: ["VK_BACK_SPACE", "VK_BACK_SPACE"],
                            action: "searchengine"
                          });
  await test_autocomplete({ desc: "BACK_SPACE the final slash, then BACK_SPACE, should search",
                            typed: "example.com",
                            autofilled: "example.com/",
                            modified: "example.co",
                            keys: ["VK_BACK_SPACE", "VK_BACK_SPACE"],
                            action: "visiturl"
                          });

  await test_autocomplete({ desc: "BACK_SPACE after blur should search",
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
  await test_autocomplete({ desc: "DELETE after blur should search",
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
  await test_autocomplete({ desc: "double BACK_SPACE after blur should search",
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

  await PlacesTestUtils.clearHistory();
});
