/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getContainerLabel } from "./containerUtils.ts";
import { getContainerColorName } from "#features-chrome/common/workspaces/utils/container-color.ts";
import i18next from "i18next";

export function SsbWindowContainerIndicator(props: {
  userContextId: number;
}) {
  const label = () => getContainerLabel(props.userContextId);
  const colorName = () => getContainerColorName(props.userContextId) ?? "blue";
  const displayLabel = () =>
    label() ?? i18next.t("ssb.window.container-fallback", {
      id: props.userContextId,
    });

  return (
    <xul:label
      id="ssb-container-indicator"
      class="ssb-container-indicator"
      data-container-color={colorName()}
      value={displayLabel()}
      crop="end"
      tooltiptext={i18next.t("ssb.window.container-tooltip", {
        name: displayLabel(),
      })}
    />
  );
}

export function getSsbWindowTitle(
  appName: string,
  userContextId: number,
): string {
  const label = getContainerLabel(userContextId);
  return label ? `${appName} (${label})` : appName;
}
