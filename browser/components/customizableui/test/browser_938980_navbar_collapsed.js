/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Customization reset should restore visibility to default-visible toolbars.
add_task(function() {
  let navbar = document.getElementById("nav-bar");
  is(navbar.collapsed, false, "Test should start with navbar visible");
  navbar.collapsed = true;
  is(navbar.collapsed, true, "navbar should be hidden now");

  yield resetCustomization();

  is(navbar.collapsed, false, "Customization reset should restore visibility to the navbar");
});
