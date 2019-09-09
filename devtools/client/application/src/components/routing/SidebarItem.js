/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const {
  img,
  li,
  span,
} = require("devtools/client/shared/vendor/react-dom-factories");

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const { PAGE_TYPES } = require("../../constants");

const ICONS = {
  [PAGE_TYPES.MANIFEST]:
    "chrome://devtools/skin/images/application-manifest.svg",
  [PAGE_TYPES.SERVICE_WORKERS]:
    "chrome://devtools/skin/images/debugging-workers.svg",
};

const LOCALIZATION_IDS = {
  [PAGE_TYPES.MANIFEST]: "sidebar-item-manifest",
  [PAGE_TYPES.SERVICE_WORKERS]: "sidebar-item-service-workers",
};

class SidebarItem extends PureComponent {
  static get propTypes() {
    return {
      page: PropTypes.oneOf(Object.values(PAGE_TYPES)),
    };
  }
  render() {
    const { page } = this.props;
    return li(
      { className: "sidebar-item" },
      Localized(
        {
          id: LOCALIZATION_IDS[page],
          attrs: {
            alt: true,
            title: true,
          },
        },
        img({
          src: ICONS[page],
          className: "sidebar-item__icon",
        })
      ),
      Localized(
        {
          id: LOCALIZATION_IDS[page],
          attrs: {
            title: true,
          },
        },
        span({ className: "devtools-ellipsis-text" })
      )
    );
  }
}

// Exports
module.exports = SidebarItem;
