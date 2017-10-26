/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component, createFactory, DOM, PropTypes } =
  require("devtools/client/shared/vendor/react");
const NetInfoParams = createFactory(require("./net-info-params"));

/**
 * This template represents 'Params' tab displayed when the user
 * expands network log in the Console panel. It's responsible for
 * displaying URL parameters (query string).
 */
class ParamsTab extends Component {
  static get propTypes() {
    return {
      data: PropTypes.shape({
        request: PropTypes.object.isRequired
      })
    };
  }

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
}

// Exports from this module
module.exports = ParamsTab;
