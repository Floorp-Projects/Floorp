/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { PAGE_TYPES } = require("../../constants");

const ManifestPage = createFactory(require("../manifest/ManifestPage"));
const WorkersPage = createFactory(require("../service-workers/WorkersPage"));

class PageSwitcher extends PureComponent {
  static get propTypes() {
    return {
      page: PropTypes.oneOf(Object.values(PAGE_TYPES)),
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
