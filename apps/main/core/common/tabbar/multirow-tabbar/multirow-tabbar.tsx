/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "../../designs/configs";
import { createEffect } from "solid-js";

export class gMultirowTabbarClass {
  private static instance: gMultirowTabbarClass;
  public static getInstance() {
    if (!gMultirowTabbarClass.instance) {
      gMultirowTabbarClass.instance = new gMultirowTabbarClass();
    }
    return gMultirowTabbarClass.instance;
  }

  private get arrowScrollbox(): XULElement | null {
    return document.querySelector("#tabbrowser-arrowscrollbox");
  }

  private get scrollboxPart(): XULElement | null {
    return this.arrowScrollbox
      ? this.arrowScrollbox.querySelector("[part=scrollbox]")
      : null;
  }

  private get aTabHeight(): number {
    return (
      document.querySelector(".tabbrowser-tab:not([hidden='true'])")
        ?.clientHeight || 30
    );
  }

  private get isMaxRowEnabled(): boolean {
    return config().tabbar.multiRowTabBar.maxRowEnabled;
  }
  private get getMultirowTabMaxHeight(): number {
    return config().tabbar.multiRowTabBar.maxRow * this.aTabHeight;
  }

  private setMultirowTabMaxHeight() {
    if (!this.isMaxRowEnabled) {
      return;
    }
    this.scrollboxPart?.setAttribute(
      "style",
      `max-height: ${this.getMultirowTabMaxHeight}px !important;`,
    );
    this.arrowScrollbox?.style.setProperty(
      "max-height",
      `${this.getMultirowTabMaxHeight}px`
    );
  }

  private removeMultirowTabMaxHeight() {
    this.scrollboxPart?.removeAttribute("style");
    this.arrowScrollbox?.style.removeProperty("max-height");
  }

  constructor() {
    createEffect(() => {
      config().tabbar.tabbarStyle === "multirow"
        ? this.setMultirowTabMaxHeight()
        : this.removeMultirowTabMaxHeight();
    });
  }
}
