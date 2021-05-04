/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { styleRuleSpec } = require("devtools/shared/specs/style-rule");
const promise = require("promise");

loader.lazyRequireGetter(
  this,
  "RuleRewriter",
  "devtools/client/fronts/inspector/rule-rewriter"
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
    if (this._mediaText) {
      this._mediaText = null;
    }
  }

  /**
   * Ensure _form is updated when location-changed is emitted.
   */
  _locationChangedPre(line, column) {
    this._clearOriginalLocation();
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
  get media() {
    return this._form.media;
  }
  get mediaText() {
    if (!this._form.media) {
      return null;
    }
    if (this._mediaText) {
      return this._mediaText;
    }
    this._mediaText = this.media.join(", ");
    return this._mediaText;
  }

  get parentRule() {
    return this.conn.getFrontByID(this._form.parentRule);
  }

  get parentStyleSheet() {
    const resourceCommand = this.targetFront.resourceCommand;
    if (
      resourceCommand?.hasResourceCommandSupport(
        resourceCommand.TYPES.STYLESHEET
      )
    ) {
      return resourceCommand.getResourceById(
        resourceCommand.TYPES.STYLESHEET,
        this._form.parentStyleSheet
      );
    }

    return this.conn.getFrontByID(this._form.parentStyleSheet);
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

  _clearOriginalLocation() {
    this._originalLocation = null;
  }

  getOriginalLocation() {
    if (this._originalLocation) {
      return promise.resolve(this._originalLocation);
    }
    const parentSheet = this.parentStyleSheet;
    if (!parentSheet) {
      // This rule doesn't belong to a stylesheet so it is an inline style.
      // Inline styles do not have any mediaText so we can return early.
      return promise.resolve(this.location);
    }
    return parentSheet
      .getOriginalLocation(this.line, this.column)
      .then(({ fromSourceMap, source, line, column }) => {
        const location = {
          href: source,
          line: line,
          column: column,
          mediaText: this.mediaText,
        };
        if (fromSourceMap === false) {
          location.source = this.parentStyleSheet;
        }
        if (!source) {
          location.href = this.href;
        }
        this._originalLocation = location;
        return location;
      });
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
