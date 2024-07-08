/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export function MenuPopup() {
  return (
    <xul:menupopup id="profile-manager-popup">
      <xul:browser
        id="profile-switcher-browser"
        src="chrome://floorp/content/profile-manager/profile-switcher.xhtml"
        flex="1"
        type="content"
        disablehistory="true"
        disableglobalhistory="true"
        context="profile-popup-contextmenu"
      />
    </xul:menupopup>
  );
}
