/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that accessing properties with getters displays a "learn more" link in the confirm
// dialog that navigates the user to the expected mdn page.

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>
<head>
  <script>
    /* Create a prototype-less object so popup does not contain native
     * Object prototype properties.
     */
    let sideEffect;
    window.foo = Object.create(null, Object.getOwnPropertyDescriptors({
      get bar() {
        sideEffect = "bar";
        return "hello";
      }
    }));
  </script>
</head>
<body>Autocomplete popup - invoke getter usage test</body>`;

const DOC_URL =
  "https://firefox-source-docs.mozilla.org/devtools-user/web_console/invoke_getters_from_autocomplete/";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const toolbox = gDevTools.getToolboxForTab(gBrowser.selectedTab);

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
  const { link } = await simulateLinkClick(learnMoreEl);
  is(link, DOC_URL, `Click on "Learn More" link navigates user to ${DOC_URL}`);

  info("Close the popup");
  EventUtils.synthesizeKey("KEY_Escape");
  await waitFor(() => !isConfirmDialogOpened(toolbox));
});
