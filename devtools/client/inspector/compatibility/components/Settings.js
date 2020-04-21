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
  updateTargetBrowsers,
} = require("devtools/client/inspector/compatibility/actions/compatibility");

const CLOSE_ICON = "chrome://devtools/skin/images/close.svg";

class Settings extends PureComponent {
  static get propTypes() {
    return {
      defaultTargetBrowsers: PropTypes.arrayOf(PropTypes.shape(Types.browser))
        .isRequired,
      targetBrowsers: PropTypes.arrayOf(PropTypes.shape(Types.browser))
        .isRequired,
      updateTargetBrowsers: PropTypes.func.isRequired,
      updateSettingsVisibility: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this._onTargetBrowserChanged = this._onTargetBrowserChanged.bind(this);

    this.state = {
      targetBrowsers: props.targetBrowsers,
    };
  }

  _onTargetBrowserChanged({ target }) {
    const { id, status } = target.dataset;
    let { targetBrowsers } = this.state;

    if (target.checked) {
      targetBrowsers = [...targetBrowsers, { id, status }];
    } else {
      targetBrowsers = targetBrowsers.filter(
        b => !(b.id === id && b.status === status)
      );
    }

    this.setState({ targetBrowsers });
  }

  _renderTargetBrowsers() {
    const { defaultTargetBrowsers } = this.props;
    const { targetBrowsers } = this.state;

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
        defaultTargetBrowsers.map(({ id, name, status, version }) => {
          const inputId = `${id}-${status}`;
          const isTargetBrowser = !!targetBrowsers.find(
            b => b.id === id && b.status === status
          );
          return dom.li(
            {
              className: "compatibility-settings__target-browsers-item",
            },
            dom.input({
              id: inputId,
              type: "checkbox",
              checked: isTargetBrowser,
              onChange: this._onTargetBrowserChanged,
              "data-id": id,
              "data-status": status,
            }),
            dom.label(
              {
                className: "compatibility-settings__target-browsers-item-label",
                htmlFor: inputId,
              },
              BrowserIcon({ id, title: `${name} ${status}` }),
              `${name} ${status} (${version})`
            )
          );
        })
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
          onClick: () => {
            const { defaultTargetBrowsers } = this.props;
            const { targetBrowsers } = this.state;

            // Sort by ordering of default browsers.
            const browsers = defaultTargetBrowsers.filter(b =>
              targetBrowsers.find(t => t.id === b.id && t.status === b.status)
            );

            if (this.props.targetBrowsers.toString() !== browsers.toString()) {
              this.props.updateTargetBrowsers(browsers);
            }

            this.props.updateSettingsVisibility();
          },
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
    targetBrowsers: state.compatibility.targetBrowsers,
  };
};

const mapDispatchToProps = dispatch => {
  return {
    updateTargetBrowsers: browsers => dispatch(updateTargetBrowsers(browsers)),
    updateSettingsVisibility: () => dispatch(updateSettingsVisibility(false)),
  };
};

module.exports = connect(mapStateToProps, mapDispatchToProps)(Settings);
