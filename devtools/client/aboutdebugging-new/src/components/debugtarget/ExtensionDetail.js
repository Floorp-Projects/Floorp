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
 * This component displays detail information for extension.
 */
class ExtensionDetail extends PureComponent {
  static get propTypes() {
    return {
      // Provided by wrapping the component with FluentReact.withLocalization.
      getString: PropTypes.func.isRequired,
      target: PropTypes.object.isRequired,
    };
  }

  renderField(key, name, value, title) {
    return [
      dom.dt({ key: `${ key }-dt` }, name),
      dom.dd(
        {
          className: "ellipsis-text",
          key: `${ key }-dd`,
          title: title || value,
        },
        value,
      ),
    ];
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

    const uuidName = this.props.getString("about-debugging-extension-uuid");
    return this.renderField("uuid", uuidName, value, uuid);
  }

  render() {
    const { target } = this.props;
    const { id, details } = target;
    const { location, uuid } = details;

    const locationName = this.props.getString("about-debugging-extension-location");
    const idName = this.props.getString("about-debugging-extension-id");

    return dom.dl(
      {
        className: "extension-detail",
      },
      location ? this.renderField("location", locationName, location) : null,
      this.renderField("extension", idName, id),
      uuid ? this.renderUUID() : null,
    );
  }
}

module.exports = FluentReact.withLocalization(ExtensionDetail);
