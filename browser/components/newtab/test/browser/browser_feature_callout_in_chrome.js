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

async function openURLInNewTab(window, url) {
  return BrowserTestUtils.openNewForegroundTab(window.gBrowser, url);
}

const pdfMatch = sinon.match(val => {
  return (
    val?.id === "featureCalloutCheck" && val?.context?.source === PDF_TEST_URL
  );
});

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

add_task(async function feature_callout_renders_in_browser_chrome_for_pdf() {
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

add_task(
  async function feature_callout_renders_and_hides_in_chrome_when_switching_tabs() {
    const sandbox = sinon.createSandbox();
    const sendTriggerStub = sandbox.stub(ASRouter, "sendTriggerMessage");
    sendTriggerStub.withArgs(pdfMatch).resolves(testMessage);
    sendTriggerStub.callThrough();

    let win = await BrowserTestUtils.openNewBrowserWindow();

    // Test that callout shows up on first PDF tab
    let doc = win.document;
    let tab1 = await BrowserTestUtils.openNewForegroundTab(
      win.gBrowser,
      PDF_TEST_URL
    );
    tab1.focus();
    await waitForCalloutScreen(doc, testMessage.message.content.screens[0].id);
    ok(
      doc.querySelector(`.${testMessage.message.content.screens[0].id}`),
      "Feature callout rendered when opening a new tab with PDF url"
    );

    // Test that callout is removed when swapping tabs
    let tab2 = await openURLInNewTab(win, "about:preferences");
    tab2.focus();
    // We hang up on newtab around here
    await BrowserTestUtils.waitForCondition(() => {
      return !doc.body.querySelector("#root.featureCallout");
    });

    ok(
      !doc.querySelector(`.${testMessage.message.content.screens[0].id}`),
      "Feature callout removed when tab without PDF URL is navigated to"
    );

    // Test that callout shows up when tabbing back to pdf
    let tab3 = await openURLInNewTab(win, PDF_TEST_URL);
    tab3.focus();
    await waitForCalloutScreen(doc, testMessage.message.content.screens[0].id);
    ok(
      doc.querySelector(`.${testMessage.message.content.screens[0].id}`),
      "Feature callout still renders when opening a new tab with PDF url after being initially rendered on another tab"
    );

    // Test that callout remains when tabbing from one pdf tab to another
    tab1.focus();
    await waitForCalloutScreen(doc, testMessage.message.content.screens[0].id);
    ok(
      doc.querySelector(`.${testMessage.message.content.screens[0].id}`),
      "Feature callout rendered on original tab after switching tabs multiple times"
    );

    await BrowserTestUtils.closeWindow(win);
    sandbox.restore();
  }
);
