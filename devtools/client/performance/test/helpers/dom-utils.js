/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const Services = require("Services");
const { waitForMozAfterPaint } = require("devtools/client/performance/test/helpers/wait-utils");

/**
 * Checks if a DOM node is considered visible.
 */
exports.isVisible = (element) => {
  return !element.classList.contains("hidden") && !element.hidden;
};

/**
 * Appends the provided element to the provided parent node. If run in e10s
 * mode, will also wait for MozAfterPaint to make sure the tab is rendered.
 * Should be reviewed if Bug 1240509 lands.
 */
exports.appendAndWaitForPaint = function (parent, element) {
  let isE10s = Services.appinfo.browserTabsRemoteAutostart;
  if (isE10s) {
    let win = parent.ownerDocument.defaultView;
    let onMozAfterPaint = waitForMozAfterPaint(win);
    parent.appendChild(element);
    return onMozAfterPaint;
  }
  parent.appendChild(element);
  return null;
};
