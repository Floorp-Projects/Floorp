/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var RuntimePanel = {
  // TODO: Expand function to save toggle state.
  toggleSidebar: function () {
    document.querySelector("#runtime-listing-panel").setAttribute("sidebar-displayed", true);
    document.querySelector("#runtime-listing-splitter").setAttribute("sidebar-displayed", true);
  }
};
