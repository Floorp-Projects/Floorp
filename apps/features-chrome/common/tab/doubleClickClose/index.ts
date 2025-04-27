/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "#features-chrome/common/designs/configs";
import { createEffect } from "solid-js";

export class TabDoubleClickClose {
  constructor() {
    createEffect(() => {
      const option = config().tab.tabDubleClickToClose;
      Services.prefs.setBoolPref("browser.tabs.closeTabByDblclick", option);
    });
  }
}
