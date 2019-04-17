/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const TextEditor = require("devtools/client/inspector/markup/views/text-editor");
const { truncateString } = require("devtools/shared/inspector/utils");
const { editableField, InplaceEditor } = require("devtools/client/shared/inplace-editor");
const { parseAttribute } = require("devtools/client/shared/node-attribute-parser");

loader.lazyRequireGetter(this, "flashElementOn", "devtools/client/inspector/markup/utils", true);
loader.lazyRequireGetter(this, "flashElementOff", "devtools/client/inspector/markup/utils", true);
loader.lazyRequireGetter(this, "getAutocompleteMaxWidth", "devtools/client/inspector/markup/utils", true);
loader.lazyRequireGetter(this, "parseAttributeValues", "devtools/client/inspector/markup/utils", true);

const {LocalizationHelper} = require("devtools/shared/l10n");
const INSPECTOR_L10N =
  new LocalizationHelper("devtools/client/locales/inspector.properties");

// Page size for pageup/pagedown
const COLLAPSE_DATA_URL_REGEX = /^data.+base64/;
const COLLAPSE_DATA_URL_LENGTH = 60;

// Contains only void (without end tag) HTML elements
const HTML_VOID_ELEMENTS = [
  "area", "base", "br", "col", "command", "embed",
  "hr", "img", "input", "keygen", "link", "meta", "param", "source",
  "track", "wbr",
];

// Contains only valid computed display property types of the node to display in the
// element markup and their respective title tooltip text.
const DISPLAY_TYPES = {
  "flex": INSPECTOR_L10N.getStr("markupView.display.flex.tooltiptext"),
  "inline-flex": INSPECTOR_L10N.getStr("markupView.display.flex.tooltiptext"),
  "grid": INSPECTOR_L10N.getStr("markupView.display.grid.tooltiptext"),
  "inline-grid": INSPECTOR_L10N.getStr("markupView.display.inlineGrid.tooltiptext"),
  "subgrid": INSPECTOR_L10N.getStr("markupView.display.subgrid.tooltiptiptext"),
  "flow-root": INSPECTOR_L10N.getStr("markupView.display.flowRoot.tooltiptext"),
  "contents": INSPECTOR_L10N.getStr("markupView.display.contents.tooltiptext2"),
};

/**
 * Creates an editor for an Element node.
 *
 * @param  {MarkupContainer} container
 *         The container owning this editor.
 * @param  {Element} node
 *         The node being edited.
 */
function ElementEditor(container, node) {
  this.container = container;
  this.node = node;
  this.markup = this.container.markup;
  this.doc = this.markup.doc;
  this.inspector = this.markup.inspector;
  this.highlighters = this.markup.highlighters;
  this._cssProperties = this.inspector.cssProperties;

  this.attrElements = new Map();
  this.animationTimers = {};

  this.elt = null;
  this.tag = null;
  this.closeTag = null;
  this.attrList = null;
  this.newAttr = null;
  this.closeElt = null;

  this.onCustomBadgeClick = this.onCustomBadgeClick.bind(this);
  this.onDisplayBadgeClick = this.onDisplayBadgeClick.bind(this);
  this.onExpandBadgeClick = this.onExpandBadgeClick.bind(this);
  this.onFlexboxHighlighterChange = this.onFlexboxHighlighterChange.bind(this);
  this.onGridHighlighterChange = this.onGridHighlighterChange.bind(this);
  this.onTagEdit = this.onTagEdit.bind(this);

  // Create the main editor
  this.buildMarkup();

  // Make the tag name editable (unless this is a remote node or
  // a document element)
  if (!node.isDocumentElement) {
    // Make the tag optionally tabbable but not by default.
    this.tag.setAttribute("tabindex", "-1");
    editableField({
      element: this.tag,
      multiline: true,
      maxWidth: () => getAutocompleteMaxWidth(this.tag, this.container.elt),
      trigger: "dblclick",
      stopOnReturn: true,
      done: this.onTagEdit,
      cssProperties: this._cssProperties,
    });
  }

  // Make the new attribute space editable.
  this.newAttr.editMode = editableField({
    element: this.newAttr,
    multiline: true,
    maxWidth: () => getAutocompleteMaxWidth(this.newAttr, this.container.elt),
    trigger: "dblclick",
    stopOnReturn: true,
    contentType: InplaceEditor.CONTENT_TYPES.CSS_MIXED,
    popup: this.markup.popup,
    done: (val, commit) => {
      if (!commit) {
        return;
      }

      const doMods = this._startModifyingAttributes();
      const undoMods = this._startModifyingAttributes();
      this._applyAttributes(val, null, doMods, undoMods);
      this.container.undo.do(() => {
        doMods.apply();
      }, function() {
        undoMods.apply();
      });
    },
    cssProperties: this._cssProperties,
  });

  const displayName = this.node.displayName;
  this.tag.textContent = displayName;
  this.closeTag.textContent = displayName;

  const isVoidElement = HTML_VOID_ELEMENTS.includes(displayName);
  if (node.isInHTMLDocument && isVoidElement) {
    this.elt.classList.add("void-element");
  }

  this.update();
  this.initialized = true;
}

ElementEditor.prototype = {
  buildMarkup: function() {
    this.elt = this.doc.createElement("span");
    this.elt.classList.add("editor");

    const open = this.doc.createElement("span");
    open.classList.add("open");
    open.appendChild(this.doc.createTextNode("<"));
    this.elt.appendChild(open);

    this.tag = this.doc.createElement("span");
    this.tag.classList.add("tag", "theme-fg-color3");
    this.tag.setAttribute("tabindex", "-1");
    open.appendChild(this.tag);

    this.attrList = this.doc.createElement("span");
    open.appendChild(this.attrList);

    this.newAttr = this.doc.createElement("span");
    this.newAttr.classList.add("newattr");
    this.newAttr.setAttribute("tabindex", "-1");
    this.newAttr.setAttribute("aria-label",
      INSPECTOR_L10N.getStr("markupView.newAttribute.label"));
    open.appendChild(this.newAttr);

    const closingBracket = this.doc.createElement("span");
    closingBracket.classList.add("closing-bracket");
    closingBracket.textContent = ">";
    open.appendChild(closingBracket);

    this.expandBadge = this.doc.createElement("span");
    this.expandBadge.classList.add("markup-expand-badge");
    this.expandBadge.addEventListener("click", this.onExpandBadgeClick);
    this.elt.appendChild(this.expandBadge);

    const close = this.doc.createElement("span");
    close.classList.add("close");
    close.appendChild(this.doc.createTextNode("</"));
    this.elt.appendChild(close);

    this.closeTag = this.doc.createElement("span");
    this.closeTag.classList.add("tag", "theme-fg-color3");
    close.appendChild(this.closeTag);

    close.appendChild(this.doc.createTextNode(">"));
  },

  set selected(value) {
    if (this.textEditor) {
      this.textEditor.selected = value;
    }
  },

  flashAttribute: function(attrName) {
    if (this.animationTimers[attrName]) {
      clearTimeout(this.animationTimers[attrName]);
    }

    flashElementOn(this.getAttributeElement(attrName));

    this.animationTimers[attrName] = setTimeout(() => {
      flashElementOff(this.getAttributeElement(attrName));
    }, this.markup.CONTAINER_FLASHING_DURATION);
  },

  /**
   * Returns information about node in the editor.
   *
   * @param  {DOMNode} node
   *         The node to get information from.
   * @return {Object} An object literal with the following information:
   *         {type: "attribute", name: "rel", value: "index", el: node}
   */
  getInfoAtNode: function(node) {
    if (!node) {
      return null;
    }

    let type = null;
    let name = null;
    let value = null;

    // Attribute
    const attribute = node.closest(".attreditor");
    if (attribute) {
      type = "attribute";
      name = attribute.dataset.attr;
      value = attribute.dataset.value;
    }

    return {type, name, value, el: node};
  },

  /**
   * Update the state of the editor from the node.
   */
  update: function() {
    const nodeAttributes = this.node.attributes || [];

    // Keep the data model in sync with attributes on the node.
    const currentAttributes = new Set(nodeAttributes.map(a => a.name));
    for (const name of this.attrElements.keys()) {
      if (!currentAttributes.has(name)) {
        this.removeAttribute(name);
      }
    }

    // Only loop through the current attributes on the node.  Missing
    // attributes have already been removed at this point.
    for (const attr of nodeAttributes) {
      const el = this.attrElements.get(attr.name);
      const valueChanged = el &&
        el.dataset.value !== attr.value;
      const isEditing = el && el.querySelector(".editable").inplaceEditor;
      const canSimplyShowEditor = el && (!valueChanged || isEditing);

      if (canSimplyShowEditor) {
        // Element already exists and doesn't need to be recreated.
        // Just show it (it's hidden by default).
        el.style.removeProperty("display");
      } else {
        // Create a new editor, because the value of an existing attribute
        // has changed.
        const attribute = this._createAttribute(attr, el);
        attribute.style.removeProperty("display");

        // Temporarily flash the attribute to highlight the change.
        // But not if this is the first time the editor instance has
        // been created.
        if (this.initialized) {
          this.flashAttribute(attr.name);
        }
      }
    }

    this.updateEventBadge();
    this.updateDisplayBadge();
    this.updateCustomBadge();
    this.updateScrollableBadge();
    this.updateTextEditor();
  },

  updateEventBadge: function() {
    const showEventBadge = this.node.hasEventListeners;
    if (this._eventBadge && !showEventBadge) {
      this._eventBadge.remove();
      this._eventBadge = null;
    } else if (showEventBadge && !this._eventBadge) {
      this._createEventBadge();
    }
  },

  _createEventBadge: function() {
    this._eventBadge = this.doc.createElement("div");
    this._eventBadge.className = "inspector-badge interactive";
    this._eventBadge.dataset.event = "true";
    this._eventBadge.textContent = "event";
    this._eventBadge.title = INSPECTOR_L10N.getStr("markupView.event.tooltiptext");
    // Badges order is [event][display][custom], insert event badge before others.
    this.elt.insertBefore(this._eventBadge, this._displayBadge || this._customBadge);
    this.markup.emit("badge-added-event");
  },

  updateScrollableBadge: function() {
    if (this.node.isScrollable && !this._scrollableBadge) {
      this._createScrollableBadge();
    } else if (this._scrollableBadge && !this.node.isScrollable) {
      this._scrollableBadge.remove();
      this._scrollableBadge = null;
    }
  },

  _createScrollableBadge: function() {
    this._scrollableBadge = this.doc.createElement("div");
    this._scrollableBadge.className = "inspector-badge scrollable-badge";
    this._scrollableBadge.textContent =
      INSPECTOR_L10N.getStr("markupView.scrollableBadge.label");
    this._scrollableBadge.title =
      INSPECTOR_L10N.getStr("markupView.scrollableBadge.tooltip");
    this.elt.insertBefore(this._scrollableBadge, this._customBadge);
  },

  /**
   * Update the markup display badge.
   */
  updateDisplayBadge: function() {
    const displayType = this.node.displayType;
    const showDisplayBadge = displayType in DISPLAY_TYPES;

    if (this._displayBadge && !showDisplayBadge) {
      this.stopTrackingFlexboxHighlighterEvents();
      this.stopTrackingGridHighlighterEvents();

      this._displayBadge.remove();
      this._displayBadge = null;
    } else if (showDisplayBadge) {
      if (!this._displayBadge) {
        this._createDisplayBadge();
      }

      this._updateDisplayBadgeContent();
    }
  },

  _createDisplayBadge: function() {
    this._displayBadge = this.doc.createElement("div");
    this._displayBadge.className = "inspector-badge";
    this._displayBadge.addEventListener("click", this.onDisplayBadgeClick);
    // Badges order is [event][display][custom], insert display badge before custom.
    this.elt.insertBefore(this._displayBadge, this._customBadge);

    this.startTrackingFlexboxHighlighterEvents();
    this.startTrackingGridHighlighterEvents();
  },

  _updateDisplayBadgeContent: function() {
    const displayType = this.node.displayType;
    this._displayBadge.textContent = displayType;
    this._displayBadge.dataset.display = displayType;
    this._displayBadge.title = DISPLAY_TYPES[displayType];
    this._displayBadge.classList.toggle("active",
      this.highlighters.flexboxHighlighterShown === this.node ||
      this.highlighters.gridHighlighters.has(this.node));

    if (displayType === "flex" || displayType === "inline-flex") {
      this._displayBadge.classList.toggle("interactive", true);
    } else if (displayType === "grid" ||
               displayType === "inline-grid" ||
               displayType === "subgrid") {
      this._displayBadge.classList.toggle("interactive",
        this.highlighters.canGridHighlighterToggle(this.node));
    } else {
      this._displayBadge.classList.remove("interactive");
    }
  },

  /**
   * Update the markup custom element badge.
   */
  updateCustomBadge: function() {
    const showCustomBadge = !!this.node.customElementLocation;
    if (this._customBadge && !showCustomBadge) {
      this._customBadge.remove();
      this._customBadge = null;
    } else if (!this._customBadge && showCustomBadge) {
      this._createCustomBadge();
    }
  },

  _createCustomBadge: function() {
    this._customBadge = this.doc.createElement("div");
    this._customBadge.className = "inspector-badge interactive";
    this._customBadge.dataset.custom = "true";
    this._customBadge.textContent = "custom…";
    this._customBadge.title = INSPECTOR_L10N.getStr("markupView.custom.tooltiptext");
    this._customBadge.addEventListener("click", this.onCustomBadgeClick);
    // Badges order is [event][display][custom], insert custom badge at the end.
    this.elt.appendChild(this._customBadge);
  },

  /**
   * Update the inline text editor in case of a single text child node.
   */
  updateTextEditor: function() {
    const node = this.node.inlineTextChild;

    if (this.textEditor && this.textEditor.node != node) {
      this.elt.removeChild(this.textEditor.elt);
      this.textEditor = null;
    }

    if (node && !this.textEditor) {
      // Create a text editor added to this editor.
      // This editor won't receive an update automatically, so we rely on
      // child text editors to let us know that we need updating.
      this.textEditor = new TextEditor(this.container, node, "text");
      this.elt.insertBefore(this.textEditor.elt, this.elt.querySelector(".close"));
    }

    if (this.textEditor) {
      this.textEditor.update();
    }
  },

  _startModifyingAttributes: function() {
    return this.node.startModifyingAttributes();
  },

  /**
   * Get the element used for one of the attributes of this element.
   *
   * @param  {String} attrName
   *         The name of the attribute to get the element for
   * @return {DOMNode}
   */
  getAttributeElement: function(attrName) {
    return this.attrList.querySelector(
      ".attreditor[data-attr=" + CSS.escape(attrName) + "] .attr-value");
  },

  /**
   * Remove an attribute from the attrElements object and the DOM.
   *
   * @param  {String} attrName
   *         The name of the attribute to remove
   */
  removeAttribute: function(attrName) {
    const attr = this.attrElements.get(attrName);
    if (attr) {
      this.attrElements.delete(attrName);
      attr.remove();
    }
  },

  _createAttribute: function(attribute, before = null) {
    const attr = this.doc.createElement("span");
    attr.dataset.attr = attribute.name;
    attr.dataset.value = attribute.value;
    attr.classList.add("attreditor");
    attr.style.display = "none";

    attr.appendChild(this.doc.createTextNode(" "));

    const inner = this.doc.createElement("span");
    inner.classList.add("editable");
    inner.setAttribute("tabindex", this.container.canFocus ? "0" : "-1");
    attr.appendChild(inner);

    const name = this.doc.createElement("span");
    name.classList.add("attr-name");
    name.classList.add("theme-fg-color1");
    inner.appendChild(name);

    inner.appendChild(this.doc.createTextNode('="'));

    const val = this.doc.createElement("span");
    val.classList.add("attr-value");
    val.classList.add("theme-fg-color2");
    inner.appendChild(val);

    inner.appendChild(this.doc.createTextNode('"'));

    // Double quotes need to be handled specially to prevent DOMParser failing.
    // name="v"a"l"u"e" when editing -> name='v"a"l"u"e"'
    // name="v'a"l'u"e" when editing -> name="v'a&quot;l'u&quot;e"
    let editValueDisplayed = attribute.value || "";
    const hasDoubleQuote = editValueDisplayed.includes('"');
    const hasSingleQuote = editValueDisplayed.includes("'");
    let initial = attribute.name + '="' + editValueDisplayed + '"';

    // Can't just wrap value with ' since the value contains both " and '.
    if (hasDoubleQuote && hasSingleQuote) {
      editValueDisplayed = editValueDisplayed.replace(/\"/g, "&quot;");
      initial = attribute.name + '="' + editValueDisplayed + '"';
    }

    // Wrap with ' since there are no single quotes in the attribute value.
    if (hasDoubleQuote && !hasSingleQuote) {
      initial = attribute.name + "='" + editValueDisplayed + "'";
    }

    // Make the attribute editable.
    attr.editMode = editableField({
      element: inner,
      trigger: "dblclick",
      stopOnReturn: true,
      selectAll: false,
      initial: initial,
      multiline: true,
      maxWidth: () => getAutocompleteMaxWidth(inner, this.container.elt),
      contentType: InplaceEditor.CONTENT_TYPES.CSS_MIXED,
      popup: this.markup.popup,
      start: (editor, event) => {
        // If the editing was started inside the name or value areas,
        // select accordingly.
        if (event && event.target === name) {
          editor.input.setSelectionRange(0, name.textContent.length);
        } else if (event && event.target.closest(".attr-value") === val) {
          const length = editValueDisplayed.length;
          const editorLength = editor.input.value.length;
          const start = editorLength - (length + 1);
          editor.input.setSelectionRange(start, start + length);
        } else {
          editor.input.select();
        }
      },
      done: (newValue, commit, direction) => {
        if (!commit || newValue === initial) {
          return;
        }

        const doMods = this._startModifyingAttributes();
        const undoMods = this._startModifyingAttributes();

        // Remove the attribute stored in this editor and re-add any attributes
        // parsed out of the input element. Restore original attribute if
        // parsing fails.
        this.refocusOnEdit(attribute.name, attr, direction);
        this._saveAttribute(attribute.name, undoMods);
        doMods.removeAttribute(attribute.name);
        this._applyAttributes(newValue, attr, doMods, undoMods);
        this.container.undo.do(() => {
          doMods.apply();
        }, () => {
          undoMods.apply();
        });
      },
      cssProperties: this._cssProperties,
    });

    // Figure out where we should place the attribute.
    if (attribute.name == "id") {
      before = this.attrList.firstChild;
    } else if (attribute.name == "class") {
      const idNode = this.attrElements.get("id");
      before = idNode ? idNode.nextSibling : this.attrList.firstChild;
    }
    this.attrList.insertBefore(attr, before);

    this.removeAttribute(attribute.name);
    this.attrElements.set(attribute.name, attr);

    // Parse the attribute value to detect whether there are linkable parts in
    // it (make sure to pass a complete list of existing attributes to the
    // parseAttribute function, by concatenating attribute, because this could
    // be a newly added attribute not yet on this.node).
    const attributes = this.node.attributes.filter(existingAttribute => {
      return existingAttribute.name !== attribute.name;
    });
    attributes.push(attribute);
    const parsedLinksData = parseAttribute(this.node.namespaceURI,
      this.node.tagName, attributes, attribute.name);

    // Create links in the attribute value, and collapse long attributes if
    // needed.
    const collapse = value => {
      if (value && value.match(COLLAPSE_DATA_URL_REGEX)) {
        return truncateString(value, COLLAPSE_DATA_URL_LENGTH);
      }
      return this.markup.collapseAttributes
        ? truncateString(value, this.markup.collapseAttributeLength)
        : value;
    };

    val.innerHTML = "";
    for (const token of parsedLinksData) {
      if (token.type === "string") {
        val.appendChild(this.doc.createTextNode(collapse(token.value)));
      } else {
        const link = this.doc.createElement("span");
        link.classList.add("link");
        link.setAttribute("data-type", token.type);
        link.setAttribute("data-link", token.value);
        link.textContent = collapse(token.value);
        val.appendChild(link);
      }
    }

    name.textContent = attribute.name;

    return attr;
  },

  /**
   * Parse a user-entered attribute string and apply the resulting
   * attributes to the node. This operation is undoable.
   *
   * @param  {String} value
   *         The user-entered value.
   * @param  {DOMNode} attrNode
   *         The attribute editor that created this
   *         set of attributes, used to place new attributes where the
   *         user put them.
   */
  _applyAttributes: function(value, attrNode, doMods, undoMods) {
    const attrs = parseAttributeValues(value, this.doc);
    for (const attr of attrs) {
      // Create an attribute editor next to the current attribute if needed.
      this._createAttribute(attr, attrNode ? attrNode.nextSibling : null);
      this._saveAttribute(attr.name, undoMods);
      doMods.setAttribute(attr.name, attr.value);
    }
  },

  /**
   * Saves the current state of the given attribute into an attribute
   * modification list.
   */
  _saveAttribute: function(name, undoMods) {
    const node = this.node;
    if (node.hasAttribute(name)) {
      const oldValue = node.getAttribute(name);
      undoMods.setAttribute(name, oldValue);
    } else {
      undoMods.removeAttribute(name);
    }
  },

  /**
   * Listen to mutations, and when the attribute list is regenerated
   * try to focus on the attribute after the one that's being edited now.
   * If the attribute order changes, go to the beginning of the attribute list.
   */
  refocusOnEdit: function(attrName, attrNode, direction) {
    // Only allow one refocus on attribute change at a time, so when there's
    // more than 1 request in parallel, the last one wins.
    if (this._editedAttributeObserver) {
      this.markup.inspector.off("markupmutation", this._editedAttributeObserver);
      this._editedAttributeObserver = null;
    }

    const activeElement = this.markup.doc.activeElement;
    if (!activeElement || !activeElement.inplaceEditor) {
      // The focus was already removed from the current inplace editor, we should not
      // refocus the editable attribute.
      return;
    }

    const container = this.markup.getContainer(this.node);

    const activeAttrs = [...this.attrList.childNodes]
      .filter(el => el.style.display != "none");
    const attributeIndex = activeAttrs.indexOf(attrNode);

    const onMutations = this._editedAttributeObserver = mutations => {
      let isDeletedAttribute = false;
      let isNewAttribute = false;

      for (const mutation of mutations) {
        const inContainer =
          this.markup.getContainer(mutation.target) === container;
        if (!inContainer) {
          continue;
        }

        const isOriginalAttribute = mutation.attributeName === attrName;

        isDeletedAttribute = isDeletedAttribute || isOriginalAttribute &&
                             mutation.newValue === null;
        isNewAttribute = isNewAttribute || mutation.attributeName !== attrName;
      }

      const isModifiedOrder = isDeletedAttribute && isNewAttribute;
      this._editedAttributeObserver = null;

      // "Deleted" attributes are merely hidden, so filter them out.
      const visibleAttrs = [...this.attrList.childNodes]
        .filter(el => el.style.display != "none");
      let activeEditor;
      if (visibleAttrs.length > 0) {
        if (!direction) {
          // No direction was given; stay on current attribute.
          activeEditor = visibleAttrs[attributeIndex];
        } else if (isModifiedOrder) {
          // The attribute was renamed, reordering the existing attributes.
          // So let's go to the beginning of the attribute list for consistency.
          activeEditor = visibleAttrs[0];
        } else {
          let newAttributeIndex;
          if (isDeletedAttribute) {
            newAttributeIndex = attributeIndex;
          } else if (direction == Services.focus.MOVEFOCUS_FORWARD) {
            newAttributeIndex = attributeIndex + 1;
          } else if (direction == Services.focus.MOVEFOCUS_BACKWARD) {
            newAttributeIndex = attributeIndex - 1;
          }

          // The number of attributes changed (deleted), or we moved through
          // the array so check we're still within bounds.
          if (newAttributeIndex >= 0 &&
              newAttributeIndex <= visibleAttrs.length - 1) {
            activeEditor = visibleAttrs[newAttributeIndex];
          }
        }
      }

      // Either we have no attributes left,
      // or we just edited the last attribute and want to move on.
      if (!activeEditor) {
        activeEditor = this.newAttr;
      }

      // Refocus was triggered by tab or shift-tab.
      // Continue in edit mode.
      if (direction) {
        activeEditor.editMode();
      } else {
        // Refocus was triggered by enter.
        // Exit edit mode (but restore focus).
        const editable = activeEditor === this.newAttr ?
          activeEditor : activeEditor.querySelector(".editable");
        editable.focus();
      }

      this.markup.emit("refocusedonedit");
    };

    // Start listening for mutations until we find an attributes change
    // that modifies this attribute.
    this.markup.inspector.once("markupmutation", onMutations);
  },

  startTrackingFlexboxHighlighterEvents() {
    this.highlighters.on("flexbox-highlighter-hidden", this.onFlexboxHighlighterChange);
    this.highlighters.on("flexbox-highlighter-shown", this.onFlexboxHighlighterChange);
  },

  startTrackingGridHighlighterEvents() {
    this.highlighters.on("grid-highlighter-hidden", this.onGridHighlighterChange);
    this.highlighters.on("grid-highlighter-shown", this.onGridHighlighterChange);
  },

  stopTrackingFlexboxHighlighterEvents() {
    this.highlighters.off("flexbox-highlighter-hidden", this.onFlexboxHighlighterChange);
    this.highlighters.off("flexbox-highlighter-shown", this.onFlexboxHighlighterChange);
  },

  stopTrackingGridHighlighterEvents() {
    this.highlighters.off("grid-highlighter-hidden", this.onGridHighlighterChange);
    this.highlighters.off("grid-highlighter-shown", this.onGridHighlighterChange);
  },

  /**
   * Called when the display badge is clicked. Toggles on the flex/grid highlighter for
   * the selected node if it is a grid container.
   */
  onDisplayBadgeClick: async function(event) {
    event.stopPropagation();

    const target = event.target;

    if (target.dataset.display === "flex" || target.dataset.display === "inline-flex") {
      // Stop tracking highlighter events to avoid flickering of the active class.
      this.stopTrackingFlexboxHighlighterEvents();

      this._displayBadge.classList.toggle("active");
      await this.highlighters.toggleFlexboxHighlighter(this.node, "markup");

      this.startTrackingFlexboxHighlighterEvents();
    }

    if (target.dataset.display === "grid" || target.dataset.display === "inline-grid" ||
        target.dataset.display === "subgrid") {
      // Don't toggle the grid highlighter if the max number of new grid highlighters
      // allowed has been reached.
      if (!this.highlighters.canGridHighlighterToggle(this.node)) {
        return;
      }

      // Stop tracking highlighter events to avoid flickering of the active class.
      this.stopTrackingGridHighlighterEvents();

      this._displayBadge.classList.toggle("active");
      await this.highlighters.toggleGridHighlighter(this.node, "markup");

      this.startTrackingGridHighlighterEvents();
    }
  },

  onCustomBadgeClick: function() {
    const { url, line, column } = this.node.customElementLocation;
    this.markup.toolbox.viewSourceInDebugger(
      url,
      line,
      column,
      null,
      "show_custom_element"
    );
  },

  onExpandBadgeClick: function() {
    this.container.expandContainer();
  },

  /**
   * Handler for "flexbox-highlighter-hidden" and "flexbox-highlighter-shown" event
   * emitted from the HighlightersOverlay. Toggles the active state of the display badge
   * if it matches the highlighted flex container node.
   */
  onFlexboxHighlighterChange: function() {
    if (!this._displayBadge) {
      return;
    }

    this._displayBadge.classList.toggle("active",
      this.highlighters.flexboxHighlighterShown === this.node);
  },

  /**
   * Handler for "grid-highlighter-hidden" and "grid-highlighter-shown" event emitted from
   * the HighlightersOverlay. Toggles the active state of the display badge if it matches
   * the highlighted grid node.
   */
  onGridHighlighterChange: function() {
    if (!this._displayBadge) {
      return;
    }

    this._displayBadge.classList.toggle("active",
      this.highlighters.gridHighlighters.has(this.node));

    this._updateDisplayBadgeContent();
  },

  /**
   * Called when the tag name editor has is done editing.
   */
  onTagEdit: function(newTagName, isCommit) {
    if (!isCommit ||
        newTagName.toLowerCase() === this.node.tagName.toLowerCase() ||
        !("editTagName" in this.markup.walker)) {
      return;
    }

    // Changing the tagName removes the node. Make sure the replacing node gets
    // selected afterwards.
    this.markup.reselectOnRemoved(this.node, "edittagname");
    this.markup.walker.editTagName(this.node, newTagName).catch(() => {
      // Failed to edit the tag name, cancel the reselection.
      this.markup.cancelReselectOnRemoved();
    });
  },

  destroy: function() {
    if (this._displayBadge) {
      this._displayBadge.removeEventListener("click", this.onDisplayBadgeClick);
      this.stopTrackingFlexboxHighlighterEvents();
      this.stopTrackingGridHighlighterEvents();
    }

    if (this._customBadge) {
      this._customBadge.removeEventListener("click", this.onCustomBadgeClick);
    }

    this.expandBadge.removeEventListener("click", this.onExpandBadgeClick);

    for (const key in this.animationTimers) {
      clearTimeout(this.animationTimers[key]);
    }
    this.animationTimers = null;
  },
};

module.exports = ElementEditor;
