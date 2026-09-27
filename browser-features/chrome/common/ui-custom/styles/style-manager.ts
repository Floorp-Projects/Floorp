/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect } from "solid-js";
import {
  config,
  getChromeExtrasSettings,
} from "#features-chrome/common/designs/configs.ts";
import {
  buildChromeExtrasCSS,
  CHROME_EXTRAS_STYLE_ID,
} from "#features-chrome/common/designs/chrome-extras.ts";

import navbarBottomCSS from "./css/options/navbar-botttom.css?inline";
import movePageInsideSearchbarCSS from "./css/options/move_page_inside_searchbar.css?inline";
import treestyletabCSS from "./css/options/treestyletab.css?inline";
import msbuttonCSS from "./css/options/msbutton.css?inline";
import disableFullScreenNotificationCSS from "./css/options/disableFullScreenNotification.css?inline";
import deleteBorderCSS from "./css/options/delete-border.css?inline";
import stgLikeFloorpWorkspacesCSS from "./css/options/STG-like-floorp-workspaces.css?inline";
import multirowTabShowNewtabInTabbarCSS from "./css/options/multirowtab-show-newtab-button-in-tabbar.css?inline";
import multirowTabShowNewtabAtEndCSS from "./css/options/multirowtab-show-newtab-button-at-end.css?inline";
import multirowTabLeptonFixCSS from "./css/options/multirowtab-lepton-fix.css?inline";
import multirowTabMacOSWindowControlsCSS from "./css/options/multirowtab-macos-window-controls.css?inline";
import bookmarkbarFocusExpandCSS from "./css/options/bookmarkbar_focus_expand.css?inline";
import bookmarkbarBottomCSS from "./css/options/bookmarkbar_bottom.css?inline";

export class StyleManager {
  private styleElements: Map<string, HTMLStyleElement> = new Map();

  setupStyleEffects() {
    this.setupNavbarEffects();
    this.setupSearchbarEffects();
    this.setupDisplayEffects();
    this.setupSpecialEffects();
    this.setupMultirowTabEffects();
    this.setupBookmarkBarEffects();
    // Chrome extras goes LAST so its stylesheet ends up at the end of <head> and
    // wins over both the design CSS and the options above.
    this.setupChromeExtrasEffects();
  }

  private setupNavbarEffects() {
    createEffect(() => {
      this.applyStyle(
        "floorp-navvarcss",
        navbarBottomCSS,
        config().uiCustomization.navbar.position === "bottom",
      );
    });
  }

  private setupSearchbarEffects() {
    createEffect(() => {
      this.applyStyle(
        "floorp-searchbartop",
        movePageInsideSearchbarCSS,
        config().uiCustomization.navbar.searchBarTop,
      );
    });
  }

  private setupDisplayEffects() {
    createEffect(() => {
      this.applyStyle(
        "floorp-DFSN",
        disableFullScreenNotificationCSS,
        config().uiCustomization.display.disableFullscreenNotification,
      );

      this.applyStyle(
        "floorp-DB",
        deleteBorderCSS,
        config().uiCustomization.display.deleteBrowserBorder,
      );


    });
  }

  private setupSpecialEffects() {
    createEffect(() => {
      this.applyStyle(
        "floorp-optimizefortreestyletab",
        treestyletabCSS,
        config().uiCustomization.special.optimizeForTreeStyleTab,
      );

      this.applyStyle(
        "floorp-hideForwardBackwardButton",
        msbuttonCSS,
        config().uiCustomization.special.hideForwardBackwardButton,
      );

      this.applyStyle(
        "floorp-STG-like-floorp-workspaces",
        stgLikeFloorpWorkspacesCSS,
        config().uiCustomization.special.stgLikeWorkspaces,
      );
    });
  }

  private setupMultirowTabEffects() {
    createEffect(() => {
      const isMultirowStyle = config().tabbar.tabbarStyle === "multirow";
      const newtabInsideEnabled =
        config().uiCustomization.multirowTab.newtabInsideEnabled;

      this.applyStyle(
        "floorp-newtabbuttoninmultirowtabbbar",
        multirowTabShowNewtabInTabbarCSS,
        newtabInsideEnabled && isMultirowStyle,
      );

      this.applyStyle(
        "floorp-newtabbuttonatendofmultirowtabbar",
        multirowTabShowNewtabAtEndCSS,
        !newtabInsideEnabled && isMultirowStyle,
      );

      this.applyStyle(
        "floorp-multirowtabforlepton",
        multirowTabLeptonFixCSS,
        isMultirowStyle,
      );

      this.applyStyle(
        "floorp-multirowtabmacoswindowcontrols",
        multirowTabMacOSWindowControlsCSS,
        isMultirowStyle,
      );
    });
  }

  private setupBookmarkBarEffects() {
    createEffect(() => {
      this.applyStyle(
        "floorp-bookmarkbar-focus-expand",
        bookmarkbarFocusExpandCSS,
        config().uiCustomization.bookmarkBar?.focusExpand ?? false,
      );

      this.applyStyle(
        "floorp-bookmarkbar-bottom",
        bookmarkbarBottomCSS,
        (config().uiCustomization.bookmarkBar?.position ?? "top") === "bottom",
      );
    });
  }

  applyStyle(id: string, cssContent: string, condition: boolean) {
    if (condition) {
      this.createStyle(id, cssContent);
    } else {
      this.removeStyle(id);
    }
  }

  /**
   * Move an existing style element to the end of `<head>` so it keeps winning
   * the cascade. Moving a node does not re-parse its stylesheet.
   *
   * Public because the design sheets can land after ours (see
   * `setupChromeExtrasEffects`), so re-appending is part of this class's
   * contract rather than an internal detail.
   */
  reappendStyle(id: string) {
    if (!document) {
      return;
    }
    const element = document.getElementById(id);
    if (element && document.head) {
      document.head.appendChild(element);
    }
  }

  /**
   * The 25 chrome-extras toggles. All of them share one `<style>` whose content
   * is rebuilt whenever a toggle changes — see `designs/chrome-extras.ts`.
   *
   * The second effect exists because of how the design sheets are inserted:
   * `browser-design-element.tsx` renders them through `<For>` inside
   * `solid-js/universal`, and when the design changes the reconciler anchors on
   * "the sibling after the last old link". Under `proton` the following `<Show>`
   * renders nothing, so that anchor is `null` and `insertBefore(node, null)`
   * appends the new `<link>` at the END of `<head>` — after this stylesheet.
   * Re-appending on every design change puts chrome-extras back on top.
   */
  private setupChromeExtrasEffects() {
    createEffect(() => {
      const css = buildChromeExtrasCSS(getChromeExtrasSettings());
      this.applyStyle(CHROME_EXTRAS_STYLE_ID, css, true);
    });

    createEffect(() => {
      void config().globalConfigs.userInterface;
      this.reappendStyle(CHROME_EXTRAS_STYLE_ID);
    });
  }

  private createStyle(id: string, cssContent: string) {
    // Remove existing style before creating new one
    this.removeStyle(id);

    if (!document || !document.head) {
      console.warn("Document or document.head not available");
      return;
    }

    try {
      const styleTag = document.createElement("style");
      styleTag.setAttribute("id", id);
      styleTag.textContent = cssContent;
      document.head.appendChild(styleTag);
      this.styleElements.set(id, styleTag);
    } catch (error) {
      console.error(`Failed to create style with id: ${id}`, error);
    }
  }

  private createInlineStyle(id: string, cssContent: string) {
    // Use the same createStyle method since they do the same thing
    this.createStyle(id, cssContent);
  }

  private removeStyle(id: string) {
    if (!document) {
      console.warn("Document not available");
      return;
    }

    try {
      // Remove from DOM
      const existingElement = document.getElementById(id);
      if (existingElement) {
        existingElement.remove();
      }

      // Remove from internal map
      this.styleElements.delete(id);
    } catch (error) {
      console.error(`Failed to remove style with id: ${id}`, error);
    }
  }
}
