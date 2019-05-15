/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { div } = require("devtools/client/shared/vendor/react-dom-factories");

const List = createFactory(require("devtools/client/shared/components/List").List);
const ColorContrastCheck =
  createFactory(require("./ColorContrastAccessibility").ColorContrastCheck);
const { L10N } = require("../utils/l10n");

const { accessibility: { AUDIT_TYPE } } = require("devtools/shared/constants");

function EmptyChecks() {
  return (
    div({
      className: "checks-empty",
      role: "presentation",
    }, L10N.getStr("accessibility.checks.empty2"))
  );
}

// Component that is responsible for rendering accessible audit data in the a11y panel
// sidebar.
class Checks extends Component {
  static get propTypes() {
    return {
      audit: PropTypes.object,
      labelledby: PropTypes.string.isRequired,
    };
  }

  [AUDIT_TYPE.CONTRAST](contrastRatio) {
    return ColorContrastCheck(contrastRatio);
  }

  render() {
    const { audit, labelledby } = this.props;
    if (!audit) {
      return EmptyChecks();
    }

    const items = [];
    for (const name in audit) {
      // There are going to be various audit reports for this object, sent by the server.
      // Iterate over them and delegate rendering to the method with the corresponding
      // name.
      if (audit[name] && this[name]) {
        items.push({
          component: this[name](audit[name]),
          className: name,
          key: name,
        });
      }
    }

    if (items.length === 0) {
      return EmptyChecks();
    }

    return (
      div({
        className: "checks",
        role: "presentation",
      }, List({ items, labelledby }))
    );
  }
}

const mapStateToProps = ({ details, ui }) => {
  if (!ui.supports.audit) {
    return {};
  }

  const { audit } = details;
  if (!audit) {
    return {};
  }

  return { audit };
};

module.exports = connect(mapStateToProps)(Checks);
