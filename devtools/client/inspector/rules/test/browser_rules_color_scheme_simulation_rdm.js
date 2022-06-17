/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test color scheme simulation when RDM is toggled
const TEST_URI = URL_ROOT + "doc_media_queries.html";

add_task(async function() {
  const tab = await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();

  info("Check that the color scheme simulation buttons exist");
  const darkButton = inspector.panelDoc.querySelector(
    "#color-scheme-simulation-dark-toggle"
  );

  // Define functions checking if the rule view display the expected property.
  const divHasDefaultStyling = async () =>
    (await getPropertiesForRuleIndex(view, 1)).has("background-color:yellow");
  const divHasDarkSchemeStyling = async () =>
    (await getPropertiesForRuleIndex(view, 1)).has("background-color:darkblue");

  info(
    "Select the div that will change according to conditions in prefered color scheme"
  );
  await selectNode("div", inspector);
  ok(
    await divHasDefaultStyling(),
    "The rule view shows the expected initial rule"
  );

  info("Open responsive design mode");
  await openRDM(tab);

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
  await closeRDM(tab);

  info("Wait for a bit before checking dark mode is still enabled");
  await wait(1000);
  ok(isButtonChecked(darkButton), "button is still checked");
  ok(
    await divHasDarkSchemeStyling(),
    "dark mode color-scheme simulation is still enabled"
  );

  info("Click the button to disable simulation");
  darkButton.click();
  await waitFor(() => !isButtonChecked(darkButton));
  ok(true, "The button isn't checked anymore");
  await waitFor(() => divHasDefaultStyling());
  ok(true, "We're not simulating color-scheme anymore");

  info("Check that enabling dark-mode simulation before RDM does work as well");
  darkButton.click();
  await waitFor(() => isButtonChecked(darkButton));
  await waitFor(() => divHasDarkSchemeStyling());
  ok(
    true,
    "The rules view was updated with the rule view from the dark scheme media query"
  );

  info("Open responsive design mode again");
  await openRDM(tab);

  info("Click the button to disable simulation while RDM is still opened");
  darkButton.click();
  await waitFor(() => !isButtonChecked(darkButton));
  ok(true, "The button isn't checked anymore");
  await waitFor(() => divHasDefaultStyling());
  ok(true, "We're not simulating color-scheme anymore");

  info("Close responsive design mode");
  await closeRDM(tab);
});

function isButtonChecked(el) {
  return el.classList.contains("checked");
}
