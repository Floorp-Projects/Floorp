/* Tests the focus behavior of the identity popup. */

// Focusing on the identity box is handled by the ToolbarKeyboardNavigator
// component (see browser/base/content/browser-toolbarKeyNav.js).
async function focusIdentityBox() {
  gURLBar.inputField.focus();
  is(document.activeElement, gURLBar.inputField, "urlbar should be focused");
  const focused = BrowserTestUtils.waitForEvent(
    gIdentityHandler._identityIconBox,
    "focus"
  );
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  EventUtils.synthesizeKey("ArrowRight");
  await focused;
}

// Access the identity popup via mouseclick. Focus should not be moved inside.
add_task(async function testIdentityPopupFocusClick() {
  await SpecialPowers.pushPrefEnv({ set: [["accessibility.tabfocus", 7]] });
  await BrowserTestUtils.withNewTab("https://example.com", async function() {
    let shown = BrowserTestUtils.waitForEvent(
      window,
      "popupshown",
      true,
      event => event.target == gIdentityHandler._identityPopup
    );
    EventUtils.synthesizeMouseAtCenter(gIdentityHandler._identityIconBox, {});
    await shown;
    isnot(
      Services.focus.focusedElement,
      document.getElementById("identity-popup-security-expander")
    );
  });
});

// Access the identity popup via keyboard. Focus should be moved inside.
add_task(async function testIdentityPopupFocusKeyboard() {
  await SpecialPowers.pushPrefEnv({ set: [["accessibility.tabfocus", 7]] });
  await BrowserTestUtils.withNewTab("https://example.com", async function() {
    await focusIdentityBox();
    let shown = BrowserTestUtils.waitForEvent(
      window,
      "popupshown",
      true,
      event => event.target == gIdentityHandler._identityPopup
    );
    EventUtils.sendString(" ");
    await shown;
    is(
      Services.focus.focusedElement,
      document.getElementById(
        gProtonDoorhangers
          ? "identity-popup-security-button"
          : "identity-popup-security-expander"
      )
    );
  });
});

// Access the Site Security panel, then move focus with the tab key.
// Tabbing should be able to reach the More Information button.
add_task(async function testSiteSecurityTabOrder() {
  await SpecialPowers.pushPrefEnv({ set: [["accessibility.tabfocus", 7]] });
  await BrowserTestUtils.withNewTab("https://example.com", async function() {
    // 1. Access the identity popup.
    await focusIdentityBox();
    let shown = BrowserTestUtils.waitForEvent(
      window,
      "popupshown",
      true,
      event => event.target == gIdentityHandler._identityPopup
    );
    EventUtils.sendString(" ");
    await shown;
    is(
      Services.focus.focusedElement,
      document.getElementById(
        gProtonDoorhangers
          ? "identity-popup-security-button"
          : "identity-popup-security-expander"
      )
    );

    // 2. Access the Site Security section.
    let securityView = document.getElementById("identity-popup-securityView");
    shown = BrowserTestUtils.waitForEvent(securityView, "ViewShown");
    EventUtils.sendString(" ");
    await shown;

    // 3. Custom root learn more info should be focused by default
    // This is probably not present in real-world scenarios, but needs to be present in our test infrastructure.
    let customRootLearnMore = document.getElementById(
      "identity-popup-custom-root-learn-more"
    );
    is(
      Services.focus.focusedElement,
      customRootLearnMore,
      "learn more option for custom roots is focused"
    );

    // 4. First press of tab should move to the More Information button.
    let moreInfoButton = document.getElementById("identity-popup-more-info");
    let focused = BrowserTestUtils.waitForEvent(
      gIdentityHandler._identityPopup,
      "focusin"
    );
    EventUtils.sendKey("tab");
    await focused;
    is(
      Services.focus.focusedElement,
      moreInfoButton,
      "more info button is focused"
    );

    // 5. Second press of tab should focus the Back button.
    let backButton = gIdentityHandler._identityPopup.querySelector(
      ".subviewbutton-back"
    );
    // Wait for focus to move somewhere. We use focusin because focus doesn't bubble.
    focused = BrowserTestUtils.waitForEvent(
      gIdentityHandler._identityPopup,
      "focusin"
    );
    EventUtils.sendKey("tab");
    await focused;
    is(Services.focus.focusedElement, backButton, "back button is focused");
  });
});
