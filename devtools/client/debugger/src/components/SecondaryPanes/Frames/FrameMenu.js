/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { showMenu } from "../../../context-menu/menu";
import { copyToTheClipboard } from "../../../utils/clipboard";
import { kebabCase } from "lodash";

const blackboxString = "ignoreContextItem.ignore";
const unblackboxString = "ignoreContextItem.unignore";

function formatMenuElement(labelString, click, disabled = false) {
  const label = L10N.getStr(labelString);
  const accesskey = L10N.getStr(`${labelString}.accesskey`);
  const id = `node-menu-${kebabCase(label)}`;
  return {
    id,
    label,
    accesskey,
    disabled,
    click,
  };
}

function copySourceElement(url) {
  return formatMenuElement("copySourceUri2", () => copyToTheClipboard(url));
}

function copyStackTraceElement(copyStackTrace) {
  return formatMenuElement("copyStackTrace", () => copyStackTrace());
}

function toggleFrameworkGroupingElement(
  toggleFrameworkGrouping,
  frameworkGroupingOn
) {
  const actionType = frameworkGroupingOn
    ? "framework.disableGrouping"
    : "framework.enableGrouping";

  return formatMenuElement(actionType, () => toggleFrameworkGrouping());
}

function blackBoxSource(cx, source, toggleBlackBox) {
  const toggleBlackBoxString = source.isBlackBoxed
    ? unblackboxString
    : blackboxString;

  return formatMenuElement(toggleBlackBoxString, () =>
    toggleBlackBox(cx, source)
  );
}

function restartFrame(cx, frame, restart) {
  return formatMenuElement("restartFrame", () => restart(cx, frame));
}

function isValidRestartFrame(frame, callbacks) {
  // Hides 'Restart Frame' item for call stack groups context menu,
  // otherwise can be misleading for the user which frame gets restarted.
  if (!callbacks.restart) {
    return false;
  }

  // Any frame state than 'on-stack' is either dismissed by the server
  // or can potentially cause unexpected errors.
  // Global frame has frame.callee equal to null and can't be restarted.
  return frame.type === "call" && frame.state === "on-stack";
}

export default function FrameMenu(
  frame,
  frameworkGroupingOn,
  callbacks,
  event,
  cx
) {
  event.stopPropagation();
  event.preventDefault();

  const menuOptions = [];

  if (isValidRestartFrame(frame, callbacks)) {
    const restartFrameItem = restartFrame(cx, frame, callbacks.restart);
    menuOptions.push(restartFrameItem);
  }

  const toggleFrameworkElement = toggleFrameworkGroupingElement(
    callbacks.toggleFrameworkGrouping,
    frameworkGroupingOn
  );
  menuOptions.push(toggleFrameworkElement);

  const { source } = frame;
  if (source) {
    const copySourceUri2 = copySourceElement(source.url);
    menuOptions.push(copySourceUri2);
    menuOptions.push(blackBoxSource(cx, source, callbacks.toggleBlackBox));
  }

  const copyStackTraceItem = copyStackTraceElement(callbacks.copyStackTrace);

  menuOptions.push(copyStackTraceItem);

  showMenu(event, menuOptions);
}
