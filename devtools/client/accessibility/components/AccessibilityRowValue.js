/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { span } = require("devtools/client/shared/vendor/react-dom-factories");

const { connect } = require("devtools/client/shared/vendor/react-redux");

const Badges = createFactory(require("./Badges"));
const AuditController = createFactory(require("./AuditController"));

const { REPS } = require("devtools/client/shared/components/reps/reps");
const { Grip } = REPS;
const Rep = createFactory(REPS.Rep);

class AccessibilityRowValue extends Component {
  static get propTypes() {
    return {
      member: PropTypes.shape({
        object: PropTypes.object,
      }).isRequired,
      supports: PropTypes.object.isRequired,
    };
  }

  render() {
    const { member, supports: { audit } } = this.props;

    return span({
      role: "presentation",
    },
      Rep({
        ...this.props,
        defaultRep: Grip,
        cropLimit: 50,
      }),
      audit && AuditController({
        accessible: member.object,
      },
        Badges()),
    );
  }
}

const mapStateToProps = ({ ui: { supports } }) => {
  return { supports };
};

module.exports = connect(mapStateToProps)(AccessibilityRowValue);
