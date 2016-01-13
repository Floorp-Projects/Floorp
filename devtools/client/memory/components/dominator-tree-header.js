/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
const { L10N } = require("../utils");

const DominatorTreeHeader = module.exports = createClass({
  displayName: "DominatorTreeHeader",

  propTypes: { },

  render() {
    return dom.div(
      {
        className: "header"
      },

      dom.span({ className: "heap-tree-item-bytes" },
               L10N.getStr("heapview.field.retainedSize")),

      dom.span({ className: "heap-tree-item-bytes" },
               L10N.getStr("heapview.field.shallowSize")),

      dom.span({ className: "heap-tree-item-name" },
               L10N.getStr("heapview.field.name"))
    );
  }
});
