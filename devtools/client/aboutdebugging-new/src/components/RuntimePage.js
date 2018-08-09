/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const DebugTargetPane = createFactory(require("./DebugTargetPane"));
const RuntimeInfo = createFactory(require("./RuntimeInfo"));

const Services = require("Services");

class RuntimePage extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      tabs: PropTypes.arrayOf(PropTypes.object).isRequired,
    };
  }

  render() {
    const { dispatch, tabs } = this.props;

    return dom.article(
      {
        className: "page",
      },
      RuntimeInfo({
        icon: "chrome://branding/content/icon64.png",
        name: Services.appinfo.name,
        version: Services.appinfo.version,
      }),
      DebugTargetPane({
        dispatch,
        name: "Tabs",
        targets: tabs
      }),
    );
  }
}

const mapStateToProps = state => {
  return {
    tabs: state.runtime.tabs,
  };
};

module.exports = connect(mapStateToProps)(RuntimePage);
