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

  renderField(name, value, title) {
    return [
      dom.dt({}, name),
      dom.dd(
        {
          className: "ellipsis-text",
          title: title || value,
        },
        value,
      ),
    ];
  }

  renderUUID() {
    const { target } = this.props;
    const { manifestURL, uuid } = target;

    const value = [
      uuid,
      dom.a(
        {
          className: "extension-detail__manifest",
          href: manifestURL,
          target: "_blank",
        },
        "Manifest URL",
      )
    ];

    return this.renderField("Internal UUID", value, uuid);
  }

  render() {
    const { target } = this.props;
    const { id, location, uuid } = target;

    return dom.dl(
      {
        className: "extension-detail",
      },
      location ? this.renderField("Location", location) : null,
      this.renderField("Extension ID", id),
      uuid ? this.renderUUID() : null,
    );
  }
}

module.exports = ExtensionDetail;
