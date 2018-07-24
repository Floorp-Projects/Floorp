/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("devtools/client/performance/modules/global");
const { ul, div } = dom;

class RecordingList extends Component {
  static get propTypes() {
    return {
      items: PropTypes.arrayOf(PropTypes.object).isRequired,
      itemComponent: PropTypes.func.isRequired
    };
  }

  render() {
    const {
      items,
      itemComponent: Item,
    } = this.props;

    return items.length > 0
      ? ul({ className: "recording-list" }, ...items.map(Item))
      : div({ className: "recording-list-empty" }, L10N.getStr("noRecordingsText"));
  }
}

module.exports = RecordingList;
