/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  section,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

const Types = require("resource://devtools/client/application/src/types/index.js");

const ManifestLoader = createFactory(
  require("resource://devtools/client/application/src/components/manifest/ManifestLoader.js")
);
const Manifest = createFactory(
  require("resource://devtools/client/application/src/components/manifest/Manifest.js")
);
const ManifestEmpty = createFactory(
  require("resource://devtools/client/application/src/components/manifest/ManifestEmpty.js")
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
