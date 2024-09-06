/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BrowserActionUtils } from "@core/utils/browser-action";
import { PopupElement } from "./popup-element";
import workspacesStyles from "./styles.css?inline";
import type { JSX } from "solid-js";

const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs",
);

export class workspacesToolbarButton {
  private static instance: workspacesToolbarButton;
  public static getInstance() {
    if (!workspacesToolbarButton.instance) {
      workspacesToolbarButton.instance = new workspacesToolbarButton();
    }
    return workspacesToolbarButton.instance;
  }

  private StyleElement = () => {
    return <style>{workspacesStyles}</style>;
  };

  constructor() {
    BrowserActionUtils.createMenuToolbarButton(
      "workspaces-toolbar-button",
      "workspaces-toolbar-button",
      "workspacesToolbarButtonPanel",
      <PopupElement />,
      null,
      null,
      CustomizableUI.AREA_TABSTRIP,
      this.StyleElement() as JSX.Element,
      -1,
    );
  }
}
