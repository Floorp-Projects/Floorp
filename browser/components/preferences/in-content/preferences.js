/* - This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/. */

// Import globals from the files imported by the .xul files.
/* import-globals-from subdialogs.js */
/* import-globals-from advanced.js */
/* import-globals-from main.js */
/* import-globals-from containers.js */
/* import-globals-from privacy.js */
/* import-globals-from applications.js */
/* import-globals-from sync.js */
/* import-globals-from findInPage.js */
/* import-globals-from ../../../base/content/utilityOverlay.js */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

var gLastHash = "";

var gCategoryInits = new Map();
function init_category_if_required(category) {
  let categoryInfo = gCategoryInits.get(category);
  if (!categoryInfo) {
    throw "Unknown in-content prefs category! Can't init " + category;
  }
  if (categoryInfo.inited) {
    return;
  }
  categoryInfo.init();
}

function register_module(categoryName, categoryObject) {
  gCategoryInits.set(categoryName, {
    inited: false,
    init() {
      categoryObject.init();
      this.inited = true;
    }
  });
}

document.addEventListener("DOMContentLoaded", init_all, {once: true});

function init_all() {
  document.documentElement.instantApply = true;

  gSubDialog.init();
  register_module("paneGeneral", gMainPane);
  register_module("panePrivacy", gPrivacyPane);
  register_module("paneContainers", gContainersPane);
  register_module("paneAdvanced", gAdvancedPane);
  register_module("paneApplications", gApplicationsPane);
  register_module("paneSync", gSyncPane);
  register_module("paneSearchResults", gSearchResultsPane);
  gSearchResultsPane.init();

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

  window.addEventListener("hashchange", onHashChange);
  gotoPref();

  init_dynamic_padding();

  var initFinished = new CustomEvent("Initialized", {
    "bubbles": true,
    "cancelable": true
  });
  document.dispatchEvent(initFinished);

  // Wait until initialization of all preferences are complete before
  // notifying observers that the UI is now ready.
  Services.obs.notifyObservers(window, "advanced-pane-loaded");
}

// Make the space above the categories list shrink on low window heights
function init_dynamic_padding() {
  let categories = document.getElementById("categories");
  let catPadding = Number.parseInt(getComputedStyle(categories)
                                     .getPropertyValue("padding-top"));
  let helpButton = document.querySelector(".help-button");
  let helpButtonCS = getComputedStyle(helpButton);
  let helpHeight = Number.parseInt(helpButtonCS.height);
  let helpBottom = Number.parseInt(helpButtonCS.bottom);
  // Reduce the padding to account for less space, but due
  // to bug 1357841, the status panel will overlap the link.
  const reducedHelpButtonBottomFactor = .75;
  let reducedHelpButtonBottom = helpBottom * reducedHelpButtonBottomFactor;
  let fullHelpHeight = helpHeight + reducedHelpButtonBottom;
  let fullHeight = categories.lastElementChild.getBoundingClientRect().bottom +
                   fullHelpHeight;
  let mediaRule = `
  @media (max-height: ${fullHeight}px) {
    #categories {
      padding-top: calc(100vh - ${fullHeight - catPadding}px);
      padding-bottom: ${fullHelpHeight}px;
    }
    .help-button {
      bottom: ${reducedHelpButtonBottom / 2}px;
    }
  }
  `;
  let mediaStyle = document.createElementNS("http://www.w3.org/1999/xhtml", "html:style");
  mediaStyle.setAttribute("type", "text/css");
  mediaStyle.appendChild(document.createCDATASection(mediaRule));
  document.documentElement.appendChild(mediaStyle);
}

function telemetryBucketForCategory(category) {
  category = category.toLowerCase();
  switch (category) {
    case "applications":
    case "advanced":
    case "containers":
    case "general":
    case "privacy":
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

function gotoPref(aCategory) {
  let categories = document.getElementById("categories");
  const kDefaultCategoryInternalName = "paneGeneral";
  let hash = document.location.hash;
  // Subcategories allow for selecting smaller sections of the preferences
  // until proper search support is enabled (bug 1353954).
  let breakIndex = hash.indexOf("-");
  let subcategory = breakIndex != -1 && hash.substring(breakIndex + 1);
  if (subcategory) {
    hash = hash.substring(0, breakIndex);
  }
  let category = aCategory || hash.substr(1) || kDefaultCategoryInternalName;
  category = friendlyPrefCategoryNameToInternalName(category);

  // Updating the hash (below) or changing the selected category
  // will re-enter gotoPref.
  if (gLastHash == category && !subcategory)
    return;
  let item = categories.querySelector(".category[value=" + category + "]");
  if (!item) {
    category = kDefaultCategoryInternalName;
    item = categories.querySelector(".category[value=" + category + "]");
  }

  let helpButton = document.querySelector(".help-button");
  helpButton.setAttribute("href", getHelpLinkURL(item.getAttribute("helpTopic")));

  try {
    init_category_if_required(category);
  } catch (ex) {
    Cu.reportError("Error initializing preference category " + category + ": " + ex);
    throw ex;
  }

  let friendlyName = internalPrefCategoryNameToFriendlyName(category);
  if (gLastHash || category != kDefaultCategoryInternalName || subcategory) {
    document.location.hash = friendlyName;
  }
  // Need to set the gLastHash before setting categories.selectedItem since
  // the categories 'select' event will re-enter the gotoPref codepath.
  gLastHash = category;
  categories.selectedItem = item;
  window.history.replaceState(category, document.title);
  search(category, "data-category", subcategory, "data-subcategory");

  let mainContent = document.querySelector(".main-content");
  mainContent.scrollTop = 0;

  Services.telemetry
          .getHistogramById("FX_PREFERENCES_CATEGORY_OPENED_V2")
          .add(telemetryBucketForCategory(friendlyName));
}

function search(aQuery, aAttribute, aSubquery, aSubAttribute) {
  let mainPrefPane = document.getElementById("mainPrefPane");
  let elements = mainPrefPane.children;
  for (let element of elements) {
    // If the "data-hidden-from-search" is "true", the
    // element will not get considered during search. This
    // should only be used when an element is still under
    // development and should not be shown for any reason.
    if (element.getAttribute("data-hidden-from-search") != "true") {
      let attributeValue = element.getAttribute(aAttribute);
      if (attributeValue == aQuery) {
        if (!element.classList.contains("header") &&
             aSubquery && aSubAttribute) {
          let subAttributeValue = element.getAttribute(aSubAttribute);
          element.hidden = subAttributeValue != aSubquery;
        } else {
          element.hidden = false;
        }
      } else {
        element.hidden = true;
      }
    }
  }

  let keysets = mainPrefPane.getElementsByTagName("keyset");
  for (let element of keysets) {
    let attributeValue = element.getAttribute(aAttribute);
    if (attributeValue == aQuery)
      element.removeAttribute("disabled");
    else
      element.setAttribute("disabled", true);
  }
}

function helpButtonCommand() {
  let pane = history.state;
  let categories = document.getElementById("categories");
  let helpTopic = categories.querySelector(".category[value=" + pane + "]")
                            .getAttribute("helpTopic");
  openHelpLink(helpTopic);
}

function friendlyPrefCategoryNameToInternalName(aName) {
  if (aName.startsWith("pane"))
    return aName;
  return "pane" + aName.substring(0, 1).toUpperCase() + aName.substr(1);
}

// This function is duplicated inside of utilityOverlay.js's openPreferences.
function internalPrefCategoryNameToFriendlyName(aName) {
  return (aName || "").replace(/^pane./, function(toReplace) { return toReplace[4].toLowerCase(); });
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
function confirmRestartPrompt(aRestartToEnable, aDefaultButtonIndex,
                              aWantRevertAsCancelButton,
			      aWantRestartLaterButton) {
  let brandName = document.getElementById("bundleBrand").getString("brandShortName");
  let bundle = document.getElementById("bundlePreferences");
  let msg = bundle.getFormattedString(aRestartToEnable ?
                                      "featureEnableRequiresRestart" :
                                      "featureDisableRequiresRestart",
                                      [brandName]);
  let title = bundle.getFormattedString("shouldRestartTitle", [brandName]);
  let prompts = Cc["@mozilla.org/embedcomp/prompt-service;1"].getService(Ci.nsIPromptService);

  // Set up the first (index 0) button:
  let button0Text = bundle.getFormattedString("okToRestartButton", [brandName]);
  let buttonFlags = (Services.prompt.BUTTON_POS_0 *
                     Services.prompt.BUTTON_TITLE_IS_STRING);


  // Set up the second (index 1) button:
  let button1Text = null;
  if (aWantRevertAsCancelButton) {
    button1Text = bundle.getString("revertNoRestartButton");
    buttonFlags += (Services.prompt.BUTTON_POS_1 *
                    Services.prompt.BUTTON_TITLE_IS_STRING);
  } else {
    buttonFlags += (Services.prompt.BUTTON_POS_1 *
                    Services.prompt.BUTTON_TITLE_CANCEL);
  }

  // Set up the third (index 2) button:
  let button2Text = null;
  if (aWantRestartLaterButton) {
    button2Text = bundle.getString("restartLater");
    buttonFlags += (Services.prompt.BUTTON_POS_2 *
                    Services.prompt.BUTTON_TITLE_IS_STRING);
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

  let buttonIndex = prompts.confirmEx(window, title, msg, buttonFlags,
                                      button0Text, button1Text, button2Text,
                                      null, {});

  // If we have the second confirmation dialog for restart, see if the user
  // cancels out at that point.
  if (buttonIndex == CONFIRM_RESTART_PROMPT_RESTART_NOW) {
    let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"]
                       .createInstance(Ci.nsISupportsPRBool);
    Services.obs.notifyObservers(cancelQuit, "quit-application-requested",
                                  "restart");
    if (cancelQuit.data) {
      buttonIndex = CONFIRM_RESTART_PROMPT_CANCEL;
    }
  }
  return buttonIndex;
}
