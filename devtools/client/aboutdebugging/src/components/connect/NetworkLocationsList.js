/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const Actions = require("resource://devtools/client/aboutdebugging/src/actions/index.js");
const Types = require("resource://devtools/client/aboutdebugging/src/types/index.js");

class NetworkLocationsList extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      networkLocations: PropTypes.arrayOf(Types.location).isRequired,
    };
  }

  renderList() {
    return dom.ul(
      {},
      this.props.networkLocations.map(location =>
        dom.li(
          {
            className: "network-location qa-network-location",
            key: location,
          },
          dom.span(
            {
              className: "ellipsis-text qa-network-location-value",
            },
            location
          ),
          Localized(
            {
              id: "about-debugging-network-locations-remove-button",
            },
            dom.button(
              {
                className: "default-button qa-network-location-remove-button",
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

  render() {
    return this.props.networkLocations.length ? this.renderList() : null;
  }
}

module.exports = NetworkLocationsList;
