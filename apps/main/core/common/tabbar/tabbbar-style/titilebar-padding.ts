/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "../../designs/configs";

export function checkPaddingEnabled() {
  const isPaddingTopEnabled = config().tabbar.verticalTabBar.paddingEnabled;
  const titlebar: null | XULElement = document.querySelector("#titlebar");

  titlebar?.style.setProperty("padding", isPaddingTopEnabled ? "5px" : "0px");
}
