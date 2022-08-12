/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");

const {
  accessibility: {
    AUDIT_TYPE: { TEXT_LABEL },
    ISSUE_TYPE,
    SCORES: { BEST_PRACTICES, FAIL, WARNING },
  },
} = require("devtools/shared/constants");

const {
  AREA_NO_NAME_FROM_ALT,
  DIALOG_NO_NAME,
  DOCUMENT_NO_TITLE,
  EMBED_NO_NAME,
  FIGURE_NO_NAME,
  FORM_FIELDSET_NO_NAME,
  FORM_FIELDSET_NO_NAME_FROM_LEGEND,
  FORM_NO_NAME,
  FORM_NO_VISIBLE_NAME,
  FORM_OPTGROUP_NO_NAME_FROM_LABEL,
  FRAME_NO_NAME,
  HEADING_NO_CONTENT,
  HEADING_NO_NAME,
  IFRAME_NO_NAME_FROM_TITLE,
  IMAGE_NO_NAME,
  INTERACTIVE_NO_NAME,
  MATHML_GLYPH_NO_NAME,
  TOOLBAR_NO_NAME,
} = ISSUE_TYPE[TEXT_LABEL];

/**
 * Check if the accessible is visible to the assistive technology.
 * @param {nsIAccessible} accessible
 *        Accessible object to be tested for visibility.
 *
 * @returns {Boolean}
 *         True if accessible object is visible to assistive technology.
 */
function isVisible(accessible) {
  const state = {};
  accessible.getState(state, {});
  return !(state.value & Ci.nsIAccessibleStates.STATE_INVISIBLE);
}

/**
 * Get related accessible objects that are targets of labelled by relation e.g.
 * labels.
 * @param {nsIAccessible} accessible
 *        Accessible objects to get labels for.
 *
 * @returns {Array}
 *          A list of accessible objects that are labels for a given accessible.
 */
function getLabels(accessible) {
  const relation = accessible.getRelationByType(
    Ci.nsIAccessibleRelation.RELATION_LABELLED_BY
  );
  return [...relation.getTargets().enumerate(Ci.nsIAccessible)];
}

/**
 * Get a trimmed name of the accessible object.
 *
 * @param {nsIAccessible} accessible
 *        Accessible objects to get a name for.
 *
 * @returns {null|String}
 *          Trimmed name of the accessible object if available.
 */
function getAccessibleName(accessible) {
  return accessible.name && accessible.name.trim();
}

/**
 * A text label rule for accessible objects that must have a non empty
 * accessible name.
 *
 * @returns {null|Object}
 *          Failure audit report if accessible object has no or empty name, null
 *          otherwise.
 */
const mustHaveNonEmptyNameRule = function(issue, accessible) {
  const name = getAccessibleName(accessible);
  return name ? null : { score: FAIL, issue };
};

/**
 * A text label rule for accessible objects that should have a non empty
 * accessible name as a best practice.
 *
 * @returns {null|Object}
 *          Best practices audit report if accessible object has no or empty
 *          name, null otherwise.
 */
const shouldHaveNonEmptyNameRule = function(issue, accessible) {
  const name = getAccessibleName(accessible);
  return name ? null : { score: BEST_PRACTICES, issue };
};

/**
 * A text label rule for accessible objects that can be activated via user
 * action and must have a non-empty name.
 *
 * @returns {null|Object}
 *          Failure audit report if interactive accessible object has no or
 *          empty name, null otherwise.
 */
const interactiveRule = mustHaveNonEmptyNameRule.bind(
  null,
  INTERACTIVE_NO_NAME
);

/**
 * A text label rule for accessible objects that correspond to dialogs and thus
 * should have a non-empty name.
 *
 * @returns {null|Object}
 *          Best practices audit report if dialog accessible object has no or
 *          empty name, null otherwise.
 */
const dialogRule = shouldHaveNonEmptyNameRule.bind(null, DIALOG_NO_NAME);

/**
 * A text label rule for accessible objects that provide visual information
 * (images, canvas, etc.) and must have a defined name (that can be empty, e.g.
 * "").
 *
 * @returns {null|Object}
 *          Failure audit report if interactive accessible object has no name,
 *          null otherwise.
 */
const imageRule = function(accessible) {
  const name = getAccessibleName(accessible);
  return name != null ? null : { score: FAIL, issue: IMAGE_NO_NAME };
};

/**
 * A text label rule for accessible objects that correspond to form elements.
 * These objects must have a non-empty name and must have a visible label.
 *
 * @returns {null|Object}
 *          Failure audit report if form element accessible object has no name,
 *          warning if the name does not come from a visible label, null
 *          otherwise.
 */
const formRule = function(accessible) {
  const name = getAccessibleName(accessible);
  if (!name) {
    return { score: FAIL, issue: FORM_NO_NAME };
  }

  const labels = getLabels(accessible);
  const hasNameFromVisibleLabel = labels.some(label => isVisible(label));

  return hasNameFromVisibleLabel
    ? null
    : { score: WARNING, issue: FORM_NO_VISIBLE_NAME };
};

/**
 * A text label rule for elements that map to ROLE_GROUPING:
 * * <OPTGROUP> must have a non-empty name and must be provided via the
 *   "label" attribute.
 * * <FIELDSET> must have a non-empty name and must be provided via the
 *   corresponding <LEGEND> element.
 *
 * @returns {null|Object}
 *          Failure audit report if form grouping accessible object has no name,
 *          or has a name that is not derived from a required location, null
 *          otherwise.
 */
const formGroupingRule = function(accessible) {
  const name = getAccessibleName(accessible);
  const { DOMNode } = accessible;

  switch (DOMNode.nodeName) {
    case "OPTGROUP":
      return name && DOMNode.label && DOMNode.label.trim() === name
        ? null
        : {
            score: FAIL,
            issue: FORM_OPTGROUP_NO_NAME_FROM_LABEL,
          };
    case "FIELDSET":
      if (!name) {
        return { score: FAIL, issue: FORM_FIELDSET_NO_NAME };
      }

      const labels = getLabels(accessible);
      const hasNameFromLegend = labels.some(
        label =>
          label.DOMNode.nodeName === "LEGEND" &&
          label.name &&
          label.name.trim() === name &&
          isVisible(label)
      );

      return hasNameFromLegend
        ? null
        : {
            score: WARNING,
            issue: FORM_FIELDSET_NO_NAME_FROM_LEGEND,
          };
    default:
      return null;
  }
};

/**
 * A text label rule for elements that map to ROLE_TEXT_CONTAINER:
 * * <METER> mapps to ROLE_TEXT_CONTAINER and must have a name provided via
 *   the visible label. Note: Will only work when bug 559770 is resolved (right
 *   now, unlabelled meters are not mapped to an accessible object).
 *
 * @returns {null|Object}
 *          Failure audit report depending on requirements for dialogs or form
 *          meter element, null otherwise.
 */
const textContainerRule = function(accessible) {
  const { DOMNode } = accessible;

  switch (DOMNode.nodeName) {
    case "DIALOG":
      return dialogRule(accessible);
    case "METER":
      return formRule(accessible);
    default:
      return null;
  }
};

/**
 * A text label rule for elements that map to ROLE_INTERNAL_FRAME:
 *  * <OBJECT> maps to ROLE_INTERNAL_FRAME. Check the type attribute and whether
 *    it includes "image/" (e.g. image/jpeg, image/png, image/gif). If so, audit
 *    it the same way other image roles are audited.
 *  * <EMBED> maps to ROLE_INTERNAL_FRAME and must have a non-empty name.
 *  * <FRAME> and <IFRAME> map to ROLE_INTERNAL_FRAME and must have a non-empty
 *    title attribute.
 *
 * @returns {null|Object}
 *          Failure audit report if the internal frame accessible object name is
 *          not provided or if it is not derived from a required location, null
 *          otherwise.
 */
const internalFrameRule = function(accessible) {
  const { DOMNode } = accessible;
  switch (DOMNode.nodeName) {
    case "FRAME":
      return mustHaveNonEmptyNameRule(FRAME_NO_NAME, accessible);
    case "IFRAME":
      const name = getAccessibleName(accessible);
      const title = DOMNode.title && DOMNode.title.trim();

      return title && title === name
        ? null
        : { score: FAIL, issue: IFRAME_NO_NAME_FROM_TITLE };
    case "OBJECT": {
      const type = DOMNode.getAttribute("type");
      if (!type || !type.startsWith("image/")) {
        return null;
      }

      return imageRule(accessible);
    }
    case "EMBED": {
      const type = DOMNode.getAttribute("type");
      if (!type || !type.startsWith("image/")) {
        return mustHaveNonEmptyNameRule(EMBED_NO_NAME, accessible);
      }
      return imageRule(accessible);
    }
    default:
      return null;
  }
};

/**
 * A text label rule for accessible objects that represent documents and should
 * have title element provided.
 *
 * @returns {null|Object}
 *          Failure audit report if document accessible object has no or empty
 *          title, null otherwise.
 */
const documentRule = function(accessible) {
  const title = accessible.DOMNode.title && accessible.DOMNode.title.trim();
  return title ? null : { score: FAIL, issue: DOCUMENT_NO_TITLE };
};

/**
 * A text label rule for accessible objects that correspond to headings and thus
 * must be non-empty.
 *
 * @returns {null|Object}
 *          Failure audit report if heading accessible object has no or
 *          empty name or if its text content is empty, null otherwise.
 */
const headingRule = function(accessible) {
  const name = getAccessibleName(accessible);
  if (!name) {
    return { score: FAIL, issue: HEADING_NO_NAME };
  }

  const content =
    accessible.DOMNode.textContent && accessible.DOMNode.textContent.trim();
  return content ? null : { score: WARNING, issue: HEADING_NO_CONTENT };
};

/**
 * A text label rule for accessible objects that represent toolbars and must
 * have a non-empty name if there is more than one toolbar present.
 *
 * @returns {null|Object}
 *          Failure audit report if toolbar accessible object is not the only
 *          toolbar in the document and has no or empty title, null otherwise.
 */
const toolbarRule = function(accessible) {
  const toolbars = accessible.DOMNode.ownerDocument.querySelectorAll(
    `[role="toolbar"]`
  );

  return toolbars.length > 1
    ? mustHaveNonEmptyNameRule(TOOLBAR_NO_NAME, accessible)
    : null;
};

/**
 * A text label rule for accessible objects that represent link (anchors, areas)
 * and must have a non-empty name.
 *
 * @returns {null|Object}
 *          Failure audit report if link accessible object has no or empty name,
 *          or in case when it's an <area> element with href attribute the name
 *          is not specified by an alt attribute, null otherwise.
 */
const linkRule = function(accessible) {
  const { DOMNode } = accessible;
  if (DOMNode.nodeName === "AREA" && DOMNode.hasAttribute("href")) {
    const alt = DOMNode.getAttribute("alt");
    const name = getAccessibleName(accessible);
    return alt && alt.trim() === name
      ? null
      : { score: FAIL, issue: AREA_NO_NAME_FROM_ALT };
  }

  return interactiveRule(accessible);
};

/**
 * A text label rule for accessible objects that are used to display
 * non-standard symbols where existing Unicode characters are not available and
 * must have a non-empty name.
 *
 * @returns {null|Object}
 *          Failure audit report if mglyph accessible object has no or empty
 *          name, and no or empty alt attribute, null otherwise.
 */
const mathmlGlyphRule = function(accessible) {
  const name = getAccessibleName(accessible);
  if (name) {
    return null;
  }

  const { DOMNode } = accessible;
  const alt = DOMNode.getAttribute("alt");
  return alt && alt.trim()
    ? null
    : { score: FAIL, issue: MATHML_GLYPH_NO_NAME };
};

const RULES = {
  [Ci.nsIAccessibleRole.ROLE_BUTTONMENU]: interactiveRule,
  [Ci.nsIAccessibleRole.ROLE_CANVAS]: imageRule,
  [Ci.nsIAccessibleRole.ROLE_CHECKBUTTON]: formRule,
  [Ci.nsIAccessibleRole.ROLE_CHECK_MENU_ITEM]: interactiveRule,
  [Ci.nsIAccessibleRole.ROLE_CHECK_RICH_OPTION]: formRule,
  [Ci.nsIAccessibleRole.ROLE_COLUMNHEADER]: interactiveRule,
  [Ci.nsIAccessibleRole.ROLE_COMBOBOX]: formRule,
  [Ci.nsIAccessibleRole.ROLE_COMBOBOX_OPTION]: interactiveRule,
  [Ci.nsIAccessibleRole.ROLE_DIAGRAM]: imageRule,
  [Ci.nsIAccessibleRole.ROLE_DIALOG]: dialogRule,
  [Ci.nsIAccessibleRole.ROLE_DOCUMENT]: documentRule,
  [Ci.nsIAccessibleRole.ROLE_EDITCOMBOBOX]: formRule,
  [Ci.nsIAccessibleRole.ROLE_ENTRY]: formRule,
  [Ci.nsIAccessibleRole.ROLE_FIGURE]: shouldHaveNonEmptyNameRule.bind(
    null,
    FIGURE_NO_NAME
  ),
  [Ci.nsIAccessibleRole.ROLE_GRAPHIC]: imageRule,
  [Ci.nsIAccessibleRole.ROLE_GROUPING]: formGroupingRule,
  [Ci.nsIAccessibleRole.ROLE_HEADING]: headingRule,
  [Ci.nsIAccessibleRole.ROLE_IMAGE_MAP]: imageRule,
  [Ci.nsIAccessibleRole.ROLE_INTERNAL_FRAME]: internalFrameRule,
  [Ci.nsIAccessibleRole.ROLE_LINK]: linkRule,
  [Ci.nsIAccessibleRole.ROLE_LISTBOX]: formRule,
  [Ci.nsIAccessibleRole.ROLE_MATHML_GLYPH]: mathmlGlyphRule,
  [Ci.nsIAccessibleRole.ROLE_MENUITEM]: interactiveRule,
  [Ci.nsIAccessibleRole.ROLE_OPTION]: interactiveRule,
  [Ci.nsIAccessibleRole.ROLE_OUTLINEITEM]: interactiveRule,
  [Ci.nsIAccessibleRole.ROLE_PAGETAB]: interactiveRule,
  [Ci.nsIAccessibleRole.ROLE_PASSWORD_TEXT]: formRule,
  [Ci.nsIAccessibleRole.ROLE_PROGRESSBAR]: formRule,
  [Ci.nsIAccessibleRole.ROLE_PUSHBUTTON]: interactiveRule,
  [Ci.nsIAccessibleRole.ROLE_RADIOBUTTON]: formRule,
  [Ci.nsIAccessibleRole.ROLE_RADIO_MENU_ITEM]: interactiveRule,
  [Ci.nsIAccessibleRole.ROLE_ROWHEADER]: interactiveRule,
  [Ci.nsIAccessibleRole.ROLE_SLIDER]: formRule,
  [Ci.nsIAccessibleRole.ROLE_SPINBUTTON]: formRule,
  [Ci.nsIAccessibleRole.ROLE_SWITCH]: formRule,
  [Ci.nsIAccessibleRole.ROLE_TEXT_CONTAINER]: textContainerRule,
  [Ci.nsIAccessibleRole.ROLE_TOGGLE_BUTTON]: interactiveRule,
  [Ci.nsIAccessibleRole.ROLE_TOOLBAR]: toolbarRule,
};

/**
 * Perform audit for WCAG 1.1 criteria related to providing alternative text
 * depending on the type of content.
 * @param {nsIAccessible} accessible
 *        Accessible object to be tested to determine if it requires and has
 *        an appropriate text alternative.
 *
 * @return {null|Object}
 *         Null if accessible does not need or has the right text alternative,
 *         audit data otherwise. This data is used in the accessibility panel
 *         for its audit filters, audit badges, sidebar checks section and
 *         highlighter.
 */
function auditTextLabel(accessible) {
  const rule = RULES[accessible.role];
  return rule ? rule(accessible) : null;
}

module.exports.auditTextLabel = auditTextLabel;
