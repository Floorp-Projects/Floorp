/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ScrollPosition"];

const Ci = Components.interfaces;

/**
 * It provides methods to collect and restore scroll positions for single
 * frames and frame trees.
 *
 * This is a child process module.
 */
this.ScrollPosition = Object.freeze({
  /**
   * Collects scroll position data for any given |frame| in the frame hierarchy.
   *
   * @param frame (DOMWindow)
   *
   * @return {scroll: "x,y"} e.g. {scroll: "100,200"}
   *         Returns null when there is no scroll data we want to store for the
   *         given |frame|.
   */
  collect: function (frame) {
    let ifreq = frame.QueryInterface(Ci.nsIInterfaceRequestor);
    let utils = ifreq.getInterface(Ci.nsIDOMWindowUtils);
    let scrollX = {}, scrollY = {};
    utils.getScrollXY(false /* no layout flush */, scrollX, scrollY);

    if (scrollX.value || scrollY.value) {
      return {scroll: scrollX.value + "," + scrollY.value};
    }

    return null;
  },

  /**
   * Restores scroll position data for any given |frame| in the frame hierarchy.
   *
   * @param frame (DOMWindow)
   * @param value (object, see collect())
   */
  restore: function (frame, value) {
    let match;

    if (value && (match = /(\d+),(\d+)/.exec(value))) {
      frame.scrollTo(match[1], match[2]);
    }
  },

  /**
   * Restores scroll position data for the current frame hierarchy starting at
   * |root| using the given scroll position |data|.
   *
   * If the given |root| frame's hierarchy doesn't match that of the given
   * |data| object we will silently discard data for unreachable frames. We
   * may as well assign scroll positions to the wrong frames if some were
   * reordered or removed.
   *
   * @param root (DOMWindow)
   * @param data (object)
   *        {
   *          scroll: "100,200",
   *          children: [
   *            {scroll: "100,200"},
   *            null,
   *            {scroll: "200,300", children: [ ... ]}
   *          ]
   *        }
   */
  restoreTree: function (root, data) {
    if (data.hasOwnProperty("scroll")) {
      this.restore(root, data.scroll);
    }

    if (!data.hasOwnProperty("children")) {
      return;
    }

    let frames = root.frames;
    data.children.forEach((child, index) => {
      if (child && index < frames.length) {
        this.restoreTree(frames[index], child);
      }
    });
  }
});
