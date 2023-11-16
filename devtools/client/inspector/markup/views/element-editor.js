/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TextEditor = require("resource://devtools/client/inspector/markup/views/text-editor.js");
const {
  truncateString,
} = require("resource://devtools/shared/inspector/utils.js");
const {
  editableField,
  InplaceEditor,
} = require("resource://devtools/client/shared/inplace-editor.js");
const {
  parseAttribute,
  ATTRIBUTE_TYPES,
} = require("resource://devtools/client/shared/node-attribute-parser.js");

loader.lazyRequireGetter(
  this,
  [
    "flashElementOn",
    "flashElementOff",
    "getAutocompleteMaxWidth",
    "parseAttributeValues",
  ],
  "resource://devtools/client/inspector/markup/utils.js",
  true
);

const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
const INSPECTOR_L10N = new LocalizationHelper(
  "devtools/client/locales/inspector.properties"
);

// Page size for pageup/pagedown
const COLLAPSE_DATA_URL_REGEX = /^data.+base64/;
const COLLAPSE_DATA_URL_LENGTH = 60;

// Contains only void (without end tag) HTML elements
const HTML_VOID_ELEMENTS = [
  "area",
  "base",
  "br",
  "col",
  "command",
  "embed",
  "hr",
  "img",
  "input",
  "keygen",
  "link",
  "meta",
  "param",
  "source",
  "track",
  "wbr",
];

// Contains only valid computed display property types of the node to display in the
// element markup and their respective title tooltip text.
const DISPLAY_TYPES = {
  flex: INSPECTOR_L10N.getStr("markupView.display.flex.tooltiptext2"),
  "inline-flex": INSPECTOR_L10N.getStr(
    "markupView.display.inlineFlex.tooltiptext2"
  ),
  grid: INSPECTOR_L10N.getStr("markupView.display.grid.tooltiptext2"),
  "inline-grid": INSPECTOR_L10N.getStr(
    "markupView.display.inlineGrid.tooltiptext2"
  ),
  subgrid: INSPECTOR_L10N.getStr("markupView.display.subgrid.tooltiptiptext"),
  "flow-root": INSPECTOR_L10N.getStr("markupView.display.flowRoot.tooltiptext"),
  contents: INSPECTOR_L10N.getStr("markupView.display.contents.tooltiptext2"),
};

/**
 * Creates an editor for an Element node.
 *
 * @param  {MarkupContainer} container
 *         The container owning this editor.
 * @param  {NodeFront} node
 *         The NodeFront being edited.
 */
function ElementEditor(container, node) {
  this.container = container;
  this.node = node;
  this.markup = this.container.markup;
  this.doc = this.markup.doc;
  this.inspector = this.markup.inspector;
  this.highlighters = this.markup.highlighters;
  this._cssProperties = this.inspector.cssProperties;

  this.isOverflowDebuggingEnabled = Services.prefs.getBoolPref(
    "devtools.overflow.debugging.enabled"
  );

  // If this is a scrollable element, this specifies whether or not its overflow causing
  // elements are highlighted. Otherwise, it is null if the element is not scrollable.
  this.highlightingOverflowCausingElements = this.node.isScrollable
    ? false
    : null;

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
  this.onScrollableBadgeClick = this.onScrollableBadgeClick.bind(this);
  this.onExpandBadgeClick = this.onExpandBadgeClick.bind(this);
  this.onTagEdit = this.onTagEdit.bind(this);

  this.buildMarkup();

  const isVoidElement = HTML_VOID_ELEMENTS.includes(this.node.displayName);
  if (node.isInHTMLDocument && isVoidElement) {
    this.elt.classList.add("void-element");
  }

  this.update();
  this.initialized = true;
}

ElementEditor.prototype = {
  buildMarkup() {
    this.elt = this.doc.createElement("span");
    this.elt.classList.add("editor");

    this.renderOpenTag();
    this.renderEventBadge();
    this.renderCloseTag();

    // Make the tag name editable (unless this is a remote node or
    // a document element)
    if (!this.node.isDocumentElement) {
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
  },

  renderOpenTag() {
    const open = this.doc.createElement("span");
    open.classList.add("open");
    open.appendChild(this.doc.createTextNode("<"));
    this.elt.appendChild(open);

    this.tag = this.doc.createElement("span");
    this.tag.classList.add("tag", "theme-fg-color3");
    this.tag.setAttribute("tabindex", "-1");
    this.tag.textContent = this.node.displayName;
    open.appendChild(this.tag);

    this.renderAttributes(open);
    this.renderNewAttributeEditor(open);

    const closingBracket = this.doc.createElement("span");
    closingBracket.classList.add("closing-bracket");
    closingBracket.textContent = ">";
    open.appendChild(closingBracket);
  },

  renderAttributes(containerEl) {
    this.attrList = this.doc.createElement("span");
    containerEl.appendChild(this.attrList);
  },

  renderNewAttributeEditor(containerEl) {
    this.newAttr = this.doc.createElement("span");
    this.newAttr.classList.add("newattr");
    this.newAttr.setAttribute("tabindex", "-1");
    this.newAttr.setAttribute(
      "aria-label",
      INSPECTOR_L10N.getStr("markupView.newAttribute.label")
    );
    containerEl.appendChild(this.newAttr);

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
        this.container.undo.do(
          () => {
            doMods.apply();
          },
          function () {
            undoMods.apply();
          }
        );
      },
      cssProperties: this._cssProperties,
    });
  },

  renderEventBadge() {
    this.expandBadge = this.doc.createElement("span");
    this.expandBadge.classList.add("markup-expand-badge");
    this.expandBadge.addEventListener("click", this.onExpandBadgeClick);
    this.elt.appendChild(this.expandBadge);
  },

  renderCloseTag() {
    const close = this.doc.createElement("span");
    close.classList.add("close");
    close.appendChild(this.doc.createTextNode("</"));
    this.elt.appendChild(close);

    this.closeTag = this.doc.createElement("span");
    this.closeTag.classList.add("tag", "theme-fg-color3");
    this.closeTag.textContent = this.node.displayName;
    close.appendChild(this.closeTag);

    close.appendChild(this.doc.createTextNode(">"));
  },

  get displayBadge() {
    return this._displayBadge;
  },

  set selected(value) {
    if (this.textEditor) {
      this.textEditor.selected = value;
    }
  },

  flashAttribute(attrName) {
    if (this.animationTimers[attrName]) {
      clearTimeout(this.animationTimers[attrName]);
    }

    flashElementOn(this.getAttributeElement(attrName), {
      backgroundClass: "theme-bg-contrast",
    });

    this.animationTimers[attrName] = setTimeout(() => {
      flashElementOff(this.getAttributeElement(attrName), {
        backgroundClass: "theme-bg-contrast",
      });
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
  getInfoAtNode(node) {
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

    return { type, name, value, el: node };
  },

  /**
   * Update the state of the editor from the node.
   */
  update() {
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
      const valueChanged = el && el.dataset.value !== attr.value;
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
    this.updateContainerBadge();
    this.updateTextEditor();
    this.updateUnavailableChildren();
    this.updateOverflowBadge();
    this.updateOverflowHighlight();
  },

  updateEventBadge() {
    const showEventBadge = this.node.hasEventListeners;
    if (this._eventBadge && !showEventBadge) {
      this._eventBadge.remove();
      this._eventBadge = null;
    } else if (showEventBadge && !this._eventBadge) {
      this._createEventBadge();
    }
  },

  _createEventBadge() {
    this._eventBadge = this.doc.createElement("button");
    this._eventBadge.className = "inspector-badge interactive";
    this._eventBadge.dataset.event = "true";
    this._eventBadge.textContent = "event";
    this._eventBadge.title = INSPECTOR_L10N.getStr(
      "markupView.event.tooltiptext2"
    );
    this._eventBadge.setAttribute("aria-pressed", "false");
    // Badges order is [event][display][custom], insert event badge before others.
    this.elt.insertBefore(
      this._eventBadge,
      this._displayBadge || this._customBadge
    );
    this.markup.emit("badge-added-event");
  },

  updateScrollableBadge() {
    if (this.node.isScrollable && !this._scrollableBadge) {
      this._createScrollableBadge();
    } else if (this._scrollableBadge && !this.node.isScrollable) {
      this._scrollableBadge.remove();
      this._scrollableBadge = null;
    }
  },

  _createScrollableBadge() {
    const isInteractive =
      this.isOverflowDebuggingEnabled &&
      // Document elements cannot have interative scrollable badges since retrieval of their
      // overflow causing elements is not supported.
      !this.node.isDocumentElement;

    this._scrollableBadge = this.doc.createElement(
      isInteractive ? "button" : "div"
    );
    this._scrollableBadge.className = `inspector-badge scrollable-badge ${
      isInteractive ? "interactive" : ""
    }`;
    this._scrollableBadge.dataset.scrollable = "true";
    this._scrollableBadge.textContent = INSPECTOR_L10N.getStr(
      "markupView.scrollableBadge.label"
    );
    this._scrollableBadge.title = INSPECTOR_L10N.getStr(
      isInteractive
        ? "markupView.scrollableBadge.interactive.tooltip"
        : "markupView.scrollableBadge.tooltip"
    );

    if (isInteractive) {
      this._scrollableBadge.addEventListener(
        "click",
        this.onScrollableBadgeClick
      );
      this._scrollableBadge.setAttribute("aria-pressed", "false");
    }
    this.elt.insertBefore(this._scrollableBadge, this._customBadge);
  },

  /**
   * Update the markup display badge.
   */
  updateDisplayBadge() {
    const displayType = this.node.displayType;
    const showDisplayBadge = displayType in DISPLAY_TYPES;

    if (this._displayBadge && !showDisplayBadge) {
      this._displayBadge.remove();
      this._displayBadge = null;
    } else if (showDisplayBadge) {
      if (!this._displayBadge) {
        this._createDisplayBadge();
      }

      this._updateDisplayBadgeContent();
    }
  },

  _createDisplayBadge() {
    this._displayBadge = this.doc.createElement("button");
    this._displayBadge.className = "inspector-badge";
    this._displayBadge.addEventListener("click", this.onDisplayBadgeClick);
    // Badges order is [event][display][custom], insert display badge before custom.
    this.elt.insertBefore(this._displayBadge, this._customBadge);
  },

  _updateDisplayBadgeContent() {
    const displayType = this.node.displayType;
    this._displayBadge.textContent = displayType;
    this._displayBadge.dataset.display = displayType;
    this._displayBadge.title = DISPLAY_TYPES[displayType];

    const isFlex = displayType === "flex" || displayType === "inline-flex";
    const isGrid =
      displayType === "grid" ||
      displayType === "inline-grid" ||
      displayType === "subgrid";

    const isInteractive =
      isFlex ||
      (isGrid && this.highlighters.canGridHighlighterToggle(this.node));

    this._displayBadge.classList.toggle("interactive", isInteractive);

    // Since the badge is a <button>, if it's not interactive we need to indicate
    // to screen readers that it shouldn't behave like a button.
    // It's easier to have the badge being a button and "downgrading" it like this,
    // than having it as a div and adding interactivity.
    if (isInteractive) {
      this._displayBadge.removeAttribute("role");
      this._displayBadge.setAttribute("aria-pressed", "false");
    } else {
      this._displayBadge.setAttribute("role", "presentation");
      this._displayBadge.removeAttribute("aria-pressed");
    }
  },

  updateOverflowBadge() {
    if (!this.isOverflowDebuggingEnabled) {
      return;
    }

    if (this.node.causesOverflow && !this._overflowBadge) {
      this._createOverflowBadge();
    } else if (!this.node.causesOverflow && this._overflowBadge) {
      this._overflowBadge.remove();
      this._overflowBadge = null;
    }
  },

  _createOverflowBadge() {
    this._overflowBadge = this.doc.createElement("div");
    this._overflowBadge.className = "inspector-badge overflow-badge";
    this._overflowBadge.textContent = INSPECTOR_L10N.getStr(
      "markupView.overflowBadge.label"
    );
    this._overflowBadge.title = INSPECTOR_L10N.getStr(
      "markupView.overflowBadge.tooltip"
    );
    this.elt.insertBefore(this._overflowBadge, this._customBadge);
  },

  /**
   * Update the markup custom element badge.
   */
  updateCustomBadge() {
    const showCustomBadge = !!this.node.customElementLocation;
    if (this._customBadge && !showCustomBadge) {
      this._customBadge.remove();
      this._customBadge = null;
    } else if (!this._customBadge && showCustomBadge) {
      this._createCustomBadge();
    }
  },

  _createCustomBadge() {
    this._customBadge = this.doc.createElement("button");
    this._customBadge.className = "inspector-badge interactive";
    this._customBadge.dataset.custom = "true";
    this._customBadge.textContent = "custom…";
    this._customBadge.title = INSPECTOR_L10N.getStr(
      "markupView.custom.tooltiptext"
    );
    this._customBadge.addEventListener("click", this.onCustomBadgeClick);
    // Badges order is [event][display][custom], insert custom badge at the end.
    this.elt.appendChild(this._customBadge);
  },

  updateContainerBadge() {
    const showContainerBadge =
      this.node.containerType === "inline-size" ||
      this.node.containerType === "size";

    if (this._containerBadge && !showContainerBadge) {
      this._containerBadge.remove();
      this._containerBadge = null;
    } else if (showContainerBadge && !this._containerBadge) {
      this._createContainerBadge();
    }
  },

  _createContainerBadge() {
    this._containerBadge = this.doc.createElement("div");
    this._containerBadge.classList.add("inspector-badge");
    this._containerBadge.dataset.container = "true";
    this._containerBadge.title = `container-type: ${this.node.containerType}`;

    this._containerBadge.append(this.doc.createTextNode("container"));
    // TODO: Move the logic to handle badges position in a dedicated helper (See Bug 1837921).
    // Ideally badges order should be [event][display][container][custom]
    this.elt.insertBefore(this._containerBadge, this._customBadge);
    this.markup.emit("badge-added-event");
  },

  /**
   * If node causes overflow, toggle its overflow highlight if its scrollable ancestor's
   * scrollable badge is active/inactive.
   */
  async updateOverflowHighlight() {
    if (!this.isOverflowDebuggingEnabled) {
      return;
    }

    let showOverflowHighlight = false;

    if (this.node.causesOverflow) {
      try {
        const scrollableAncestor =
          await this.node.walkerFront.getScrollableAncestorNode(this.node);
        const markupContainer = scrollableAncestor
          ? this.markup.getContainer(scrollableAncestor)
          : null;

        showOverflowHighlight =
          !!markupContainer?.editor.highlightingOverflowCausingElements;
      } catch (e) {
        // This call might fail if called asynchrously after the toolbox is finished
        // closing.
        return;
      }
    }

    this.setOverflowHighlight(showOverflowHighlight);
  },

  /**
   * Show overflow highlight if showOverflowHighlight is true, otherwise hide it.
   *
   * @param {Boolean} showOverflowHighlight
   */
  setOverflowHighlight(showOverflowHighlight) {
    this.container.tagState.classList.toggle(
      "overflow-causing-highlighted",
      showOverflowHighlight
    );
  },

  /**
   * Update the inline text editor in case of a single text child node.
   */
  updateTextEditor() {
    const node = this.node.inlineTextChild;

    if (this.textEditor && this.textEditor.node != node) {
      this.elt.removeChild(this.textEditor.elt);
      this.textEditor.destroy();
      this.textEditor = null;
    }

    if (node && !this.textEditor) {
      // Create a text editor added to this editor.
      // This editor won't receive an update automatically, so we rely on
      // child text editors to let us know that we need updating.
      this.textEditor = new TextEditor(this.container, node, "text");
      this.elt.insertBefore(
        this.textEditor.elt,
        this.elt.querySelector(".close")
      );
    }

    if (this.textEditor) {
      this.textEditor.update();
    }
  },

  hasUnavailableChildren() {
    return !!this.childrenUnavailableElt;
  },

  /**
   * Update a special badge displayed for nodes which have children that can't
   * be inspected by the current session (eg a parent-process only toolbox
   * inspecting a content browser).
   */
  updateUnavailableChildren() {
    const childrenUnavailable = this.node.childrenUnavailable;

    if (this.childrenUnavailableElt) {
      this.elt.removeChild(this.childrenUnavailableElt);
      this.childrenUnavailableElt = null;
    }

    if (childrenUnavailable) {
      this.childrenUnavailableElt = this.doc.createElement("div");
      this.childrenUnavailableElt.className = "unavailable-children";
      this.childrenUnavailableElt.dataset.label = INSPECTOR_L10N.getStr(
        "markupView.unavailableChildren.label"
      );
      this.childrenUnavailableElt.title = INSPECTOR_L10N.getStr(
        "markupView.unavailableChildren.title"
      );
      this.elt.insertBefore(
        this.childrenUnavailableElt,
        this.elt.querySelector(".close")
      );
    }
  },

  _startModifyingAttributes() {
    return this.node.startModifyingAttributes();
  },

  /**
   * Get the element used for one of the attributes of this element.
   *
   * @param  {String} attrName
   *         The name of the attribute to get the element for
   * @return {DOMNode}
   */
  getAttributeElement(attrName) {
    return this.attrList.querySelector(
      ".attreditor[data-attr=" + CSS.escape(attrName) + "] .attr-value"
    );
  },

  /**
   * Remove an attribute from the attrElements object and the DOM.
   *
   * @param  {String} attrName
   *         The name of the attribute to remove
   */
  removeAttribute(attrName) {
    const attr = this.attrElements.get(attrName);
    if (attr) {
      this.attrElements.delete(attrName);
      attr.remove();
    }
  },

  /**
   * Creates and returns the DOM for displaying an attribute with the following DOM
   * structure:
   *
   * dom.span(
   *   {
   *     className: "attreditor",
   *     "data-attr": attribute.name,
   *     "data-value": attribute.value,
   *   },
   *   " ",
   *   dom.span(
   *     { className: "editable", tabIndex: 0 },
   *     dom.span({ className: "attr-name theme-fg-color1" }, attribute.name),
   *     '="',
   *     dom.span({ className: "attr-value theme-fg-color2" }, attribute.value),
   *     '"'
   *   )
   */
  _createAttribute(attribute, before = null) {
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
    name.textContent = attribute.name;
    inner.appendChild(name);

    inner.appendChild(this.doc.createTextNode('="'));

    const val = this.doc.createElement("span");
    val.classList.add("attr-value");
    val.classList.add("theme-fg-color2");
    inner.appendChild(val);

    inner.appendChild(this.doc.createTextNode('"'));

    this._setupAttributeEditor(attribute, attr, inner, name, val);

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

    this._appendAttributeValue(attribute, val);

    return attr;
  },

  /**
   * Setup the editable field for the given attribute.
   *
   * @param  {Object} attribute
   *         An object containing the name and value of a DOM attribute.
   * @param  {Element} attrEditorEl
   *         The attribute container <span class="attreditor"> element.
   * @param  {Element} editableEl
   *         The editable <span class="editable"> element that is setup to be
   *         an editable field.
   * @param  {Element} attrNameEl
   *         The attribute name <span class="attr-name"> element.
   * @param  {Element} attrValueEl
   *         The attribute value <span class="attr-value"> element.
   */
  _setupAttributeEditor(
    attribute,
    attrEditorEl,
    editableEl,
    attrNameEl,
    attrValueEl
  ) {
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
    attrEditorEl.editMode = editableField({
      element: editableEl,
      trigger: "dblclick",
      stopOnReturn: true,
      selectAll: false,
      initial,
      multiline: true,
      maxWidth: () => getAutocompleteMaxWidth(editableEl, this.container.elt),
      contentType: InplaceEditor.CONTENT_TYPES.CSS_MIXED,
      popup: this.markup.popup,
      start: (editor, event) => {
        // If the editing was started inside the name or value areas,
        // select accordingly.
        if (event?.target === attrNameEl) {
          editor.input.setSelectionRange(0, attrNameEl.textContent.length);
        } else if (event?.target.closest(".attr-value") === attrValueEl) {
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
        this.refocusOnEdit(attribute.name, attrEditorEl, direction);
        this._saveAttribute(attribute.name, undoMods);
        doMods.removeAttribute(attribute.name);
        this._applyAttributes(newValue, attrEditorEl, doMods, undoMods);
        this.container.undo.do(
          () => {
            doMods.apply();
          },
          () => {
            undoMods.apply();
          }
        );
      },
      cssProperties: this._cssProperties,
    });
  },

  /**
   * Appends the attribute value to the given attribute value <span> element.
   *
   * @param  {Object} attribute
   *         An object containing the name and value of a DOM attribute.
   * @param  {Element} attributeValueEl
   *         The attribute value <span class="attr-value"> element to append
   *         the parsed attribute values to.
   */
  _appendAttributeValue(attribute, attributeValueEl) {
    // Parse the attribute value to detect whether there are linkable parts in
    // it (make sure to pass a complete list of existing attributes to the
    // parseAttribute function, by concatenating attribute, because this could
    // be a newly added attribute not yet on this.node).
    const attributes = this.node.attributes.filter(
      existingAttribute => existingAttribute.name !== attribute.name
    );
    attributes.push(attribute);

    const parsedLinksData = parseAttribute(
      this.node.namespaceURI,
      this.node.tagName,
      attributes,
      attribute.name,
      attribute.value
    );

    attributeValueEl.innerHTML = "";

    // Create links in the attribute value, and truncate long attribute values if needed.
    for (const token of parsedLinksData) {
      if (token.type === "string") {
        attributeValueEl.appendChild(
          this.doc.createTextNode(this._truncateAttributeValue(token.value))
        );
      } else {
        const link = this.doc.createElement("span");
        link.classList.add("link");
        link.setAttribute("data-type", token.type);
        link.setAttribute("data-link", token.value);
        link.textContent = this._truncateAttributeValue(token.value);
        attributeValueEl.append(link);

        // Add a "select node" button when we reference element ids
        if (
          token.type === ATTRIBUTE_TYPES.TYPE_IDREF ||
          token.type === ATTRIBUTE_TYPES.TYPE_IDREF_LIST
        ) {
          const button = this.doc.createElement("button");
          button.classList.add("select-node");
          button.setAttribute(
            "title",
            INSPECTOR_L10N.getFormatStr(
              "inspector.menu.selectElement.label",
              token.value
            )
          );
          link.append(button);
        }
      }
    }
  },

  /**
   * Truncates the given attribute value if it is a base64 data URL or the
   * collapse attributes pref is enabled.
   *
   * @param  {String} value
   *         Attribute value.
   * @return {String} truncated attribute value.
   */
  _truncateAttributeValue(value) {
    if (value && value.match(COLLAPSE_DATA_URL_REGEX)) {
      return truncateString(value, COLLAPSE_DATA_URL_LENGTH);
    }

    return this.markup.collapseAttributes
      ? truncateString(value, this.markup.collapseAttributeLength)
      : value;
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
  _applyAttributes(value, attrNode, doMods, undoMods) {
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
  _saveAttribute(name, undoMods) {
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
  refocusOnEdit(attrName, attrNode, direction) {
    // Only allow one refocus on attribute change at a time, so when there's
    // more than 1 request in parallel, the last one wins.
    if (this._editedAttributeObserver) {
      this.markup.inspector.off(
        "markupmutation",
        this._editedAttributeObserver
      );
      this._editedAttributeObserver = null;
    }

    const activeElement = this.markup.doc.activeElement;
    if (!activeElement || !activeElement.inplaceEditor) {
      // The focus was already removed from the current inplace editor, we should not
      // refocus the editable attribute.
      return;
    }

    const container = this.markup.getContainer(this.node);

    const activeAttrs = [...this.attrList.childNodes].filter(
      el => el.style.display != "none"
    );
    const attributeIndex = activeAttrs.indexOf(attrNode);

    const onMutations = (this._editedAttributeObserver = mutations => {
      let isDeletedAttribute = false;
      let isNewAttribute = false;

      for (const mutation of mutations) {
        const inContainer =
          this.markup.getContainer(mutation.target) === container;
        if (!inContainer) {
          continue;
        }

        const isOriginalAttribute = mutation.attributeName === attrName;

        isDeletedAttribute =
          isDeletedAttribute ||
          (isOriginalAttribute && mutation.newValue === null);
        isNewAttribute = isNewAttribute || mutation.attributeName !== attrName;
      }

      const isModifiedOrder = isDeletedAttribute && isNewAttribute;
      this._editedAttributeObserver = null;

      // "Deleted" attributes are merely hidden, so filter them out.
      const visibleAttrs = [...this.attrList.childNodes].filter(
        el => el.style.display != "none"
      );
      let activeEditor;
      if (visibleAttrs.length) {
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
          if (
            newAttributeIndex >= 0 &&
            newAttributeIndex <= visibleAttrs.length - 1
          ) {
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
        const editable =
          activeEditor === this.newAttr
            ? activeEditor
            : activeEditor.querySelector(".editable");
        editable.focus();
      }

      this.markup.emit("refocusedonedit");
    });

    // Start listening for mutations until we find an attributes change
    // that modifies this attribute.
    this.markup.inspector.once("markupmutation", onMutations);
  },

  /**
   * Called when the display badge is clicked. Toggles on the flexbox/grid highlighter for
   * the selected node if it is a grid container.
   *
   * Event handling for highlighter events is delegated up to the Markup view panel.
   * When a flexbox/grid highlighter is shown or hidden, the corresponding badge will
   * be marked accordingly. See MarkupView.handleHighlighterEvent()
   */
  async onDisplayBadgeClick(event) {
    event.stopPropagation();

    const target = event.target;

    if (
      target.dataset.display === "flex" ||
      target.dataset.display === "inline-flex"
    ) {
      await this.highlighters.toggleFlexboxHighlighter(this.node, "markup");
    }

    if (
      target.dataset.display === "grid" ||
      target.dataset.display === "inline-grid" ||
      target.dataset.display === "subgrid"
    ) {
      // Don't toggle the grid highlighter if the max number of new grid highlighters
      // allowed has been reached.
      if (!this.highlighters.canGridHighlighterToggle(this.node)) {
        return;
      }

      await this.highlighters.toggleGridHighlighter(this.node, "markup");
    }
  },

  async onCustomBadgeClick() {
    const { url, line, column } = this.node.customElementLocation;

    this.markup.toolbox.viewSourceInDebugger(
      url,
      line,
      column,
      null,
      "show_custom_element"
    );
  },

  onExpandBadgeClick() {
    this.container.expandContainer();
  },

  /**
   * Called when the scrollable badge is clicked. Shows the overflow causing elements and
   * highlights their container if the scroll badge is active.
   */
  async onScrollableBadgeClick() {
    this.highlightingOverflowCausingElements =
      this._scrollableBadge.classList.toggle("active");
    this._scrollableBadge.setAttribute(
      "aria-pressed",
      this.highlightingOverflowCausingElements
    );

    const { nodes } = await this.node.walkerFront.getOverflowCausingElements(
      this.node
    );

    for (const node of nodes) {
      if (this.highlightingOverflowCausingElements) {
        await this.markup.showNode(node);
      }

      const markupContainer = this.markup.getContainer(node);

      if (markupContainer) {
        markupContainer.editor.setOverflowHighlight(
          this.highlightingOverflowCausingElements
        );
      }
    }

    this.markup.telemetry.scalarAdd(
      "devtools.markup.scrollable.badge.clicked",
      1
    );
  },

  /**
   * Called when the tag name editor has is done editing.
   */
  onTagEdit(newTagName, isCommit) {
    if (
      !isCommit ||
      newTagName.toLowerCase() === this.node.tagName.toLowerCase() ||
      !("editTagName" in this.markup.walker)
    ) {
      return;
    }

    // Changing the tagName removes the node. Make sure the replacing node gets
    // selected afterwards.
    this.markup.reselectOnRemoved(this.node, "edittagname");
    this.node.walkerFront.editTagName(this.node, newTagName).catch(() => {
      // Failed to edit the tag name, cancel the reselection.
      this.markup.cancelReselectOnRemoved();
    });
  },

  destroy() {
    if (this._displayBadge) {
      this._displayBadge.removeEventListener("click", this.onDisplayBadgeClick);
    }

    if (this._customBadge) {
      this._customBadge.removeEventListener("click", this.onCustomBadgeClick);
    }

    if (this._scrollableBadge) {
      this._scrollableBadge.removeEventListener(
        "click",
        this.onScrollableBadgeClick
      );
    }

    this.expandBadge.removeEventListener("click", this.onExpandBadgeClick);

    for (const key in this.animationTimers) {
      clearTimeout(this.animationTimers[key]);
    }
    this.animationTimers = null;
  },
};

module.exports = ElementEditor;
