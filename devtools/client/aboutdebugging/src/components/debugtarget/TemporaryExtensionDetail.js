/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const ExtensionDetail = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/debugtarget/ExtensionDetail.js")
);
const FieldPair = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/debugtarget/FieldPair.js")
);

const Types = require("resource://devtools/client/aboutdebugging/src/types/index.js");

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
      })
    );
  }

  render() {
    return ExtensionDetail(
      {
        target: this.props.target,
      },
      FieldPair({ label: this.renderTemporaryIdMessage() })
    );
  }
}

module.exports = FluentReact.withLocalization(TemporaryExtensionDetail);
