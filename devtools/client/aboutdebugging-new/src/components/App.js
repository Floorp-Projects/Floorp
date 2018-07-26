/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Runtime = require("../runtimes/runtime");
const ThisFirefox = require("../runtimes/this-firefox");

const RuntimesPane = createFactory(require("./RuntimesPane"));

class App extends PureComponent {
  static get propTypes() {
    return {
      selectedRuntime: PropTypes.instanceOf(Runtime),
      thisFirefox: PropTypes.instanceOf(ThisFirefox).isRequired,
    };
  }

  render() {
    const { selectedRuntime, thisFirefox } = this.props;

    return dom.div(
      {
        className: "app",
      },
      RuntimesPane({ selectedRuntime, thisFirefox })
    );
  }
}

const mapStateToProps = state => {
  return {
    selectedRuntime: state.runtimes.selectedRuntime,
  };
};

module.exports = connect(mapStateToProps)(App);
