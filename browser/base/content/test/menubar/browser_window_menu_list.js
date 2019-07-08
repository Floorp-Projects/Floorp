/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_window_menu_list() {
  // This title is different depending on the build. For example, it's "Nightly"
  // for a local build, "Mozilla Firefox" for an official release build.
  const windowTitle = window.document.title;
  await checkWindowMenu([windowTitle, "Browser chrome tests"]);
  let newWindow = await BrowserTestUtils.openNewBrowserWindow();
  await checkWindowMenu([windowTitle, "Browser chrome tests", windowTitle]);
  await BrowserTestUtils.closeWindow(newWindow);
});

async function checkWindowMenu(labels) {
  let menu = document.querySelector("#windowMenu");
  // We can't toggle menubar items on OSX, so mocking instead.
  await new Promise(resolve => {
    menu.addEventListener("popupshown", resolve, { once: true });
    menu.dispatchEvent(new MouseEvent("popupshowing"));
    menu.dispatchEvent(new MouseEvent("popupshown"));
  });

  let menuitems = [...menu.querySelectorAll("menuseparator ~ menuitem")];
  is(menuitems.length, labels.length, "Correct number of windows in the menu");
  is(
    menuitems.map(item => item.label).join(","),
    labels.join(","),
    "Correct labels on menuitems"
  );
  for (let menuitem of menuitems) {
    ok(
      menuitem instanceof customElements.get("menuitem"),
      "sibling is menuitem"
    );
  }

  // We can't toggle menubar items on OSX, so mocking instead.
  await new Promise(resolve => {
    menu.addEventListener("popuphidden", resolve, { once: true });
    menu.dispatchEvent(new MouseEvent("popuphiding"));
    menu.dispatchEvent(new MouseEvent("popuphidden"));
  });
}
