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

const PageSwitcher = createFactory(
  require("devtools/client/application/src/components/routing/PageSwitcher")
);
const Sidebar = createFactory(
  require("devtools/client/application/src/components/routing/Sidebar")
);

/**
 * This is the main component for the application panel.
 */
class App extends PureComponent {
  static get propTypes() {
    return {
      fluentBundles: PropTypes.array.isRequired,
    };
  }

  render() {
    const { fluentBundles } = this.props;

    return LocalizationProvider(
      { bundles: fluentBundles },
      main({ className: `app` }, Sidebar({}), PageSwitcher({}))
    );
  }
}

module.exports = App;
