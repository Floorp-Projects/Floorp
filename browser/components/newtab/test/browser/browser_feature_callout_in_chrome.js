/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const calloutId = "multi-stage-message-root";
const calloutSelector = `#${calloutId}.featureCallout`;
const primaryButtonSelector = `#${calloutId} .primary`;
const PDF_TEST_URL =
  "https://example.com/browser/browser/components/newtab/test/browser/file_pdf.PDF";

const waitForCalloutScreen = async (doc, screenId) => {
  await BrowserTestUtils.waitForCondition(() => {
    return doc.querySelector(`${calloutSelector}:not(.hidden) .${screenId}`);
  });
};

const waitForRemoved = async doc => {
  await BrowserTestUtils.waitForCondition(() => {
    return !doc.querySelector(calloutSelector);
  });
};

async function openURLInWindow(window, url) {
  const { selectedBrowser } = window.gBrowser;
  BrowserTestUtils.loadURIString(selectedBrowser, url);
  await BrowserTestUtils.browserLoaded(selectedBrowser, false, url);
}

async function openURLInNewTab(window, url) {
  return BrowserTestUtils.openNewForegroundTab(window.gBrowser, url);
}

const pdfMatch = sinon.match(val => {
  return val?.id === "featureCalloutCheck" && val?.context?.source === "chrome";
});

const validateCalloutCustomPosition = (element, positionOverride, doc) => {
  const browserBox = doc.querySelector("hbox#browser");
  for (let position in positionOverride) {
    if (Object.prototype.hasOwnProperty.call(positionOverride, position)) {
      // The substring here is to remove the `px` at the end of our position override strings
      const relativePos = positionOverride[position].substring(
        0,
        positionOverride[position].length - 2
      );
      const elPos = element.getBoundingClientRect()[position];
      const browserPos = browserBox.getBoundingClientRect()[position];

      if (position in ["top", "left"]) {
        if (elPos !== browserPos + relativePos) {
          return false;
        }
      } else if (position in ["right", "bottom"]) {
        if (elPos !== browserPos - relativePos) {
          return false;
        }
      }
    }
  }
  return true;
};

const validateCalloutRTLPosition = (element, positionOverride) => {
  for (let position in positionOverride) {
    if (Object.prototype.hasOwnProperty.call(positionOverride, position)) {
      const pixelPosition = positionOverride[position];
      if (position === "left") {
        const actualLeft = Number(
          pixelPosition.substring(0, pixelPosition.length - 2)
        );
        if (element.getBoundingClientRect().right !== actualLeft) {
          return false;
        }
      } else if (position === "right") {
        const expectedLeft = Number(
          pixelPosition.substring(0, pixelPosition.length - 2)
        );
        if (element.getBoundingClientRect().left !== expectedLeft) {
          return false;
        }
      }
    }
  }
  return true;
};

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

const testMessageCalloutSelector = testMessage.message.content.screens[0].id;

add_setup(async function() {
  requestLongerTimeout(2);
});

add_task(async function feature_callout_renders_in_browser_chrome_for_pdf() {
  const sandbox = sinon.createSandbox();
  const sendTriggerStub = sandbox.stub(ASRouter, "sendTriggerMessage");
  sendTriggerStub.withArgs(pdfMatch).resolves(testMessage);
  sendTriggerStub.callThrough();

  const win = await BrowserTestUtils.openNewBrowserWindow();
  await openURLInWindow(win, PDF_TEST_URL);
  const doc = win.document;
  await waitForCalloutScreen(doc, testMessageCalloutSelector);
  const container = doc.querySelector(calloutSelector);
  ok(
    container,
    "Feature Callout is rendered in the browser chrome with a new window when a message is available"
  );

  // click primary button to close
  doc.querySelector(primaryButtonSelector).click();
  await waitForRemoved(doc);
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

    const win = await BrowserTestUtils.openNewBrowserWindow();

    const doc = win.document;
    const tab1 = await BrowserTestUtils.openNewForegroundTab(
      win.gBrowser,
      PDF_TEST_URL
    );
    tab1.focus();
    await waitForCalloutScreen(doc, testMessageCalloutSelector);
    ok(
      doc.querySelector(`.${testMessageCalloutSelector}`),
      "Feature callout rendered when opening a new tab with PDF url"
    );

    const tab2 = await openURLInNewTab(win, "about:preferences");
    tab2.focus();
    await BrowserTestUtils.waitForCondition(() => {
      return !doc.body.querySelector(
        "#multi-stage-message-root.featureCallout"
      );
    });

    ok(
      !doc.querySelector(`.${testMessageCalloutSelector}`),
      "Feature callout removed when tab without PDF URL is navigated to"
    );

    const tab3 = await openURLInNewTab(win, PDF_TEST_URL);
    tab3.focus();
    await waitForCalloutScreen(doc, testMessageCalloutSelector);
    ok(
      doc.querySelector(`.${testMessageCalloutSelector}`),
      "Feature callout still renders when opening a new tab with PDF url after being initially rendered on another tab"
    );

    tab1.focus();
    await waitForCalloutScreen(doc, testMessageCalloutSelector);
    ok(
      doc.querySelector(`.${testMessageCalloutSelector}`),
      "Feature callout rendered on original tab after switching tabs multiple times"
    );

    await BrowserTestUtils.closeWindow(win);
    sandbox.restore();
  }
);

add_task(
  async function feature_callout_disappears_when_navigating_to_non_pdf_url_in_same_tab() {
    const sandbox = sinon.createSandbox();
    const sendTriggerStub = sandbox.stub(ASRouter, "sendTriggerMessage");
    sendTriggerStub.withArgs(pdfMatch).resolves(testMessage);
    sendTriggerStub.callThrough();

    const win = await BrowserTestUtils.openNewBrowserWindow();

    const doc = win.document;
    const tab1 = await BrowserTestUtils.openNewForegroundTab(
      win.gBrowser,
      PDF_TEST_URL
    );
    tab1.focus();
    await waitForCalloutScreen(doc, testMessageCalloutSelector);
    ok(
      doc.querySelector(`.${testMessageCalloutSelector}`),
      "Feature callout rendered when opening a new tab with PDF url"
    );

    BrowserTestUtils.loadURIString(win.gBrowser, "about:preferences");
    await BrowserTestUtils.waitForLocationChange(
      win.gBrowser,
      "about:preferences"
    );
    await waitForRemoved(doc);

    ok(
      !doc.querySelector(`.${testMessageCalloutSelector}`),
      "Feature callout not rendered on original tab after navigating to non pdf URL"
    );

    await BrowserTestUtils.closeWindow(win);
    sandbox.restore();
  }
);

add_task(
  async function feature_callout_disappears_when_closing_foreground_pdf_tab() {
    const sandbox = sinon.createSandbox();
    const sendTriggerStub = sandbox.stub(ASRouter, "sendTriggerMessage");
    sendTriggerStub.withArgs(pdfMatch).resolves(testMessage);
    sendTriggerStub.callThrough();

    const win = await BrowserTestUtils.openNewBrowserWindow();

    const doc = win.document;
    const tab1 = await BrowserTestUtils.openNewForegroundTab(
      win.gBrowser,
      PDF_TEST_URL
    );
    tab1.focus();
    await waitForCalloutScreen(doc, testMessageCalloutSelector);
    ok(
      doc.querySelector(`.${testMessageCalloutSelector}`),
      "Feature callout rendered when opening a new tab with PDF url"
    );

    BrowserTestUtils.removeTab(tab1);
    await waitForRemoved(doc);

    ok(
      !doc.querySelector(`.${testMessageCalloutSelector}`),
      "Feature callout disappears after closing foreground tab"
    );

    await BrowserTestUtils.closeWindow(win);
    sandbox.restore();
  }
);

add_task(
  async function feature_callout_does_not_appear_when_opening_background_pdf_tab() {
    const sandbox = sinon.createSandbox();
    const sendTriggerStub = sandbox.stub(ASRouter, "sendTriggerMessage");
    sendTriggerStub.withArgs(pdfMatch).resolves(testMessage);
    sendTriggerStub.callThrough();

    const win = await BrowserTestUtils.openNewBrowserWindow();
    const doc = win.document;

    const tab1 = await BrowserTestUtils.addTab(win.gBrowser, PDF_TEST_URL);
    ok(
      !doc.querySelector(`.${testMessageCalloutSelector}`),
      "Feature callout not rendered when opening a background tab with PDF url"
    );

    BrowserTestUtils.removeTab(tab1);

    ok(
      !doc.querySelector(`.${testMessageCalloutSelector}`),
      "Feature callout still not rendered after closing background tab with PDF url"
    );

    await BrowserTestUtils.closeWindow(win);
    sandbox.restore();
  }
);

add_task(
  async function feature_callout_is_positioned_relative_to_browser_window() {
    // Deep copying our test message so we can alter it without disrupting future tests
    const pdfTestMessage = JSON.parse(JSON.stringify(testMessage));
    const pdfTestMessageCalloutSelector =
      pdfTestMessage.message.content.screens[0].id;

    pdfTestMessage.message.content.screens[0].parent_selector = "hbox#browser";
    pdfTestMessage.message.content.screens[0].content.callout_position_override = {
      top: "45px",
      right: "25px",
    };

    const sandbox = sinon.createSandbox();
    const sendTriggerStub = sandbox.stub(ASRouter, "sendTriggerMessage");
    sendTriggerStub.withArgs(pdfMatch).resolves(pdfTestMessage);
    sendTriggerStub.callThrough();

    const win = await BrowserTestUtils.openNewBrowserWindow();
    await openURLInWindow(win, PDF_TEST_URL);
    const doc = win.document;
    await waitForCalloutScreen(doc, pdfTestMessageCalloutSelector);

    // Verify that callout renders in appropriate position (without infobar element)
    const callout = doc.querySelector(`.${pdfTestMessageCalloutSelector}`);
    ok(callout, "Callout is rendered when navigating to PDF file");
    ok(
      validateCalloutCustomPosition(
        callout,
        pdfTestMessage.message.content.screens[0].content
          .callout_position_override,
        doc
      ),
      "Callout custom position is as expected"
    );

    // Add height to the top of the browser to simulate an infobar or other element
    const navigatorToolBox = doc.querySelector("#navigator-toolbox-background");
    navigatorToolBox.style.height = "150px";
    // We test in a new tab because the callout does not adjust itself
    // when size of the navigator-toolbox-background box changes.
    const tab = await openURLInNewTab(win, "https://example.com/some2.pdf");
    // Verify that callout renders in appropriate position (with infobar element displayed)
    ok(
      validateCalloutCustomPosition(
        callout,
        pdfTestMessage.message.content.screens[0].content
          .callout_position_override,
        doc
      ),
      "Callout custom position is as expected while navigator toolbox height is extended"
    );
    BrowserTestUtils.removeTab(tab);
    await BrowserTestUtils.closeWindow(win);
    sandbox.restore();
  }
);

add_task(
  async function custom_position_callout_is_horizontally_reversed_in_rtl_layouts() {
    // Deep copying our test message so we can alter it without disrupting future tests
    const pdfTestMessage = JSON.parse(JSON.stringify(testMessage));
    const pdfTestMessageCalloutSelector =
      pdfTestMessage.message.content.screens[0].id;

    pdfTestMessage.message.content.screens[0].parent_selector = "hbox#browser";
    pdfTestMessage.message.content.screens[0].content.callout_position_override = {
      top: "45px",
      right: "25px",
    };

    const sandbox = sinon.createSandbox();
    const sendTriggerStub = sandbox.stub(ASRouter, "sendTriggerMessage");
    sendTriggerStub.withArgs(pdfMatch).resolves(pdfTestMessage);
    sendTriggerStub.callThrough();

    const win = await BrowserTestUtils.openNewBrowserWindow();
    win.document.dir = "rtl";
    ok(
      win.document.documentElement.getAttribute("dir") === "rtl",
      "browser window is in RTL"
    );

    await openURLInWindow(win, PDF_TEST_URL);
    const doc = win.document;
    await waitForCalloutScreen(doc, pdfTestMessageCalloutSelector);

    const callout = doc.querySelector(`.${pdfTestMessageCalloutSelector}`);
    ok(callout, "Callout is rendered when navigating to PDF file");
    ok(
      validateCalloutRTLPosition(
        callout,
        pdfTestMessage.message.content.screens[0].content
          .callout_position_override
      ),
      "Callout custom position is rendered appropriately in RTL mode"
    );

    await BrowserTestUtils.closeWindow(win);
    sandbox.restore();
  }
);

add_task(async function feature_callout_dismissed_on_escape() {
  const sandbox = sinon.createSandbox();
  const sendTriggerStub = sandbox.stub(ASRouter, "sendTriggerMessage");
  sendTriggerStub.withArgs(pdfMatch).resolves(testMessage);
  sendTriggerStub.callThrough();

  const win = await BrowserTestUtils.openNewBrowserWindow();
  await openURLInWindow(win, PDF_TEST_URL);
  const doc = win.document;
  await waitForCalloutScreen(doc, testMessageCalloutSelector);
  const container = doc.querySelector(calloutSelector);
  ok(
    container,
    "Feature Callout is rendered in the browser chrome with a new window when a message is available"
  );

  // Ensure the browser is focused
  win.gBrowser.selectedBrowser.focus();

  // Press Escape to close
  EventUtils.synthesizeKey("KEY_Escape", {}, win);
  await waitForRemoved(doc);
  ok(true, "Feature callout dismissed after pressing Escape");

  await BrowserTestUtils.closeWindow(win);
  sandbox.restore();
});

add_task(
  async function feature_callout_not_dismissed_on_escape_with_interactive_elm_focused() {
    const sandbox = sinon.createSandbox();
    const sendTriggerStub = sandbox.stub(ASRouter, "sendTriggerMessage");
    sendTriggerStub.withArgs(pdfMatch).resolves(testMessage);
    sendTriggerStub.callThrough();

    const win = await BrowserTestUtils.openNewBrowserWindow();
    await openURLInWindow(win, PDF_TEST_URL);
    const doc = win.document;
    await waitForCalloutScreen(doc, testMessageCalloutSelector);
    const container = doc.querySelector(calloutSelector);
    ok(
      container,
      "Feature Callout is rendered in the browser chrome with a new window when a message is available"
    );

    // Ensure an interactive element is focused
    win.gURLBar.focus();

    // Press Escape to close
    EventUtils.synthesizeKey("KEY_Escape", {}, win);
    await TestUtils.waitForTick();
    // Wait 500ms for transition to complete
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 500));
    ok(
      doc.querySelector(calloutSelector),
      "Feature callout is not dismissed after pressing Escape because an interactive element is focused"
    );

    await BrowserTestUtils.closeWindow(win);
    sandbox.restore();
  }
);
