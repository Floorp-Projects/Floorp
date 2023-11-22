/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the Web Console CSP messages for two META policies
// are correctly displayed. See Bug 1247459.

"use strict";

add_task(async function () {
  const TEST_URI =
    "data:text/html;charset=utf8,<!DOCTYPE html>Web Console CSP violation test";
  const hud = await openNewTabAndConsole(TEST_URI);
  await clearOutput(hud);
  {
    const TEST_VIOLATION =
      "https://example.com/browser/devtools/client/webconsole/" +
      "test/browser/test-csp-violation.html";
    const CSP_VIOLATION_MSG =
      "Content-Security-Policy: The page\u2019s settings " +
      "blocked the loading of a resource at " +
      "http://some.example.com/test.png (\u201cimg-src\u201d).";
    const onRepeatedMessage = waitForRepeatedMessageByType(
      hud,
      CSP_VIOLATION_MSG,
      ".error",
      2
    );
    await navigateTo(TEST_VIOLATION);
    await onRepeatedMessage;
    ok(true, "Received expected messages");
  }
  await clearOutput(hud);
  // Testing CSP Inline Violations
  {
    const TEST_VIOLATION =
      "https://example.com/browser/devtools/client/webconsole/" +
      "test/browser/test-csp-violation-inline.html";
    const CSP_VIOLATION =
      `Content-Security-Policy: The page’s settings blocked` +
      ` the loading of a resource at inline (“style-src”).`;
    const VIOLATION_LOCATION_HTML = "test-csp-violation-inline.html:18:1";
    const VIOLATION_LOCATION_JS = "test-csp-violation-inline.html:14:25";
    await navigateTo(TEST_VIOLATION);
    // Triggering the Violation via HTML
    let msg = await waitFor(() => findErrorMessage(hud, CSP_VIOLATION));
    let locationNode = msg.querySelector(".message-location");
    info(`EXPECT ${VIOLATION_LOCATION_HTML} GOT: ${locationNode.textContent}`);
    ok(
      locationNode.textContent == VIOLATION_LOCATION_HTML,
      "Printed the CSP Violation with HTML Context"
    );
    // Triggering the Violation via JS
    await clearOutput(hud);
    msg = await executeAndWaitForErrorMessage(
      hud,
      "window.violate()",
      CSP_VIOLATION
    );
    locationNode = msg.node.querySelector(".message-location");
    info(`EXPECT ${VIOLATION_LOCATION_JS} GOT: ${locationNode.textContent}`);
    ok(
      locationNode.textContent == VIOLATION_LOCATION_JS,
      "Printed the CSP Violation with JS Context"
    );
  }
  await clearOutput(hud);
  // Testing Base URI
  {
    const TEST_VIOLATION =
      "https://example.com/browser/devtools/client/webconsole/" +
      "test/browser/test-csp-violation-base-uri.html";
    const CSP_VIOLATION = `Content-Security-Policy: The page’s settings blocked the loading of a resource at https://evil.com/ (“base-uri”).`;
    const VIOLATION_LOCATION = "test-csp-violation-base-uri.html:15:25";
    await navigateTo(TEST_VIOLATION);
    let msg = await waitFor(() => findErrorMessage(hud, CSP_VIOLATION));
    ok(msg, "Base-URI validation was Printed");
    // Triggering the Violation via JS
    await clearOutput(hud);
    msg = await executeAndWaitForErrorMessage(
      hud,
      "window.violate()",
      CSP_VIOLATION
    );
    const locationNode = msg.node.querySelector(".message-location");
    console.log(locationNode.textContent);
    ok(
      locationNode.textContent == VIOLATION_LOCATION,
      "Base-URI validation was Printed with the Responsible JS Line"
    );
  }
  await clearOutput(hud);
  // Testing CSP Form Action
  {
    const TEST_VIOLATION =
      "https://example.com/browser/devtools/client/webconsole/" +
      "test/browser/test-csp-violation-form-action.html";
    const CSP_VIOLATION = `Content-Security-Policy: The page’s settings blocked the loading of a resource at https://evil.com/evil.com (“form-action”).`;
    const VIOLATION_LOCATION = "test-csp-violation-form-action.html:14:40";

    await navigateTo(TEST_VIOLATION);
    const msg = await waitFor(() => findErrorMessage(hud, CSP_VIOLATION));
    const locationNode = msg.querySelector(".message-location");
    info(`EXPECT ${VIOLATION_LOCATION} GOT: ${locationNode.textContent}`);
    ok(
      locationNode.textContent == VIOLATION_LOCATION,
      "JS Line which Triggered the CSP-Form Action Violation was Printed"
    );
  }
  await clearOutput(hud);
  // Testing CSP Frame Ancestors Directive
  {
    const TEST_VIOLATION =
      "https://example.com/browser/devtools/client/webconsole/" +
      "test/browser/test-csp-violation-frame-ancestor-parent.html";
    const CSP_VIOLATION =
      `Content-Security-Policy: The page’s settings blocked` +
      ` the loading of a resource at ${TEST_VIOLATION} (“frame-ancestors”).`;
    await navigateTo(TEST_VIOLATION);
    const msg = await waitFor(() => findErrorMessage(hud, CSP_VIOLATION));
    ok(msg, "Frame-Ancestors violation by html was printed");
  }
  await clearOutput(hud);
  // Testing CSP inline event handler violations
  {
    const TEST_VIOLATION =
      "https://example.com/browser/devtools/client/webconsole/" +
      "test/browser/test-csp-violation-event-handler.html";
    const CSP_VIOLATION = `Content-Security-Policy: The page’s settings blocked the loading of a resource at inline (“script-src”).
Source: document.body.textContent = 'JavaScript …`;
    // Future-Todo: Include line and column number.
    const VIOLATION_LOCATION = "test-csp-violation-event-handler.html";
    await navigateTo(TEST_VIOLATION);
    const msg = await waitFor(() => findErrorMessage(hud, CSP_VIOLATION));
    const locationNode = msg.querySelector(".message-location");
    is(
      locationNode.textContent,
      VIOLATION_LOCATION,
      "Inline event handler location doesn't yet include the line/column"
    );
  }
});
