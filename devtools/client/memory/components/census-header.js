/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass } = require("devtools/client/shared/vendor/react");
const { L10N } = require("../utils");
const models = require("../models");

const CensusHeader = module.exports = createClass({
  displayName: "CensusHeader",

  propTypes: {
    diffing: models.diffingModel,
  },

  render() {
    let individualsCell;
    if (!this.props.diffing) {
      individualsCell = dom.span({
        className: "heap-tree-item-field heap-tree-item-individuals"
      });
    }

    return dom.div(
      {
        className: "header"
      },

      dom.span(
        {
          className: "heap-tree-item-bytes",
          title: L10N.getStr("heapview.field.bytes.tooltip"),
        },
        L10N.getStr("heapview.field.bytes")
      ),

      dom.span(
        {
          className: "heap-tree-item-count",
          title: L10N.getStr("heapview.field.count.tooltip"),
        },
        L10N.getStr("heapview.field.count")
      ),

      dom.span(
        {
          className: "heap-tree-item-total-bytes",
          title: L10N.getStr("heapview.field.totalbytes.tooltip"),
        },
        L10N.getStr("heapview.field.totalbytes")
      ),

      dom.span(
        {
          className: "heap-tree-item-total-count",
          title: L10N.getStr("heapview.field.totalcount.tooltip"),
        },
        L10N.getStr("heapview.field.totalcount")
      ),

      individualsCell,

      dom.span(
        {
          className: "heap-tree-item-name",
          title: L10N.getStr("heapview.field.name.tooltip"),
        },
        L10N.getStr("heapview.field.name")
      )
    );
  }
});
