/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const React = require("devtools/client/shared/vendor/react");
const NetInfoParams = React.createFactory(require("./net-info-params"));

// Shortcuts
const DOM = React.DOM;
const PropTypes = React.PropTypes;

/**
 * This template represents 'Params' tab displayed when the user
 * expands network log in the Console panel. It's responsible for
 * displaying URL parameters (query string).
 */
var ParamsTab = React.createClass({
  propTypes: {
    data: PropTypes.shape({
      request: PropTypes.object.isRequired
    })
  },

  displayName: "ParamsTab",

  render() {
    let data = this.props.data;

    return (
      DOM.div({className: "paramsTabBox"},
        DOM.div({className: "panelContent"},
          NetInfoParams({params: data.request.queryString})
        )
      )
    );
  }
});

// Exports from this module
module.exports = ParamsTab;
