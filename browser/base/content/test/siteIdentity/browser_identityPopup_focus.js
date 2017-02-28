/* Tests the focus behavior of the identity popup. */

// Access the identity popup via mouseclick. Focus should not be moved inside.
add_task(function* testIdentityPopupFocusClick() {
  yield SpecialPowers.pushPrefEnv({"set": [["accessibility.tabfocus", 7]]});
  yield BrowserTestUtils.withNewTab("https://example.com", function*() {
    let shown = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
    EventUtils.synthesizeMouseAtCenter(gIdentityHandler._identityBox, {});
    yield shown;
    isnot(Services.focus.focusedElement, document.getElementById("identity-popup-security-expander"));
  });
});

// Access the identity popup via keyboard. Focus should be moved inside.
add_task(function* testIdentityPopupFocusKeyboard() {
  yield SpecialPowers.pushPrefEnv({"set": [["accessibility.tabfocus", 7]]});
  yield BrowserTestUtils.withNewTab("https://example.com", function*() {
    let focused = BrowserTestUtils.waitForEvent(gIdentityHandler._identityBox, "focus");
    gIdentityHandler._identityBox.focus();
    yield focused;
    let shown = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
    EventUtils.synthesizeKey(" ", {});
    yield shown;
    is(Services.focus.focusedElement, document.getElementById("identity-popup-security-expander"));
  });
});

