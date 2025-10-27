/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BrowserActionUtils } from "#features-chrome/utils/browser-action.tsx";
import { MenuPopup } from "./components/popup.tsx";
import toolbarStyles from "./styles.css?inline";
import type { JSX } from "solid-js";

const { CustomizableUI } = ChromeUtils.importESModule(
  "moz-src:///browser/components/customizableui/CustomizableUI.sys.mjs",
);

export class ProfileManagerService {
  private static _instance: ProfileManagerService | null = null;

  static getInstance(): ProfileManagerService {
    if (!this._instance) this._instance = new ProfileManagerService();
    return this._instance;
  }

  private StyleElement = () => {
    return <style>{toolbarStyles}</style>;
  };


  init(): void {
    try {
      BrowserActionUtils.createMenuToolbarButton(
        "profile-manager-button",
        "profile-manager-button",
        "profile-manager-popup",
        <MenuPopup />,
        null,
        null,
        CustomizableUI.AREA_NAVBAR,
        this.StyleElement() as JSX.Element,
        0,
      );
    } catch (e) {
      console.error("Failed to initialize ProfileManagerService:", e);
    }
  }
}

export default ProfileManagerService;
