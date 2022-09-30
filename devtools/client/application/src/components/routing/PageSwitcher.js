/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

const {
  PAGE_TYPES,
} = require("resource://devtools/client/application/src/constants.js");
const Types = require("resource://devtools/client/application/src/types/index.js");

const ManifestPage = createFactory(
  require("resource://devtools/client/application/src/components/manifest/ManifestPage.js")
);
const WorkersPage = createFactory(
  require("resource://devtools/client/application/src/components/service-workers/WorkersPage.js")
);

class PageSwitcher extends PureComponent {
  static get propTypes() {
    return {
      page: Types.page.isRequired,
    };
  }

  render() {
    let component = null;

    switch (this.props.page) {
      case PAGE_TYPES.MANIFEST:
        component = ManifestPage({});
        break;
      case PAGE_TYPES.SERVICE_WORKERS:
        component = WorkersPage({});
        break;
      default:
        console.error("Unknown path. Can not direct to a page.");
        return null;
    }

    return component;
  }
}

function mapStateToProps(state) {
  return {
    page: state.ui.selectedPage,
  };
}

module.exports = connect(mapStateToProps)(PageSwitcher);
