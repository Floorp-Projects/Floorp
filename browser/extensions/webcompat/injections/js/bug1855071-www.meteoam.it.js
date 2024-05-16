/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * www.meteoam.it - virtual keyboard is hidden as it is opened
 * webcompat issue #121197 - https://webcompat.com/issues/121197
 *
 * the site's map is 75vh tall, and it hides the keyboard onresize,
 * meaning the keyboard is closed as it it brought up in Firefox.
 */

console.info(
  "Map iframe height is being managed for compatibility reasons. see https://webcompat.com/issues/77221 for details."
);

const selector = "#iframe_map";

const moOptions = {
  childList: true,
  subtree: true,
};

const mo = new MutationObserver(() => {
  const map = document.querySelector(selector);
  let lastSize;
  if (map) {
    mo.disconnect();
    const maybeGrowMap = () => {
      const winHeight = window.outerHeight;
      if (lastSize && lastSize > winHeight) {
        return;
      }
      map.style.height = winHeight * 0.75 + "px";
      lastSize = winHeight;
    };
    maybeGrowMap();
    window.addEventListener("resize", () =>
      window.requestAnimationFrame(maybeGrowMap)
    );
  }
});

mo.observe(document.documentElement, moOptions);
