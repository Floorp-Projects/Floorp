/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var ProjectPanel = {
  // TODO: Expand function to save toggle state.
  toggleSidebar: function() {
    document.querySelector("#project-listing-panel").setAttribute("sidebar-displayed", true);
    document.querySelector("#project-listing-splitter").setAttribute("sidebar-displayed", true);
  },

  showPopup: function() {
    let deferred = promise.defer();

    let panelNode = document.querySelector("#project-panel");
    let panelVboxNode = document.querySelector("#project-panel > #project-panel-box");
    let anchorNode = document.querySelector("#project-panel-button > .panel-button-anchor");

    window.setTimeout(() => {
      // Open the popup only when the projects are added.
      // Not doing it in the next tick can cause mis-calculations
      // of the size of the panel.
      function onPopupShown() {
        panelNode.removeEventListener("popupshown", onPopupShown);
        deferred.resolve();
      }

      panelNode.addEventListener("popupshown", onPopupShown);
      panelNode.openPopup(anchorNode);
      panelVboxNode.scrollTop = 0;
    }, 0);

    return deferred.promise;
  }
};
