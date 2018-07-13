/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const {createNode} = require("devtools/client/inspector/animation-old/utils");
const { LocalizationHelper } = require("devtools/shared/l10n");

const STRINGS_URI = "devtools/client/locales/inspector.properties";
const L10N = new LocalizationHelper(STRINGS_URI);

/**
 * UI component responsible for displaying a preview of a dom node.
 * @param {InspectorPanel} inspector Requires a reference to the inspector-panel
 * to highlight and select the node, as well as refresh it when there are
 * mutations.
 * @param {Object} options Supported properties are:
 * - compact {Boolean} Defaults to false.
 *   By default, nodes are previewed like <tag id="id" class="class">
 *   If true, nodes will be previewed like tag#id.class instead.
 */
function DomNodePreview(inspector, options = {}) {
  this.inspector = inspector;
  this.options = options;

  this.onPreviewMouseOver = this.onPreviewMouseOver.bind(this);
  this.onPreviewMouseOut = this.onPreviewMouseOut.bind(this);
  this.onSelectElClick = this.onSelectElClick.bind(this);
  this.onMarkupMutations = this.onMarkupMutations.bind(this);
  this.onHighlightElClick = this.onHighlightElClick.bind(this);
  this.onHighlighterLocked = this.onHighlighterLocked.bind(this);

  EventEmitter.decorate(this);
}

exports.DomNodePreview = DomNodePreview;

DomNodePreview.prototype = {
  init: function(containerEl) {
    const document = containerEl.ownerDocument;

    // Init the markup for displaying the target node.
    this.el = createNode({
      parent: containerEl,
      attributes: {
        "class": "animation-target"
      }
    });

    // Icon to select the node in the inspector.
    this.highlightNodeEl = createNode({
      parent: this.el,
      nodeType: "span",
      attributes: {
        "class": "node-highlighter",
        "title": L10N.getStr("inspector.nodePreview.highlightNodeLabel")
      }
    });

    // Wrapper used for mouseover/out event handling.
    this.previewEl = createNode({
      parent: this.el,
      nodeType: "span",
      attributes: {
        "title": L10N.getStr("inspector.nodePreview.selectNodeLabel")
      }
    });

    if (!this.options.compact) {
      this.previewEl.appendChild(document.createTextNode("<"));
    }

    // Only used for ::before and ::after pseudo-elements.
    this.pseudoEl = createNode({
      parent: this.previewEl,
      nodeType: "span",
      attributes: {
        "class": "pseudo-element theme-fg-color5"
      }
    });

    // Tag name.
    this.tagNameEl = createNode({
      parent: this.previewEl,
      nodeType: "span",
      attributes: {
        "class": "tag-name theme-fg-color3"
      }
    });

    // Id attribute container.
    this.idEl = createNode({
      parent: this.previewEl,
      nodeType: "span"
    });

    if (!this.options.compact) {
      createNode({
        parent: this.idEl,
        nodeType: "span",
        attributes: {
          "class": "attribute-name theme-fg-color2"
        },
        textContent: "id"
      });
      this.idEl.appendChild(document.createTextNode("=\""));
    } else {
      createNode({
        parent: this.idEl,
        nodeType: "span",
        attributes: {
          "class": "theme-fg-color4"
        },
        textContent: "#"
      });
    }

    createNode({
      parent: this.idEl,
      nodeType: "span",
      attributes: {
        "class": "attribute-value theme-fg-color4"
      }
    });

    if (!this.options.compact) {
      this.idEl.appendChild(document.createTextNode("\""));
    }

    // Class attribute container.
    this.classEl = createNode({
      parent: this.previewEl,
      nodeType: "span"
    });

    if (!this.options.compact) {
      createNode({
        parent: this.classEl,
        nodeType: "span",
        attributes: {
          "class": "attribute-name theme-fg-color2"
        },
        textContent: "class"
      });
      this.classEl.appendChild(document.createTextNode("=\""));
    } else {
      createNode({
        parent: this.classEl,
        nodeType: "span",
        attributes: {
          "class": "theme-fg-color4"
        },
        textContent: "."
      });
    }

    createNode({
      parent: this.classEl,
      nodeType: "span",
      attributes: {
        "class": "attribute-value theme-fg-color4"
      }
    });

    if (!this.options.compact) {
      this.classEl.appendChild(document.createTextNode("\""));
      this.previewEl.appendChild(document.createTextNode(">"));
    }

    this.startListeners();
  },

  startListeners: function() {
    // Init events for highlighting and selecting the node.
    this.previewEl.addEventListener("mouseover", this.onPreviewMouseOver);
    this.previewEl.addEventListener("mouseout", this.onPreviewMouseOut);
    this.previewEl.addEventListener("click", this.onSelectElClick);
    this.highlightNodeEl.addEventListener("click", this.onHighlightElClick);

    // Start to listen for markupmutation events.
    this.inspector.on("markupmutation", this.onMarkupMutations);

    // Listen to the target node highlighter.
    HighlighterLock.on("highlighted", this.onHighlighterLocked);
  },

  stopListeners: function() {
    HighlighterLock.off("highlighted", this.onHighlighterLocked);
    this.inspector.off("markupmutation", this.onMarkupMutations);
    this.previewEl.removeEventListener("mouseover", this.onPreviewMouseOver);
    this.previewEl.removeEventListener("mouseout", this.onPreviewMouseOut);
    this.previewEl.removeEventListener("click", this.onSelectElClick);
    this.highlightNodeEl.removeEventListener("click", this.onHighlightElClick);
  },

  destroy: function() {
    HighlighterLock.unhighlight().catch(console.error);

    this.stopListeners();

    this.el.remove();
    this.el = this.tagNameEl = this.idEl = this.classEl = this.pseudoEl = null;
    this.highlightNodeEl = this.previewEl = null;
    this.nodeFront = this.inspector = null;
  },

  get highlighterUtils() {
    if (this.inspector && this.inspector.toolbox) {
      return this.inspector.toolbox.highlighterUtils;
    }
    return null;
  },

  onPreviewMouseOver: function() {
    if (!this.nodeFront || !this.highlighterUtils) {
      return;
    }
    this.highlighterUtils.highlightNodeFront(this.nodeFront)
                         .catch(console.error);
  },

  onPreviewMouseOut: function() {
    if (!this.nodeFront || !this.highlighterUtils) {
      return;
    }
    this.highlighterUtils.unhighlight()
                         .catch(console.error);
  },

  onSelectElClick: function() {
    if (!this.nodeFront) {
      return;
    }
    this.inspector.selection.setNodeFront(this.nodeFront, { reason: "dom-node-preview" });
  },

  onHighlightElClick: function(e) {
    e.stopPropagation();

    const classList = this.highlightNodeEl.classList;
    const isHighlighted = classList.contains("selected");

    if (isHighlighted) {
      classList.remove("selected");
      HighlighterLock.unhighlight().then(() => {
        this.emit("target-highlighter-unlocked");
      }, console.error);
    } else {
      classList.add("selected");
      HighlighterLock.highlight(this).then(() => {
        this.emit("target-highlighter-locked");
      }, console.error);
    }
  },

  onHighlighterLocked: function(domNodePreview) {
    if (domNodePreview !== this) {
      this.highlightNodeEl.classList.remove("selected");
    }
  },

  onMarkupMutations: function(mutations) {
    if (!this.nodeFront) {
      return;
    }

    for (const {target} of mutations) {
      if (target === this.nodeFront) {
        // Re-render with the same nodeFront to update the output.
        this.render(this.nodeFront);
        break;
      }
    }
  },

  render: function(nodeFront) {
    this.nodeFront = nodeFront;
    const {displayName, attributes} = nodeFront;

    if (nodeFront.isPseudoElement) {
      this.pseudoEl.textContent = nodeFront.isBeforePseudoElement
                                   ? "::before"
                                   : "::after";
      this.pseudoEl.style.display = "inline";
      this.tagNameEl.style.display = "none";
    } else {
      this.tagNameEl.textContent = displayName;
      this.pseudoEl.style.display = "none";
      this.tagNameEl.style.display = "inline";
    }

    const idIndex = attributes.findIndex(({name}) => name === "id");
    if (idIndex > -1 && attributes[idIndex].value) {
      this.idEl.querySelector(".attribute-value").textContent =
        attributes[idIndex].value;
      this.idEl.style.display = "inline";
    } else {
      this.idEl.style.display = "none";
    }

    const classIndex = attributes.findIndex(({name}) => name === "class");
    if (classIndex > -1 && attributes[classIndex].value) {
      let value = attributes[classIndex].value;
      if (this.options.compact) {
        value = value.split(" ").join(".");
      }

      this.classEl.querySelector(".attribute-value").textContent = value;
      this.classEl.style.display = "inline";
    } else {
      this.classEl.style.display = "none";
    }
  }
};

/**
 * HighlighterLock is a helper used to lock the highlighter on DOM nodes in the
 * page.
 * It instantiates a new highlighter that is then shared amongst all instances
 * of DomNodePreview. This is useful because that means showing the highlighter
 * on one node will unhighlight the previously highlighted one, but will not
 * interfere with the default inspector highlighter.
 */
var HighlighterLock = {
  highlighter: null,
  isShown: false,

  async highlight(animationTargetNode) {
    if (!this.highlighter) {
      const util = animationTargetNode.inspector.toolbox.highlighterUtils;
      this.highlighter = await util.getHighlighterByType("BoxModelHighlighter");
    }

    await this.highlighter.show(animationTargetNode.nodeFront);
    this.isShown = true;
    this.emit("highlighted", animationTargetNode);
  },

  async unhighlight() {
    if (!this.highlighter || !this.isShown) {
      return;
    }

    await this.highlighter.hide();
    this.isShown = false;
    this.emit("unhighlighted");
  }
};

EventEmitter.decorate(HighlighterLock);
