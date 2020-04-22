/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Types = require("devtools/client/inspector/compatibility/types");

const BrowserIcon = createFactory(
  require("devtools/client/inspector/compatibility/components/BrowserIcon")
);

const {
  updateSettingsVisibility,
} = require("devtools/client/inspector/compatibility/actions/compatibility");

const CLOSE_ICON = "chrome://devtools/skin/images/close.svg";

class Settings extends PureComponent {
  static get propTypes() {
    return {
      defaultTargetBrowsers: PropTypes.arrayOf(PropTypes.shape(Types.browser))
        .isRequired,
      updateSettingsVisibility: PropTypes.func.isRequired,
    };
  }

  _renderTargetBrowsers() {
    const { defaultTargetBrowsers } = this.props;

    return dom.section(
      {
        className: "compatibility-settings__target-browsers",
      },
      dom.header(
        {
          className: "compatibility-settings__target-browsers-header",
        },
        "Target Browsers"
      ),
      dom.ul(
        {
          className: "compatibility-settings__target-browsers-list",
        },
        defaultTargetBrowsers.map(({ id, name, status, version }) =>
          dom.li(
            {
              className: "compatibility-settings__target-browsers-item",
            },
            BrowserIcon({ id, title: `${name} ${status}` }),
            `${name} ${status} (${version})`
          )
        )
      )
    );
  }

  _renderHeader() {
    return dom.header(
      {
        className: "compatibility-settings__header",
      },
      dom.label(
        {
          className: "compatibility-settings__header-label",
        },
        "Settings"
      ),
      dom.button(
        {
          className: "compatibility-settings__header-button",
          title: "Close settings",
          onClick: this.props.updateSettingsVisibility,
        },
        dom.img({
          className: "compatibility-settings__header-icon",
          src: CLOSE_ICON,
        })
      )
    );
  }

  render() {
    return dom.section(
      {
        className: "compatibility-settings",
      },
      this._renderHeader(),
      this._renderTargetBrowsers()
    );
  }
}

const mapStateToProps = state => {
  return {
    defaultTargetBrowsers: state.compatibility.defaultTargetBrowsers,
  };
};

const mapDispatchToProps = dispatch => {
  return {
    updateSettingsVisibility: () => dispatch(updateSettingsVisibility(false)),
  };
};

module.exports = connect(mapStateToProps, mapDispatchToProps)(Settings);
