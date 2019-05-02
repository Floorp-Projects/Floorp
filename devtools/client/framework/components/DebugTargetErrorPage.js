/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * This component is displayed when the about:devtools-toolbox fails to load
 * properly due to wrong parameters or debug targets that don't exist.
 */
class DebugTargetErrorPage extends PureComponent {
  static get propTypes() {
    return {
      errorMessage: PropTypes.string.isRequired,
      L10N: PropTypes.object.isRequired,
    };
  }

  render() {
    const { errorMessage, L10N } = this.props;

    return dom.article(
      {
        className: "error-page qa-error-page",
      },
      dom.h1(
        {
          className: "error-page__title",
        },
        L10N.getStr("toolbox.debugTargetErrorPage.title"),
      ),
      dom.p(
        {},
        L10N.getStr("toolbox.debugTargetErrorPage.description"),
      ),
      dom.output(
        {
          className: "error-page__details",
        },
        errorMessage,
      ),
    );
  }
}

module.exports = DebugTargetErrorPage;
