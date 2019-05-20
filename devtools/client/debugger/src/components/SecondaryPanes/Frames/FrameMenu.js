/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import { showMenu } from "devtools-contextmenu";
import { copyToTheClipboard } from "../../../utils/clipboard";
import type { ContextMenuItem, Frame } from "../../../types";
import { kebabCase } from "lodash";

const blackboxString = "sourceFooter.blackbox";
const unblackboxString = "sourceFooter.unblackbox";

function formatMenuElement(
  labelString: string,
  click: Function,
  disabled: boolean = false
): ContextMenuItem {
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

function blackBoxSource(source, toggleBlackBox) {
  const toggleBlackBoxString = source.isBlackBoxed
    ? unblackboxString
    : blackboxString;

  return formatMenuElement(toggleBlackBoxString, () => toggleBlackBox(source));
}

export default function FrameMenu(
  frame: Frame,
  frameworkGroupingOn: boolean,
  callbacks: Object,
  event: SyntheticMouseEvent<HTMLElement>
) {
  event.stopPropagation();
  event.preventDefault();

  const menuOptions = [];

  const source = frame.source;

  const toggleFrameworkElement = toggleFrameworkGroupingElement(
    callbacks.toggleFrameworkGrouping,
    frameworkGroupingOn
  );
  menuOptions.push(toggleFrameworkElement);

  if (source) {
    const copySourceUri2 = copySourceElement(source.url);
    menuOptions.push(copySourceUri2);
    menuOptions.push(blackBoxSource(source, callbacks.toggleBlackBox));
  }

  const copyStackTraceItem = copyStackTraceElement(callbacks.copyStackTrace);

  menuOptions.push(copyStackTraceItem);

  showMenu(event, menuOptions);
}
