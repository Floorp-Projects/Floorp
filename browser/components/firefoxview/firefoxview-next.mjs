/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let pageList = [];

function onHashChange() {
  changePage(document.location.hash.substring(1));
}

function changePage(page) {
  let navigation = document.querySelector("fxview-category-navigation");
  let currentCategoryButton;
  if (pageList.includes(page)) {
    document.querySelector("named-deck").selectedViewName = page;
    navigation.currentCategory = page;
    // Move focus if activeElement is a category button when changing page
    if (navigation.categoryButtons.includes(document.activeElement)) {
      currentCategoryButton = navigation.categoryButtons.filter(
        categoryButton => categoryButton.name === page
      );
      currentCategoryButton[0]?.focus();
    }
  } else {
    // Select first category if page not found in pageList
    document.location.hash = pageList[0];
    navigation.currentCategory = pageList[0];
    // Move focus if activeElement is a category button when changing page
    if (navigation.categoryButtons.includes(document.activeElement)) {
      currentCategoryButton = navigation.categoryButtons[0];
      currentCategoryButton?.focus();
    }
  }
}

window.addEventListener("DOMContentLoaded", async () => {
  if (document.location.hash) {
    changePage(document.location.hash.substring(1));
  }
  window.addEventListener("hashchange", onHashChange);
  let navigation = document.querySelector("fxview-category-navigation");
  for (const item of navigation.categoryButtons) {
    pageList.push(item.getAttribute("name"));
  }
  window.addEventListener("change-category", function (event) {
    location.hash = event.target.getAttribute("name");
  });
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
