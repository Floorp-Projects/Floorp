/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is used as a stub object for platforms which
// don't have CAN_DRAW_IN_TITLEBAR defined.

var TabsInTitlebar = {
  init() {},
  uninit() {},
  allowedBy(condition, allow) {},
  updateAppearance: function updateAppearance(aForce) {},
  get enabled() {
    return document.documentElement.getAttribute("tabsintitlebar") == "true";
  },
};
