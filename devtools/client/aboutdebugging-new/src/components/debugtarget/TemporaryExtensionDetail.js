/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const ExtensionDetail = createFactory(require("./ExtensionDetail"));
const FieldPair = createFactory(require("./FieldPair"));

const Types = require("../../types/index");

const TEMP_ID_DOC_URL =
  "https://developer.mozilla.org/Add-ons/WebExtensions/WebExtensions_and_the_Add-on_ID";

/**
 * This component displays detail information for a temporary extension.
 */
class TemporaryExtensionDetail extends PureComponent {
  static get propTypes() {
    return {
      // Provided by wrapping the component with FluentReact.withLocalization.
      getString: PropTypes.func.isRequired,
      target: Types.debugTarget.isRequired,
    };
  }

  renderTemporaryIdMessage() {
    return Localized(
      {
        id: "about-debugging-tmp-extension-temporary-id",
        a: dom.a({
          className: "qa-temporary-id-link",
          href: TEMP_ID_DOC_URL,
          target: "_blank",
        }),
      },
      dom.div({
        className: "qa-temporary-id-message",
      }),
    );
  }

  render() {
    return ExtensionDetail(
      {
        target: this.props.target,
      },
      FieldPair({ label: this.renderTemporaryIdMessage() }),
    );
  }
}

module.exports = FluentReact.withLocalization(TemporaryExtensionDetail);
