/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export function MenuPopup() {
  return (
    <xul:panelview
      id="profile-manager-popup"
      type="arrow"
      position="bottomleft topleft"
    >
      <xul:vbox id="profile-manager-box" flex="1">
        <xul:vbox id="profile-manager-content" flex="1">
          <xul:browser
            id="profile-switcher-browser"
            src={import.meta.env.DEV
              ? "http://localhost:5179"
              : "chrome://noraneko-profile-manager/content/index.html"}
            style="width:100%; height:100%;"
            disablehistory="true"
            disableglobalhistory="true"
            xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
            disablefullscreen="true"
            tooltip="aHTMLTooltip"
            autoscroll="false"
            messagemanagergroup="browsers"
            autocompletepopup="PopupAutoComplete"
            {...(import.meta.env.DEV
              ? {
                type: "content",
                remote: "true",
                maychangeremoteness: "true",
              }
              : {})}
          />
        </xul:vbox>
      </xul:vbox>
    </xul:panelview>
  );
}
