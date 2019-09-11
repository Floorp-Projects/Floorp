/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cu } = require("chrome");
loader.lazyRequireGetter(
  this,
  "CssLogic",
  "devtools/server/actors/inspector/css-logic",
  true
);
loader.lazyRequireGetter(
  this,
  "getCSSStyleRules",
  "devtools/shared/inspector/css-logic",
  true
);
loader.lazyRequireGetter(this, "InspectorUtils", "InspectorUtils");
loader.lazyRequireGetter(
  this,
  "nodeConstants",
  "devtools/shared/dom-node-constants"
);

const {
  accessibility: {
    AUDIT_TYPE: { KEYBOARD },
    ISSUE_TYPE: {
      [KEYBOARD]: {
        FOCUSABLE_NO_SEMANTICS,
        FOCUSABLE_POSITIVE_TABINDEX,
        INTERACTIVE_NO_ACTION,
        INTERACTIVE_NOT_FOCUSABLE,
        MOUSE_INTERACTIVE_ONLY,
        NO_FOCUS_VISIBLE,
      },
    },
    SCORES: { FAIL, WARNING },
  },
} = require("devtools/shared/constants");

// Specified by the author CSS rule type.
const STYLE_RULE = 1;

// Accessible action for showing long description.
const CLICK_ACTION = "click";

/**
 * Focus specific pseudo classes that the keyboard audit simulates to determine
 * focus styling.
 */
const FOCUS_PSEUDO_CLASS = ":focus";
const MOZ_FOCUSRING_PSEUDO_CLASS = ":-moz-focusring";

const KEYBOARD_FOCUSABLE_ROLES = new Set([
  Ci.nsIAccessibleRole.ROLE_BUTTONMENU,
  Ci.nsIAccessibleRole.ROLE_CHECKBUTTON,
  Ci.nsIAccessibleRole.ROLE_COMBOBOX,
  Ci.nsIAccessibleRole.ROLE_EDITCOMBOBOX,
  Ci.nsIAccessibleRole.ROLE_ENTRY,
  Ci.nsIAccessibleRole.ROLE_LINK,
  Ci.nsIAccessibleRole.ROLE_LISTBOX,
  Ci.nsIAccessibleRole.ROLE_PASSWORD_TEXT,
  Ci.nsIAccessibleRole.ROLE_PUSHBUTTON,
  Ci.nsIAccessibleRole.ROLE_RADIOBUTTON,
  Ci.nsIAccessibleRole.ROLE_SLIDER,
  Ci.nsIAccessibleRole.ROLE_SPINBUTTON,
  Ci.nsIAccessibleRole.ROLE_SUMMARY,
  Ci.nsIAccessibleRole.ROLE_SWITCH,
  Ci.nsIAccessibleRole.ROLE_TOGGLE_BUTTON,
]);

const INTERACTIVE_ROLES = new Set([
  ...KEYBOARD_FOCUSABLE_ROLES,
  Ci.nsIAccessibleRole.ROLE_CHECK_MENU_ITEM,
  Ci.nsIAccessibleRole.ROLE_CHECK_RICH_OPTION,
  Ci.nsIAccessibleRole.ROLE_COMBOBOX_OPTION,
  Ci.nsIAccessibleRole.ROLE_MENUITEM,
  Ci.nsIAccessibleRole.ROLE_OPTION,
  Ci.nsIAccessibleRole.ROLE_OUTLINE,
  Ci.nsIAccessibleRole.ROLE_OUTLINEITEM,
  Ci.nsIAccessibleRole.ROLE_PAGETAB,
  Ci.nsIAccessibleRole.ROLE_PARENT_MENUITEM,
  Ci.nsIAccessibleRole.ROLE_RADIO_MENU_ITEM,
  Ci.nsIAccessibleRole.ROLE_RICH_OPTION,
]);

/**
 * Determine if a node is dead or is not an element node.
 *
 * @param   {DOMNode} node
 *          Node to be tested for validity.
 *
 * @returns {Boolean}
 *          True if the node is either dead or is not an element node.
 */
function isInvalidNode(node) {
  return (
    !node ||
    Cu.isDeadWrapper(node) ||
    node.nodeType !== nodeConstants.ELEMENT_NODE ||
    !node.ownerGlobal
  );
}

/**
 * Determine if a current node has focus specific styling by applying a
 * focus-related pseudo class (such as :focus or :-moz-focusring) to a focusable
 * node.
 *
 * @param   {DOMNode} focusableNode
 *          Node to apply focus-related pseudo class to.
 * @param   {DOMNode} currentNode
 *          Node to be checked for having focus specific styling.
 * @param   {String} pseudoClass
 *          A focus related pseudo-class to be simulated for style comparison.
 *
 * @returns {Boolean}
 *          True if the currentNode has focus specific styling.
 */
function hasStylesForFocusRelatedPseudoClass(
  focusableNode,
  currentNode,
  pseudoClass
) {
  const defaultRules = getCSSStyleRules(currentNode);

  InspectorUtils.addPseudoClassLock(focusableNode, pseudoClass);

  // Determine a set of properties that are specific to CSS rules that are only
  // present when a focus related pseudo-class is locked in.
  const tempRules = getCSSStyleRules(currentNode);
  const properties = new Set();
  for (const rule of tempRules) {
    if (rule.type !== STYLE_RULE) {
      continue;
    }

    if (!defaultRules.includes(rule)) {
      for (let index = 0; index < rule.style.length; index++) {
        properties.add(rule.style.item(index));
      }
    }
  }

  // If there are no focus specific CSS rules or properties, currentNode does
  // node have any focus specific styling, we are done.
  if (properties.size === 0) {
    InspectorUtils.removePseudoClassLock(focusableNode, pseudoClass);
    return false;
  }

  // Determine values for properties that are focus specific.
  const tempStyle = CssLogic.getComputedStyle(currentNode);
  const focusStyle = {};
  for (const name of properties.values()) {
    focusStyle[name] = tempStyle.getPropertyValue(name);
  }

  InspectorUtils.removePseudoClassLock(focusableNode, pseudoClass);

  // If values for focus specific properties are different from default style
  // values, assume we have focus spefic styles for the currentNode.
  const defaultStyle = CssLogic.getComputedStyle(currentNode);
  for (const name of properties.values()) {
    if (defaultStyle.getPropertyValue(name) !== focusStyle[name]) {
      return true;
    }
  }

  return false;
}

/**
 * Check if an element node (currentNode) has distinct focus styling. This
 * function also takes into account a case when focus styling is applied to a
 * descendant too.
 *
 * @param   {DOMNode} focusableNode
 *          Node to apply focus-related pseudo class to.
 * @param   {DOMNode} currentNode
 *          Node to be checked for having focus specific styling.
 *
 * @returns {Boolean}
 *          True if the node or its descendant has distinct focus styling.
 */
function hasFocusStyling(focusableNode, currentNode) {
  if (isInvalidNode(currentNode)) {
    return false;
  }

  // Check if an element node has distinct :-moz-focusring styling.
  const hasStylesForMozFocusring = hasStylesForFocusRelatedPseudoClass(
    focusableNode,
    currentNode,
    MOZ_FOCUSRING_PSEUDO_CLASS
  );
  if (hasStylesForMozFocusring) {
    return true;
  }

  // Check if an element node has distinct :focus styling.
  const hasStylesForFocus = hasStylesForFocusRelatedPseudoClass(
    focusableNode,
    currentNode,
    FOCUS_PSEUDO_CLASS
  );
  if (hasStylesForFocus) {
    return true;
  }

  // If no element specific focus styles where found, check if its element
  // children have them.
  for (
    let child = currentNode.firstElementChild;
    child;
    child = currentNode.nextnextElementSibling
  ) {
    if (hasFocusStyling(focusableNode, child)) {
      return true;
    }
  }

  return false;
}

/**
 * A rule that determines if a focusable accessible object has appropriate focus
 * styling.
 *
 * @param  {nsIAccessible} accessible
 *         Accessible to be checked for being focusable and having focus
 *         styling.
 *
 * @return {null|Object}
 *         Null if accessible has keyboard focus styling, audit report object
 *         otherwise.
 */
function focusStyleRule(accessible) {
  const { DOMNode } = accessible;
  if (isInvalidNode(DOMNode)) {
    return null;
  }

  // Ignore non-focusable elements.
  const state = {};
  accessible.getState(state, {});
  if (!(state.value & Ci.nsIAccessibleStates.STATE_FOCUSABLE)) {
    return null;
  }

  if (hasFocusStyling(DOMNode, DOMNode)) {
    return null;
  }

  // If no browser or author focus styling was found, check if the node is a
  // widget that is themed by platform native theme.
  if (InspectorUtils.isElementThemed(DOMNode)) {
    return null;
  }

  return { score: WARNING, issue: NO_FOCUS_VISIBLE };
}

/**
 * A rule that determines if an interactive accessible has any associated
 * accessible actions with it. If the element is interactive but and has no
 * actions, assistive technology users will not be able to interact with it.
 *
 * @param  {nsIAccessible} accessible
 *         Accessible to be checked for being interactive and having accessible
 *         actions.
 *
 * @return {null|Object}
 *         Null if accessible is not interactive or if it is and it has
 *         accessible action associated with it, audit report object otherwise.
 */
function interactiveRule(accessible) {
  if (!INTERACTIVE_ROLES.has(accessible.role)) {
    return null;
  }

  if (accessible.actionCount > 0) {
    return null;
  }

  return { score: FAIL, issue: INTERACTIVE_NO_ACTION };
}

/**
 * A rule that determines if an interactive accessible is also focusable when
 * not disabled.
 *
 * @param  {nsIAccessible} accessible
 *         Accessible to be checked for being interactive and being focusable
 *         when enabled.
 *
 * @return {null|Object}
 *         Null if accessible is not interactive or if it is and it is focusable
 *         when enabled, audit report object otherwise.
 */
function focusableRule(accessible) {
  if (!KEYBOARD_FOCUSABLE_ROLES.has(accessible.role)) {
    return null;
  }

  const state = {};
  accessible.getState(state, {});
  // We only expect in interactive accessible object to be focusable if it is
  // not disabled.
  if (state.value & Ci.nsIAccessibleStates.STATE_UNAVAILABLE) {
    return null;
  }

  // State will be focusable even if the tabindex is negative.
  if (
    state.value & Ci.nsIAccessibleStates.STATE_FOCUSABLE &&
    accessible.DOMNode.tabIndex > -1
  ) {
    return null;
  }

  let ariaRoles;
  try {
    ariaRoles = accessible.attributes.getStringProperty("xml-roles");
  } catch (e) {
    // No xml-roles. nsPersistentProperties throws if the attribute for a key
    // is not found.
  }
  if (
    ariaRoles &&
    (ariaRoles.includes("combobox") || ariaRoles.includes("listbox"))
  ) {
    // Do not force ARIA combobox or listbox to be focusable.
    return null;
  }

  return { score: FAIL, issue: INTERACTIVE_NOT_FOCUSABLE };
}

/**
 * A rule that determines if a focusable accessible has an associated
 * interactive role.
 *
 * @param  {nsIAccessible} accessible
 *         Accessible to be checked for having an interactive role if it is
 *         focusable.
 *
 * @return {null|Object}
 *         Null if accessible is not interactive or if it is and it has an
 *         interactive role, audit report object otherwise.
 */
function semanticsRule(accessible) {
  if (
    INTERACTIVE_ROLES.has(accessible.role) ||
    // Visible listboxes will have focusable state when inside comboboxes.
    accessible.role === Ci.nsIAccessibleRole.ROLE_COMBOBOX_LIST
  ) {
    return null;
  }

  const state = {};
  accessible.getState(state, {});
  if (state.value & Ci.nsIAccessibleStates.STATE_FOCUSABLE) {
    return { score: WARNING, issue: FOCUSABLE_NO_SEMANTICS };
  }

  if (
    // Ignore text leafs.
    accessible.role === Ci.nsIAccessibleRole.ROLE_TEXT_LEAF ||
    // Ignore accessibles with no accessible actions.
    accessible.actionCount === 0 ||
    // Ignore labels that have a label for relation with their target because
    // they are clickable.
    (accessible.role === Ci.nsIAccessibleRole.ROLE_LABEL &&
      accessible.getRelationByType(Ci.nsIAccessibleRelation.RELATION_LABEL_FOR)
        .targetsCount > 0) ||
    // Ignore images that are inside an anchor (have linked state).
    (accessible.role === Ci.nsIAccessibleRole.ROLE_GRAPHIC &&
      state.value & Ci.nsIAccessibleStates.STATE_LINKED)
  ) {
    return null;
  }

  // Ignore anything but a click action in the list of actions.
  for (let i = 0; i < accessible.actionCount; i++) {
    if (accessible.getActionName(i) === CLICK_ACTION) {
      return { score: FAIL, issue: MOUSE_INTERACTIVE_ONLY };
    }
  }

  return null;
}

/**
 * A rule that determines if an element associated with a focusable accessible
 * has a positive tabindex.
 *
 * @param  {nsIAccessible} accessible
 *         Accessible to be checked for having an element with positive tabindex
 *         attribute.
 *
 * @return {null|Object}
 *         Null if accessible is not focusable or if it is and its element's
 *         tabindex attribute is less than 1, audit report object otherwise.
 */
function tabIndexRule(accessible) {
  const { DOMNode } = accessible;
  if (isInvalidNode(DOMNode)) {
    return null;
  }

  const state = {};
  accessible.getState(state, {});
  if (!(state.value & Ci.nsIAccessibleStates.STATE_FOCUSABLE)) {
    return null;
  }

  if (DOMNode.tabIndex > 0) {
    return { score: WARNING, issue: FOCUSABLE_POSITIVE_TABINDEX };
  }

  return null;
}

function auditKeyboard(accessible) {
  // Do not test anything on accessible objects for documents or frames.
  if (
    accessible.role === Ci.nsIAccessibleRole.ROLE_DOCUMENT ||
    accessible.role === Ci.nsIAccessibleRole.ROLE_INTERNAL_FRAME
  ) {
    return null;
  }

  // Check if interactive accessible can be used by the assistive
  // technology.
  let issue = interactiveRule(accessible);
  if (issue) {
    return issue;
  }

  // Check if interactive accessible is also focusable when enabled.
  issue = focusableRule(accessible);
  if (issue) {
    return issue;
  }

  // Check if accessible object has an element with a positive tabindex.
  issue = tabIndexRule(accessible);
  if (issue) {
    return issue;
  }

  // Check if a focusable accessible has interactive semantics.
  issue = semanticsRule(accessible);
  if (issue) {
    return issue;
  }

  // Check if focusable accessible has associated focus styling.
  issue = focusStyleRule(accessible);
  if (issue) {
    return issue;
  }

  return issue;
}

module.exports.auditKeyboard = auditKeyboard;
