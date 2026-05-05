/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { manager } from "./index.ts";
import i18next from "i18next";
import { signal } from "@preact/signals";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

const TRANSLATION_KEY = "statusbar.toggle";

export function ContextMenu() {
  const label = signal(i18next.t(TRANSLATION_KEY));

  addI18nObserver(() => {
    label.value = i18next.t(TRANSLATION_KEY, { mark: "(S)" });
  });

  return (
    <xul:menuitem
      label={label.value}
      type="checkbox"
      id="toggle_statusBar"
      data-toolbar-id="nora-statusbar"
      checked={manager.showStatusBar.value}
      onCommand={() => {
        manager.showStatusBar.value = !manager.showStatusBar.value;
      }}
    />
  );
}
