/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Actions = require("../actions/index");

const DOC_URL =
  "https://developer.mozilla.org/docs/Tools/about:debugging#Enabling_add-on_debugging";

class ExtensionDebugSetting extends PureComponent {
  static get propTypes() {
    return {
      className: PropTypes.string,
      dispatch: PropTypes.func.isRequired,
      extensionDebugEnabled: PropTypes.bool.isRequired,
    };
  }

  onToggle() {
    const { extensionDebugEnabled, dispatch } = this.props;
    dispatch(Actions.updateExtensionDebugSetting(!extensionDebugEnabled));
  }

  renderCheckbox() {
    const { extensionDebugEnabled } = this.props;

    return dom.input(
      {
        type: "checkbox",
        id: "extension-debug-setting-input",
        className: "default-checkbox  qa-extension-debug-checkbox",
        checked: extensionDebugEnabled,
        onChange: () => this.onToggle(),
      }
    );
  }

  renderLabel() {
    return Localized(
      {
        id: "about-debugging-extension-debug-setting-label",
        a: dom.a({
          href: DOC_URL,
          target: "_blank",
        }),
      },
      dom.label(
        {
          className: "unselectable-text",
          htmlFor: "extension-debug-setting-input",
        },
        "Enable extension debugging [Learn more]"
      )
    );
  }

  render() {
    const { className } = this.props;

    return dom.aside(
      { className },
      this.renderCheckbox(),
      this.renderLabel(),
    );
  }
}

module.exports = ExtensionDebugSetting;
