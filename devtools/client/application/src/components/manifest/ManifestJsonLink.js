/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { openDocLink } = require("resource://devtools/client/shared/link.js");

const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");

const {
  a,
  p,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

/**
 * This component displays an "Open JSON" link for the Manifest
 */
class ManifestJsonLink extends PureComponent {
  static get propTypes() {
    return {
      url: PropTypes.string.isRequired,
    };
  }

  get shouldRenderLink() {
    const { url } = this.props;
    // Firefox blocks the loading of Data URLs with mimetypes manifest+json unless
    // explicitly typed by the user in the address bar.
    // So we are not showing the link in this case.
    // See more details in this post:
    // https://blog.mozilla.org/security/2017/11/27/blocking-top-level-navigations-data-urls-firefox-59/
    return !url.startsWith("data:");
  }

  renderLink() {
    const { url } = this.props;

    return a(
      {
        className: "js-manifest-json-link devtools-ellipsis-text",
        href: "#",
        title: url,
        onClick: () => openDocLink(url),
      },
      url
    );
  }

  render() {
    return p(
      { className: "manifest-json-link" },
      this.shouldRenderLink
        ? this.renderLink()
        : Localized({ id: "manifest-json-link-data-url" })
    );
  }
}

module.exports = ManifestJsonLink;
