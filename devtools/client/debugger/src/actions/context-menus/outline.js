/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { showMenu } from "../../context-menu/menu";
import { copyToTheClipboard } from "../../utils/clipboard";
import { findFunctionText } from "../../utils/function";

import {
  getSelectedLocation,
  getSelectedSource,
  getSymbols,
  getSelectedSourceTextContent,
} from "../../selectors";

import { flashLineRange } from "../../actions/ui";

export function showOutlineContextMenu(event, func) {
  return async ({ dispatch, getState }) => {
    const state = getState();

    const selectedSource = getSelectedSource(state);
    if (!selectedSource) {
      return;
    }
    const symbols = getSymbols(state, getSelectedLocation(state));
    const selectedSourceTextContent = getSelectedSourceTextContent(state);

    const sourceLine = func.location.start.line;
    const functionText = findFunctionText(
      sourceLine,
      selectedSource,
      selectedSourceTextContent,
      symbols
    );

    const copyFunctionItem = {
      id: "node-menu-copy-function",
      label: L10N.getStr("copyFunction.label"),
      accesskey: L10N.getStr("copyFunction.accesskey"),
      disabled: !functionText,
      click: () => {
        dispatch(
          flashLineRange({
            start: sourceLine,
            end: func.location.end.line,
            sourceId: selectedSource.id,
          })
        );
        return copyToTheClipboard(functionText);
      },
    };

    const items = [copyFunctionItem];
    showMenu(event, items);
  };
}
