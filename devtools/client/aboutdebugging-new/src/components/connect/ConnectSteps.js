/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PureComponent,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

class ConnectSteps extends PureComponent {
  static get propTypes() {
    return {
      steps: PropTypes.arrayOf(
        PropTypes.shape({
          localizationId: PropTypes.string.isRequired,
        }).isRequired
      ),
    };
  }

  render() {
    return dom.ul(
      {
        className: "connect-page__step-list",
      },
      ...this.props.steps.map(step =>
        Localized(
          {
            id: step.localizationId,
          },
          dom.li(
            {
              className: "connect-page__step",
              key: step.localizationId,
            },
            step.localizationId
          )
        )
      )
    );
  }
}

module.exports = ConnectSteps;
