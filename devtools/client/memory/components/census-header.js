/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass } = require("devtools/client/shared/vendor/react");
const { L10N } = require("../utils");

const CensusHeader = module.exports = createClass({
  displayName: "CensusHeader",

  propTypes: { },

  render() {
    return dom.div(
      {
        className: "header"
      },

      dom.span({ className: "heap-tree-item-bytes" },
               L10N.getStr("heapview.field.bytes")),

      dom.span({ className: "heap-tree-item-count" },
               L10N.getStr("heapview.field.count")),

      dom.span({ className: "heap-tree-item-total-bytes" },
               L10N.getStr("heapview.field.totalbytes")),

      dom.span({ className: "heap-tree-item-total-count" },
               L10N.getStr("heapview.field.totalcount")),

      dom.span({ className: "heap-tree-item-name" },
               L10N.getStr("heapview.field.name"))
    );
  }
});
