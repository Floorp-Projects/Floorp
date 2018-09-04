/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * This component displays detail information for extension.
 */
class ExtensionDetail extends PureComponent {
  static get propTypes() {
    return {
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
      dom.a(
        {
          className: "extension-detail__manifest",
          key: "manifest",
          href: manifestURL,
          target: "_blank",
        },
        "Manifest URL",
      )
    ];

    return this.renderField("uuid", "Internal UUID", value, uuid);
  }

  render() {
    const { target } = this.props;
    const { id, details } = target;
    const { location, uuid } = details;

    return dom.dl(
      {
        className: "extension-detail",
      },
      location ? this.renderField("location", "Location", location) : null,
      this.renderField("extension", "Extension ID", id),
      uuid ? this.renderUUID() : null,
    );
  }
}

module.exports = ExtensionDetail;
