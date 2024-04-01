/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let pageList = [];
let viewsDeck = null;
let pageNav = null;
let activeComponent = null;
let searchKeyboardShortcut = null;

const { topChromeWindow } = window.browsingContext;

function onHashChange() {
  let view = document.location?.hash.substring(1);
  if (!view || !pageList.includes(view)) {
    view = "recentbrowsing";
  }
  changeView(view);
}

function changeView(view) {
  viewsDeck.selectedViewName = view;
  pageNav.currentView = view;
}

function onViewsDeckViewChange() {
  for (const child of viewsDeck.children) {
    if (child.getAttribute("name") == viewsDeck.selectedViewName) {
      child.enter();
      activeComponent = child;
    } else {
      child.exit();
    }
  }
}

function recordNavigationTelemetry(source, eventTarget) {
  let view = "recentbrowsing";
  if (source === "category-navigation") {
    view = eventTarget.parentNode.currentView;
  } else if (source === "view-all") {
    view = eventTarget.shortPageName;
  }
  // Record telemetry
  Services.telemetry.recordEvent(
    "firefoxview_next",
    "change_page",
    "navigation",
    null,
    {
      page: view,
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
  let maxLength = 30;
  for (const msg of await document.l10n.formatMessages(msgs)) {
    const placeholder = msg.attributes[0].value;
    maxLength = Math.max(maxLength, placeholder.length);
  }
  for (const child of viewsDeck.children) {
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

function updateSyncVisibility() {
  const syncEnabled = Services.prefs.getBoolPref(
    "identity.fxaccounts.enabled",
    false
  );
  for (const el of document.querySelectorAll(".sync-ui-item")) {
    el.hidden = !syncEnabled;
  }
}

window.addEventListener("DOMContentLoaded", async () => {
  recordEnteredTelemetry();

  pageNav = document.querySelector("moz-page-nav");
  viewsDeck = document.querySelector("named-deck");

  for (const item of pageNav.pageNavButtons) {
    pageList.push(item.getAttribute("view"));
  }
  window.addEventListener("hashchange", onHashChange);
  window.addEventListener("change-view", function (event) {
    location.hash = event.target.getAttribute("view");
    window.scrollTo(0, 0);
    recordNavigationTelemetry("category-navigation", event.target);
  });
  window.addEventListener("card-container-view-all", function (event) {
    recordNavigationTelemetry("view-all", event.originalTarget);
  });

  viewsDeck.addEventListener("view-changed", onViewsDeckViewChange);

  // set the initial state
  onHashChange();
  onViewsDeckViewChange();
  await updateSearchTextboxSize();
  await updateSearchKeyboardShortcut();
  updateSyncVisibility();

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
    Services.prefs.removeObserver(
      "identity.fxaccounts.enabled",
      updateSyncVisibility
    );
  },
  { once: true }
);

topChromeWindow.addEventListener("command", onCommand);
Services.obs.addObserver(onLocalesChanged, "intl:app-locales-changed");
Services.prefs.addObserver("identity.fxaccounts.enabled", updateSyncVisibility);

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
