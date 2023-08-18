/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let pageList = [];

function onHashChange() {
  changePage(document.location.hash.substring(1));
}

function changePage(page) {
  let navigation = document.querySelector("fxview-category-navigation");
  if (page && !pageList.includes(page)) {
    page = "recentbrowsing";
    document.location.hash = "recentbrowsing";
  }
  document.querySelector("named-deck").selectedViewName = page || pageList[0];
  navigation.currentCategory = page || pageList[0];
  if (navigation.categoryButtons.includes(document.activeElement)) {
    let currentCategoryButton = navigation.categoryButtons.find(
      categoryButton => categoryButton.name === page
    );
    (currentCategoryButton || navigation.categoryButtons[0]).focus();
  }

  // Record telemetry
  Services.telemetry.recordEvent(
    "firefoxview_next",
    "change_page",
    "navigation",
    null,
    {
      page: navigation.currentCategory,
    }
  );
}

window.addEventListener("DOMContentLoaded", async () => {
  Services.telemetry.setEventRecordingEnabled("firefoxview_next", true);
  let navigation = document.querySelector("fxview-category-navigation");
  for (const item of navigation.categoryButtons) {
    pageList.push(item.getAttribute("name"));
  }
  window.addEventListener("hashchange", onHashChange);
  window.addEventListener("change-category", function (event) {
    location.hash = event.target.getAttribute("name");
  });
  if (document.location.hash) {
    changePage(document.location.hash.substring(1));
  }
});

document
  .querySelector("named-deck")
  .addEventListener("view-changed", async event => {
    for (const child of event.target.children) {
      if (child.getAttribute("name") == event.target.selectedViewName) {
        child.enter();
      } else {
        child.exit();
      }
    }
  });

window.addEventListener(
  "unload",
  () => {
    // Clear out the document so the disconnectedCallback will trigger
    // properly and all of the custom elements can cleanup.
    document.body.textContent = "";
  },
  { once: true }
);
