/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const FieldPair = createFactory(require("./FieldPair"));

const Types = require("../../types/index");

/**
 * This component displays detail information for extension.
 */
class ExtensionDetail extends PureComponent {
  static get propTypes() {
    return {
      // Provided by wrapping the component with FluentReact.withLocalization.
      getString: PropTypes.func.isRequired,
      target: Types.debugTarget.isRequired,
    };
  }

  renderUUID() {
    const { target } = this.props;
    const { manifestURL, uuid } = target.details;

    const value = [
      uuid,
      Localized(
        {
          id: "about-debugging-extension-manifest-link",
          key: "manifest",
        },
        dom.a(
          {
            className: "extension-detail__manifest",
            href: manifestURL,
            target: "_blank",
          },
          "Manifest URL",
        )
      ),
    ];

    return Localized(
      {
        id: "about-debugging-extension-uuid",
        attrs: { label: true },
      },
      FieldPair(
        {
          slug: "uuid",
          label: "Internal UUID",
          value,
        }
      )
    );
  }

  renderLocation() {
    const { location } = this.props.target.details;

    return Localized(
      {
        id: "about-debugging-extension-location",
        attrs: { label: true },
      },
      FieldPair(
        {
          slug: "location",
          label: "Location",
          value: location,
        }
      )
    );
  }

  render() {
    const { target } = this.props;
    const { id, details } = target;
    const { location, uuid } = details;

    return dom.dl(
      {
        className: "extension-detail",
      },
      location ? this.renderLocation() : null,
      Localized(
        {
          id: "about-debugging-extension-id",
          attrs: { label: true },
        },
        FieldPair(
          {
            slug: "extension",
            label: "Extension ID",
            value: id,
          }
        )
      ),
      uuid ? this.renderUUID() : null,
    );
  }
}

module.exports = FluentReact.withLocalization(ExtensionDetail);
