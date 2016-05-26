/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

require("devtools/shared/fronts/stylesheets");
const {
  Front,
  FrontClassWithSpec,
  custom,
  preEvent
} = require("devtools/shared/protocol");
const {
  pageStyleSpec,
  styleRuleSpec
} = require("devtools/shared/specs/styles");
const promise = require("promise");
const { Task } = require("devtools/shared/task");
const { Class } = require("sdk/core/heritage");

loader.lazyGetter(this, "RuleRewriter", () => {
  return require("devtools/shared/css-parsing-utils").RuleRewriter;
});

/**
 * PageStyleFront, the front object for the PageStyleActor
 */
const PageStyleFront = FrontClassWithSpec(pageStyleSpec, {
  initialize: function (conn, form, ctx, detail) {
    Front.prototype.initialize.call(this, conn, form, ctx, detail);
    this.inspector = this.parent();
  },

  form: function (form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this._form = form;
  },

  destroy: function () {
    Front.prototype.destroy.call(this);
  },

  get walker() {
    return this.inspector.walker;
  },

  get supportsAuthoredStyles() {
    return this._form.traits && this._form.traits.authoredStyles;
  },

  getMatchedSelectors: custom(function (node, property, options) {
    return this._getMatchedSelectors(node, property, options).then(ret => {
      return ret.matched;
    });
  }, {
    impl: "_getMatchedSelectors"
  }),

  getApplied: custom(Task.async(function* (node, options = {}) {
    // If the getApplied method doesn't recreate the style cache itself, this
    // means a call to cssLogic.highlight is required before trying to access
    // the applied rules. Issue a request to getLayout if this is the case.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1103993#c16.
    if (!this._form.traits || !this._form.traits.getAppliedCreatesStyleCache) {
      yield this.getLayout(node);
    }
    let ret = yield this._getApplied(node, options);
    return ret.entries;
  }), {
    impl: "_getApplied"
  }),

  addNewRule: custom(function (node, pseudoClasses) {
    let addPromise;
    if (this.supportsAuthoredStyles) {
      addPromise = this._addNewRule(node, pseudoClasses, true);
    } else {
      addPromise = this._addNewRule(node, pseudoClasses);
    }
    return addPromise.then(ret => {
      return ret.entries[0];
    });
  }, {
    impl: "_addNewRule"
  })
});

exports.PageStyleFront = PageStyleFront;

/**
 * StyleRuleFront, the front for the StyleRule actor.
 */
const StyleRuleFront = FrontClassWithSpec(styleRuleSpec, {
  initialize: function (client, form, ctx, detail) {
    Front.prototype.initialize.call(this, client, form, ctx, detail);
  },

  destroy: function () {
    Front.prototype.destroy.call(this);
  },

  form: function (form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this.actorID = form.actor;
    this._form = form;
    if (this._mediaText) {
      this._mediaText = null;
    }
  },

  /**
   * Ensure _form is updated when location-changed is emitted.
   */
  _locationChangedPre: preEvent("location-changed", function (line, column) {
    this._clearOriginalLocation();
    this._form.line = line;
    this._form.column = column;
  }),

  /**
   * Return a new RuleModificationList or RuleRewriter for this node.
   * A RuleRewriter will be returned when the rule's canSetRuleText
   * trait is true; otherwise a RuleModificationList will be
   * returned.
   */
  startModifyingProperties: function () {
    if (this.canSetRuleText) {
      return new RuleRewriter(this, this.authoredText);
    }
    return new RuleModificationList(this);
  },

  get type() {
    return this._form.type;
  },
  get line() {
    return this._form.line || -1;
  },
  get column() {
    return this._form.column || -1;
  },
  get cssText() {
    return this._form.cssText;
  },
  get authoredText() {
    return this._form.authoredText || this._form.cssText;
  },
  get declarations() {
    return this._form.declarations || [];
  },
  get keyText() {
    return this._form.keyText;
  },
  get name() {
    return this._form.name;
  },
  get selectors() {
    return this._form.selectors;
  },
  get media() {
    return this._form.media;
  },
  get mediaText() {
    if (!this._form.media) {
      return null;
    }
    if (this._mediaText) {
      return this._mediaText;
    }
    this._mediaText = this.media.join(", ");
    return this._mediaText;
  },

  get parentRule() {
    return this.conn.getActor(this._form.parentRule);
  },

  get parentStyleSheet() {
    return this.conn.getActor(this._form.parentStyleSheet);
  },

  get element() {
    return this.conn.getActor(this._form.element);
  },

  get href() {
    if (this._form.href) {
      return this._form.href;
    }
    let sheet = this.parentStyleSheet;
    return sheet ? sheet.href : "";
  },

  get nodeHref() {
    let sheet = this.parentStyleSheet;
    return sheet ? sheet.nodeHref : "";
  },

  get supportsModifySelectorUnmatched() {
    return this._form.traits && this._form.traits.modifySelectorUnmatched;
  },

  get canSetRuleText() {
    return this._form.traits && this._form.traits.canSetRuleText;
  },

  get location() {
    return {
      source: this.parentStyleSheet,
      href: this.href,
      line: this.line,
      column: this.column
    };
  },

  _clearOriginalLocation: function () {
    this._originalLocation = null;
  },

  getOriginalLocation: function () {
    if (this._originalLocation) {
      return promise.resolve(this._originalLocation);
    }
    let parentSheet = this.parentStyleSheet;
    if (!parentSheet) {
      // This rule doesn't belong to a stylesheet so it is an inline style.
      // Inline styles do not have any mediaText so we can return early.
      return promise.resolve(this.location);
    }
    return parentSheet.getOriginalLocation(this.line, this.column)
      .then(({ fromSourceMap, source, line, column }) => {
        let location = {
          href: source,
          line: line,
          column: column,
          mediaText: this.mediaText
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
  },

  modifySelector: custom(Task.async(function* (node, value) {
    let response;
    if (this.supportsModifySelectorUnmatched) {
      // If the debugee supports adding unmatched rules (post FF41)
      if (this.canSetRuleText) {
        response = yield this.modifySelector2(node, value, true);
      } else {
        response = yield this.modifySelector2(node, value);
      }
    } else {
      response = yield this._modifySelector(value);
    }

    if (response.ruleProps) {
      response.ruleProps = response.ruleProps.entries[0];
    }
    return response;
  }), {
    impl: "_modifySelector"
  }),

  setRuleText: custom(function (newText) {
    this._form.authoredText = newText;
    return this._setRuleText(newText);
  }, {
    impl: "_setRuleText"
  })
});

exports.StyleRuleFront = StyleRuleFront;

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
var RuleModificationList = Class({
  /**
   * Initialize a RuleModificationList.
   * @param {StyleRuleFront} rule the associated rule
   */
  initialize: function (rule) {
    this.rule = rule;
    this.modifications = [];
  },

  /**
   * Apply the modifications in this object to the associated rule.
   *
   * @return {Promise} A promise which will be resolved when the modifications
   *         are complete; @see StyleRuleActor.modifyProperties.
   */
  apply: function () {
    return this.rule.modifyProperties(this.modifications);
  },

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
  setProperty: function (index, name, value, priority) {
    this.modifications.push({
      type: "set",
      name: name,
      value: value,
      priority: priority
    });
  },

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
  removeProperty: function (index, name) {
    this.modifications.push({
      type: "remove",
      name: name
    });
  },

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
  renameProperty: function (index, name) {
    this.removeProperty(index, name);
  },

  /**
   * Enable or disable a property.  This implementation acts like
   * |removeProperty| when disabling, or a no-op when enabling,
   * because |setRuleText| is not available.
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
  setPropertyEnabled: function (index, name, isEnabled) {
    if (!isEnabled) {
      this.removeProperty(index, name);
    }
  },

  /**
   * Create a new property.  This implementation does nothing, because
   * |setRuleText| is not available.
   *
   * These parameter are passed, but as they are not used in this
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
   */
  createProperty: function () {
    // Nothing.
  },
});

