/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("../utils");

class DominatorTreeHeader extends Component {
  static get propTypes() {
    return {};
  }

  render() {
    return dom.div(
      {
        className: "header",
      },

      dom.span(
        {
          className: "heap-tree-item-bytes",
          title: L10N.getStr("heapview.field.retainedSize.tooltip"),
        },
        L10N.getStr("heapview.field.retainedSize")
      ),

      dom.span(
        {
          className: "heap-tree-item-bytes",
          title: L10N.getStr("heapview.field.shallowSize.tooltip"),
        },
        L10N.getStr("heapview.field.shallowSize")
      ),

      dom.span(
        {
          className: "heap-tree-item-name",
          title: L10N.getStr("dominatortree.field.label.tooltip"),
        },
        L10N.getStr("dominatortree.field.label")
      )
    );
  }
}

module.exports = DominatorTreeHeader;
