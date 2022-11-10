/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const calloutId = "root";
const calloutSelector = `#${calloutId}.featureCallout`;
const primaryButtonSelector = `#${calloutId} .primary`;
const PDF_TEST_URL = "https://example.com/some.pdf";

const waitForCalloutScreen = async (doc, screenId) => {
  await BrowserTestUtils.waitForCondition(() => {
    return doc.querySelector(`${calloutSelector}:not(.hidden) .${screenId}`);
  });
};

const waitForRemoved = async doc => {
  await BrowserTestUtils.waitForCondition(() => {
    return !document.querySelector(calloutSelector);
  });
};

async function openURLInWindow(window, url) {
  const { selectedBrowser } = window.gBrowser;
  BrowserTestUtils.loadURI(selectedBrowser, url);
  await BrowserTestUtils.browserLoaded(selectedBrowser, false, url);
}

const pdfMatch = sinon.match(val => {
  return (
    val?.id === "featureCalloutCheck" && val?.context?.source === PDF_TEST_URL
  );
});

add_task(async function feature_callout_renders_in_browser_chrome_for_pdf() {
  const testMessage = {
    message: {
      id: "TEST_MESSAGE",
      template: "feature_callout",
      content: {
        id: "TEST_MESSAGE",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        screens: [
          {
            id: "TEST_MESSAGE_1",
            parent_selector: "#urlbar-container",
            content: {
              position: "callout",
              arrow_position: "top-end",
              title: {
                raw: "Test title",
              },
              subtitle: {
                raw: "Test subtitle",
              },
              primary_button: {
                label: {
                  raw: "Done",
                },
                action: {
                  navigate: true,
                },
              },
            },
          },
        ],
      },
      priority: 1,
      targeting: "true",
      trigger: { id: "featureCalloutCheck" },
    },
  };

  const sandbox = sinon.createSandbox();
  const sendTriggerStub = sandbox.stub(ASRouter, "sendTriggerMessage");
  sendTriggerStub.withArgs(pdfMatch).resolves(testMessage);
  sendTriggerStub.callThrough();

  let win = await BrowserTestUtils.openNewBrowserWindow();
  await openURLInWindow(win, PDF_TEST_URL);
  let doc = win.document;
  await waitForCalloutScreen(doc, testMessage.message.content.screens[0].id);
  let container = doc.querySelector(calloutSelector);
  ok(
    container,
    "Feature Callout is rendered in the browser chrome with a new window when a message is available"
  );

  // click primary button to close
  doc.querySelector(primaryButtonSelector).click();
  await waitForRemoved();
  ok(
    true,
    "Feature callout removed from browser chrome after clicking button configured to navigate"
  );

  await BrowserTestUtils.closeWindow(win);
  sandbox.restore();
});
