/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ClassList = require("devtools/client/inspector/rules/models/class-list");

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(
  "devtools/client/locales/inspector.properties"
);
const AutocompletePopup = require("devtools/client/shared/autocomplete-popup");
const { debounce } = require("devtools/shared/debounce");

/**
 * This UI widget shows a textfield and a series of checkboxes in the rule-view. It is
 * used to toggle classes on the current node selection, and add new classes.
 */
class ClassListPreviewer {
  /*
   * @param {Inspector} inspector
   *        The current inspector instance.
   * @param {DomNode} containerEl
   *        The element in the rule-view where the widget should go.
   */
  constructor(inspector, containerEl) {
    this.inspector = inspector;
    this.containerEl = containerEl;
    this.model = new ClassList(inspector);

    this.onNewSelection = this.onNewSelection.bind(this);
    this.onCheckBoxChanged = this.onCheckBoxChanged.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.onAddElementInputModified = debounce(
      this.onAddElementInputModified,
      75,
      this
    );
    this.onCurrentNodeClassChanged = this.onCurrentNodeClassChanged.bind(this);
    this.onNodeFrontWillUnset = this.onNodeFrontWillUnset.bind(this);
    this.onAutocompleteClassHovered = debounce(
      this.onAutocompleteClassHovered,
      75,
      this
    );
    this.onAutocompleteClosed = this.onAutocompleteClosed.bind(this);

    // Create the add class text field.
    this.addEl = this.doc.createElement("input");
    this.addEl.classList.add("devtools-textinput");
    this.addEl.classList.add("add-class");
    this.addEl.setAttribute(
      "placeholder",
      L10N.getStr("inspector.classPanel.newClass.placeholder")
    );
    this.addEl.addEventListener("keydown", this.onKeyDown);
    this.addEl.addEventListener("input", this.onAddElementInputModified);
    this.containerEl.appendChild(this.addEl);

    // Create the class checkboxes container.
    this.classesEl = this.doc.createElement("div");
    this.classesEl.classList.add("classes");
    this.containerEl.appendChild(this.classesEl);

    // Create the autocomplete popup
    this.autocompletePopup = new AutocompletePopup(this.inspector.toolbox.doc, {
      listId: "inspector_classListPreviewer_autocompletePopupListBox",
      position: "bottom",
      autoSelect: true,
      useXulWrapper: true,
      input: this.addEl,
      onClick: (e, item) => {
        if (item) {
          this.addEl.value = item.label;
          this.autocompletePopup.hidePopup();
          this.autocompletePopup.clearItems();
          this.model.previewClass(item.label);
        }
      },
      onSelect: item => {
        if (item) {
          this.onAutocompleteClassHovered(item?.label);
        }
      },
    });

    // Start listening for interesting events.
    this.inspector.selection.on("new-node-front", this.onNewSelection);
    this.inspector.selection.on(
      "node-front-will-unset",
      this.onNodeFrontWillUnset
    );
    this.containerEl.addEventListener("input", this.onCheckBoxChanged);
    this.model.on("current-node-class-changed", this.onCurrentNodeClassChanged);
    this.autocompletePopup.on("popup-closed", this.onAutocompleteClosed);

    this.onNewSelection();
  }

  destroy() {
    this.inspector.selection.off("new-node-front", this.onNewSelection);
    this.inspector.selection.off(
      "node-front-will-unset",
      this.onNodeFrontWillUnset
    );
    this.autocompletePopup.off("popup-closed", this.onAutocompleteClosed);
    this.addEl.removeEventListener("keydown", this.onKeyDown);
    this.addEl.removeEventListener("input", this.onAddElementInputModified);
    this.containerEl.removeEventListener("input", this.onCheckBoxChanged);

    this.autocompletePopup.destroy();

    this.containerEl.innerHTML = "";

    this.model.destroy();
    this.containerEl = null;
    this.inspector = null;
    this.addEl = null;
    this.classesEl = null;
  }

  get doc() {
    return this.containerEl.ownerDocument;
  }

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
  }

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
  }

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
  }

  /**
   * Focus the add-class text field.
   */
  focusAddClassField() {
    if (this.addEl) {
      this.addEl.focus();
    }
  }

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
  }

  onKeyDown(event) {
    // If the popup is already open, all the keyboard interaction are handled
    // directly by the popup component.
    if (this.autocompletePopup.isOpen) {
      return;
    }

    // Open the autocomplete popup on Ctrl+Space / ArrowDown (when the input isn't empty)
    if (
      (this.addEl.value && event.key === " " && event.ctrlKey) ||
      event.key === "ArrowDown"
    ) {
      this.onAddElementInputModified();
      return;
    }

    if (this.addEl.value !== "" && event.key === "Enter") {
      this.addClassName(this.addEl.value);
    }
  }

  async onAddElementInputModified() {
    const newValue = this.addEl.value;

    // if the input is empty, let's close the popup, if it was open.
    if (newValue === "") {
      if (this.autocompletePopup.isOpen) {
        this.autocompletePopup.hidePopup();
        this.autocompletePopup.clearItems();
      } else {
        this.model.previewClass("");
      }
      return;
    }

    // Otherwise, we need to update the popup items to match the new input.
    let items = [];
    try {
      const classNames = await this.model.getClassNames(newValue);
      if (!this.autocompletePopup.isOpen) {
        this._previewClassesBeforeAutocompletion = this.model.previewClasses.map(
          previewClass => previewClass.className
        );
      }
      items = classNames.map(className => {
        return {
          preLabel: className.substring(0, newValue.length),
          label: className,
        };
      });
    } catch (e) {
      // If there was an error while retrieving the classNames, we'll simply NOT show the
      // popup, which is okay.
      console.warn("Error when calling getClassNames", e);
    }

    if (!items.length || (items.length == 1 && items[0].label === newValue)) {
      this.autocompletePopup.clearItems();
      await this.autocompletePopup.hidePopup();
      this.model.previewClass(newValue);
    } else {
      this.autocompletePopup.setItems(items);
      this.autocompletePopup.openPopup();
    }
  }

  async addClassName(className) {
    try {
      await this.model.addClassName(className);
      this.render();
      this.addEl.value = "";
    } catch (e) {
      // Only log the error if the panel wasn't destroyed in the meantime.
      if (this.containerEl) {
        console.error(e);
      }
    }
  }

  onNewSelection() {
    this.render();
  }

  onCurrentNodeClassChanged() {
    this.render();
  }

  onNodeFrontWillUnset() {
    this.model.eraseClassPreview();
    this.addEl.value = "";
  }

  onAutocompleteClassHovered(autocompleteItemLabel = "") {
    if (this.autocompletePopup.isOpen) {
      this.model.previewClass(autocompleteItemLabel);
    }
  }

  onAutocompleteClosed() {
    const inputValue = this.addEl.value;
    this.model.previewClass(inputValue);
  }
}

module.exports = ClassListPreviewer;
