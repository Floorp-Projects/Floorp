/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BrowserActionUtils } from "@core/utils/browser-action";
import { MenuPopup } from "./menupopup";
import profileManagerStyle from "./profile-manager.css?inline";

const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs",
);

export class gFloorpProfileManager {
  private static instance: gFloorpProfileManager;
  public static getInstance() {
    if (!gFloorpProfileManager.instance) {
      gFloorpProfileManager.instance = new gFloorpProfileManager();
    }
    return gFloorpProfileManager.instance;
  }

  private StyleElement = () => {
    return <style>{profileManagerStyle}</style>;
  };

  constructor() {
    BrowserActionUtils.createMenuToolbarButton(
      "profile-manager",
      "profile-manager",
      <MenuPopup />,
      async () => {
        const popup = document.getElementById(
          "profile-manager-popup",
        ) as XULPopupElement;
        popup?.openPopup(
          document.getElementById("profile-manager-popup"),
          "after_start",
          0,
          0,
          false,
          false,
        );
      },
      CustomizableUI.AREA_NAVBAR,
      this.StyleElement(),
      15,
    );
  }
}
