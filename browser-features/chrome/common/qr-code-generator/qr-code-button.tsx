/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import qrCodeIcon from "./icons/send.svg?inline";
import i18next from "i18next";
import { useSignal } from "@preact/signals";
import { useEffect } from "preact/hooks";
import type { ComponentChild } from "preact";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { config } from "../designs/configs.ts";

export function QRCodePageActionButton(): ComponentChild {
  const tooltip = useSignal(i18next.t("qrcode-generate-page-action"));

  useEffect(() => {
    addI18nObserver(() => {
      tooltip.value = i18next.t("qrcode-generate-page-action");
    });
  }, []);

  return (
    <>
      {!config.value.uiCustomization.qrCode.disableButton && (
        <xul:hbox
          id="QRCodeGeneratePageAction"
          class="urlbar-page-action"
          style={{
            "list-style-image": `url("${qrCodeIcon}")`,
          }}
          title={tooltip.value}
          role="button"
          popup="qrcode-panel"
        >
          <xul:image id="QRCodeGeneratePageAction-image" class="urlbar-icon" />
        </xul:hbox>
      )}
    </>
  );
}
