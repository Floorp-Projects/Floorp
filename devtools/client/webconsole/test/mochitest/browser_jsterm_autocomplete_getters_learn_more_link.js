/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that accessing properties with getters displays a "learn more" link in the confirm
// dialog that navigates the user to the expected mdn page.

const TEST_URI = `data:text/html;charset=utf-8,
<head>
  <script>
    /* Create a prototype-less object so popup does not contain native
     * Object prototype properties.
     */
    window.foo = Object.create(null, Object.getOwnPropertyDescriptors({
      get bar() {
        return "hello";
      }
    }));
  </script>
</head>
<body>Autocomplete popup - invoke getter usage test</body>`;

const MDN_URL =
  "https://developer.mozilla.org/docs/Tools/Web_Console/Invoke_getters_from_autocomplete";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  const toolbox = gDevTools.getToolbox(target);

  const tooltip = await setInputValueForGetterConfirmDialog(
    toolbox,
    hud,
    "window.foo.bar."
  );
  const labelEl = tooltip.querySelector(".confirm-label");
  is(
    labelEl.textContent,
    "Invoke getter window.foo.bar to retrieve the property list?",
    "Dialog has expected text content"
  );
  const learnMoreEl = tooltip.querySelector(".learn-more-link");
  is(learnMoreEl.textContent, "Learn More", `There's a "Learn more" link`);

  info(
    `Check that clicking on the "Learn more" link navigates to the expected page`
  );
  const expectedUri = MDN_URL + GA_PARAMS;
  const { link } = await simulateLinkClick(learnMoreEl);
  is(
    link,
    expectedUri,
    `Click on "Learn More" link navigates user to ${expectedUri}`
  );

  info("Close the popup");
  EventUtils.synthesizeKey("KEY_Escape");
  await waitFor(() => !isConfirmDialogOpened(toolbox));
});
