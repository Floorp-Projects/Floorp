/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const React = require("devtools/client/shared/vendor/react");
const NetInfoGroup = React.createFactory(require("./net-info-group"));

// Shortcuts
const DOM = React.DOM;
const PropTypes = React.PropTypes;

/**
 * This template is responsible for rendering sections/groups inside tabs.
 * It's used e.g to display Response and Request headers as separate groups.
 */
var NetInfoGroupList = React.createClass({
  propTypes: {
    groups: PropTypes.array.isRequired,
  },

  displayName: "NetInfoGroupList",

  render() {
    let groups = this.props.groups;

    // Filter out empty groups.
    groups = groups.filter(group => {
      return group && ((group.params && group.params.length) || group.content);
    });

    // Render groups
    groups = groups.map(group => {
      group.type = group.key;
      return NetInfoGroup(group);
    });

    return (
      DOM.div({className: "netInfoGroupList"},
        groups
      )
    );
  }
});

// Exports from this module
module.exports = NetInfoGroupList;
