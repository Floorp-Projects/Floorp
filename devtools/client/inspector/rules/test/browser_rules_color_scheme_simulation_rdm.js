/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test color scheme simulation when RDM is toggled
const TEST_URI = URL_ROOT + "doc_media_queries.html";

const ResponsiveUIManager = require("devtools/client/responsive/manager");
const ResponsiveMessageHelper = require("devtools/client/responsive/utils/message");

add_task(async function() {
  await pushPref("devtools.inspector.color-scheme-simulation.enabled", true);
  // Use a local file for the device list, otherwise the panel tries to reach an external
  // URL, which makes the test fail.
  await pushPref(
    "devtools.devices.url",
    "http://example.com/browser/devtools/client/responsive/test/browser/devices.json"
  );

  const tab = await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();

  info("Check that the color scheme simulation buttons exist");
  const darkButton = inspector.panelDoc.querySelector(
    "#color-scheme-simulation-dark-toggle"
  );

  // Define functions checking if the rule view display the expected property.
  const divHasDefaultStyling = () =>
    getPropertiesForRuleIndex(view, 1).has("background-color:yellow");
  const divHasDarkSchemeStyling = () =>
    getPropertiesForRuleIndex(view, 1).has("background-color:darkblue");

  info(
    "Select the div that will change according to conditions in prefered color scheme"
  );
  await selectNode("div", inspector);
  ok(divHasDefaultStyling(), "The rule view shows the expected initial rule");

  info("Open responsive design mode");
  let { toolWindow } = await ResponsiveUIManager.openIfNeeded(
    tab.ownerGlobal,
    tab,
    {
      trigger: "test",
    }
  );
  await ResponsiveMessageHelper.wait(toolWindow, "post-init");

  info("Click on the dark button");
  darkButton.click();
  await waitFor(() => isButtonChecked(darkButton));
  ok(true, "The dark button is checked");

  await waitFor(() => divHasDarkSchemeStyling());
  ok(
    true,
    "The rules view was updated with the rule view from the dark scheme media query"
  );

  info("Close responsive design mode");
  await ResponsiveUIManager.closeIfNeeded(tab.ownerGlobal, tab);

  info("Wait for a bit before checking dark mode is still enabled");
  await wait(1000);
  ok(isButtonChecked(darkButton), "button is still checked");
  ok(
    divHasDarkSchemeStyling(),
    "dark mode color-scheme simulation is still enabled"
  );

  info("Click the button to disable simulation");
  const onRuleViewRefreshed = view.once("ruleview-refreshed");
  darkButton.click();
  await waitFor(() => !isButtonChecked(darkButton));
  ok(true, "The button isn't checked anymore");
  await onRuleViewRefreshed;
  ok(divHasDefaultStyling(), "We're not simulating color-scheme anymore");

  info("Check that enabling dark-mode simulation before RDM does work as well");
  darkButton.click();
  await waitFor(() => isButtonChecked(darkButton));
  await waitFor(() => divHasDarkSchemeStyling());
  ok(
    true,
    "The rules view was updated with the rule view from the dark scheme media query"
  );

  info("Open responsive design mode again");
  ({ toolWindow } = await ResponsiveUIManager.openIfNeeded(
    tab.ownerGlobal,
    tab,
    {
      trigger: "test",
    }
  ));
  await ResponsiveMessageHelper.wait(toolWindow, "post-init");

  info("Click the button to disable simulation while RDM is still opened");
  darkButton.click();
  await waitFor(() => !isButtonChecked(darkButton));
  ok(true, "The button isn't checked anymore");
  await waitFor(() => divHasDefaultStyling());
  ok(true, "We're not simulating color-scheme anymore");

  info("Close responsive design mode");
  await ResponsiveUIManager.closeIfNeeded(tab.ownerGlobal, tab);
});

function isButtonChecked(el) {
  return el.classList.contains("checked");
}
