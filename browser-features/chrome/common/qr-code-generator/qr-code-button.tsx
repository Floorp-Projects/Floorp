/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import qrCodeIcon from "./icons/send.svg?inline";
import i18next from "i18next";
import { createSignal } from "solid-js";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { config } from "../designs/configs.ts";
import { Show } from "solid-js";

export function QRCodePageActionButton() {
  const [tooltip, setTooltip] = createSignal(
    i18next.t("qrcode-generate-page-action"),
  );

  addI18nObserver(() => {
    setTooltip(i18next.t("qrcode-generate-page-action"));
  });

  return (
    <Show when={!config().uiCustomization.qrCode.disableButton}>
      <xul:hbox
        id="QRCodeGeneratePageAction"
        data-l10n-id="qrcode-generate-page-action"
        class="urlbar-page-action"
        style={{
          "list-style-image": `url("${qrCodeIcon}")`,
        }}
        title={tooltip()}
        role="button"
        popup="qrcode-panel"
      >
        <xul:image id="QRCodeGeneratePageAction-image" class="urlbar-icon" />
      </xul:hbox>
    </Show>
  );
}
