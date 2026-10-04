/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useEffect } from "preact/hooks";
import { manager } from "./index.ts";
import i18next from "i18next";
import { useSignal } from "@preact/signals";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

const TRANSLATION_KEY = "statusbar.toggle";

export function ContextMenu() {
  const label = useSignal(i18next.t(TRANSLATION_KEY));

  useEffect(() => addI18nObserver(() => {
    label.value = i18next.t(TRANSLATION_KEY, { mark: "(S)" });
  }), []);

  return (
    <xul:menuitem
      label={label.value}
      type="checkbox"
      id="toggle_statusBar"
      data-floorp-context-menu-key="floorp.statusbar.toggle"
      data-toolbar-id="nora-statusbar"
      checked={manager.showStatusBar.value}
      onCommand={() => {
        manager.showStatusBar.value = !manager.showStatusBar.value;
      }}
    />
  );
}
