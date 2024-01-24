/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const WIDGET_ID = "search-container";

registerCleanupFunction(() => {
  CustomizableUI.reset();
  Services.prefs.clearUserPref("browser.search.widget.inNavBar");
});

add_task(async function test_syncPreferenceWithWidget() {
  // Move the searchbar to the nav bar.
  CustomizableUI.addWidgetToArea(WIDGET_ID, CustomizableUI.AREA_NAVBAR);

  let container = document.getElementById(WIDGET_ID);
  // Set a disproportionately large width, which could be from a saved bigger
  // window, or what not.
  let width = window.innerWidth * 2;
  container.setAttribute("width", width);
  container.style.width = `${width}px`;

  // Stuff shouldn't overflow.
  Assert.less(
    container.getBoundingClientRect().width,
    window.innerWidth,
    "Searchbar shouldn't overflow"
  );
});
