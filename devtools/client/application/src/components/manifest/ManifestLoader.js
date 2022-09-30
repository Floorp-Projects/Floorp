/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const {
  aside,
  h1,
  p,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

const {
  fetchManifest,
} = require("resource://devtools/client/application/src/actions/manifest.js");

class ManifestLoader extends PureComponent {
  static get propTypes() {
    return {
      // these props get automatically injected via `connect`
      dispatch: PropTypes.func.isRequired,
      error: PropTypes.string,
      hasFetchedManifest: PropTypes.bool.isRequired,
      isLoading: PropTypes.bool.isRequired,
    };
  }

  componentDidMount() {
    this.loadManifestIfNeeded();
  }

  componentDidUpdate() {
    this.loadManifestIfNeeded();
  }

  loadManifestIfNeeded() {
    const { isLoading, hasFetchedManifest } = this.props;
    const shallLoad = !isLoading && !hasFetchedManifest;
    if (shallLoad) {
      this.props.dispatch(fetchManifest());
    }
  }

  renderResult() {
    return Localized(
      { id: "manifest-loaded-ok" },
      p({ className: "js-manifest-loaded-ok" })
    );
  }

  renderError() {
    const { error } = this.props;

    return [
      Localized(
        {
          id: "manifest-loaded-error",
          key: "manifest-error-label",
        },
        h1({ className: "js-manifest-loaded-error app-page__title" })
      ),
      p({ className: "technical-text", key: "manifest-error-message" }, error),
    ];
  }

  render() {
    const { error, isLoading } = this.props;

    const loadingDOM = isLoading
      ? Localized(
          { id: "manifest-loading" },
          p({ className: "manifest-loader__load js-manifest-loading" })
        )
      : null;

    const errorDOM = error ? this.renderError() : null;
    const resultDOM = !isLoading && !error ? this.renderResult() : null;

    return aside(
      { className: "manifest-loader" },
      loadingDOM,
      errorDOM,
      resultDOM
    );
  }
}

const mapDispatchToProps = dispatch => ({ dispatch });
const mapStateToProps = state => ({
  error: state.manifest.errorMessage,
  hasFetchedManifest: typeof state.manifest.manifest !== "undefined",
  isLoading: state.manifest.isLoading,
});

module.exports = connect(mapStateToProps, mapDispatchToProps)(ManifestLoader);
