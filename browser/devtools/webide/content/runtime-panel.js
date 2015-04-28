/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let RuntimePanel = {
  // TODO: Expand function to save toggle state.
  toggleSidebar: function() {
    document.querySelector("#runtime-listing-panel").setAttribute("sidebar-displayed", true);
    document.querySelector("#runtime-listing-splitter").setAttribute("sidebar-displayed", true);
  },

  showPopup: function() {
    let deferred = promise.defer();
    let panel = document.querySelector("#runtime-panel");
    let anchor = document.querySelector("#runtime-panel-button > .panel-button-anchor");

    function onPopupShown() {
      panel.removeEventListener("popupshown", onPopupShown);
      deferred.resolve();
    }

    panel.addEventListener("popupshown", onPopupShown);
    panel.openPopup(anchor);
    return deferred.promise;
  }
};
