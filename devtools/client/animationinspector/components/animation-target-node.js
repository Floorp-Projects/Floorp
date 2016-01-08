"use strict";

const {Cu} = require("chrome");
Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");
const {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
const {
  createNode,
  TargetNodeHighlighter
} = require("devtools/client/animationinspector/utils");

const STRINGS_URI = "chrome://devtools/locale/animationinspector.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);

/**
 * UI component responsible for displaying a preview of the target dom node of
 * a given animation.
 * @param {InspectorPanel} inspector Requires a reference to the inspector-panel
 * to highlight and select the node, as well as refresh it when there are
 * mutations.
 * @param {Object} options Supported properties are:
 * - compact {Boolean} Defaults to false. If true, nodes will be previewed like
 *   tag#id.class instead of <tag id="id" class="class">
 */
function AnimationTargetNode(inspector, options = {}) {
  this.inspector = inspector;
  this.options = options;

  this.onPreviewMouseOver = this.onPreviewMouseOver.bind(this);
  this.onPreviewMouseOut = this.onPreviewMouseOut.bind(this);
  this.onSelectNodeClick = this.onSelectNodeClick.bind(this);
  this.onMarkupMutations = this.onMarkupMutations.bind(this);
  this.onHighlightNodeClick = this.onHighlightNodeClick.bind(this);
  this.onTargetHighlighterLocked = this.onTargetHighlighterLocked.bind(this);

  EventEmitter.decorate(this);
}

exports.AnimationTargetNode = AnimationTargetNode;

AnimationTargetNode.prototype = {
  init: function(containerEl) {
    let document = containerEl.ownerDocument;

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
        "title": L10N.getStr("node.highlightNodeLabel")
      }
    });

    // Wrapper used for mouseover/out event handling.
    this.previewEl = createNode({
      parent: this.el,
      nodeType: "span",
      attributes: {
        "title": L10N.getStr("node.selectNodeLabel")
      }
    });

    if (!this.options.compact) {
      this.previewEl.appendChild(document.createTextNode("<"));
    }

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
          "class": "theme-fg-color2"
        },
        textContent: "#"
      });
    }

    createNode({
      parent: this.idEl,
      nodeType: "span",
      attributes: {
        "class": "attribute-value theme-fg-color6"
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
          "class": "theme-fg-color6"
        },
        textContent: "."
      });
    }

    createNode({
      parent: this.classEl,
      nodeType: "span",
      attributes: {
        "class": "attribute-value theme-fg-color6"
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
    this.previewEl.addEventListener("click", this.onSelectNodeClick);
    this.highlightNodeEl.addEventListener("click", this.onHighlightNodeClick);

    // Start to listen for markupmutation events.
    this.inspector.on("markupmutation", this.onMarkupMutations);

    // Listen to the target node highlighter.
    TargetNodeHighlighter.on("highlighted", this.onTargetHighlighterLocked);
  },

  stopListeners: function() {
    TargetNodeHighlighter.off("highlighted", this.onTargetHighlighterLocked);
    this.inspector.off("markupmutation", this.onMarkupMutations);
    this.previewEl.removeEventListener("mouseover", this.onPreviewMouseOver);
    this.previewEl.removeEventListener("mouseout", this.onPreviewMouseOut);
    this.previewEl.removeEventListener("click", this.onSelectNodeClick);
    this.highlightNodeEl.removeEventListener("click", this.onHighlightNodeClick);
  },

  destroy: function() {
    TargetNodeHighlighter.unhighlight().catch(e => console.error(e));

    this.stopListeners();

    this.el.remove();
    this.el = this.tagNameEl = this.idEl = this.classEl = null;
    this.highlightNodeEl = this.previewEl = null;
    this.nodeFront = this.inspector = this.playerFront = null;
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
                         .catch(e => console.error(e));
  },

  onPreviewMouseOut: function() {
    if (!this.nodeFront || !this.highlighterUtils) {
      return;
    }
    this.highlighterUtils.unhighlight()
                         .catch(e => console.error(e));
  },

  onSelectNodeClick: function() {
    if (!this.nodeFront) {
      return;
    }
    this.inspector.selection.setNodeFront(this.nodeFront, "animationinspector");
  },

  onHighlightNodeClick: function(e) {
    e.stopPropagation();

    let classList = this.highlightNodeEl.classList;

    let isHighlighted = classList.contains("selected");
    if (isHighlighted) {
      classList.remove("selected");
      TargetNodeHighlighter.unhighlight().then(() => {
        this.emit("target-highlighter-unlocked");
      }, e => console.error(e));
    } else {
      classList.add("selected");
      TargetNodeHighlighter.highlight(this).then(() => {
        this.emit("target-highlighter-locked");
      }, e => console.error(e));
    }
  },

  onTargetHighlighterLocked: function(e, animationTargetNode) {
    if (animationTargetNode !== this) {
      this.highlightNodeEl.classList.remove("selected");
    }
  },

  onMarkupMutations: function(e, mutations) {
    if (!this.nodeFront || !this.playerFront) {
      return;
    }

    for (let {target} of mutations) {
      if (target === this.nodeFront) {
        // Re-render with the same nodeFront to update the output.
        this.render(this.playerFront);
        break;
      }
    }
  },

  render: Task.async(function*(playerFront) {
    this.playerFront = playerFront;
    this.nodeFront = undefined;

    try {
      this.nodeFront = yield this.inspector.walker.getNodeFromActor(
                             playerFront.actorID, ["node"]);
    } catch (e) {
      if (!this.el) {
        // The panel was destroyed in the meantime. Just log a warning.
        console.warn("Cound't retrieve the animation target node, widget " +
                     "destroyed");
      } else {
        // This was an unexpected error, log it.
        console.error(e);
      }
      return;
    }

    if (!this.nodeFront || !this.el) {
      return;
    }

    let {tagName, attributes} = this.nodeFront;

    this.tagNameEl.textContent = tagName.toLowerCase();

    let idIndex = attributes.findIndex(({name}) => name === "id");
    if (idIndex > -1 && attributes[idIndex].value) {
      this.idEl.querySelector(".attribute-value").textContent =
        attributes[idIndex].value;
      this.idEl.style.display = "inline";
    } else {
      this.idEl.style.display = "none";
    }

    let classIndex = attributes.findIndex(({name}) => name === "class");
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

    this.emit("target-retrieved");
  })
};
