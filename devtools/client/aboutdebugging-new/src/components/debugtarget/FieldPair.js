/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
/* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/* Renders a pair of `<dt>` (label) + `<dd>` (value) field.
 * It also needs a 'slug' prop, which it's an ID that will be used to generate
 * a React key for the dom element */
class FieldPair extends PureComponent {
  static get propTypes() {
    return {
      slug: PropTypes.string.isRequired,
      label: PropTypes.node.isRequired,
      value: PropTypes.node,
    };
  }

  render() {
    const { slug, label, value } = this.props;
    return [
      dom.dt(
        { key: `${slug}-dt` },
        label
      ),
      value ? dom.dd(
        {
          key: `${slug}-dd`,
          className: "ellipsis-text",
        },
        value) : null,
    ];
  }
}

module.exports = FieldPair;
