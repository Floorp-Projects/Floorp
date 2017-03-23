/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");

module.exports = createClass({

  displayName: "ComputedProperty",

  propTypes: {
    name: PropTypes.string.isRequired,
    value: PropTypes.string,
  },

  mixins: [ addons.PureRenderMixin ],

  onFocus() {
    this.container.focus();
  },

  render() {
    const { name, value } = this.props;

    return dom.div(
      {
        className: "property-view",
        "data-property-name": name,
        tabIndex: "0",
        ref: container => {
          this.container = container;
        },
      },
      dom.div(
        {
          className: "property-name-container",
        },
        dom.div(
          {
            className: "property-name theme-fg-color5",
            tabIndex: "",
            title: name,
            onClick: this.onFocus,
          },
          name
        )
      ),
      dom.div(
        {
          className: "property-value-container",
        },
        dom.div(
          {
            className: "property-value theme-fg-color1",
            dir: "ltr",
            tabIndex: "",
            onClick: this.onFocus,
          },
          value
        )
      )
    );
  },

});
