/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { Task } = require("devtools/shared/task");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { getToplevelWindow } = require("sdk/window/utils");
const { DOM: dom, createClass, addons, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");
const e10s = require("../utils/e10s");
const message = require("../utils/message");

module.exports = createClass({

  /**
   * This component is not allowed to depend directly on frequently changing
   * data (width, height) due to the use of `dangerouslySetInnerHTML` below.
   * Any changes in props will cause the <iframe> to be removed and added again,
   * throwing away the current state of the page.
   */
  displayName: "Browser",

  propTypes: {
    location: Types.location.isRequired,
    swapAfterMount: PropTypes.bool.isRequired,
    onBrowserMounted: PropTypes.func.isRequired,
    onContentResize: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  /**
   * Once the browser element has mounted, load the frame script and enable
   * various features, like floating scrollbars.
   */
  componentDidMount: Task.async(function* () {
    // If we are not swapping browsers after mount, it's safe to start the frame
    // script now.
    if (!this.props.swapAfterMount) {
      yield this.startFrameScript();
    }

    // Notify manager.js that this browser has mounted, so that it can trigger
    // a swap if needed and continue with the rest of its startup.
    this.props.onBrowserMounted();

    // If we are swapping browsers after mount, wait for the swap to complete
    // and start the frame script after that.
    if (this.props.swapAfterMount) {
      yield message.wait(window, "start-frame-script");
      yield this.startFrameScript();
      message.post(window, "start-frame-script:done");
    }

    // Stop the frame script when requested in the future.
    message.wait(window, "stop-frame-script").then(() => {
      this.stopFrameScript();
    });
  }),

  onContentResize(msg) {
    let { onContentResize } = this.props;
    let { width, height } = msg.data;
    onContentResize({
      width,
      height,
    });
  },

  startFrameScript: Task.async(function* () {
    let { onContentResize } = this;
    let browser = this.refs.browserContainer.querySelector("iframe.browser");
    let mm = browser.frameLoader.messageManager;

    // Notify tests when the content has received a resize event.  This is not
    // quite the same timing as when we _set_ a new size around the browser,
    // since it still needs to do async work before the content is actually
    // resized to match.
    e10s.on(mm, "OnContentResize", onContentResize);

    let ready = e10s.once(mm, "ChildScriptReady");
    mm.loadFrameScript("resource://devtools/client/responsivedesign/" +
                       "responsivedesign-child.js", true);
    yield ready;

    let browserWindow = getToplevelWindow(window);
    let requiresFloatingScrollbars =
      !browserWindow.matchMedia("(-moz-overlay-scrollbars)").matches;

    yield e10s.request(mm, "Start", {
      requiresFloatingScrollbars,
      // Tests expect events on resize to yield on various size changes
      notifyOnResize: DevToolsUtils.testing,
    });
  }),

  stopFrameScript: Task.async(function* () {
    let { onContentResize } = this;
    let browser = this.refs.browserContainer.querySelector("iframe.browser");
    let mm = browser.frameLoader.messageManager;
    e10s.off(mm, "OnContentResize", onContentResize);
    yield e10s.request(mm, "Stop");
    message.post(window, "stop-frame-script:done");
  }),

  render() {
    let {
      location,
    } = this.props;

    // innerHTML expects & to be an HTML entity
    location = location.replace(/&/g, "&amp;");

    return dom.div(
      {
        ref: "browserContainer",
        className: "browser-container",

        /**
         * React uses a whitelist for attributes, so we need some way to set
         * attributes it does not know about, such as @mozbrowser.  If this were
         * the only issue, we could use componentDidMount or ref: node => {} to
         * set the atttibutes. In the case of @remote, the attribute must be set
         * before the element is added to the DOM to have any effect, which we
         * are able to do with this approach.
         *
         * @noisolation and @allowfullscreen are needed so that these frames
         * have the same access to browser features as regular browser tabs.
         * The `swapFrameLoaders` platform API we use compares such features
         * before allowing the swap to proceed.
         */
        dangerouslySetInnerHTML: {
          __html: `<iframe class="browser" mozbrowser="true" remote="true"
                           noisolation="true" allowfullscreen="true"
                           src="${location}" width="100%" height="100%">
                   </iframe>`
        }
      }
    );
  },

});
