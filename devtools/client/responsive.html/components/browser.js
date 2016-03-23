/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, PropTypes, addons } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");

module.exports = createClass({

  displayName: "Browser",

  mixins: [ addons.PureRenderMixin ],

  propTypes: {
    location: Types.location.isRequired,
    width: Types.viewport.width.isRequired,
    height: Types.viewport.height.isRequired,
    isResizing: PropTypes.bool.isRequired,
  },

  render() {
    let {
      location,
      width,
      height,
      isResizing,
    } = this.props;

    let className = "browser";
    if (isResizing) {
      className += " resizing";
    }

    // innerHTML expects & to be an HTML entity
    location = location.replace(/&/g, "&amp;");

    return dom.div(
      {
        /**
         * React uses a whitelist for attributes, so we need some way to set
         * attributes it does not know about, such as @mozbrowser.  If this were
         * the only issue, we could use componentDidMount or ref: node => {} to
         * set the atttibutes. In the case of @remote, the attribute must be set
         * before the element is added to the DOM to have any effect, which we
         * are able to do with this approach.
         */
        dangerouslySetInnerHTML: {
          __html: `<iframe class="${className}" mozbrowser="true" remote="true"
                           noisolation="true" src="${location}"
                           width="${width}" height="${height}"></iframe>`
        }
      }
    );
  },

});
