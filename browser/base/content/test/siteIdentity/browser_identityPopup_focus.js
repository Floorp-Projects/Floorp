/* Tests the focus behavior of the identity popup. */

// Access the identity popup via mouseclick. Focus should not be moved inside.
add_task(async function testIdentityPopupFocusClick() {
  await SpecialPowers.pushPrefEnv({"set": [["accessibility.tabfocus", 7]]});
  await BrowserTestUtils.withNewTab("https://example.com", async function() {
    let shown = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
    EventUtils.synthesizeMouseAtCenter(gIdentityHandler._identityBox, {});
    await shown;
    isnot(Services.focus.focusedElement, document.getElementById("identity-popup-security-expander"));
  });
});

// Access the identity popup via keyboard. Focus should be moved inside.
add_task(async function testIdentityPopupFocusKeyboard() {
  await SpecialPowers.pushPrefEnv({"set": [["accessibility.tabfocus", 7]]});
  await BrowserTestUtils.withNewTab("https://example.com", async function() {
    let focused = BrowserTestUtils.waitForEvent(gIdentityHandler._identityBox, "focus");
    gIdentityHandler._identityBox.focus();
    await focused;
    let shown = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
    EventUtils.synthesizeKey(" ", {});
    await shown;
    is(Services.focus.focusedElement, document.getElementById("identity-popup-security-expander"));
  });
});

