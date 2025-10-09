/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { manager } from "./index.ts";
import i18next from "i18next";
import { createSignal } from "solid-js";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

export function QRCodePanel() {
  const [title, setTitle] = createSignal(
    i18next.t("qrcode-generate-page-action-title"),
  );

  addI18nObserver(() => {
    setTitle(i18next.t("qrcode-generate-page-action-title"));
  });

  return (
    <xul:panel
      id="qrcode-panel"
      type="arrow"
      position="bottomright topright"
      onPopupShowing={() => manager.handlePopupShowing()}
    >
      <xul:vbox id="qrcode-box">
        <xul:vbox class="panel-header qrcode-header">
          <xul:label
            data-l10n-id="qrcode-generate-page-action-title"
            class="qrcode-title"
          >
            {title()}
          </xul:label>
        </xul:vbox>

        <xul:toolbarseparator class="qrcode-separator" />

        <xul:vbox
          id="qrcode-img-vbox"
        />
      </xul:vbox>
    </xul:panel>
  );
}
