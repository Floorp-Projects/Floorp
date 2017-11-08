/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { LocalizationHelper } = require("devtools/shared/l10n");

const L10N =
  new LocalizationHelper("devtools/client/locales/animationinspector.properties");

class NoAnimationPanel extends PureComponent {
  static get propTypes() {
    return {
      elementPicker: PropTypes.object.isRequired,
      toggleElementPicker: PropTypes.func.isRequired,
    };
  }

  shouldComponentUpdate(nextProps, nextState) {
    return this.props.elementPicker.isEnabled != nextProps.elementPicker.isEnabled;
  }

  render() {
    const { elementPicker, toggleElementPicker } = this.props;

    return dom.div(
      {
        className: "animation-error-message devtools-sidepanel-no-result"
      },
      dom.p(
        null,
        L10N.getStr("panel.noAnimation")
      ),
      dom.button(
        {
          className: "animation-element-picker devtools-button"
                     + (elementPicker.isEnabled ? " checked" : ""),
          "data-standalone": true,
          onClick: toggleElementPicker
        }
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    elementPicker: state.animationElementPicker
  };
};

module.exports = connect(mapStateToProps)(NoAnimationPanel);
