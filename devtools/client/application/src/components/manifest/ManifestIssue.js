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
  img,
  li,
  span,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const {
  MANIFEST_ISSUE_LEVELS,
} = require("resource://devtools/client/application/src/constants.js");
const Types = require("resource://devtools/client/application/src/types/index.js");

/**
 * A Manifest validation issue (warning, error)
 */
class ManifestIssue extends PureComponent {
  static get propTypes() {
    return {
      className: PropTypes.string,
      ...Types.manifestIssue, // { level, message }
    };
  }

  getIconData(level) {
    switch (level) {
      case MANIFEST_ISSUE_LEVELS.WARNING:
        return {
          src: "chrome://devtools/skin/images/alert-small.svg",
          localizationId: "icon-warning",
        };
      case MANIFEST_ISSUE_LEVELS.ERROR:
      default:
        return {
          src: "resource://devtools-shared-images/error-small.svg",
          localizationId: "icon-error",
        };
    }
  }

  render() {
    const { level, message, className } = this.props;
    const icon = this.getIconData(level);

    return li(
      { className: `js-manifest-issue ${className ? className : ""}` },
      Localized(
        { id: icon.localizationId, attrs: { alt: true, title: true } },
        img({
          className: `manifest-issue__icon manifest-issue__icon--${level}`,
          src: icon.src,
        })
      ),
      span({}, message)
    );
  }
}

// Exports
module.exports = ManifestIssue;
