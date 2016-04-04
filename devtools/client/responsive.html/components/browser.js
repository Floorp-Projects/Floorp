/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, addons } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");

module.exports = createClass({

  displayName: "Browser",

  mixins: [ addons.PureRenderMixin ],

  /**
   * This component is not allowed to depend directly on frequently changing
   * data (width, height) due to the use of `dangerouslySetInnerHTML` below.
   * Any changes in props will cause the <iframe> to be removed and added again,
   * throwing away the current state of the page.
   */
  propTypes: {
    location: Types.location.isRequired,
  },

  render() {
    let {
      location,
    } = this.props;

    // innerHTML expects & to be an HTML entity
    location = location.replace(/&/g, "&amp;");

    return dom.div(
      {
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
