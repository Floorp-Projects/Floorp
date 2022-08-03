/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

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
class ClassList {
  constructor(inspector) {
    EventEmitter.decorate(this);

    this.inspector = inspector;

    this.onMutations = this.onMutations.bind(this);
    this.inspector.on("markupmutation", this.onMutations);

    this.classListProxyNode = this.inspector.panelDoc.createElement("div");
    this.previewClasses = [];
    this.unresolvedStateChanges = [];
  }

  destroy() {
    this.inspector.off("markupmutation", this.onMutations);
    this.inspector = null;
    this.classListProxyNode = null;
  }

  /**
   * The current node selection (which only returns if the node is an ELEMENT_NODE type
   * since that's the only type this model can work with.)
   */
  get currentNode() {
    if (
      this.inspector.selection.isElementNode() &&
      !this.inspector.selection.isPseudoElementNode()
    ) {
      return this.inspector.selection.nodeFront;
    }
    return null;
  }

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
      const nodeClasses = [...new Set([...this.classListProxyNode.classList])]
        .filter(
          className =>
            !this.previewClasses.some(
              previewClass =>
                previewClass.className === className &&
                !previewClass.wasAppliedOnNode
            )
        )
        .map(name => {
          return { name, isApplied: true };
        });

      CLASSES.set(this.currentNode, nodeClasses);
    }

    return CLASSES.get(this.currentNode);
  }

  /**
   * Same as currentClasses, but returns it in the form of a className string, where only
   * enabled classes are added.
   */
  get currentClassesPreview() {
    const currentClasses = this.currentClasses
      .filter(({ isApplied }) => isApplied)
      .map(({ name }) => name);
    const previewClasses = this.previewClasses
      .filter(previewClass => !currentClasses.includes(previewClass.className))
      .filter(item => item !== "")
      .map(({ className }) => className);

    return currentClasses
      .concat(previewClasses)
      .join(" ")
      .trim();
  }

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
    const nodeClasses = this.currentClasses;
    nodeClasses.find(({ name: cName }) => cName === name).isApplied = isApplied;

    return this.applyClassState();
  }

  /**
   * Add several classes to the current node at once.
   *
   * @param {String} classNameString
   *        The string that contains all classes.
   * @return {Promise} Resolves when the change has been made in the DOM.
   */
  addClassName(classNameString) {
    this.classListProxyNode.className = classNameString;
    this.eraseClassPreview();
    return Promise.all(
      [...new Set([...this.classListProxyNode.classList])].map(name => {
        return this.addClass(name);
      })
    );
  }

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
  }

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

    // Remember which node & className we applied until their mutation event is received, so we
    // can filter out dom mutations that are caused by us in onMutations, even in situations when
    // a new change is applied before that the event of the previous one has been received yet
    this.unresolvedStateChanges.push({
      node: this.currentNode,
      className: this.currentClassesPreview,
    });

    // Apply the change to the node.
    const mod = this.currentNode.startModifyingAttributes();
    mod.setAttribute("class", this.currentClassesPreview);
    return mod.apply();
  }

  onMutations(mutations) {
    for (const { type, target, attributeName } of mutations) {
      // Only care if this mutation is for the class attribute.
      if (type !== "attributes" || attributeName !== "class") {
        continue;
      }

      const isMutationForOurChange = this.unresolvedStateChanges.some(
        previousStateChange =>
          previousStateChange.node === target &&
          previousStateChange.className === target.className
      );

      if (!isMutationForOurChange) {
        CLASSES.delete(target);
        if (target === this.currentNode) {
          this.emit("current-node-class-changed");
        }
      } else {
        this.removeResolvedStateChanged(target, target.className);
      }
    }
  }

  /**
   * Get the available classNames in the document where the current selected node lives:
   * - the one already used on elements of the document
   * - the one defined in Stylesheets of the document
   *
   * @param {String} filter: A string the classNames should start with (an insensitive
   *                         case matching will be done).
   * @returns {Promise<Array<String>>} A promise that resolves with an array of strings
   *                                   matching the passed filter.
   */
  getClassNames(filter) {
    return this.currentNode.inspectorFront.pageStyle.getAttributesInOwnerDocument(
      filter,
      "class",
      this.currentNode
    );
  }

  previewClass(inputClasses) {
    if (
      this.previewClasses
        .map(previewClass => previewClass.className)
        .join(" ") !== inputClasses
    ) {
      this.previewClasses = [];
      inputClasses.split(" ").forEach(className => {
        this.previewClasses.push({
          className,
          wasAppliedOnNode: this.isClassAlreadyApplied(className),
        });
      });
      this.applyClassState();
    }
  }

  eraseClassPreview() {
    this.previewClass("");
  }

  removeResolvedStateChanged(currentNode, currentClassesPreview) {
    this.unresolvedStateChanges.splice(
      0,
      this.unresolvedStateChanges.findIndex(
        previousState =>
          previousState.node === currentNode &&
          previousState.className === currentClassesPreview
      ) + 1
    );
  }

  isClassAlreadyApplied(className) {
    return this.currentClasses.some(({ name }) => name === className);
  }
}

module.exports = ClassList;
