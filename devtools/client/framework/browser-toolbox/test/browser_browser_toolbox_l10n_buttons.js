/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);

// On debug test machine, it takes about 50s to run the test.
requestLongerTimeout(4);

/**
 * In the browser toolbox there are options to switch the language to the "bidi" and
 * "accented" languages. These are useful for making sure the browser is correctly
 * localized. This test opens the browser toolbox, and checks that these buttons
 * work.
 */
add_task(async function () {
  const ToolboxTask = await initBrowserToolboxTask();
  await ToolboxTask.importFunctions({ clickMeatballItem });

  is(getPseudoLocale(), "", "Starts out as empty");

  await ToolboxTask.spawn(null, () => clickMeatballItem("accented"));
  is(getPseudoLocale(), "accented", "Enabled the accented pseudo-locale");

  await ToolboxTask.spawn(null, () => clickMeatballItem("accented"));
  is(getPseudoLocale(), "", "Disabled the accented pseudo-locale.");

  await ToolboxTask.spawn(null, () => clickMeatballItem("bidi"));
  is(getPseudoLocale(), "bidi", "Enabled the bidi pseudo-locale.");

  await ToolboxTask.spawn(null, () => clickMeatballItem("bidi"));
  is(getPseudoLocale(), "", "Disabled the bidi pseudo-locale.");

  await ToolboxTask.spawn(null, () => clickMeatballItem("bidi"));
  is(getPseudoLocale(), "bidi", "Enabled the bidi before closing.");

  await ToolboxTask.destroy();

  is(getPseudoLocale(), "", "After closing the pseudo-locale is disabled.");
});

/**
 * Return the pseudo-locale preference of the debuggee browser (not the browser toolbox).
 *
 * Another option for this test would be to test the text and layout of the
 * browser directly, but this could be brittle. Checking the preference will
 * hopefully provide adequate coverage.
 */
function getPseudoLocale() {
  return Services.prefs.getCharPref("intl.l10n.pseudo");
}

/**
 * This function is a ToolboxTask and is cloned into the toolbox context. It opens the
 * "meatball menu" in the browser toolbox, clicks one of the pseudo-locale
 * options, and finally returns the pseudo-locale preference from the target browser.
 *
 * @param {"accented" | "bidi"} type
 */
function clickMeatballItem(type) {
  return new Promise(resolve => {
    /* global gToolbox */

    dump(`Opening the meatball menu in the browser toolbox.\n`);
    gToolbox.doc.getElementById("toolbox-meatball-menu-button").click();

    gToolbox.doc.addEventListener(
      "popupshown",
      async () => {
        const menuItem = gToolbox.doc.getElementById(
          "toolbox-meatball-menu-pseudo-locale-" + type
        );
        dump(`Clicking the meatball menu item: "${type}".\n`);
        menuItem.click();

        // Request the pseudo-locale so that we know the preference actor is fully
        // done setting the debuggee browser.
        await gToolbox.getPseudoLocale();
        resolve();
      },
      { once: true }
    );
  });
}
