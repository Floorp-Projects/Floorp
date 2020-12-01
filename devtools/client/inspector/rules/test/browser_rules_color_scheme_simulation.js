/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test color scheme simulation.
const TEST_URI = URL_ROOT + "doc_media_queries.html";

add_task(async function() {
  await pushPref("devtools.inspector.color-scheme-simulation.enabled", true);
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();

  info("Check that the color scheme simulation button exists");
  const button = inspector.panelDoc.querySelector(
    "#color-scheme-simulation-toggle"
  );
  ok(button, "The color scheme simulation button exists");

  is(
    button.getAttribute("state"),
    null,
    "At first, the button has no specific state"
  );

  // Define functions checking if the rule view display the expected property.
  const divHasDefaultStyling = () =>
    getPropertiesForRuleIndex(view, 1).has("background-color:yellow");
  const divHasDarkSchemeStyling = () =>
    getPropertiesForRuleIndex(view, 1).has("background-color:darkblue");
  const iframeElHasDefaultStyling = () =>
    getPropertiesForRuleIndex(view, 1).has("background:cyan");
  const iframeHasDarkSchemeStyling = () =>
    getPropertiesForRuleIndex(view, 1).has("background:darkred");

  info(
    "Select the div that will change according to conditions in prefered color scheme"
  );
  await selectNode("div", inspector);
  ok(divHasDefaultStyling(), "The rule view shows the expected initial rule");

  info("Click on the button until we get to the dark scheme simulation state");
  button.click();
  await waitFor(() => button.getAttribute("state") === "dark");
  ok(true, "The button has the expected dark state");

  await waitFor(() => divHasDarkSchemeStyling());
  ok(
    true,
    "The rules view was updated with the rule view from the dark scheme media query"
  );

  info("Select the node from the remote iframe");
  const iframeEl = await getNodeFrontInFrame("html", "iframe", inspector);
  await selectNode(iframeEl, inspector);

  // This fails when Fission is enabled. See Bug 1678945.
  ok(
    iframeHasDarkSchemeStyling(),
    "The simulation is also applied on the remote iframe"
  );

  info("Select the top level div again");
  await selectNode("div", inspector);

  info("Click the button again to simulate light mode");
  button.click();
  await waitFor(() => button.getAttribute("state") === "light");
  ok(true, "The button has the expected light state");
  // TODO: Actually simulate light mode. This might require to set the OS-level preference
  // to dark as the default state might consume the rule from the like scheme media query.

  await waitFor(() => divHasDefaultStyling());

  info("Click the button once more to disable simulation");
  const onRuleViewRefreshed = view.once("ruleview-refreshed");
  button.click();
  await waitFor(() => button.getAttribute("state") === null);
  ok(true, "The button has no specific state again");
  await onRuleViewRefreshed;
  ok(divHasDefaultStyling(), "We're not simulating color-scheme anymore");

  info("Select the node from the remote iframe again");
  await selectNode(iframeEl, inspector);
  await waitFor(() => iframeElHasDefaultStyling());
  ok(true, "The simulation stopped on the remote iframe as well");
});
