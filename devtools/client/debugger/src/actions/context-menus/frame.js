/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { showMenu } from "../../context-menu/menu";
import { copyToTheClipboard } from "../../utils/clipboard";
import {
  getShouldSelectOriginalLocation,
  getCurrentThreadFrames,
  getFrameworkGroupingState,
} from "../../selectors/index";
import { toggleFrameworkGrouping } from "../../actions/ui";
import { restart, toggleBlackBox } from "../../actions/pause/index";
import { formatCopyName } from "../../utils/pause/frames/index";

function formatMenuElement(labelString, click, disabled = false) {
  const label = L10N.getStr(labelString);
  const accesskey = L10N.getStr(`${labelString}.accesskey`);
  const id = `node-menu-${labelString}`;
  return {
    id,
    label,
    accesskey,
    disabled,
    click,
  };
}

function isValidRestartFrame(frame, callbacks) {
  // Any frame state than 'on-stack' is either dismissed by the server
  // or can potentially cause unexpected errors.
  // Global frame has frame.callee equal to null and can't be restarted.
  return frame.type === "call" && frame.state === "on-stack";
}

function copyStackTrace() {
  return async ({ dispatch, getState }) => {
    const frames = getCurrentThreadFrames(getState());
    const shouldDisplayOriginalLocation = getShouldSelectOriginalLocation(
      getState()
    );

    const framesToCopy = frames
      .map(frame => formatCopyName(frame, L10N, shouldDisplayOriginalLocation))
      .join("\n");
    copyToTheClipboard(framesToCopy);
  };
}

export function showFrameContextMenu(event, frame, hideRestart = false) {
  return async ({ dispatch, getState }) => {
    const items = [];

    // Hides 'Restart Frame' item for call stack groups context menu,
    // otherwise can be misleading for the user which frame gets restarted.
    if (!hideRestart && isValidRestartFrame(frame)) {
      items.push(
        formatMenuElement("restartFrame", () => dispatch(restart(frame)))
      );
    }

    const toggleFrameWorkL10nLabel = getFrameworkGroupingState(getState())
      ? "framework.disableGrouping"
      : "framework.enableGrouping";
    items.push(
      formatMenuElement(toggleFrameWorkL10nLabel, () =>
        dispatch(
          toggleFrameworkGrouping(!getFrameworkGroupingState(getState()))
        )
      )
    );

    const { source } = frame;
    if (frame.source) {
      items.push(
        formatMenuElement("copySourceUri2", () =>
          copyToTheClipboard(source.url)
        )
      );

      const toggleBlackBoxL10nLabel = source.isBlackBoxed
        ? "ignoreContextItem.unignore"
        : "ignoreContextItem.ignore";
      items.push(
        formatMenuElement(toggleBlackBoxL10nLabel, () =>
          dispatch(toggleBlackBox(source))
        )
      );
    }

    items.push(
      formatMenuElement("copyStackTrace", () => dispatch(copyStackTrace()))
    );

    showMenu(event, items);
  };
}
