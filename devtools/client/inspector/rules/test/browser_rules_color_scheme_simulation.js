/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test color scheme simulation.
const TEST_URI = URL_ROOT + "doc_media_queries.html";

add_task(async function() {
  await pushPref("devtools.inspector.color-scheme-simulation.enabled", true);
  await addTab(TEST_URI);
  const { inspector, view, toolbox } = await openRuleView();

  info("Check that the color scheme simulation buttons exist");
  const lightButton = inspector.panelDoc.querySelector(
    "#color-scheme-simulation-light-toggle"
  );
  const darkButton = inspector.panelDoc.querySelector(
    "#color-scheme-simulation-dark-toggle"
  );
  ok(lightButton, "The light color scheme simulation button exists");
  ok(darkButton, "The dark color scheme simulation button exists");

  is(
    isButtonChecked(lightButton),
    false,
    "At first, the light button isn't checked"
  );
  is(
    isButtonChecked(darkButton),
    false,
    "At first, the dark button isn't checked"
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

  info("Click on the dark button");
  darkButton.click();
  await waitFor(() => isButtonChecked(darkButton));
  ok(true, "The dark button is checked");
  is(
    isButtonChecked(lightButton),
    false,
    "the light button state didn't change when enabling dark mode"
  );

  await waitFor(() => divHasDarkSchemeStyling());
  ok(
    true,
    "The rules view was updated with the rule view from the dark scheme media query"
  );

  info("Select the node from the remote iframe");
  await selectNodeInFrames(["iframe", "html"], inspector);

  ok(
    iframeHasDarkSchemeStyling(),
    "The simulation is also applied on the remote iframe"
  );

  info("Select the top level div again");
  await selectNode("div", inspector);

  info("Click the light button simulate light mode");
  lightButton.click();
  await waitFor(() => isButtonChecked(lightButton));
  ok(true, "The button has the expected light state");
  // TODO: Actually simulate light mode. This might require to set the OS-level preference
  // to dark as the default state might consume the rule from the like scheme media query.

  is(
    isButtonChecked(darkButton),
    false,
    "the dark button was unchecked when enabling light mode"
  );

  await waitFor(() => divHasDefaultStyling());

  info("Click the light button to disable simulation");
  const onRuleViewRefreshed = view.once("ruleview-refreshed");
  lightButton.click();
  await waitFor(() => !isButtonChecked(lightButton));
  ok(true, "The button isn't checked anymore");
  await onRuleViewRefreshed;
  ok(divHasDefaultStyling(), "We're not simulating color-scheme anymore");

  info("Select the node from the remote iframe again");
  await selectNodeInFrames(["iframe", "html"], inspector);
  await waitFor(() => iframeElHasDefaultStyling());
  ok(true, "The simulation stopped on the remote iframe as well");

  info("Check that reloading keep the selected simulation");
  await selectNode("div", inspector);
  darkButton.click();
  await waitFor(() => divHasDarkSchemeStyling());

  await navigateTo(TEST_URI);
  await selectNode("div", inspector);
  await waitFor(() => view.element.children[1]);
  ok(
    divHasDarkSchemeStyling(),
    "dark mode is still simulated after reloading the page"
  );

  await selectNodeInFrames(["iframe", "html"], inspector);
  await waitFor(() => iframeHasDarkSchemeStyling());
  ok(true, "simulation is still applied to the iframe after reloading");

  info("Check that closing DevTools reset the simulation");
  await toolbox.destroy();
  const matchesPrefersDarkColorSchemeMedia = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      const { matches } = content.matchMedia("(prefers-color-scheme: dark)");
      return matches;
    }
  );
  is(
    matchesPrefersDarkColorSchemeMedia,
    false,
    "color scheme simulation is disabled after closing DevTools"
  );
});

function isButtonChecked(el) {
  return el.classList.contains("checked");
}
