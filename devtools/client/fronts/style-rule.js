/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const {
  styleRuleSpec,
} = require("resource://devtools/shared/specs/style-rule.js");

loader.lazyRequireGetter(
  this,
  "RuleRewriter",
  "resource://devtools/client/fronts/inspector/rule-rewriter.js"
);

/**
 * StyleRuleFront, the front for the StyleRule actor.
 */
class StyleRuleFront extends FrontClassWithSpec(styleRuleSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this.before("location-changed", this._locationChangedPre.bind(this));
  }

  form(form) {
    this.actorID = form.actor;
    this._form = form;
    this.traits = form.traits || {};
  }

  /**
   * Ensure _form is updated when location-changed is emitted.
   */
  _locationChangedPre(line, column) {
    this._form.line = line;
    this._form.column = column;
  }

  /**
   * Return a new RuleModificationList or RuleRewriter for this node.
   * A RuleRewriter will be returned when the rule's canSetRuleText
   * trait is true; otherwise a RuleModificationList will be
   * returned.
   *
   * @param {CssPropertiesFront} cssProperties
   *                             This is needed by the RuleRewriter.
   * @return {RuleModificationList}
   */
  startModifyingProperties(cssProperties) {
    if (this.canSetRuleText) {
      return new RuleRewriter(cssProperties.isKnown, this, this.authoredText);
    }
    return new RuleModificationList(this);
  }

  get type() {
    return this._form.type;
  }
  get line() {
    return this._form.line || -1;
  }
  get column() {
    return this._form.column || -1;
  }
  get cssText() {
    return this._form.cssText;
  }
  get authoredText() {
    return typeof this._form.authoredText === "string"
      ? this._form.authoredText
      : this._form.cssText;
  }
  get declarations() {
    return this._form.declarations || [];
  }
  get keyText() {
    return this._form.keyText;
  }
  get name() {
    return this._form.name;
  }
  get selectors() {
    return this._form.selectors;
  }

  /**
   * When a rule is nested in another non-at-rule (aka CSS Nesting), this will return
   * the "full" selectors, which includes ancestor rules selectors.
   * To compute it, the parent selector (&) is recursively replaced by the parent
   * rule selector wrapped in `:is()`.
   * For example, with the following nested rule: `body { & > main {} }`,
   * the desugared selectors will be [`:is(body) > main`],
   * while the "regular" selectors will only be [`main`].
   *
   * See https://www.w3.org/TR/css-nesting-1/#nest-selector for more information.
   *
   * @returns {Array<String>} An array of the desugared selectors for this rule.
   *                          This falls back to the regular list of selectors
   *                          when desugared selectors are not sent by the server.
   */
  get desugaredSelectors() {
    // We don't send the desugaredSelectors for top-level selectors, so fall back to
    // the regular selectors in that case.
    return this._form.desugaredSelectors || this._form.selectors;
  }

  get selectorWarnings() {
    return this._form.selectorWarnings;
  }

  get parentStyleSheet() {
    const resourceCommand = this.targetFront.commands.resourceCommand;
    return resourceCommand.getResourceById(
      resourceCommand.TYPES.STYLESHEET,
      this._form.parentStyleSheet
    );
  }

  get element() {
    return this.conn.getFrontByID(this._form.element);
  }

  get href() {
    if (this._form.href) {
      return this._form.href;
    }
    const sheet = this.parentStyleSheet;
    return sheet ? sheet.href : "";
  }

  get nodeHref() {
    const sheet = this.parentStyleSheet;
    return sheet ? sheet.nodeHref : "";
  }

  get canSetRuleText() {
    return this._form.traits && this._form.traits.canSetRuleText;
  }

  get location() {
    return {
      source: this.parentStyleSheet,
      href: this.href,
      line: this.line,
      column: this.column,
    };
  }

  get ancestorData() {
    return this._form.ancestorData;
  }

  get userAdded() {
    return this._form.userAdded;
  }

  async modifySelector(node, value) {
    const response = await super.modifySelector(
      node,
      value,
      this.canSetRuleText
    );

    if (response.ruleProps) {
      response.ruleProps = response.ruleProps.entries[0];
    }
    return response;
  }

  setRuleText(newText, modifications) {
    this._form.authoredText = newText;
    return super.setRuleText(newText, modifications);
  }
}

exports.StyleRuleFront = StyleRuleFront;
registerFront(StyleRuleFront);

/**
 * Convenience API for building a list of attribute modifications
 * for the `modifyProperties` request.  A RuleModificationList holds a
 * list of modifications that will be applied to a StyleRuleActor.
 * The modifications are processed in the order in which they are
 * added to the RuleModificationList.
 *
 * Objects of this type expose the same API as @see RuleRewriter.
 * This lets the inspector use (mostly) the same code, regardless of
 * whether the server implements setRuleText.
 */
class RuleModificationList {
  /**
   * Initialize a RuleModificationList.
   * @param {StyleRuleFront} rule the associated rule
   */
  constructor(rule) {
    this.rule = rule;
    this.modifications = [];
  }

  /**
   * Apply the modifications in this object to the associated rule.
   *
   * @return {Promise} A promise which will be resolved when the modifications
   *         are complete; @see StyleRuleActor.modifyProperties.
   */
  apply() {
    return this.rule.modifyProperties(this.modifications);
  }

  /**
   * Add a "set" entry to the modification list.
   *
   * @param {Number} index index of the property in the rule.
   *                       This can be -1 in the case where
   *                       the rule does not support setRuleText;
   *                       generally for setting properties
   *                       on an element's style.
   * @param {String} name the property's name
   * @param {String} value the property's value
   * @param {String} priority the property's priority, either the empty
   *                          string or "important"
   */
  setProperty(index, name, value, priority) {
    this.modifications.push({ type: "set", index, name, value, priority });
  }

  /**
   * Add a "remove" entry to the modification list.
   *
   * @param {Number} index index of the property in the rule.
   *                       This can be -1 in the case where
   *                       the rule does not support setRuleText;
   *                       generally for setting properties
   *                       on an element's style.
   * @param {String} name the name of the property to remove
   */
  removeProperty(index, name) {
    this.modifications.push({ type: "remove", index, name });
  }

  /**
   * Rename a property.  This implementation acts like
   * |removeProperty|, because |setRuleText| is not available.
   *
   * @param {Number} index index of the property in the rule.
   *                       This can be -1 in the case where
   *                       the rule does not support setRuleText;
   *                       generally for setting properties
   *                       on an element's style.
   * @param {String} name current name of the property
   *
   * This parameter is also passed, but as it is not used in this
   * implementation, it is omitted.  It is documented here as this
   * code also defined the interface implemented by @see RuleRewriter.
   * @param {String} newName new name of the property
   */
  renameProperty(index, name) {
    this.removeProperty(index, name);
  }

  /**
   * Enable or disable a property.  This implementation acts like
   * a no-op when enabling, because |setRuleText| is not available.
   *
   * @param {Number} index index of the property in the rule.
   *                       This can be -1 in the case where
   *                       the rule does not support setRuleText;
   *                       generally for setting properties
   *                       on an element's style.
   * @param {String} name current name of the property
   * @param {Boolean} isEnabled true if the property should be enabled;
   *                        false if it should be disabled
   */
  setPropertyEnabled(index, name, isEnabled) {
    if (!isEnabled) {
      this.modifications.push({ type: "disable", index, name });
    }
  }

  /**
   * Create a new property.  This implementation does nothing, because
   * |setRuleText| is not available.
   *
   * These parameters are passed, but as they are not used in this
   * implementation, they are omitted.  They are documented here as
   * this code also defined the interface implemented by @see
   * RuleRewriter.
   *
   * @param {Number} index index of the property in the rule.
   *                       This can be -1 in the case where
   *                       the rule does not support setRuleText;
   *                       generally for setting properties
   *                       on an element's style.
   * @param {String} name name of the new property
   * @param {String} value value of the new property
   * @param {String} priority priority of the new property; either
   *                          the empty string or "important"
   * @param {Boolean} enabled True if the new property should be
   *                          enabled, false if disabled
   */
  createProperty() {
    // Nothing.
  }
}
