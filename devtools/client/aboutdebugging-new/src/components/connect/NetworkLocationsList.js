/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Actions = require("../../actions/index");

class NetworkLocationsList extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      networkLocations: PropTypes.arrayOf(PropTypes.string).isRequired,
    };
  }

  renderList() {
    return dom.ul(
      {},
      this.props.networkLocations.map(location =>
        dom.li(
          {
            className: "connect-page__network-location js-network-location",
            key: location,
          },
          dom.span(
            {
              className: "ellipsis-text js-network-location-value",
            },
            location
          ),
          Localized(
            {
              id: "about-debugging-network-locations-remove-button",
            },
            dom.button(
              {
                className: "std-button js-network-location-remove-button",
                onClick: () => {
                  this.props.dispatch(Actions.removeNetworkLocation(location));
                },
              },
              "Remove"
            )
          )
        )
      )
    );
  }

  renderEmpty() {
    return Localized(
      {
        id: "about-debugging-network-locations-empty-text",
      },
      dom.p(
        {},
        "No network locations have been added yet."
      )
    );
  }

  render() {
    return this.props.networkLocations.length > 0 ?
      this.renderList() : this.renderEmpty();
  }
}

module.exports = NetworkLocationsList;
