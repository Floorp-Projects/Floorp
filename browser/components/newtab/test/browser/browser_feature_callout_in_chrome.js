/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { FeatureCalloutMessages } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/FeatureCalloutMessages.sys.mjs"
);
const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  FeatureCalloutBroker:
    "resource://activity-stream/lib/FeatureCalloutBroker.sys.mjs",
});

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

const pdfMatch = sinon.match(
  val =>
    val?.id === "pdfJsFeatureCalloutCheck" && val?.context?.source === "open"
);

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
    trigger: { id: "pdfJsFeatureCalloutCheck" },
  },
};

const testMessageCalloutSelector = testMessage.message.content.screens[0].id;

add_setup(async function () {
  requestLongerTimeout(2);
});

// Test that a feature callout message can be loaded into ASRouter and displayed
// via a standard trigger. Also test that the callout can be a feature tour,
// even if its tour pref doesn't exist in Firefox. The tour pref will be created
// and cleaned up automatically. This allows a feature callout to be implemented
// entirely off-train in an experiment, without landing anything in tree.
add_task(async function triggered_feature_tour_with_custom_pref() {
  let sandbox = sinon.createSandbox();
  const TEST_MESSAGES = [
    {
      id: "TEST_FEATURE_TOUR",
      template: "feature_callout",
      content: {
        id: "TEST_FEATURE_TOUR",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        tour_pref_name: "messaging-system-action.browser.test.feature-tour",
        tour_pref_default_value: JSON.stringify({
          screen: "FEATURE_CALLOUT_1",
          complete: false,
        }),
        screens: [
          {
            id: "FEATURE_CALLOUT_1",
            parent_selector: "#PanelUI-menu-button",
            content: {
              position: "callout",
              arrow_position: "top-center-arrow-end",
              title: {
                string_id: "callout-pdfjs-edit-title",
              },
              subtitle: {
                string_id: "callout-pdfjs-edit-body-b",
              },
              primary_button: {
                label: {
                  string_id: "callout-pdfjs-edit-button",
                },
                action: {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: "messaging-system-action.browser.test.feature-tour",
                      value: JSON.stringify({
                        screen: "FEATURE_CALLOUT_2",
                        complete: false,
                      }),
                    },
                  },
                },
              },
              dismiss_button: {
                action: {
                  type: "MULTI_ACTION",
                  dismiss: true,
                  data: {
                    actions: [
                      {
                        type: "BLOCK_MESSAGE",
                        data: { id: "TEST_FEATURE_TOUR" },
                      },
                      {
                        type: "SET_PREF",
                        data: {
                          pref: {
                            name: "messaging-system-action.browser.test.feature-tour",
                          },
                        },
                      },
                    ],
                  },
                },
              },
            },
          },
          {
            id: "FEATURE_CALLOUT_2",
            parent_selector: "#back-button",
            content: {
              position: "callout",
              arrow_position: "top-center-arrow-start",
              title: {
                string_id: "callout-pdfjs-draw-title",
              },
              subtitle: {
                string_id: "callout-pdfjs-draw-body-b",
              },
              primary_button: {
                label: {
                  string_id: "callout-pdfjs-draw-button",
                },
                action: {
                  type: "MULTI_ACTION",
                  dismiss: true,
                  data: {
                    actions: [
                      {
                        type: "BLOCK_MESSAGE",
                        data: { id: "TEST_FEATURE_TOUR" },
                      },
                      {
                        type: "SET_PREF",
                        data: {
                          pref: {
                            name: "messaging-system-action.browser.test.feature-tour",
                          },
                        },
                      },
                    ],
                  },
                },
              },
              dismiss_button: {
                action: {
                  type: "MULTI_ACTION",
                  dismiss: true,
                  data: {
                    actions: [
                      {
                        type: "BLOCK_MESSAGE",
                        data: { id: "TEST_FEATURE_TOUR" },
                      },
                      {
                        type: "SET_PREF",
                        data: {
                          pref: {
                            name: "messaging-system-action.browser.test.feature-tour",
                          },
                        },
                      },
                    ],
                  },
                },
              },
            },
          },
        ],
      },
      priority: 2,
      targeting: `(('messaging-system-action.browser.test.feature-tour' | preferenceValue) ? (('messaging-system-action.browser.test.feature-tour' | preferenceValue | regExpMatch('(?<=complete":)(.*)(?=})')) ? ('messaging-system-action.browser.test.feature-tour' | preferenceValue | regExpMatch('(?<=complete":)(.*)(?=})')[1] != "true") : true) : true)`,
      trigger: { id: "nthTabClosed" },
    },
    {
      id: "TEST_FEATURE_TOUR_2",
      template: "feature_callout",
      content: {
        id: "TEST_FEATURE_TOUR_2",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "FEATURE_CALLOUT_TEST",
            parent_selector: "#urlbar-container",
            content: {
              position: "callout",
              arrow_position: "top-center-arrow-end",
              title: {
                string_id: "callout-pdfjs-edit-title",
              },
              subtitle: {
                string_id: "callout-pdfjs-edit-body-b",
              },
              primary_button: {
                label: {
                  string_id: "callout-pdfjs-edit-button",
                },
                action: {
                  dismiss: true,
                },
              },
              dismiss_button: {
                action: {
                  dismiss: true,
                },
              },
            },
          },
        ],
      },
      priority: 1,
      targeting: "true",
      trigger: { id: "nthTabClosed" },
    },
  ];
  const getMessagesStub = sandbox.stub(FeatureCalloutMessages, "getMessages");
  getMessagesStub.returns(TEST_MESSAGES);
  await ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders(
    ASRouter.state.providers.filter(p => p.id === "onboarding")
  );

  // Test that callout is triggered and shown in browser chrome
  const win1 = await BrowserTestUtils.openNewBrowserWindow();
  win1.focus();
  const tab1 = await BrowserTestUtils.openNewForegroundTab(win1.gBrowser);
  win1.gBrowser.removeTab(tab1);
  await waitForCalloutScreen(
    win1.document,
    TEST_MESSAGES[0].content.screens[0].id
  );
  ok(
    win1.document.querySelector(calloutSelector),
    "Feature Callout is rendered in the browser chrome when a message is available"
  );

  // Test that a callout does NOT appear if another is already shown in any window.
  const showFeatureCalloutSpy = sandbox.spy(
    lazy.FeatureCalloutBroker,
    "showFeatureCallout"
  );
  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  win2.focus();
  const tab2 = await BrowserTestUtils.openNewForegroundTab(win2.gBrowser);
  win2.gBrowser.removeTab(tab2);
  await BrowserTestUtils.waitForCondition(async () => {
    const rvs = await Promise.all(showFeatureCalloutSpy.returnValues);
    return (
      showFeatureCalloutSpy.calledWith(
        win2.gBrowser.selectedBrowser,
        sinon.match(TEST_MESSAGES[0])
      ) && rvs.every(rv => !rv)
    );
  }, "Waiting for showFeatureCallout to be called");
  ok(
    !win2.document.querySelector(calloutSelector),
    "Feature Callout is not rendered when a callout is already shown in any window"
  );
  await BrowserTestUtils.closeWindow(win2);
  win1.focus();
  await BrowserTestUtils.waitForCondition(
    async () => Services.focus.activeWindow === win1,
    "Waiting for window 1 to be active"
  );

  // Test that the tour pref doesn't exist yet
  ok(
    !Services.prefs.prefHasUserValue(TEST_MESSAGES[0].content.tour_pref_name),
    "Tour pref does not exist yet"
  );

  // Test that the callout advances screen and sets the tour pref
  win1.document.querySelector(primaryButtonSelector).click();
  await BrowserTestUtils.waitForCondition(
    () =>
      Services.prefs.prefHasUserValue(TEST_MESSAGES[0].content.tour_pref_name),
    "Waiting for tour pref to be set"
  );
  ok(true, "Tour pref is set");
  await waitForCalloutScreen(
    win1.document,
    TEST_MESSAGES[0].content.screens[1].id
  );
  ok(
    win1.document.querySelector(calloutSelector),
    "Feature Callout screen 2 is rendered"
  );
  SimpleTest.isDeeply(
    JSON.parse(
      Services.prefs.getStringPref(
        TEST_MESSAGES[0].content.tour_pref_name,
        "{}"
      )
    ),
    { screen: "FEATURE_CALLOUT_2", complete: false },
    "Tour pref is set correctly"
  );

  // Test that the callout is dismissed and cleans up the tour pref
  win1.document.querySelector(primaryButtonSelector).click();
  await waitForRemoved(win1.document);
  ok(
    !win1.document.querySelector(calloutSelector),
    "Feature Callout is not rendered after being dismissed"
  );
  ok(
    !Services.prefs.prefHasUserValue(TEST_MESSAGES[0].content.tour_pref_name),
    "Tour pref is cleaned up correctly"
  );

  // Test that the message was blocked so a different callout is shown
  const tab3 = await BrowserTestUtils.openNewForegroundTab(win1.gBrowser);
  win1.gBrowser.removeTab(tab3);
  await waitForCalloutScreen(
    win1.document,
    TEST_MESSAGES[1].content.screens[0].id
  );
  ok(
    win1.document.querySelector(calloutSelector),
    "A different Feature Callout is rendered"
  );
  win1.document.querySelector(primaryButtonSelector).click();
  await waitForRemoved(win1.document);
  ok(
    !lazy.FeatureCalloutBroker.isCalloutShowing,
    "No Feature Callout is shown"
  );

  BrowserTestUtils.closeWindow(win1);

  sandbox.restore();
  await ASRouter.unblockMessageById(TEST_MESSAGES[0].id);
  await ASRouter.resetMessageState();
  await ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders(
    ASRouter.state.providers.filter(p => p.id === "onboarding")
  );
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
    pdfTestMessage.message.content.screens[0].content.callout_position_override =
      {
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
    pdfTestMessage.message.content.screens[0].content.callout_position_override =
      {
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
