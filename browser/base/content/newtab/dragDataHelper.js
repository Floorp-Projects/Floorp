#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

let gDragDataHelper = {
  get mimeType() {
    return "text/x-moz-url";
  },

  getLinkFromDragEvent: function DragDataHelper_getLinkFromDragEvent(aEvent) {
    let dt = aEvent.dataTransfer;
    if (!dt || !dt.types.contains(this.mimeType)) {
      return null;
    }

    let data = dt.getData(this.mimeType) || "";
    let [url, title] = data.split(/[\r\n]+/);
    return {url: url, title: title};
  }
};
