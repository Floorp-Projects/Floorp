/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let pageList = [];
let categoryPagesDeck = null;
let categoryNavigation = null;
let activeComponent = null;
let searchKeyboardShortcut = null;

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
      activeComponent = child;
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

async function updateSearchTextboxSize() {
  const msgs = [
    { id: "firefoxview-search-text-box-recentbrowsing" },
    { id: "firefoxview-search-text-box-opentabs" },
    { id: "firefoxview-search-text-box-recentlyclosed" },
    { id: "firefoxview-search-text-box-syncedtabs" },
    { id: "firefoxview-search-text-box-history" },
  ];
  let maxLength = 0;
  for (const msg of await document.l10n.formatMessages(msgs)) {
    const placeholder = msg.attributes[0].value;
    maxLength = Math.max(maxLength, placeholder.length);
  }
  for (const child of categoryPagesDeck.children) {
    child.searchTextboxSize = maxLength;
  }
}

async function updateSearchKeyboardShortcut() {
  const [message] = await topChromeWindow.document.l10n.formatMessages([
    { id: "find-shortcut" },
  ]);
  const key = message.attributes[0].value;
  searchKeyboardShortcut = key.toLocaleLowerCase();
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
    window.scrollTo(0, 0);
    recordNavigationTelemetry("category-navigation", event.target);
  });
  window.addEventListener("card-container-view-all", function (event) {
    recordNavigationTelemetry("view-all", event.originalTarget);
  });

  categoryPagesDeck.addEventListener("view-changed", onPagesDeckViewChange);

  // set the initial state
  onHashChange();
  onPagesDeckViewChange();
  await updateSearchTextboxSize();
  await updateSearchKeyboardShortcut();

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

document.addEventListener("keydown", e => {
  if (e.getModifierState("Accel") && e.key === searchKeyboardShortcut) {
    activeComponent.searchTextbox?.focus();
  }
});

window.addEventListener(
  "unload",
  () => {
    // Clear out the document so the disconnectedCallback will trigger
    // properly and all of the custom elements can cleanup.
    document.body.textContent = "";
    topChromeWindow.removeEventListener("command", onCommand);
    Services.obs.removeObserver(onLocalesChanged, "intl:app-locales-changed");
  },
  { once: true }
);

topChromeWindow.addEventListener("command", onCommand);
Services.obs.addObserver(onLocalesChanged, "intl:app-locales-changed");

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

function onLocalesChanged() {
  requestIdleCallback(() => {
    updateSearchTextboxSize();
    updateSearchKeyboardShortcut();
  });
}
