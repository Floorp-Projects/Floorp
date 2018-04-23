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
    EventUtils.sendString(" ");
    await shown;
    is(Services.focus.focusedElement, document.getElementById("identity-popup-security-expander"));
  });
});

// Access the Site Security panel, then move focus with the tab key.
// Tabbing should be able to reach the More Information button.
add_task(async function testSiteSecurityTabOrder() {
  await SpecialPowers.pushPrefEnv({"set": [["accessibility.tabfocus", 7]]});
  await BrowserTestUtils.withNewTab("https://example.com", async function() {
    // 1. Access the identity popup.
    let focused = BrowserTestUtils.waitForEvent(gIdentityHandler._identityBox, "focus");
    gIdentityHandler._identityBox.focus();
    await focused;
    let shown = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
    EventUtils.sendString(" ");
    await shown;
    is(Services.focus.focusedElement, document.getElementById("identity-popup-security-expander"));

    // 2. Access the Site Security section.
    let securityView = document.getElementById("identity-popup-securityView");
    shown = BrowserTestUtils.waitForEvent(securityView, "ViewShown");
    EventUtils.sendString(" ");
    await shown;

    // 3. First press of tab should focus the Back button.
    let backButton = gIdentityHandler._identityPopup.querySelector(".subviewbutton-back");
    // Wait for focus to move somewhere. We use focusin because focus doesn't bubble.
    focused = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "focusin");
    EventUtils.sendKey("tab");
    await focused;
    is(Services.focus.focusedElement, backButton);

    // 4. Second press of tab should move to the More Information button.
    let moreInfoButton = document.getElementById("identity-popup-more-info");
    focused = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "focusin");
    EventUtils.sendKey("tab");
    await focused;
    isnot(Services.focus.focusedElement, backButton);
    is(Services.focus.focusedElement, moreInfoButton);
  });
});
