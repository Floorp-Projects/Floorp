/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let pageList = [];
let categoryPagesDeck = null;
let categoryNavigation = null;

const { topChromeWindow } = window.browsingContext;

function onHashChange() {
  let page = document.location?.hash.substring(1);
  if (!page || !pageList.includes(page)) {
    page = "recentbrowsing";
  }
  changePage(page);
}

function changePage(page) {
  categoryPagesDeck.selectedViewName = page;
  categoryNavigation.currentCategory = page;
  if (categoryNavigation.categoryButtons.includes(document.activeElement)) {
    let currentCategoryButton = categoryNavigation.categoryButtons.find(
      categoryButton => categoryButton.name === page
    );
    (currentCategoryButton || categoryNavigation.categoryButtons[0]).focus();
  }
}

function onPagesDeckViewChange() {
  for (const child of categoryPagesDeck.children) {
    if (child.getAttribute("name") == categoryPagesDeck.selectedViewName) {
      child.enter();
    } else {
      child.exit();
    }
  }
}

function recordNavigationTelemetry(source, eventTarget) {
  let page = "recentbrowsing";
  if (source === "category-navigation") {
    page = eventTarget.parentNode.currentCategory;
  } else if (source === "view-all") {
    page = eventTarget.shortPageName;
  }
  // Record telemetry
  Services.telemetry.recordEvent(
    "firefoxview_next",
    "change_page",
    "navigation",
    null,
    {
      page,
      source,
    }
  );
}

window.addEventListener("DOMContentLoaded", async () => {
  recordEnteredTelemetry();

  categoryNavigation = document.querySelector("fxview-category-navigation");
  categoryPagesDeck = document.querySelector("named-deck");

  for (const item of categoryNavigation.categoryButtons) {
    pageList.push(item.getAttribute("name"));
  }
  window.addEventListener("hashchange", onHashChange);
  window.addEventListener("change-category", function (event) {
    location.hash = event.target.getAttribute("name");
    recordNavigationTelemetry("category-navigation", event.target);
  });
  window.addEventListener("card-container-view-all", function (event) {
    recordNavigationTelemetry("view-all", event.originalTarget);
  });

  categoryPagesDeck.addEventListener("view-changed", onPagesDeckViewChange);

  // set the initial state
  onHashChange();
  onPagesDeckViewChange();

  if (Cu.isInAutomation) {
    Services.obs.notifyObservers(null, "firefoxview-entered");
  }
});

document.addEventListener("visibilitychange", () => {
  if (document.visibilityState === "visible") {
    recordEnteredTelemetry();
    if (Cu.isInAutomation) {
      // allow all the component visibilitychange handlers to execute before notifying
      requestAnimationFrame(() => {
        Services.obs.notifyObservers(null, "firefoxview-entered");
      });
    }
  }
});

function recordEnteredTelemetry() {
  Services.telemetry.recordEvent(
    "firefoxview_next",
    "entered",
    "firefoxview",
    null,
    {
      page: document.location?.hash?.substring(1) || "recentbrowsing",
    }
  );
}

window.addEventListener(
  "unload",
  () => {
    // Clear out the document so the disconnectedCallback will trigger
    // properly and all of the custom elements can cleanup.
    document.body.textContent = "";
    topChromeWindow.removeEventListener("command", onCommand);
  },
  { once: true }
);

topChromeWindow.addEventListener("command", onCommand);

function onCommand(e) {
  if (document.hidden || !e.target.closest("#contentAreaContextMenu")) {
    return;
  }
  const item =
    e.target.closest("#context-openlinkinusercontext-menu") || e.target;
  Services.telemetry.recordEvent(
    "firefoxview_next",
    "browser_context_menu",
    "tabs",
    null,
    {
      menu_action: item.id,
      page: location.hash?.substring(1) || "recentbrowsing",
    }
  );
}
