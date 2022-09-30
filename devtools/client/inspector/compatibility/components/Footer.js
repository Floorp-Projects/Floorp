/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const {
  updateSettingsVisibility,
} = require("resource://devtools/client/inspector/compatibility/actions/compatibility.js");

const SETTINGS_ICON = "chrome://devtools/skin/images/settings.svg";

class Footer extends PureComponent {
  static get propTypes() {
    return {
      updateSettingsVisibility: PropTypes.func.isRequired,
    };
  }

  _renderButton(icon, labelId, titleId, onClick) {
    return Localized(
      {
        id: titleId,
        attrs: { title: true },
      },
      dom.button(
        {
          className: "compatibility-footer__button",
          title: titleId,
          onClick,
        },
        dom.img({
          className: "compatibility-footer__icon",
          src: icon,
        }),
        Localized(
          {
            id: labelId,
          },
          dom.label(
            {
              className: "compatibility-footer__label",
            },
            labelId
          )
        )
      )
    );
  }

  render() {
    return dom.footer(
      {
        className: "compatibility-footer",
      },
      this._renderButton(
        SETTINGS_ICON,
        "compatibility-settings-button-label",
        "compatibility-settings-button-title",
        this.props.updateSettingsVisibility
      )
    );
  }
}

const mapDispatchToProps = dispatch => {
  return {
    updateSettingsVisibility: () => dispatch(updateSettingsVisibility(true)),
  };
};

module.exports = connect(null, mapDispatchToProps)(Footer);
