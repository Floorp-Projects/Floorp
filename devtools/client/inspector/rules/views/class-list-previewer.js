/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const {LocalizationHelper} = require("devtools/shared/l10n");

const L10N = new LocalizationHelper("devtools/client/locales/inspector.properties");

// This serves as a local cache for the classes applied to each of the node we care about
// here.
// The map is indexed by NodeFront. Any time a new node is selected in the inspector, an
// entry is added here, indexed by the corresponding NodeFront.
// The value for each entry is an array of each of the class this node has. Items of this
// array are objects like: { name, isApplied } where the name is the class itself, and
// isApplied is a Boolean indicating if the class is applied on the node or not.
const CLASSES = new WeakMap();

/**
 * Manages the list classes per DOM elements we care about.
 * The actual list is stored in the CLASSES const, indexed by NodeFront objects.
 * The responsibility of this class is to be the source of truth for anyone who wants to
 * know which classes a given NodeFront has, and which of these are enabled and which are
 * disabled.
 * It also reacts to DOM mutations so the list of classes is up to date with what is in
 * the DOM.
 * It can also be used to enable/disable a given class, or add classes.
 *
 * @param {Inspector} inspector
 *        The current inspector instance.
 */
function ClassListPreviewerModel(inspector) {
  EventEmitter.decorate(this);

  this.inspector = inspector;

  this.onMutations = this.onMutations.bind(this);
  this.inspector.on("markupmutation", this.onMutations);

  this.classListProxyNode = this.inspector.panelDoc.createElement("div");
}

ClassListPreviewerModel.prototype = {
  destroy() {
    this.inspector.off("markupmutation", this.onMutations);
    this.inspector = null;
    this.classListProxyNode = null;
  },

  /**
   * The current node selection (which only returns if the node is an ELEMENT_NODE type
   * since that's the only type this model can work with.)
   */
  get currentNode() {
    if (this.inspector.selection.isElementNode() &&
        !this.inspector.selection.isPseudoElementNode()) {
      return this.inspector.selection.nodeFront;
    }
    return null;
  },

  /**
   * The class states for the current node selection. See the documentation of the CLASSES
   * constant.
   */
  get currentClasses() {
    if (!this.currentNode) {
      return [];
    }

    if (!CLASSES.has(this.currentNode)) {
      // Use the proxy node to get a clean list of classes.
      this.classListProxyNode.className = this.currentNode.className;
      let nodeClasses = [...new Set([...this.classListProxyNode.classList])].map(name => {
        return { name, isApplied: true };
      });

      CLASSES.set(this.currentNode, nodeClasses);
    }

    return CLASSES.get(this.currentNode);
  },

  /**
   * Same as currentClasses, but returns it in the form of a className string, where only
   * enabled classes are added.
   */
  get currentClassesPreview() {
    return this.currentClasses.filter(({ isApplied }) => isApplied)
                              .map(({ name }) => name)
                              .join(" ");
  },

  /**
   * Set the state for a given class on the current node.
   *
   * @param {String} name
   *        The class which state should be changed.
   * @param {Boolean} isApplied
   *        True if the class should be enabled, false otherwise.
   * @return {Promise} Resolves when the change has been made in the DOM.
   */
  setClassState(name, isApplied) {
    // Do the change in our local model.
    let nodeClasses = this.currentClasses;
    nodeClasses.find(({ name: cName }) => cName === name).isApplied = isApplied;

    return this.applyClassState();
  },

  /**
   * Add several classes to the current node at once.
   *
   * @param {String} classNameString
   *        The string that contains all classes.
   * @return {Promise} Resolves when the change has been made in the DOM.
   */
  addClassName(classNameString) {
    this.classListProxyNode.className = classNameString;
    return Promise.all([...new Set([...this.classListProxyNode.classList])].map(name => {
      return this.addClass(name);
    }));
  },

  /**
   * Add a class to the current node at once.
   *
   * @param {String} name
   *        The class to be added.
   * @return {Promise} Resolves when the change has been made in the DOM.
   */
  addClass(name) {
    // Avoid adding the same class again.
    if (this.currentClasses.some(({ name: cName }) => cName === name)) {
      return Promise.resolve();
    }

    // Change the local model, so we retain the state of the existing classes.
    this.currentClasses.push({ name, isApplied: true });

    return this.applyClassState();
  },

  /**
   * Used internally by other functions like addClass or setClassState. Actually applies
   * the class change to the DOM.
   *
   * @return {Promise} Resolves when the change has been made in the DOM.
   */
  applyClassState() {
    // If there is no valid inspector selection, bail out silently. No need to report an
    // error here.
    if (!this.currentNode) {
      return Promise.resolve();
    }

    // Remember which node we changed and the className we applied, so we can filter out
    // dom mutations that are caused by us in onMutations.
    this.lastStateChange = {
      node: this.currentNode,
      className: this.currentClassesPreview
    };

    // Apply the change to the node.
    let mod = this.currentNode.startModifyingAttributes();
    mod.setAttribute("class", this.currentClassesPreview);
    return mod.apply();
  },

  onMutations(mutations) {
    for (let {type, target, attributeName} of mutations) {
      // Only care if this mutation is for the class attribute.
      if (type !== "attributes" || attributeName !== "class") {
        continue;
      }

      let isMutationForOurChange = this.lastStateChange &&
                                   target === this.lastStateChange.node &&
                                   target.className === this.lastStateChange.className;

      if (!isMutationForOurChange) {
        CLASSES.delete(target);
        if (target === this.currentNode) {
          this.emit("current-node-class-changed");
        }
      }
    }
  }
};

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
  this.model = new ClassListPreviewerModel(inspector);

  this.onNewSelection = this.onNewSelection.bind(this);
  this.onCheckBoxChanged = this.onCheckBoxChanged.bind(this);
  this.onKeyPress = this.onKeyPress.bind(this);
  this.onCurrentNodeClassChanged = this.onCurrentNodeClassChanged.bind(this);

  // Create the add class text field.
  this.addEl = this.doc.createElement("input");
  this.addEl.classList.add("devtools-textinput");
  this.addEl.classList.add("add-class");
  this.addEl.setAttribute("placeholder",
    L10N.getStr("inspector.classPanel.newClass.placeholder"));
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

    for (let { name, isApplied } of this.model.currentClasses) {
      let checkBox = this.renderCheckBox(name, isApplied);
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
    let box = this.doc.createElement("input");
    box.setAttribute("type", "checkbox");
    if (isApplied) {
      box.setAttribute("checked", "checked");
    }
    box.dataset.name = name;

    let labelWrapper = this.doc.createElement("label");
    labelWrapper.setAttribute("title", name);
    labelWrapper.appendChild(box);

    // A child element is required to do the ellipsis.
    let label = this.doc.createElement("span");
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
    let msg = this.doc.createElement("p");
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

    this.model.addClassName(this.addEl.value).then(() => {
      this.render();
      this.addEl.value = "";
    }).catch(e => {
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
  }
};

module.exports = ClassListPreviewer;
