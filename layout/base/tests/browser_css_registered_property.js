"use strict";

add_task(async function test() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "data:text/html,<meta charset=utf8>" },
    async function (browser) {
      await SpecialPowers.spawn(browser, [], testRegisterProperty);
    }
  );
});

// This function runs entirely in the content process. It doesn't have access
// any free variables in this file.
async function testRegisterProperty() {
  let doc = content.document;
  doc.styleSheetChangeEventsEnabled = true;

  const EVENT_NAME = "csscustompropertyregistered";

  const unexpectedContentEvent = event =>
    ok(false, "Received a " + event.type + " event on content");
  doc.addEventListener(EVENT_NAME, unexpectedContentEvent);
  doc.defaultView.addEventListener(EVENT_NAME, unexpectedContentEvent);
  doc.addEventListener(EVENT_NAME, unexpectedContentEvent);
  doc.defaultView.addEventListener(EVENT_NAME, unexpectedContentEvent);

  function waitForCssCustomPropertyRegistered() {
    return ContentTaskUtils.waitForEvent(
      docShell.chromeEventHandler,
      EVENT_NAME,
      true
    );
  }

  function checkCssCustomPropertyRegisteredEvent(event) {
    is(event.type, EVENT_NAME, "event.type has expected value");
    is(event.target, doc, "event targets correct document");
  }

  let onCustomPropertyRegistered, evt;

  info("Register property and wait for event");
  onCustomPropertyRegistered = waitForCssCustomPropertyRegistered();
  content.CSS.registerProperty({ name: "--a", syntax: "*", inherits: false });
  evt = await onCustomPropertyRegistered;
  ok(true, `Received ${EVENT_NAME} event after registering --a`);
  checkCssCustomPropertyRegisteredEvent(evt);

  info("Register another property and wait for a new event");
  onCustomPropertyRegistered = waitForCssCustomPropertyRegistered();
  content.CSS.registerProperty({ name: "--b", syntax: "*", inherits: false });
  evt = await onCustomPropertyRegistered;
  ok(true, `Received ${EVENT_NAME} event after registering --b`);
  checkCssCustomPropertyRegisteredEvent(evt);

  info("Register existing property and assert that we don't get an event");
  onCustomPropertyRegistered = waitForCssCustomPropertyRegistered();
  const timeout = new Promise(resolve =>
    content.setTimeout(() => resolve("TIMEOUT"), 500)
  );
  try {
    content.CSS.registerProperty({ name: "--b", syntax: "*", inherits: false });
  } catch (e) {
    ok(true, "CSS.registerProperty threw");
  }
  const res = await Promise.race([onCustomPropertyRegistered, timeout]);
  is(
    res,
    "TIMEOUT",
    `Did not receive ${EVENT_NAME} event when registration failed`
  );
}
