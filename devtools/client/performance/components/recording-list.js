/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {DOM, createClass} = require("devtools/client/shared/vendor/react");
const {L10N} = require("devtools/client/performance/modules/global");
const {ul, div} = DOM;

module.exports = createClass({
  displayName: "Recording List",

  render() {
    const {
      items,
      itemComponent: Item,
    } = this.props;

    return items.length > 0
      ? ul({ className: "recording-list" }, ...items.map(Item))
      : div({ className: "recording-list-empty" }, L10N.getStr("noRecordingsText"));
  }
});
