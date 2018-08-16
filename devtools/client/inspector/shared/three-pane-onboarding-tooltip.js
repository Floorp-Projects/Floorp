/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { openDocLink } = require("devtools/client/shared/link");
const { HTMLTooltip } = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/inspector.properties");

const SHOW_THREE_PANE_ONBOARDING_PREF = "devtools.inspector.show-three-pane-tooltip";

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const CONTAINER_WIDTH = 300;
const LEARN_MORE_LINK = "https://developer.mozilla.org/en-US/docs/Tools/Page_Inspector/Use_the_3-pane_inspector?utm_source=devtools&utm_medium=3-pane-onboarding";

/**
 * Three pane inspector onboarding tooltip that is shown on the 3 pane inspector toggle
 * button when the pref is on.
 */
class ThreePaneOnboardingTooltip {
  constructor(toolbox, doc) {
    this.toolbox = toolbox;
    this.doc = doc;
    this.tooltip = new HTMLTooltip(this.toolbox.doc, {
      type: "arrow",
      useXulWrapper: true,
    });

    this.onCloseButtonClick = this.onCloseButtonClick.bind(this);
    this.onLearnMoreLinkClick = this.onLearnMoreLinkClick.bind(this);

    const container = doc.createElementNS(XHTML_NS, "div");
    container.className = "three-pane-onboarding-container";

    const icon = doc.createElementNS(XHTML_NS, "span");
    icon.className = "three-pane-onboarding-icon";
    container.appendChild(icon);

    const content = doc.createElementNS(XHTML_NS, "div");
    content.className = "three-pane-onboarding-content";
    container.appendChild(content);

    const message = doc.createElementNS(XHTML_NS, "div");
    const learnMoreString = L10N.getStr("inspector.threePaneOnboarding.learnMoreLink");
    const messageString = L10N.getFormatStr("inspector.threePaneOnboarding.content",
      learnMoreString);
    const learnMoreStartIndex = messageString.indexOf(learnMoreString);

    message.append(messageString.substring(0, learnMoreStartIndex));

    this.learnMoreLink = doc.createElementNS(XHTML_NS, "a");
    this.learnMoreLink.className = "three-pane-onboarding-link";
    this.learnMoreLink.href = "#";
    this.learnMoreLink.textContent = learnMoreString;

    message.append(this.learnMoreLink);
    message.append(messageString.substring(learnMoreStartIndex + learnMoreString.length));
    content.append(message);

    this.closeButton = doc.createElementNS(XHTML_NS, "button");
    this.closeButton.className = "three-pane-onboarding-close-button devtools-button";
    container.appendChild(this.closeButton);

    this.closeButton.addEventListener("click", this.onCloseButtonClick);
    this.learnMoreLink.addEventListener("click", this.onLearnMoreLinkClick);

    this.tooltip.setContent(container, { width: CONTAINER_WIDTH });
    this.tooltip.show(this.doc.querySelector("#inspector-sidebar .sidebar-toggle"), {
      position: "top",
    });
  }

  destroy() {
    this.closeButton.removeEventListener("click", this.onCloseButtonClick);
    this.learnMoreLink.removeEventListener("click", this.onLearnMoreLinkClick);

    this.tooltip.destroy();

    this.closeButton = null;
    this.doc = null;
    this.learnMoreLink = null;
    this.toolbox = null;
    this.tooltip = null;
  }

  /**
   * Handler for the "click" event on the close button. Hides the onboarding tooltip
   * and sets the show three pane onboarding tooltip pref to false.
   */
  onCloseButtonClick() {
    Services.prefs.setBoolPref(SHOW_THREE_PANE_ONBOARDING_PREF, false);
    this.tooltip.hide();
  }

  /**
   * Handler for the "click" event on the learn more button. Hides the onboarding tooltip
   * and opens the link to the mdn page in a new tab.
   */
  onLearnMoreLinkClick() {
    Services.prefs.setBoolPref(SHOW_THREE_PANE_ONBOARDING_PREF, false);
    this.tooltip.hide();
    openDocLink(LEARN_MORE_LINK);
  }
}

module.exports = ThreePaneOnboardingTooltip;
