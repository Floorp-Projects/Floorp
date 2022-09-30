/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const { KeyCodes } = require("resource://devtools/client/shared/keycodes.js");

const {
  getStr,
} = require("resource://devtools/client/responsive/utils/l10n.js");

class UserAgentInput extends PureComponent {
  static get propTypes() {
    return {
      onChangeUserAgent: PropTypes.func.isRequired,
      userAgent: PropTypes.string.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      // The user agent input value.
      value: this.props.userAgent,
    };

    this.onChange = this.onChange.bind(this);
    this.onKeyUp = this.onKeyUp.bind(this);
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    if (this.props.userAgent !== nextProps.userAgent) {
      this.setState({ value: nextProps.userAgent });
    }
  }

  /**
   * Input change handler.
   *
   * @param  {Event} event
   */
  onChange({ target }) {
    const value = target.value;

    this.setState(prevState => {
      return {
        ...prevState,
        value,
      };
    });
  }

  /**
   * Input key up handler.
   *
   * @param  {Event} event
   */
  onKeyUp({ target, keyCode }) {
    if (keyCode == KeyCodes.DOM_VK_RETURN) {
      this.props.onChangeUserAgent(target.value);
      target.blur();
    }

    if (keyCode == KeyCodes.DOM_VK_ESCAPE) {
      target.blur();
    }
  }

  render() {
    return dom.label(
      { id: "user-agent-label" },
      "UA:",
      dom.input({
        id: "user-agent-input",
        className: "text-input",
        onChange: this.onChange,
        onKeyUp: this.onKeyUp,
        placeholder: getStr("responsive.customUserAgent"),
        type: "text",
        value: this.state.value,
      })
    );
  }
}

const mapStateToProps = state => {
  return {
    userAgent: state.ui.userAgent,
  };
};

module.exports = connect(mapStateToProps)(UserAgentInput);
