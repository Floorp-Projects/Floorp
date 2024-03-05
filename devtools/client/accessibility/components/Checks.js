/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const {
  Component,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  div,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const List = createFactory(
  require("resource://devtools/client/shared/components/List.js").List
);
const ColorContrastCheck = createFactory(
  require("resource://devtools/client/accessibility/components/ColorContrastAccessibility.js")
    .ColorContrastCheck
);
const TextLabelCheck = createFactory(
  require("resource://devtools/client/accessibility/components/TextLabelCheck.js")
);
const KeyboardCheck = createFactory(
  require("resource://devtools/client/accessibility/components/KeyboardCheck.js")
);
const {
  L10N,
} = require("resource://devtools/client/accessibility/utils/l10n.js");

const {
  accessibility: { AUDIT_TYPE },
} = require("resource://devtools/shared/constants.js");

function EmptyChecks() {
  return div(
    {
      className: "checks-empty",
      role: "presentation",
      tabIndex: "-1",
    },
    L10N.getStr("accessibility.checks.empty2")
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

  [AUDIT_TYPE.KEYBOARD](keyboardCheck) {
    return KeyboardCheck(keyboardCheck);
  }

  [AUDIT_TYPE.TEXT_LABEL](textLabelCheck) {
    return TextLabelCheck(textLabelCheck);
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

    return div(
      {
        className: "checks",
        role: "presentation",
        tabIndex: "-1",
      },
      List({ items, labelledby })
    );
  }
}

const mapStateToProps = ({ details }) => {
  const { audit } = details;
  if (!audit) {
    return {};
  }

  return { audit };
};

module.exports = connect(mapStateToProps)(Checks);
