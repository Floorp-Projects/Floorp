/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function getTestMessage() {
  return {
    id: "TEST_PANEL_FEATURE_CALLOUT",
    template: "feature_callout",
    groups: [],
    content: {
      id: "TEST_PANEL_FEATURE_CALLOUT",
      template: "multistage",
      backdrop: "transparent",
      transitions: false,
      disableHistoryUpdates: true,
      screens: [
        {
          id: "TEST_PANEL_FEATURE_CALLOUT",
          anchors: [
            {
              selector: "#PanelUI-menu-button",
              panel_position: {
                anchor_attachment: "bottomcenter",
                callout_attachment: "topright",
              },
            },
          ],
          content: {
            position: "callout",
            title: { raw: "Panel Feature Callout" },
            dismiss_button: {
              action: { dismiss: true },
            },
          },
        },
      ],
    },
  };
}

/**
 * Set up a callout and show it.
 *
 * @param {MozBrowser} browser Probably the selected browser in the top window.
 * @param {object} message The message to show.
 * @returns {Promise<{featureCallout: FeatureCallout, showing: boolean, closed: Promise}>}
 *   A promise that resolves to an object containing the FeatureCallout
 *   instance, a boolean for whether the callout started showing correctly, and
 *   a promise that resolves when the callout is closed.
 */
async function showFeatureCallout(browser, message) {
  let resolveClosed;
  let closed = new Promise(resolve => {
    resolveClosed = resolve;
  });
  const config = {
    win: browser.ownerGlobal,
    location: "chrome",
    context: "chrome",
    browser,
    theme: { preset: "chrome" },
    listener: (_, event) => {
      if (event === "end") {
        resolveClosed();
      }
    },
  };
  const featureCallout = new FeatureCallout(config);
  let showing = await featureCallout.showFeatureCallout(message);
  return { featureCallout, showing, closed };
}

/**
 * Make a new window, open a feature callout in it, run a function to hide the
 * callout, and assert that the callout is hidden correctly. Optionally run a
 * function after the callout is closed, for additional assertions. Finally,
 * close the window.
 *
 * @param {function(Window, Element, FeatureCallout)} hideFn A function that
 *   hides the callout. Passed the following params: window, callout container,
 *   and FeatureCallout instance.
 * @param {function(Window, Element, FeatureCallout)} afterCloseFn An optional
 *   function to run after the callout is closed. Same params as hideFn.
 * @param {object} message The message to show.
 */
async function testCalloutHiddenIf(
  hideFn,
  afterCloseFn,
  message = getTestMessage()
) {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const doc = win.document;
  const browser = win.gBrowser.selectedBrowser;
  const { featureCallout, showing, closed } = await showFeatureCallout(
    browser,
    message
  );

  await waitForCalloutScreen(doc, message.content.screens[0].id);
  let calloutContainer = featureCallout._container;
  ok(showing && calloutContainer, "Feature callout should be showing");

  await hideFn(win, calloutContainer, featureCallout);

  await closed;
  await waitForCalloutRemoved(doc);
  ok(!doc.querySelector(calloutSelector), "Feature callout should be hidden");

  await afterCloseFn?.(win, calloutContainer, featureCallout);
  await BrowserTestUtils.closeWindow(win);
}

// Test that the callout is correctly created as a panel and positioned.
add_task(async function panel_feature_callout() {
  await testCalloutHiddenIf(async (win, calloutContainer) => {
    is(calloutContainer.localName, "panel", "Callout container is a panel");
    await BrowserTestUtils.waitForMutationCondition(
      calloutContainer,
      { attributeFilter: ["arrow-position"] },
      () => calloutContainer.getAttribute("arrow-position") === "top-end"
    );
    is(
      calloutContainer.anchorNode.id,
      "PanelUI-menu-button",
      "Callout container is anchored to the app menu button"
    );
    is(
      calloutContainer.getAttribute("arrow-position"),
      "top-end",
      "Callout container arrow is positioned correctly"
    );

    win.document.querySelector(calloutDismissSelector).click();
  });
});

// Test that the callout is hidden if another popup is shown.
add_task(async function panel_feature_callout_hidden_on_popupshowing() {
  await testCalloutHiddenIf(async win => {
    // Click the app menu button to open the panel.
    win.document.querySelector("#PanelUI-menu-button").click();
  });
});

// Test that the callout is hidden if its anchor node is hidden.
add_task(async function panel_feature_callout_hidden_on_anchor_hidden() {
  await testCalloutHiddenIf(async win => {
    // Hide the app menu button.
    win.document.querySelector("#PanelUI-menu-button").hidden = true;
  });
});

// Panels automatically track the movement of their anchor nodes, so test that
// the callout moves with its anchor node.
add_task(async function panel_feature_callout_follows_anchor() {
  await testCalloutHiddenIf(async (win, calloutContainer) => {
    let startingX = calloutContainer.getBoundingClientRect().x;

    // Move the app menu button away from the right edge of the window.
    calloutContainer.anchorNode.style.marginInlineEnd = "100px";

    // Wait for the callout to reposition itself.
    await BrowserTestUtils.waitForCondition(
      () => calloutContainer.getBoundingClientRect().x !== startingX,
      "Callout should reposition itself"
    );

    win.document.querySelector(calloutDismissSelector).click();
  });
});

// Panels normally set the `[open]` attribute on their anchor node when they're
// open, so that the anchor node can be styled differently when the panel is
// open. Not every anchor node has styles for this, but e.g. chrome buttons do.
add_task(async function panel_feature_callout_anchor_open_attr() {
  let anchor;
  await testCalloutHiddenIf(
    async (win, calloutContainer) => {
      anchor = calloutContainer.anchorNode;
      ok(
        anchor.hasAttribute("open"),
        "Callout container's anchor node should have its [open] attribute set"
      );

      win.document.querySelector(calloutDismissSelector).click();
    },
    (win, calloutContainer) => {
      ok(
        !anchor.hasAttribute("open"),
        "Callout container's anchor node should not have its [open] attribute set"
      );
    }
  );
});

// However, some panels don't want to set the `[open]` attribute on their anchor
// node. Sometimes the panel is more of a hint than a menu, and the `[open]`
// style could give the impression that it's a menu. Or the anchor might already
// have its `[open]` attribute set by something else, and we may not want to
// interfere with that. So this feature is configurable by adding the
// no_open_on_anchor property to the anchor.
add_task(async function panel_feature_callout_no_anchor_open_attr() {
  let message = getTestMessage();
  message.content.screens[0].anchors[0].no_open_on_anchor = true;
  await testCalloutHiddenIf(
    async (win, calloutContainer) => {
      let anchor = calloutContainer.anchorNode;
      ok(
        !anchor.hasAttribute("open"),
        "Callout container's anchor node should not have its [open] attribute set"
      );

      win.document.querySelector(calloutDismissSelector).click();
    },
    null,
    message
  );
});
