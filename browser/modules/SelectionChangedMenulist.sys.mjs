/* - This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/. */

export class SelectionChangedMenulist {
  // A menulist wrapper that will open the popup when navigating with the
  // keyboard on Windows and trigger the provided handler when the popup
  // is hiding. This matches the behaviour of MacOS and Linux more closely.

  constructor(menulist, onCommand) {
    let popup = menulist.menupopup;
    let lastEvent;

    menulist.addEventListener("command", event => {
      lastEvent = event;
      if (popup.state != "open" && popup.state != "showing") {
        popup.openPopup();
      }
    });

    popup.addEventListener("popuphiding", () => {
      if (lastEvent) {
        onCommand(lastEvent);
        lastEvent = null;
      }
    });
  }
}
