/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const { ASRouterTargeting } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTargeting.jsm"
);

const { OnboardingMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/OnboardingMessageProvider.jsm"
);

async function waitForClick(selector, win) {
  await TestUtils.waitForCondition(() => win.document.querySelector(selector));
  win.document.querySelector(selector).click();
}
add_task(async function test_foxdoodle_spotlight() {
  const sandbox = sinon.createSandbox();

  let promise = TestUtils.topicObserved("subdialog-loaded");
  let message = (await OnboardingMessageProvider.getMessages()).find(
    m => m.id === "FOX_DOODLE_SET_DEFAULT"
  );

  Assert.ok(message, "Message exists.");

  let routedMessage = ASRouter.routeCFRMessage(
    message,
    gBrowser,
    undefined,
    false
  );

  Assert.ok(
    JSON.stringify(routedMessage) === JSON.stringify({ message: {} }),
    "Message is not routed when skipInTests is truthy and ID is not present in messagesEnabledInAutomation"
  );

  sandbox
    .stub(ASRouter, "messagesEnabledInAutomation")
    .value(["FOX_DOODLE_SET_DEFAULT"]);

  routedMessage = ASRouter.routeCFRMessage(message, gBrowser, undefined, false);
  Assert.ok(
    JSON.stringify(routedMessage.message) === JSON.stringify(message),
    "Message is routed when skipInTests is truthy and ID is present in messagesEnabledInAutomation"
  );

  delete message.skipInTests;
  let unskippedRoutedMessage = ASRouter.routeCFRMessage(
    message,
    gBrowser,
    undefined,
    false
  );
  Assert.ok(
    unskippedRoutedMessage,
    "Message is routed when skipInTests property is falsy"
  );
  let [win] = await promise;
  await waitForClick("button.dismiss-button", win);
  win.close();
  sandbox.restore();
});
