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

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const {
  updateSettingsVisibility,
} = require("devtools/client/inspector/compatibility/actions/compatibility");

loader.lazyRequireGetter(
  this,
  "openDocLink",
  "devtools/client/shared/link",
  true
);

const FEEDBACK_LINK =
  "https://docs.google.com/forms/d/e/1FAIpQLSeevOHveQ1tDuKYY5Fxyb3vvbKKumdLWUT5-RuwJWoAAOST5g/viewform";

const REPORT_ICON = "chrome://devtools/skin/images/report.svg";
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
      ),
      this._renderButton(
        REPORT_ICON,
        "compatibility-feedback-button-label",
        "compatibility-feedback-button-title",
        () => openDocLink(FEEDBACK_LINK)
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
