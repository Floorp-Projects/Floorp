/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { gMultirowTabbarClass } from "./multirow-tabbar/multirow-tabbar";
import { gVerticalTabbarClass } from "./vertical-tabbar/vertical-tabbar";
import { gTabbarStyleClass } from "./tabbbar-style/tabbar-style";

export function initTabbar() {
    gTabbarStyleClass.getInstance();
    gMultirowTabbarClass.getInstance();
    gVerticalTabbarClass.getInstance();
}
