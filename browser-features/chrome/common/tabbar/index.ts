/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { MultirowTabbarClass } from "./multirow-tabbar/multirow-tabbar";
import { TabbarStyleClass } from "./tabbbar-style/tabbar-style";
import { initializeTabbarWhenReady } from "./lifecycle.ts";

@noraComponent("TabBar", import.meta.hot)
export default class TabBar extends NoraComponentBase {
  init() {
    initializeTabbarWhenReady(globalThis.SessionStore.promiseInitialized, () => {
      new TabbarStyleClass();
      new MultirowTabbarClass();
    });
  }
}
