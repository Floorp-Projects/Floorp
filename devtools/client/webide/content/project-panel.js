/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported ProjectPanel */
var ProjectPanel = {
  // TODO: Expand function to save toggle state.
  toggleSidebar: function() {
    document.querySelector("#project-listing-panel").setAttribute("sidebar-displayed", true);
    document.querySelector("#project-listing-splitter").setAttribute("sidebar-displayed", true);
  }
};
