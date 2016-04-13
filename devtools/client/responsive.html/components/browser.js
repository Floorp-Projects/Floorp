/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { Task } = require("resource://gre/modules/Task.jsm");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { getToplevelWindow } = require("sdk/window/utils");
const { DOM: dom, createClass, addons, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");
const { waitForMessage } = require("../utils/e10s");

module.exports = createClass({
  /**
   * This component is not allowed to depend directly on frequently changing
   * data (width, height) due to the use of `dangerouslySetInnerHTML` below.
   * Any changes in props will cause the <iframe> to be removed and added again,
   * throwing away the current state of the page.
   */
  propTypes: {
    location: Types.location.isRequired,
    onBrowserMounted: PropTypes.func.isRequired,
    onContentResize: PropTypes.func.isRequired,
  },

  displayName: "Browser",

  mixins: [ addons.PureRenderMixin ],

  /**
   * Once the browser element has mounted, load the frame script and enable
   * various features, like floating scrollbars.
   */
  componentDidMount: Task.async(function* () {
    let { onContentResize } = this;
    let browser = this.refs.browserContainer.querySelector("iframe.browser");
    let mm = browser.frameLoader.messageManager;

    // Notify tests when the content has received a resize event.  This is not
    // quite the same timing as when we _set_ a new size around the browser,
    // since it still needs to do async work before the content is actually
    // resized to match.
    mm.addMessageListener("ResponsiveMode:OnContentResize", onContentResize);

    let ready = waitForMessage(mm, "ResponsiveMode:ChildScriptReady");
    mm.loadFrameScript("resource://devtools/client/responsivedesign/" +
                       "responsivedesign-child.js", true);
    yield ready;

    let browserWindow = getToplevelWindow(window);
    let requiresFloatingScrollbars =
      !browserWindow.matchMedia("(-moz-overlay-scrollbars)").matches;
    let started = waitForMessage(mm, "ResponsiveMode:Start:Done");
    mm.sendAsyncMessage("ResponsiveMode:Start", {
      requiresFloatingScrollbars,
      // Tests expect events on resize to yield on various size changes
      notifyOnResize: DevToolsUtils.testing,
    });
    yield started;

    // manager.js waits for this signal before allowing browser tests to start
    this.props.onBrowserMounted();
  }),

  componentWillUnmount() {
    let { onContentResize } = this;
    let browser = this.refs.browserContainer.querySelector("iframe.browser");
    let mm = browser.frameLoader.messageManager;
    mm.removeMessageListener("ResponsiveMode:OnContentResize", onContentResize);
    mm.sendAsyncMessage("ResponsiveMode:Stop");
  },

  onContentResize(msg) {
    let { onContentResize } = this.props;
    let { width, height } = msg.data;
    onContentResize({
      width,
      height,
    });
  },

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
         */
        dangerouslySetInnerHTML: {
          __html: `<iframe class="browser" mozbrowser="true" remote="true"
                           noisolation="true" src="${location}"
                           width="100%" height="100%"></iframe>`
        }
      }
    );
  },

});
