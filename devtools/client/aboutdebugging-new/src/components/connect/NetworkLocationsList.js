/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Actions = require("../../actions/index");

class NetworkLocationsList extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      networkLocations: PropTypes.arrayOf(PropTypes.object).isRequired,
    };
  }

  render() {
    return dom.ul(
      {},
      this.props.networkLocations.map(location =>
        dom.li(
          {
            className: "connect-page__network-location"
          },
          dom.span(
            {
              className: "ellipsis-text"
            },
            location
          ),
          dom.button(
            {
              className: "aboutdebugging-button",
              onClick: () => {
                this.props.dispatch(Actions.removeNetworkLocation(location));
              }
            },
            "Remove"
          )
        )
      )
    );
  }
}

module.exports = NetworkLocationsList;
