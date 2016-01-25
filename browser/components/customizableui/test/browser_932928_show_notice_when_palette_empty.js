/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// There should be an advert to get more addons when the palette is empty.
add_task(function*() {
  yield startCustomizing();
  let visiblePalette = document.getElementById("customization-palette");
  let emptyPaletteNotice = document.getElementById("customization-empty");
  is(emptyPaletteNotice.hidden, true, "The empty palette notice should not be shown when there are items in the palette.");

  while (visiblePalette.childElementCount) {
    gCustomizeMode.addToToolbar(visiblePalette.children[0]);
  }
  is(visiblePalette.childElementCount, 0, "There shouldn't be any items remaining in the visible palette.");
  is(emptyPaletteNotice.hidden, false, "The empty palette notice should be shown when there are no items in the palette.");

  yield endCustomizing();
  yield startCustomizing();
  visiblePalette = document.getElementById("customization-palette");
  emptyPaletteNotice = document.getElementById("customization-empty");
  is(emptyPaletteNotice.hidden, false,
     "The empty palette notice should be shown when there are no items in the palette and cust. mode is re-entered.");

  gCustomizeMode.removeFromArea(document.getElementById("wrapper-home-button"));
  is(emptyPaletteNotice.hidden, true,
     "The empty palette notice should not be shown when there is at least one item in the palette.");
});

add_task(function* asyncCleanup() {
  yield endCustomizing();
  yield resetCustomization();
});
