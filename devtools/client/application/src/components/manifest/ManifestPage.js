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
      // these props are automatically injected via connect
      hasLoadingFailed: PropTypes.bool.isRequired,
      isManifestLoading: PropTypes.bool.isRequired,
      manifest: PropTypes.object,
    };
  }

  get shouldShowLoader() {
    const { isManifestLoading, hasLoadingFailed } = this.props;
    const mustLoadManifest = typeof this.props.manifest === "undefined";
    return isManifestLoading || mustLoadManifest || hasLoadingFailed;
  }

  renderManifest() {
    const { manifest } = this.props;
    return manifest ? Manifest({ ...manifest }) : ManifestEmpty({});
  }

  render() {
    const { manifest } = this.props;

    return section(
      {
        className: `app-page js-manifest-page ${
          !manifest ? "app-page--empty" : ""
        }`,
      },
      this.shouldShowLoader ? ManifestLoader({}) : this.renderManifest()
    );
  }
}

function mapStateToProps(state) {
  return {
    hasLoadingFailed: !!state.manifest.errorMessage,
    isManifestLoading: state.manifest.isLoading,
    manifest: state.manifest.manifest,
  };
}

// Exports
module.exports = connect(mapStateToProps)(ManifestPage);
