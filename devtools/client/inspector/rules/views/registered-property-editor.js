/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("resource://devtools/shared/event-emitter.js");
const {
  appendText,
  createChild,
} = require("resource://devtools/client/inspector/shared/utils.js");

const INDENT_SIZE = 2;
const INDENT_STR = " ".repeat(INDENT_SIZE);

/**
 * RegisteredPropertyEditor creates a list of TextPropertyEditors for a given
 * CSS registered property propertyDefinition that can be rendered in the Rules view.
 *
 * @param {CssRuleView} ruleView
 *        The CssRuleView containg the document holding this rule editor.
 * @param {Rule} rule
 *        The Rule object we're editing.
 */
class RegisteredPropertyEditor extends EventEmitter {
  /**
   * @param {CssRuleView} ruleView
   *        The CssRuleView containing the document holding this rule editor.
   * @param {Object} propertyDefinition
   *        The property definition data as returned by PageStyleActor's getRegisteredProperties
   */
  constructor(ruleView, propertyDefinition) {
    super();

    this.#doc = ruleView.styleDocument;
    this.#propertyDefinition = propertyDefinition;
    this.#createElement();
  }

  #doc;
  #propertyDefinition;
  // The HTMLElement that will represent the registered property. Populated in #createElement.
  element = null;

  #createElement() {
    this.element = this.#doc.createElement("div");
    this.element.className = "ruleview-rule devtools-monospace";
    this.element.setAttribute("uneditable", true);
    this.element.setAttribute("unmatched", false);
    this.element.setAttribute("data-name", this.#propertyDefinition.name);

    // Give a relative position for the inplace editor's measurement
    // span to be placed absolutely against.
    this.element.style.position = "relative";

    const code = createChild(this.element, "code", {
      class: "ruleview-code",
    });

    const header = createChild(code, "header", {});

    this.propertyName = createChild(header, "span", {
      class: "ruleview-registered-property-name",
      textContent: this.#propertyDefinition.name,
    });

    this.openBrace = createChild(header, "span", {
      class: "ruleview-ruleopen",
      textContent: " {",
    });

    // We can't use a proper "ol" as it will mess with selection copy text,
    // adding spaces on list item instead of the one we craft (.ruleview-rule-indent)
    this.propertyList = createChild(code, "div", {
      class: "ruleview-propertylist",
      role: "list",
    });

    this.#populateProperties();

    this.closeBrace = createChild(code, "div", {
      class: "ruleview-ruleclose",
      textContent: "}",
    });
  }

  /**
   * Sets the content of this.#propertyList with the contents of the registered property .
   */
  #populateProperties() {
    const properties = [
      {
        name: "syntax",
        value: `"${this.#propertyDefinition.syntax}"`,
      },
      {
        name: "inherits",
        value: this.#propertyDefinition.inherits,
      },
    ];

    // The initial value may not be set, when syntax is "*", so let's only display
    // it when it is actually set.
    if (this.#propertyDefinition.initialValue !== null) {
      // For JS-defined properties, we want to display them in the same syntax that
      // was used in CSS.registerProperty (so we'll show `initialValue` and not `initial-value`).
      properties.push({
        name: this.#propertyDefinition.fromJS
          ? "initialValue"
          : "initial-value",
        value: this.#propertyDefinition.fromJS
          ? `"${this.#propertyDefinition.initialValue}"`
          : this.#propertyDefinition.initialValue,
      });
    }

    // When the property is registered with CSS.registerProperty, we want to match the
    // object shape of the parameter, so include the "name" property.
    if (this.#propertyDefinition.fromJS) {
      properties.unshift({
        name: "name",
        value: `"${this.#propertyDefinition.name}"`,
      });
    }

    for (const { name, value } of properties) {
      // XXX: We could use the TextPropertyEditor here.
      // Pros:
      // - we'd get the similar markup, so styling would be easier
      // - the value would be properly parsed so our various swatches and popups would work
      //   out of the box
      // - rule view filtering would also work out of the box
      // Cons:
      // - it is quite tied with the Rules view regular rule, which mean we'd have
      //   to modify it to accept registered properties.

      const element = createChild(this.propertyList, "div", {
        role: "listitem",
      });
      const container = createChild(element, "div", {
        class: "ruleview-propertycontainer",
      });

      createChild(container, "span", {
        class: "ruleview-rule-indent clipboard-only",
        textContent: INDENT_STR,
      });

      const nameContainer = createChild(container, "span", {
        class: "ruleview-namecontainer",
      });

      createChild(nameContainer, "span", {
        class: "ruleview-propertyname theme-fg-color3",
        textContent: name,
      });

      appendText(nameContainer, ": ");

      // Create a span that will hold the property and semicolon.
      // Use this span to create a slightly larger click target
      // for the value.
      const valueContainer = createChild(container, "span", {
        class: "ruleview-propertyvaluecontainer",
      });

      // Property value, editable when focused.  Changes to the
      // property value are applied as they are typed, and reverted
      // if the user presses escape.
      createChild(valueContainer, "span", {
        class: "ruleview-propertyvalue theme-fg-color1",
        textContent: value,
      });

      appendText(valueContainer, this.#propertyDefinition.fromJS ? "," : ";");

      this.propertyList.appendChild(element);
    }
  }
}

module.exports = RegisteredPropertyEditor;
