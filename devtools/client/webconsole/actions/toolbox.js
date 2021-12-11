/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function openNetworkPanel(messageId) {
  return ({ hud }) => {
    hud.openNetworkPanel(messageId);
  };
}

function resendNetworkRequest(messageId) {
  return ({ hud }) => {
    hud.resendNetworkRequest(messageId);
  };
}

function highlightDomElement(grip) {
  return ({ hud }) => {
    const highlighter = hud.getHighlighter();
    if (highlighter) {
      highlighter.highlight(grip);
    }
  };
}

function unHighlightDomElement(grip) {
  return ({ hud }) => {
    const highlighter = hud.getHighlighter();
    if (highlighter) {
      highlighter.unhighlight(grip);
    }
  };
}

function openNodeInInspector(contentDomReference) {
  return ({ hud }) => {
    hud.openNodeInInspector({ contentDomReference });
  };
}

module.exports = {
  highlightDomElement,
  unHighlightDomElement,
  openNetworkPanel,
  resendNetworkRequest,
  openNodeInInspector,
};
