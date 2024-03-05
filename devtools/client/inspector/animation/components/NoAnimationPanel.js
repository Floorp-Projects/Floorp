/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");

const L10N = new LocalizationHelper(
  "devtools/client/locales/animationinspector.properties"
);

class NoAnimationPanel extends Component {
  static get propTypes() {
    return {
      elementPickerEnabled: PropTypes.bool.isRequired,
      toggleElementPicker: PropTypes.func.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return this.props.elementPickerEnabled != nextProps.elementPickerEnabled;
  }

  render() {
    const { elementPickerEnabled, toggleElementPicker } = this.props;

    return dom.div(
      {
        className: "animation-error-message devtools-sidepanel-no-result",
      },
      dom.p(null, L10N.getStr("panel.noAnimation")),
      dom.button({
        className:
          "animation-element-picker devtools-button" +
          (elementPickerEnabled ? " checked" : ""),
        "data-standalone": true,
        onClick: event => {
          event.stopPropagation();
          toggleElementPicker();
        },
      })
    );
  }
}

const mapStateToProps = state => {
  return {
    elementPickerEnabled: state.animations.elementPickerEnabled,
  };
};

module.exports = connect(mapStateToProps)(NoAnimationPanel);
