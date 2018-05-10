"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = FrameMenu;

var _devtoolsContextmenu = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-contextmenu"];

var _clipboard = require("../../../utils/clipboard");

var _lodash = require("devtools/client/shared/vendor/lodash");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const blackboxString = "sourceFooter.blackbox";
const unblackboxString = "sourceFooter.unblackbox";

function formatMenuElement(labelString, click, disabled = false) {
  const label = L10N.getStr(labelString);
  const accesskey = L10N.getStr(`${labelString}.accesskey`);
  const id = `node-menu-${(0, _lodash.kebabCase)(label)}`;
  return {
    id,
    label,
    accesskey,
    disabled,
    click
  };
}

function copySourceElement(url) {
  return formatMenuElement("copySourceUri2", () => (0, _clipboard.copyToTheClipboard)(url));
}

function copyStackTraceElement(copyStackTrace) {
  return formatMenuElement("copyStackTrace", () => copyStackTrace());
}

function toggleFrameworkGroupingElement(toggleFrameworkGrouping, frameworkGroupingOn) {
  const actionType = frameworkGroupingOn ? "framework.disableGrouping" : "framework.enableGrouping";
  return formatMenuElement(actionType, () => toggleFrameworkGrouping());
}

function blackBoxSource(source, toggleBlackBox) {
  const toggleBlackBoxString = source.isBlackBoxed ? unblackboxString : blackboxString;
  return formatMenuElement(toggleBlackBoxString, () => toggleBlackBox(source));
}

function FrameMenu(frame, frameworkGroupingOn, callbacks, event) {
  event.stopPropagation();
  event.preventDefault();
  const menuOptions = [];
  const source = frame.source;
  const toggleFrameworkElement = toggleFrameworkGroupingElement(callbacks.toggleFrameworkGrouping, frameworkGroupingOn);
  menuOptions.push(toggleFrameworkElement);

  if (source) {
    const copySourceUri2 = copySourceElement(source.url);
    menuOptions.push(copySourceUri2);
    menuOptions.push(blackBoxSource(source, callbacks.toggleBlackBox));
  }

  const copyStackTraceItem = copyStackTraceElement(callbacks.copyStackTrace);
  menuOptions.push(copyStackTraceItem);
  (0, _devtoolsContextmenu.showMenu)(event, menuOptions);
}