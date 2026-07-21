// SPDX-License-Identifier: MPL-2.0

import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import i18next from "i18next";
import { createSignal } from "solid-js";
import silverPassIcon from "./icons/silver-pass.svg?inline";

export function SilverPassPageActionButton() {
  const [tooltip, setTooltip] = createSignal(
    i18next.t("silverPassDemo.pageActionTooltip"),
  );

  addI18nObserver(() => {
    setTooltip(i18next.t("silverPassDemo.pageActionTooltip"));
  });

  return (
    <xul:hbox
      id="SilverPassPageAction"
      class="urlbar-page-action"
      style={{ "list-style-image": `url("${silverPassIcon}")` }}
      title={tooltip()}
      aria-label={tooltip()}
      role="button"
      popup="silver-pass-panel"
    >
      <xul:image id="SilverPassPageAction-image" class="urlbar-icon" />
    </xul:hbox>
  );
}
