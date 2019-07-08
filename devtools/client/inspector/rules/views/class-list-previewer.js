/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ClassList = require("devtools/client/inspector/rules/models/class-list");
const { LocalizationHelper } = require("devtools/shared/l10n");

const L10N = new LocalizationHelper(
  "devtools/client/locales/inspector.properties"
);

/**
 * This UI widget shows a textfield and a series of checkboxes in the rule-view. It is
 * used to toggle classes on the current node selection, and add new classes.
 *
 * @param {Inspector} inspector
 *        The current inspector instance.
 * @param {DomNode} containerEl
 *        The element in the rule-view where the widget should go.
 */
function ClassListPreviewer(inspector, containerEl) {
  this.inspector = inspector;
  this.containerEl = containerEl;
  this.model = new ClassList(inspector);

  this.onNewSelection = this.onNewSelection.bind(this);
  this.onCheckBoxChanged = this.onCheckBoxChanged.bind(this);
  this.onKeyPress = this.onKeyPress.bind(this);
  this.onCurrentNodeClassChanged = this.onCurrentNodeClassChanged.bind(this);

  // Create the add class text field.
  this.addEl = this.doc.createElement("input");
  this.addEl.classList.add("devtools-textinput");
  this.addEl.classList.add("add-class");
  this.addEl.setAttribute(
    "placeholder",
    L10N.getStr("inspector.classPanel.newClass.placeholder")
  );
  this.addEl.addEventListener("keypress", this.onKeyPress);
  this.containerEl.appendChild(this.addEl);

  // Create the class checkboxes container.
  this.classesEl = this.doc.createElement("div");
  this.classesEl.classList.add("classes");
  this.containerEl.appendChild(this.classesEl);

  // Start listening for interesting events.
  this.inspector.selection.on("new-node-front", this.onNewSelection);
  this.containerEl.addEventListener("input", this.onCheckBoxChanged);
  this.model.on("current-node-class-changed", this.onCurrentNodeClassChanged);

  this.onNewSelection();
}

ClassListPreviewer.prototype = {
  destroy() {
    this.inspector.selection.off("new-node-front", this.onNewSelection);
    this.addEl.removeEventListener("keypress", this.onKeyPress);
    this.containerEl.removeEventListener("input", this.onCheckBoxChanged);

    this.containerEl.innerHTML = "";

    this.model.destroy();
    this.containerEl = null;
    this.inspector = null;
    this.addEl = null;
    this.classesEl = null;
  },

  get doc() {
    return this.containerEl.ownerDocument;
  },

  /**
   * Render the content of the panel. You typically don't need to call this as the panel
   * renders itself on inspector selection changes.
   */
  render() {
    this.classesEl.innerHTML = "";

    for (const { name, isApplied } of this.model.currentClasses) {
      const checkBox = this.renderCheckBox(name, isApplied);
      this.classesEl.appendChild(checkBox);
    }

    if (!this.model.currentClasses.length) {
      this.classesEl.appendChild(this.renderNoClassesMessage());
    }
  },

  /**
   * Render a single checkbox for a given classname.
   *
   * @param {String} name
   *        The name of this class.
   * @param {Boolean} isApplied
   *        Is this class currently applied on the DOM node.
   * @return {DOMNode} The DOM element for this checkbox.
   */
  renderCheckBox(name, isApplied) {
    const box = this.doc.createElement("input");
    box.setAttribute("type", "checkbox");
    if (isApplied) {
      box.setAttribute("checked", "checked");
    }
    box.dataset.name = name;

    const labelWrapper = this.doc.createElement("label");
    labelWrapper.setAttribute("title", name);
    labelWrapper.appendChild(box);

    // A child element is required to do the ellipsis.
    const label = this.doc.createElement("span");
    label.textContent = name;
    labelWrapper.appendChild(label);

    return labelWrapper;
  },

  /**
   * Render the message displayed in the panel when the current element has no classes.
   *
   * @return {DOMNode} The DOM element for the message.
   */
  renderNoClassesMessage() {
    const msg = this.doc.createElement("p");
    msg.classList.add("no-classes");
    msg.textContent = L10N.getStr("inspector.classPanel.noClasses");
    return msg;
  },

  /**
   * Focus the add-class text field.
   */
  focusAddClassField() {
    if (this.addEl) {
      this.addEl.focus();
    }
  },

  onCheckBoxChanged({ target }) {
    if (!target.dataset.name) {
      return;
    }

    this.model.setClassState(target.dataset.name, target.checked).catch(e => {
      // Only log the error if the panel wasn't destroyed in the meantime.
      if (this.containerEl) {
        console.error(e);
      }
    });
  },

  onKeyPress(event) {
    if (event.key !== "Enter" || this.addEl.value === "") {
      return;
    }

    this.model
      .addClassName(this.addEl.value)
      .then(() => {
        this.render();
        this.addEl.value = "";
      })
      .catch(e => {
        // Only log the error if the panel wasn't destroyed in the meantime.
        if (this.containerEl) {
          console.error(e);
        }
      });
  },

  onNewSelection() {
    this.render();
  },

  onCurrentNodeClassChanged() {
    this.render();
  },
};

module.exports = ClassListPreviewer;
