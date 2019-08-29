/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { span } = require("devtools/client/shared/vendor/react-dom-factories");

const { L10N } = require("../utils/l10n");

const {
  accessibility: { AUDIT_TYPE },
} = require("devtools/shared/constants");

loader.lazyGetter(this, "ContrastBadge", () =>
  createFactory(require("./ContrastBadge"))
);

loader.lazyGetter(this, "KeyboardBadge", () =>
  createFactory(require("./KeyboardBadge"))
);

loader.lazyGetter(this, "TextLabelBadge", () =>
  createFactory(require("./TextLabelBadge"))
);

function getComponentForAuditType(type) {
  const auditTypeToComponentMap = {
    [AUDIT_TYPE.CONTRAST]: ContrastBadge,
    [AUDIT_TYPE.KEYBOARD]: KeyboardBadge,
    [AUDIT_TYPE.TEXT_LABEL]: TextLabelBadge,
  };

  return auditTypeToComponentMap[type];
}

class Badges extends Component {
  static get propTypes() {
    return {
      checks: PropTypes.object,
    };
  }

  render() {
    const { checks } = this.props;
    if (!checks) {
      return null;
    }

    const items = [];
    for (const type in checks) {
      const component = getComponentForAuditType(type);
      if (checks[type] && component) {
        items.push(component(checks[type]));
      }
    }

    if (items.length === 0) {
      return null;
    }

    return span(
      {
        className: "badges",
        role: "group",
        "aria-label": L10N.getStr("accessibility.badges"),
      },
      items
    );
  }
}

module.exports = Badges;
