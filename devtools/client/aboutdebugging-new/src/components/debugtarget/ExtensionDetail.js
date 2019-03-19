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
const Message = createFactory(require("../shared/Message"));

const { MESSAGE_LEVEL } = require("../../constants");
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

  renderWarnings() {
    const { warnings } = this.props.target.details;
    return dom.section(
      {},
      warnings.map((warning, index) => {
        return Message(
          {
            level: MESSAGE_LEVEL.WARNING,
            key: `warning-${index}`,
          },
          dom.p(
            {
              className: "technical-text",
            },
            warning
          )
        );
      })
    );
  }

  renderUUID() {
    const { manifestURL, uuid } = this.props.target.details;
    if (!uuid) {
      return null;
    }

    const value = [
      uuid,
      Localized(
        {
          id: "about-debugging-extension-manifest-link",
          key: "manifest",
        },
        dom.a(
          {
            className: "extension-detail__manifest js-manifest-url",
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

  renderExtensionId() {
    const { id } = this.props.target;

    return Localized(
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
    );
  }

  renderLocation() {
    const { location } = this.props.target.details;
    if (!location) {
      return null;
    }

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
    return dom.section(
      {
        className: "debug-target-item__detail",
      },
      this.renderWarnings(),
      dom.dl(
        {
          className: "extension-detail",
        },
        this.renderLocation(),
        this.renderExtensionId(),
        this.renderUUID(),
      ),
    );
  }
}

module.exports = FluentReact.withLocalization(ExtensionDetail);
