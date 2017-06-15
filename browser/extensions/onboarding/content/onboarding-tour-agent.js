/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* globals Mozilla */

"use strict";

document.getElementById("onboarding-overlay-dialog")
                        .addEventListener("click", evt => {
  switch (evt.target.id) {
    case "onboarding-tour-addons-button":
      Mozilla.UITour.showHighlight("addons");
      break;
    case "onboarding-tour-customize-button":
      Mozilla.UITour.showHighlight("customize");
      break;
   case "onboarding-tour-default-browser-button":
      Mozilla.UITour.setConfiguration("defaultBrowser");
      break;
    case "onboarding-tour-private-browsing-button":
      Mozilla.UITour.showHighlight("privateWindow");
      break;
    case "onboarding-tour-search-button":
      Mozilla.UITour.openSearchPanel(() => {});
      break;
  }
});
