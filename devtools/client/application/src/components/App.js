/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const { main } = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const LocalizationProvider = createFactory(FluentReact.LocalizationProvider);

const WorkersPage = createFactory(require("./service-workers/WorkersPage"));

/**
 * This is the main component for the application panel.
 */
class App extends PureComponent {
  static get propTypes() {
    return {
      client: PropTypes.object.isRequired,
      fluentBundles: PropTypes.array.isRequired,
      serviceContainer: PropTypes.object.isRequired,
    };
  }

  render() {
    const { client, fluentBundles, serviceContainer } = this.props;

    return LocalizationProvider(
      { messages: fluentBundles },
      main(
        { className: `application` },
        WorkersPage({
          client,
          serviceContainer,
        })
      )
    );
  }
}

module.exports = App;
