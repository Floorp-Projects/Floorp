/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const {
  br,
  code,
  img,
  span,
} = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);
const { l10n } = require("devtools/client/application/src/modules/l10n");

const Types = require("devtools/client/application/src/types/index");
const ManifestItem = createFactory(
  require("devtools/client/application/src/components/manifest/ManifestItem")
);

/**
 * This component displays a Manifest member which holds a color value
 */
class ManifestIconItem extends PureComponent {
  static get propTypes() {
    return {
      // {
      //   label: { contentType, sizes },
      //   value: { src, purpose }
      // }
      ...Types.manifestItemIcon,
    };
  }

  getLocalizedImgTitle() {
    const { sizes } = this.props.label;

    return sizes && sizes.length
      ? l10n.getString("manifest-icon-img-title", { sizes })
      : l10n.getString("manifest-icon-img-title-no-sizes");
  }

  renderLabel() {
    const { contentType, sizes } = this.props.label;

    // sinze both `contentType` and `sizes` may be undefined, we don't need to
    // render the <br> if one –or both– are not to be displayed
    const shallRenderBr = contentType && sizes;

    return [
      sizes ? sizes : null,
      shallRenderBr ? br({ key: "label-br" }) : null,
      contentType ? contentType : null,
    ];
  }

  render() {
    const { src, purpose } = this.props.value;

    return ManifestItem(
      {
        label: this.renderLabel(),
      },
      Localized(
        {
          id: "manifest-icon-img",
          attrs: {
            alt: true,
          },
        },
        img({
          className: "manifest-item__icon",
          src,
          title: this.getLocalizedImgTitle(),
        })
      ),
      br({}),
      Localized(
        {
          id: "manifest-icon-purpose",
          code: code({}),
          $purpose: purpose,
        },
        span({})
      )
    );
  }
}

module.exports = ManifestIconItem;
