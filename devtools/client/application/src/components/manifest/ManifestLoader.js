/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const {
  aside,
  p,
} = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const { connect } = require("devtools/client/shared/vendor/react-redux");

const { services } = require("../../modules/services");
const { updateManifest } = require("../../actions/manifest");

class ManifestLoader extends PureComponent {
  static get propTypes() {
    return {
      // these props get automatically injected via `connect`
      dispatch: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.state = { error: "", hasLoaded: false, hasManifest: false };
  }

  componentDidMount() {
    services.fetchManifest().then(({ manifest, errorMessage }) => {
      this.props.dispatch(updateManifest(manifest, errorMessage));
      this.setState({
        error: errorMessage,
        hasLoaded: true,
        hasManifest: !!manifest,
      });
    });
  }

  renderResult() {
    return this.state.hasManifest
      ? Localized(
          { id: "manifest-loaded-ok" },
          p({ className: "js-manifest-loaded-ok" })
        )
      : Localized(
          { id: "manifest-non-existing" },
          p({ className: "js-manifest-non-existing" })
        );
  }

  renderError() {
    const { error } = this.state;

    return [
      Localized(
        {
          id: "manifest-loaded-error",
          key: "manifest-error-label",
        },
        p({ className: "js-manifest-loaded-error" })
      ),
      p({ className: "technical-text", key: "manifest-error-message" }, error),
    ];
  }

  render() {
    const { error, hasLoaded } = this.state;

    const loadingDOM = hasLoaded
      ? null
      : Localized({ id: "manifest-loading" }, p({}));

    const errorDOM = error ? this.renderError() : null;
    const resultDOM =
      this.state.hasLoaded && !error ? this.renderResult() : null;

    return aside({}, loadingDOM, errorDOM, resultDOM);
  }
}

const mapDispatchToProps = dispatch => ({ dispatch });

module.exports = connect(mapDispatchToProps)(ManifestLoader);
