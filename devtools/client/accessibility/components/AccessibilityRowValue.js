/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { span } = require("devtools/client/shared/vendor/react-dom-factories");

const Badges = createFactory(
  require("devtools/client/accessibility/components/Badges")
);
const AuditController = createFactory(
  require("devtools/client/accessibility/components/AuditController")
);

const { REPS } = require("devtools/client/shared/components/reps/reps");
const { Grip } = REPS;
const Rep = createFactory(REPS.Rep);

class AccessibilityRowValue extends Component {
  static get propTypes() {
    return {
      member: PropTypes.shape({
        object: PropTypes.object,
      }).isRequired,
    };
  }

  render() {
    return span(
      {
        role: "presentation",
      },
      Rep({
        ...this.props,
        defaultRep: Grip,
        cropLimit: 50,
      }),
      AuditController(
        {
          accessibleFront: this.props.member.object,
        },
        Badges()
      )
    );
  }
}

module.exports = AccessibilityRowValue;
