/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let ContentAreaDownloadsView = {
  init: function CADV_init() {
    let view = new DownloadsPlacesView(document.getElementById("downloadsRichListBox"));
    view.place = "place:transition=7&sort=4";
  }
};
