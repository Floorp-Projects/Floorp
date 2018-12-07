/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

/**
 * This component is designed to display an error message. It is composed of a header
 * displaying an error icon followed by a provided localized error description.
 * Children passed to this component will be displayed beflow the message header.
 */
class ErrorMessage extends PureComponent {
  static get propTypes() {
    return {
      children: PropTypes.node.isRequired,
      // Should match a valid localized string key.
      errorDescriptionKey: PropTypes.string.isRequired,
    };
  }

  render() {
    return dom.div(
      {
        className: "error-message",
      },
      dom.div(
        {
          className: "error-message__header",
        },
        dom.img(
          {
            // Temporary image chosen to match error container in about:addons.
            // Pending UX in Bug 1509091
            src: "chrome://mozapps/skin/extensions/alerticon-error.svg",
          }
        ),
        Localized(
          {
            id: this.props.errorDescriptionKey,
          },
          dom.span({}, this.props.errorDescriptionKey)
        )
      ),
      this.props.children
    );
  }
}

module.exports = ErrorMessage;
