/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  HTMLTooltip,
} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");

const { getStr } = require("./utils/l10n");

const SHOW_SETTING_TOOLTIP_PREF = "devtools.responsive.show-setting-tooltip";

const CONTAINER_WIDTH = 270;

/**
 * Setting onboarding tooltip that is shown on the setting menu button in the RDM toolbar
 * when the pref is on.
 */
class SettingOnboardingTooltip {
  constructor(doc) {
    this.doc = doc;
    this.tooltip = new HTMLTooltip(this.doc, {
      consumeOutsideClicks: false,
      type: "arrow",
    });

    this.onCloseButtonClick = this.onCloseButtonClick.bind(this);

    const container = doc.createElement("div");
    container.className = "onboarding-container";

    const icon = doc.createElement("span");
    icon.className = "onboarding-icon";
    container.appendChild(icon);

    const content = doc.createElement("div");
    content.className = "onboarding-content";
    content.textContent = getStr("responsive.settingOnboarding.content");
    container.appendChild(content);

    this.closeButton = doc.createElement("button");
    this.closeButton.className = "onboarding-close-button devtools-button";
    container.appendChild(this.closeButton);

    this.closeButton.addEventListener("click", this.onCloseButtonClick);

    this.tooltip.panel.appendChild(container);
    this.tooltip.setContentSize({ width: CONTAINER_WIDTH });
    this.tooltip.show(this.doc.getElementById("settings-button"), {
      position: "bottom",
    });
  }

  destroy() {
    this.closeButton.removeEventListener("click", this.onCloseButtonClick);

    this.tooltip.destroy();

    this.closeButton = null;
    this.doc = null;
    this.tooltip = null;
  }

  /**
   * Handler for the "click" event on the close button. Hides the onboarding tooltip
   * and sets the show three pane onboarding tooltip pref to false.
   */
  onCloseButtonClick() {
    Services.prefs.setBoolPref(SHOW_SETTING_TOOLTIP_PREF, false);
    this.tooltip.hide();
  }
}

module.exports = SettingOnboardingTooltip;
