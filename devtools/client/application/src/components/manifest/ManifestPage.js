/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const {
  section,
} = require("devtools/client/shared/vendor/react-dom-factories");

const { connect } = require("devtools/client/shared/vendor/react-redux");

const ManifestLoader = createFactory(require("../manifest/ManifestLoader"));

const Manifest = createFactory(require("./Manifest"));
const ManifestEmpty = createFactory(require("./ManifestEmpty"));

class ManifestPage extends PureComponent {
  // TODO: Use well-defined types
  //       See https://bugzilla.mozilla.org/show_bug.cgi?id=1576881
  static get propTypes() {
    return {
      manifest: PropTypes.object,
    };
  }

  render() {
    const { manifest } = this.props;

    return section(
      {
        className: `app-page ${!manifest ? "app-page--empty" : ""}`,
      },
      ManifestLoader({}),
      manifest ? Manifest({ ...manifest }) : ManifestEmpty({})
    );
  }
}
function mapStateToProps(state) {
  return {
    manifest: state.manifest.manifest,
  };
}
// Exports
module.exports = connect(mapStateToProps)(ManifestPage);
