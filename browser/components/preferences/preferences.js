/* - This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/. */

// Import globals from the files imported by the .xul files.
/* import-globals-from subdialogs.js */
/* import-globals-from main.js */
/* import-globals-from home.js */
/* import-globals-from search.js */
/* import-globals-from containers.js */
/* import-globals-from privacy.js */
/* import-globals-from sync.js */
/* import-globals-from findInPage.js */
/* import-globals-from ../../base/content/utilityOverlay.js */
/* import-globals-from ../../../toolkit/content/preferencesBindings.js */

"use strict";

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "AMTelemetry",
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "formAutofillParent",
  "resource://formautofill/FormAutofillParent.jsm"
);

var gLastCategory = { category: undefined, subcategory: undefined };
const gXULDOMParser = new DOMParser();

var gCategoryInits = new Map();
function init_category_if_required(category) {
  let categoryInfo = gCategoryInits.get(category);
  if (!categoryInfo) {
    throw new Error(
      "Unknown in-content prefs category! Can't init " + category
    );
  }
  if (categoryInfo.inited) {
    return null;
  }
  return categoryInfo.init();
}

function register_module(categoryName, categoryObject) {
  gCategoryInits.set(categoryName, {
    inited: false,
    async init() {
      let template = document.getElementById("template-" + categoryName);
      if (template) {
        // Replace the template element with the nodes inside of it.
        let frag = template.content;
        await document.l10n.translateFragment(frag);

        // Actually insert them into the DOM.
        document.l10n.pauseObserving();
        template.replaceWith(frag);
        document.l10n.resumeObserving();

        // Asks Preferences to update the attribute value of the entire
        // document again (this can be simplified if we could seperate the
        // preferences of each pane.)
        Preferences.updateAllElements();
      }
      categoryObject.init();
      this.inited = true;
    },
  });
}

document.addEventListener("DOMContentLoaded", init_all, { once: true });

function init_all() {
  Preferences.forceEnableInstantApply();

  gSubDialog.init();
  register_module("paneGeneral", gMainPane);
  register_module("paneHome", gHomePane);
  register_module("paneSearch", gSearchPane);
  register_module("panePrivacy", gPrivacyPane);
  register_module("paneContainers", gContainersPane);
  if (Services.prefs.getBoolPref("identity.fxaccounts.enabled")) {
    document.getElementById("category-sync").hidden = false;
    register_module("paneSync", gSyncPane);
  } else {
    // Remove the pane from the DOM so it doesn't get incorrectly included in search results.
    document.getElementById("template-paneSync").remove();
  }
  register_module("paneSearchResults", gSearchResultsPane);
  gSearchResultsPane.init();
  gMainPane.preInit();

  let categories = document.getElementById("categories");
  categories.addEventListener("select", event => gotoPref(event.target.value));

  document.documentElement.addEventListener("keydown", function(event) {
    if (event.keyCode == KeyEvent.DOM_VK_TAB) {
      categories.setAttribute("keyboard-navigation", "true");
    }
  });
  categories.addEventListener("mousedown", function() {
    this.removeAttribute("keyboard-navigation");
  });

  maybeDisplayPoliciesNotice();

  window.addEventListener("hashchange", onHashChange);

  gotoPref().then(() => {
    let helpButton = document.getElementById("helpButton");
    let helpUrl =
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "preferences";
    helpButton.setAttribute("href", helpUrl);

    document.getElementById("addonsButton").addEventListener("click", e => {
      if (e.button >= 2) {
        // Ignore right clicks.
        return;
      }
      let mainWindow = window.docShell.rootTreeItem.domWindow;
      mainWindow.BrowserOpenAddonsMgr();
      AMTelemetry.recordLinkEvent({
        object: "aboutPreferences",
        value: "about:addons",
      });
    });

    document.dispatchEvent(
      new CustomEvent("Initialized", {
        bubbles: true,
        cancelable: true,
      })
    );
  });
}

function telemetryBucketForCategory(category) {
  category = category.toLowerCase();
  switch (category) {
    case "containers":
    case "general":
    case "home":
    case "privacy":
    case "search":
    case "sync":
    case "searchresults":
      return category;
    default:
      return "unknown";
  }
}

function onHashChange() {
  gotoPref();
}

async function gotoPref(aCategory) {
  let categories = document.getElementById("categories");
  const kDefaultCategoryInternalName = "paneGeneral";
  const kDefaultCategory = "general";
  let hash = document.location.hash;

  let category = aCategory || hash.substr(1) || kDefaultCategoryInternalName;
  let breakIndex = category.indexOf("-");
  // Subcategories allow for selecting smaller sections of the preferences
  // until proper search support is enabled (bug 1353954).
  let subcategory = breakIndex != -1 && category.substring(breakIndex + 1);
  if (subcategory) {
    category = category.substring(0, breakIndex);
  }
  category = friendlyPrefCategoryNameToInternalName(category);
  if (category != "paneSearchResults") {
    gSearchResultsPane.query = null;
    gSearchResultsPane.searchInput.value = "";
    gSearchResultsPane.getFindSelection(window).removeAllRanges();
    gSearchResultsPane.removeAllSearchTooltips();
    gSearchResultsPane.removeAllSearchMenuitemIndicators();
  } else if (!gSearchResultsPane.searchInput.value) {
    // Something tried to send us to the search results pane without
    // a query string. Default to the General pane instead.
    category = kDefaultCategoryInternalName;
    document.location.hash = kDefaultCategory;
    gSearchResultsPane.query = null;
  }

  // Updating the hash (below) or changing the selected category
  // will re-enter gotoPref.
  if (gLastCategory.category == category && !subcategory) {
    return;
  }

  let item;
  if (category != "paneSearchResults") {
    // Hide second level headers in normal view
    for (let element of document.querySelectorAll(".search-header")) {
      element.hidden = true;
    }

    item = categories.querySelector(".category[value=" + category + "]");
    if (!item) {
      category = kDefaultCategoryInternalName;
      item = categories.querySelector(".category[value=" + category + "]");
    }
  }

  if (
    gLastCategory.category ||
    category != kDefaultCategoryInternalName ||
    subcategory
  ) {
    let friendlyName = internalPrefCategoryNameToFriendlyName(category);
    document.location.hash = friendlyName;
  }
  // Need to set the gLastCategory before setting categories.selectedItem since
  // the categories 'select' event will re-enter the gotoPref codepath.
  gLastCategory.category = category;
  gLastCategory.subcategory = subcategory;
  if (item) {
    categories.selectedItem = item;
  } else {
    categories.clearSelection();
  }
  window.history.replaceState(category, document.title);

  try {
    await init_category_if_required(category);
  } catch (ex) {
    Cu.reportError(
      new Error(
        "Error initializing preference category " + category + ": " + ex
      )
    );
    throw ex;
  }

  // Bail out of this goToPref if the category
  // or subcategory changed during async operation.
  if (
    gLastCategory.category !== category ||
    gLastCategory.subcategory !== subcategory
  ) {
    return;
  }

  search(category, "data-category");

  let mainContent = document.querySelector(".main-content");
  mainContent.scrollTop = 0;

  spotlight(subcategory, category);
}

function search(aQuery, aAttribute) {
  let mainPrefPane = document.getElementById("mainPrefPane");
  let elements = mainPrefPane.children;
  for (let element of elements) {
    // If the "data-hidden-from-search" is "true", the
    // element will not get considered during search.
    if (
      element.getAttribute("data-hidden-from-search") != "true" ||
      element.getAttribute("data-subpanel") == "true"
    ) {
      let attributeValue = element.getAttribute(aAttribute);
      if (attributeValue == aQuery) {
        element.hidden = false;
      } else {
        element.hidden = true;
      }
    } else if (
      element.getAttribute("data-hidden-from-search") == "true" &&
      !element.hidden
    ) {
      element.hidden = true;
    }
    element.classList.remove("visually-hidden");
  }

  let keysets = mainPrefPane.getElementsByTagName("keyset");
  for (let element of keysets) {
    let attributeValue = element.getAttribute(aAttribute);
    if (attributeValue == aQuery) {
      element.removeAttribute("disabled");
    } else {
      element.setAttribute("disabled", true);
    }
  }
}

async function spotlight(subcategory, category) {
  let highlightedElements = document.querySelectorAll(".spotlight");
  if (highlightedElements.length) {
    for (let element of highlightedElements) {
      element.classList.remove("spotlight");
    }
  }
  if (subcategory) {
    scrollAndHighlight(subcategory, category);
  }
}

async function scrollAndHighlight(subcategory, category) {
  let element = document.querySelector(`[data-subcategory="${subcategory}"]`);
  if (!element) {
    return;
  }
  let header = getClosestDisplayedHeader(element);

  scrollContentTo(header);
  element.classList.add("spotlight");
}

/**
 * If there is no visible second level header it will return first level header,
 * otherwise return second level header.
 * @returns {Element} - The closest displayed header.
 */
function getClosestDisplayedHeader(element) {
  let header = element.closest("groupbox");
  let searchHeader = header.querySelector(".search-header");
  if (
    searchHeader &&
    searchHeader.hidden &&
    header.previousElementSibling.classList.contains("subcategory")
  ) {
    header = header.previousElementSibling;
  }
  return header;
}

function scrollContentTo(element) {
  const STICKY_CONTAINER_HEIGHT = document.querySelector(".sticky-container")
    .clientHeight;
  let mainContent = document.querySelector(".main-content");
  let top = element.getBoundingClientRect().top - STICKY_CONTAINER_HEIGHT;
  mainContent.scroll({
    top,
    behavior: "smooth",
  });
}

function friendlyPrefCategoryNameToInternalName(aName) {
  if (aName.startsWith("pane")) {
    return aName;
  }
  return "pane" + aName.substring(0, 1).toUpperCase() + aName.substr(1);
}

// This function is duplicated inside of utilityOverlay.js's openPreferences.
function internalPrefCategoryNameToFriendlyName(aName) {
  return (aName || "").replace(/^pane./, function(toReplace) {
    return toReplace[4].toLowerCase();
  });
}

// Put up a confirm dialog with "ok to restart", "revert without restarting"
// and "restart later" buttons and returns the index of the button chosen.
// We can choose not to display the "restart later", or "revert" buttons,
// altough the later still lets us revert by using the escape key.
//
// The constants are useful to interpret the return value of the function.
const CONFIRM_RESTART_PROMPT_RESTART_NOW = 0;
const CONFIRM_RESTART_PROMPT_CANCEL = 1;
const CONFIRM_RESTART_PROMPT_RESTART_LATER = 2;
async function confirmRestartPrompt(
  aRestartToEnable,
  aDefaultButtonIndex,
  aWantRevertAsCancelButton,
  aWantRestartLaterButton
) {
  let [
    msg,
    title,
    restartButtonText,
    noRestartButtonText,
    restartLaterButtonText,
  ] = await document.l10n.formatValues([
    {
      id: aRestartToEnable
        ? "feature-enable-requires-restart"
        : "feature-disable-requires-restart",
    },
    { id: "should-restart-title" },
    { id: "should-restart-ok" },
    { id: "cancel-no-restart-button" },
    { id: "restart-later" },
  ]);

  // Set up the first (index 0) button:
  let buttonFlags =
    Services.prompt.BUTTON_POS_0 * Services.prompt.BUTTON_TITLE_IS_STRING;

  // Set up the second (index 1) button:
  if (aWantRevertAsCancelButton) {
    buttonFlags +=
      Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_IS_STRING;
  } else {
    noRestartButtonText = null;
    buttonFlags +=
      Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_CANCEL;
  }

  // Set up the third (index 2) button:
  if (aWantRestartLaterButton) {
    buttonFlags +=
      Services.prompt.BUTTON_POS_2 * Services.prompt.BUTTON_TITLE_IS_STRING;
  } else {
    restartLaterButtonText = null;
  }

  switch (aDefaultButtonIndex) {
    case 0:
      buttonFlags += Services.prompt.BUTTON_POS_0_DEFAULT;
      break;
    case 1:
      buttonFlags += Services.prompt.BUTTON_POS_1_DEFAULT;
      break;
    case 2:
      buttonFlags += Services.prompt.BUTTON_POS_2_DEFAULT;
      break;
    default:
      break;
  }

  let buttonIndex = Services.prompt.confirmEx(
    window,
    title,
    msg,
    buttonFlags,
    restartButtonText,
    noRestartButtonText,
    restartLaterButtonText,
    null,
    {}
  );

  // If we have the second confirmation dialog for restart, see if the user
  // cancels out at that point.
  if (buttonIndex == CONFIRM_RESTART_PROMPT_RESTART_NOW) {
    let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
      Ci.nsISupportsPRBool
    );
    Services.obs.notifyObservers(
      cancelQuit,
      "quit-application-requested",
      "restart"
    );
    if (cancelQuit.data) {
      buttonIndex = CONFIRM_RESTART_PROMPT_CANCEL;
    }
  }
  return buttonIndex;
}

// This function is used to append search keywords found
// in the related subdialog to the button that will activate the subdialog.
function appendSearchKeywords(aId, keywords) {
  let element = document.getElementById(aId);
  let searchKeywords = element.getAttribute("searchkeywords");
  if (searchKeywords) {
    keywords.push(searchKeywords);
  }
  element.setAttribute("searchkeywords", keywords.join(" "));
}

function maybeDisplayPoliciesNotice() {
  if (Services.policies.status == Services.policies.ACTIVE) {
    document.getElementById("policies-container").removeAttribute("hidden");
  }
}

/**
 * Filter the lastFallbackLocale from availableLocales if it doesn't have all
 * of the needed strings.
 *
 * When the lastFallbackLocale isn't the defaultLocale, then by default only
 * fluent strings are included. To fully use that locale you need the langpack
 * to be installed, so if it isn't installed remove it from availableLocales.
 */
async function getAvailableLocales() {
  let { availableLocales, defaultLocale, lastFallbackLocale } = Services.locale;
  // If defaultLocale isn't lastFallbackLocale, then we still need the langpack
  // for lastFallbackLocale for it to be useful.
  if (defaultLocale != lastFallbackLocale) {
    let lastFallbackId = `langpack-${lastFallbackLocale}@firefox.mozilla.org`;
    let lastFallbackInstalled = await AddonManager.getAddonByID(lastFallbackId);
    if (!lastFallbackInstalled) {
      return availableLocales.filter(locale => locale != lastFallbackLocale);
    }
  }
  return availableLocales;
}
