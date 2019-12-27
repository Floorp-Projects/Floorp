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

const Types = require("devtools/client/application/src/types/index");

const ManifestLoader = createFactory(
  require("devtools/client/application/src/components/manifest/ManifestLoader")
);
const Manifest = createFactory(
  require("devtools/client/application/src/components/manifest/Manifest")
);
const ManifestEmpty = createFactory(
  require("devtools/client/application/src/components/manifest/ManifestEmpty")
);

class ManifestPage extends PureComponent {
  static get propTypes() {
    return {
      // these props are automatically injected via connect
      hasLoadingFailed: PropTypes.bool.isRequired,
      isManifestLoading: PropTypes.bool.isRequired,
      manifest: PropTypes.shape(Types.manifest),
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
