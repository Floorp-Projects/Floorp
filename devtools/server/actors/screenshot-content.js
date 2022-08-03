/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const {
  screenshotContentSpec,
} = require("devtools/shared/specs/screenshot-content");

const { LocalizationHelper } = require("devtools/shared/l10n");
const STRINGS_URI = "devtools/shared/locales/screenshot.properties";
const L10N = new LocalizationHelper(STRINGS_URI);
loader.lazyRequireGetter(
  this,
  ["getCurrentZoom", "getRect"],
  "devtools/shared/layout/utils",
  true
);

exports.ScreenshotContentActor = ActorClassWithSpec(screenshotContentSpec, {
  initialize(conn, targetActor) {
    Actor.prototype.initialize.call(this, conn);
    this.targetActor = targetActor;
  },

  _getRectForNode(node) {
    const originWindow = this.targetActor.ignoreSubFrames
      ? node.ownerGlobal
      : node.ownerGlobal.top;
    return getRect(originWindow, node, node.ownerGlobal);
  },

  /**
   * Retrieve some window-related information that will be passed to the parent process
   * to actually generate the screenshot.
   *
   * @param {Object} args
   * @param {Boolean} args.fullpage: Should the screenshot be the height of the whole page
   * @param {String} args.selector: A CSS selector for the element we should take the
   *                 screenshot of. The function will return true for the `error` property
   *                 if the screenshot does not match any element.
   * @param {String} args.nodeActorID: The actorID of the node actor matching the element
   *                 we should take the screenshot of.
   * @returns {Object} An object with the following properties:
   *          - error {Boolean}: Set to true if an issue was encountered that prevents
   *            taking the screenshot
   *          - messages {Array<Object{text, level}>}: An array of objects representing
   *            the messages emitted throught the process and their level.
   *          - windowDpr {Number}: Value of window.devicePixelRatio
   *          - windowZoom {Number}: The page current zoom level
   *          - rect {Object}: Object with left, top, width and height properties
   *            representing the rect **inside the browser element** that should be rendered.
   *            For screenshot of the current viewport, we return null, as expected by the
   *            `drawSnapshot` API.
   */
  prepareCapture({ fullpage, selector, nodeActorID }) {
    const { window } = this.targetActor;
    // Use the override if set, note that the override is not returned by
    // devicePixelRatio on privileged code, see bug 1759962.
    //
    // FIXME(bug 1760711): Whether zoom is included in devicePixelRatio depends
    // on whether there's an override, this is a bit suspect.
    const windowDpr =
      window.browsingContext.top.overrideDPPX || window.devicePixelRatio;
    const windowZoom = getCurrentZoom(window);
    const messages = [];

    // If we're going to take the current view of the page, we don't need to compute a rect,
    // since it's the default behaviour of drawSnapshot.
    if (!fullpage && !selector && !nodeActorID) {
      return {
        rect: null,
        messages,
        windowDpr,
        windowZoom,
      };
    }

    let left;
    let top;
    let width;
    let height;

    if (fullpage) {
      // We don't want to render the scrollbars
      const winUtils = window.windowUtils;
      const scrollbarHeight = {};
      const scrollbarWidth = {};
      winUtils.getScrollbarSize(false, scrollbarWidth, scrollbarHeight);

      left = 0;
      top = 0;
      width =
        window.innerWidth +
        window.scrollMaxX -
        window.scrollMinX -
        scrollbarWidth.value;
      height =
        window.innerHeight +
        window.scrollMaxY -
        window.scrollMinY -
        scrollbarHeight.value;
    } else if (selector) {
      const node = window.document.querySelector(selector);

      if (!node) {
        messages.push({
          level: "warn",
          text: L10N.getFormatStr("screenshotNoSelectorMatchWarning", selector),
        });

        return {
          error: true,
          messages,
        };
      }

      ({ left, top, width, height } = this._getRectForNode(node));
    } else if (nodeActorID) {
      const nodeActor = this.conn.getActor(nodeActorID);
      if (!nodeActor) {
        messages.push({
          level: "error",
          text: `Screenshot actor failed to find Node actor for '${nodeActorID}'`,
        });

        return {
          error: true,
          messages,
        };
      }

      ({ left, top, width, height } = this._getRectForNode(nodeActor.rawNode));
    }

    return {
      windowDpr,
      windowZoom,
      rect: { left, top, width, height },
      messages,
    };
  },
});
