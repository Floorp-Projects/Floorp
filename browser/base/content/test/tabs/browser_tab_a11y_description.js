/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

async function waitForFocusAfterKey(ariaFocus, element, key, accel = false) {
  let event = ariaFocus ? "AriaFocus" : "focus";
  let friendlyKey = key;
  if (accel) {
    friendlyKey = "Accel+" + key;
  }
  key = "KEY_" + key;
  let focused = BrowserTestUtils.waitForEvent(element, event);
  EventUtils.synthesizeKey(key, { accelKey: accel });
  await focused;
  ok(true, element.label + " got " + event + " after " + friendlyKey);
}

function getA11yDescription(element) {
  let descId = element.getAttribute("aria-describedby");
  if (!descId) {
    return null;
  }
  let descElem = document.getElementById(descId);
  if (!descElem) {
    return null;
  }
  return descElem.textContent;
}

add_task(async function testTabA11yDescription() {
  const tab1 = await addTab("http://mochi.test:8888/1", { userContextId: 1 });
  tab1.label = "tab1";
  const context1 = ContextualIdentityService.getUserContextLabel(1);
  const tab2 = await addTab("http://mochi.test:8888/2", { userContextId: 2 });
  tab2.label = "tab2";
  const context2 = ContextualIdentityService.getUserContextLabel(2);

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  let focused = BrowserTestUtils.waitForEvent(tab1, "focus");
  tab1.focus();
  await focused;
  ok(true, "tab1 initially focused");
  ok(
    getA11yDescription(tab1).endsWith(context1),
    "tab1 has correct a11y description"
  );
  ok(!getA11yDescription(tab2), "tab2 has no a11y description");

  info("Moving DOM focus to tab2");
  await waitForFocusAfterKey(false, tab2, "ArrowRight");
  ok(
    getA11yDescription(tab2).endsWith(context2),
    "tab2 has correct a11y description"
  );
  ok(!getA11yDescription(tab1), "tab1 has no a11y description");

  info("Moving ARIA focus to tab1");
  await waitForFocusAfterKey(true, tab1, "ArrowLeft", true);
  ok(
    getA11yDescription(tab1).endsWith(context1),
    "tab1 has correct a11y description"
  );
  ok(!getA11yDescription(tab2), "tab2 has no a11y description");

  info("Removing ARIA focus (reverting to DOM focus)");
  await waitForFocusAfterKey(true, tab2, "ArrowRight");
  ok(
    getA11yDescription(tab2).endsWith(context2),
    "tab2 has correct a11y description"
  );
  ok(!getA11yDescription(tab1), "tab1 has no a11y description");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
