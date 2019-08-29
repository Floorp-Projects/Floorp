/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const ReactDOM = require("devtools/client/shared/vendor/react-dom-factories");

const Check = createFactory(
  require("devtools/client/accessibility/components/Check")
);

const { A11Y_KEYBOARD_LINKS } = require("../constants");
const {
  accessibility: {
    AUDIT_TYPE: { KEYBOARD },
    ISSUE_TYPE: {
      [KEYBOARD]: {
        FOCUSABLE_NO_SEMANTICS,
        FOCUSABLE_POSITIVE_TABINDEX,
        INTERACTIVE_NO_ACTION,
        INTERACTIVE_NOT_FOCUSABLE,
        NO_FOCUS_VISIBLE,
      },
    },
  },
} = require("devtools/shared/constants");

/**
 * A map from text label issues to annotation component properties.
 */
const ISSUE_TO_ANNOTATION_MAP = {
  [FOCUSABLE_NO_SEMANTICS]: {
    href: A11Y_KEYBOARD_LINKS.FOCUSABLE_NO_SEMANTICS,
    l10nId: "accessibility-keyboard-issue-semantics",
  },
  [FOCUSABLE_POSITIVE_TABINDEX]: {
    href: A11Y_KEYBOARD_LINKS.FOCUSABLE_POSITIVE_TABINDEX,
    l10nId: "accessibility-keyboard-issue-tabindex",
    args: {
      get code() {
        return ReactDOM.code({}, "tabindex");
      },
    },
  },
  [INTERACTIVE_NO_ACTION]: {
    href: A11Y_KEYBOARD_LINKS.INTERACTIVE_NO_ACTION,
    l10nId: "accessibility-keyboard-issue-action",
  },
  [INTERACTIVE_NOT_FOCUSABLE]: {
    href: A11Y_KEYBOARD_LINKS.INTERACTIVE_NOT_FOCUSABLE,
    l10nId: "accessibility-keyboard-issue-focusable",
  },
  [NO_FOCUS_VISIBLE]: {
    href: A11Y_KEYBOARD_LINKS.NO_FOCUS_VISIBLE,
    l10nId: "accessibility-keyboard-issue-focus-visible",
  },
};

/**
 * Component for rendering a check for text label accessibliity check failures,
 * warnings and best practices suggestions association with a given
 * accessibility object in the accessibility tree.
 */
class KeyboardCheck extends PureComponent {
  static get propTypes() {
    return {
      issue: PropTypes.string.isRequired,
      score: PropTypes.string.isRequired,
    };
  }

  render() {
    return Check({
      ...this.props,
      getAnnotation: issue => ISSUE_TO_ANNOTATION_MAP[issue],
      id: "accessibility-keyboard-header",
    });
  }
}

module.exports = KeyboardCheck;
