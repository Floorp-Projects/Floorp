/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu } = require("chrome");
const { Class } = require("sdk/core/heritage");
const { emit } = require("sdk/event/core");
const { EventTarget } = require("sdk/event/target");
const { merge } = require("sdk/util/object");
const promise = require("promise");
const { InplaceEditor } = require("devtools/shared/inplace-editor");
const { on, forget } = require("projecteditor/helpers/event");
const { OS } = Cu.import("resource://gre/modules/osfile.jsm", {});

const HTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * ResourceContainer is used as the view of a single Resource in
 * the tree.  It is not exported.
 */
var ResourceContainer = Class({
  /**
   * @param ProjectTreeView tree
   * @param Resource resource
   */
  initialize: function(tree, resource) {
    this.tree = tree;
    this.resource = resource;
    this.elt = null;
    this.expander = null;
    this.children = null;

    let doc = tree.doc;

    this.elt = doc.createElementNS(HTML_NS, "li");
    this.elt.classList.add("child");

    this.line = doc.createElementNS(HTML_NS, "div");
    this.line.classList.add("child");
    this.line.classList.add("entry");
    this.line.setAttribute("theme", "dark");
    this.line.setAttribute("tabindex", "0");

    this.elt.appendChild(this.line);

    this.highlighter = doc.createElementNS(HTML_NS, "span");
    this.highlighter.classList.add("highlighter");
    this.line.appendChild(this.highlighter);

    this.expander = doc.createElementNS(HTML_NS, "span");
    this.expander.className = "arrow expander";
    this.expander.setAttribute("open", "");
    this.line.appendChild(this.expander);

    this.label = doc.createElementNS(HTML_NS, "span");
    this.label.className = "file-label";
    this.line.appendChild(this.label);

    this.line.addEventListener("contextmenu", (ev) => {
      this.select();
      this.openContextMenu(ev);
    }, false);

    this.children = doc.createElementNS(HTML_NS, "ul");
    this.children.classList.add("children");

    this.elt.appendChild(this.children);

    this.line.addEventListener("click", (evt) => {
      this.select();
      this.toggleExpansion();
      evt.stopPropagation();
    }, false);
    this.expander.addEventListener("click", (evt) => {
      this.toggleExpansion();
      this.select();
      evt.stopPropagation();
    }, true);

    if (!this.resource.isRoot) {
      this.expanded = false;
    }
    this.update();
  },

  toggleExpansion: function() {
    if (!this.resource.isRoot) {
      this.expanded = !this.expanded;
    } else {
      this.expanded = true;
    }
  },

  destroy: function() {
    this.elt.remove();
    this.expander.remove();
    this.highlighter.remove();
    this.children.remove();
    this.label.remove();
    this.elt = this.expander = this.highlighter = this.children = this.label = null;
  },

  /**
   * Open the context menu when right clicking on the view.
   * XXX: We could pass this to plugins to allow themselves
   * to be register/remove items from the context menu if needed.
   *
   * @param Event e
   */
  openContextMenu: function(ev) {
    ev.preventDefault();
    let popup = this.tree.options.contextMenuPopup;
    popup.openPopupAtScreen(ev.screenX, ev.screenY, true);
  },

  /**
   * Update the view based on the current state of the Resource.
   */
  update: function() {
    let visible = this.tree.options.resourceVisible ?
      this.tree.options.resourceVisible(this.resource) :
      true;

    this.elt.hidden = !visible;

    this.tree.options.resourceFormatter(this.resource, this.label);

    this.expander.style.visibility = this.resource.hasChildren ? "visible" : "hidden";

  },

  /**
   * Select this view in the ProjectTreeView.
   */
  select: function() {
    this.tree.selectContainer(this);
  },

  /**
   * @returns Boolean
   *          Is this view currently selected
   */
  get selected() {
    return this.line.classList.contains("selected");
  },

  /**
   * Set the selected state in the UI.
   */
  set selected(v) {
    if (v) {
      this.line.classList.add("selected");
    } else {
      this.line.classList.remove("selected");
    }
  },

  /**
   * @returns Boolean
   *          Are any children visible.
   */
  get expanded() {
    return !this.elt.classList.contains("tree-collapsed");
  },

  /**
   * Set the visiblity state of children.
   */
  set expanded(v) {
    if (v) {
      this.elt.classList.remove("tree-collapsed");
      this.expander.setAttribute("open", "");
    } else {
      this.expander.removeAttribute("open");
      this.elt.classList.add("tree-collapsed");
    }
  }
});

/**
 * TreeView is a view managing a list of children.
 * It is not to be instantiated directly - only extended.
 * Use ProjectTreeView instead.
 */
var TreeView = Class({
  extends: EventTarget,

  /**
   * @param Document document
   * @param Object options
   *               - contextMenuPopup: a <menupopup> element
   *               - resourceFormatter: a function(Resource, DOMNode)
   *                 that renders the resource into the view
   *               - resourceVisible: a function(Resource) -> Boolean
   *                 that determines if the resource should show up.
   */
  initialize: function(doc, options) {
    this.doc = doc;
    this.options = merge({
      resourceFormatter: function(resource, elt) {
        elt.textContent = resource.toString();
      }
    }, options);
    this.models = new Set();
    this.roots = new Set();
    this._containers = new Map();
    this.elt = this.doc.createElementNS(HTML_NS, "div");
    this.elt.tree = this;
    this.elt.className = "sources-tree";
    this.elt.setAttribute("with-arrows", "true");
    this.elt.setAttribute("theme", "dark");
    this.elt.setAttribute("flex", "1");

    this.children = this.doc.createElementNS(HTML_NS, "ul");
    this.elt.appendChild(this.children);

    this.resourceChildrenChanged = this.resourceChildrenChanged.bind(this);
    this.removeResource = this.removeResource.bind(this);
    this.updateResource = this.updateResource.bind(this);
  },

  destroy: function() {
    this._destroyed = true;
    this.elt.remove();
  },

  /**
   * Prompt the user to create a new file in the tree.
   *
   * @param string initial
   *               The suggested starting file name
   * @param Resource parent
   * @param Resource sibling
   *                 Which resource to put this next to.  If not set,
   *                 it will be put in front of all other children.
   *
   * @returns Promise
   *          Resolves once the prompt has been successful,
   *          Rejected if it is cancelled
   */
  promptNew: function(initial, parent, sibling=null) {
    let deferred = promise.defer();

    let parentContainer = this._containers.get(parent);
    let item = this.doc.createElement("li");
    item.className = "child";
    let placeholder = this.doc.createElementNS(HTML_NS, "div");
    placeholder.className = "child";
    item.appendChild(placeholder);

    let children = parentContainer.children;
    sibling = sibling ? this._containers.get(sibling).elt : null;
    parentContainer.children.insertBefore(item, sibling ? sibling.nextSibling : children.firstChild);

    new InplaceEditor({
      element: placeholder,
      initial: initial,
      start: editor => {
        editor.input.select();
      },
      done: function(val, commit) {
        if (commit) {
          deferred.resolve(val);
        } else {
          deferred.reject(val);
        }
        parentContainer.line.focus();
      },
      destroy: () => {
        item.parentNode.removeChild(item);
      },
    });

    return deferred.promise;
  },

  /**
   * Prompt the user to rename file in the tree.
   *
   * @param string initial
   *               The suggested starting file name
   * @param resource
   *
   * @returns Promise
   *          Resolves once the prompt has been successful,
   *          Rejected if it is cancelled
   */
  promptEdit: function(initial, resource) {
    let deferred = promise.defer();
    let placeholder = this._containers.get(resource).elt;

    new InplaceEditor({
      element: placeholder,
      initial: initial,
      start: editor => {
        editor.input.select();
      },
      done: function(val, commit) {
        if (commit) {
          deferred.resolve(val);
        } else {
          deferred.reject(val);
        }
      },
    });

    return deferred.promise;
  },

  /**
   * Add a new Store into the TreeView
   *
   * @param Store model
   */
  addModel: function(model) {
    if (this.models.has(model)) {
      // Requesting to add a model that already exists
      return;
    }
    this.models.add(model);
    let placeholder = this.doc.createElementNS(HTML_NS, "li");
    placeholder.style.display = "none";
    this.children.appendChild(placeholder);
    this.roots.add(model.root);
    model.root.refresh().then(root => {
      if (this._destroyed || !this.models.has(model)) {
        // model may have been removed during the initial refresh.
        // In this case, do not import the resource or add to DOM, just leave it be.
        return;
      }
      let container = this.importResource(root);
      container.line.classList.add("entry-group-title");
      container.line.setAttribute("theme", "dark");
      this.selectContainer(container);

      this.children.insertBefore(container.elt, placeholder);
      this.children.removeChild(placeholder);
    });
  },

  /**
   * Remove a Store from the TreeView
   *
   * @param Store model
   */
  removeModel: function(model) {
    this.models.delete(model);
    this.removeResource(model.root);
  },


  /**
   * Get the ResourceContainer.  Used for testing the view.
   *
   * @param Resource resource
   * @returns ResourceContainer
   */
  getViewContainer: function(resource) {
    return this._containers.get(resource);
  },

  /**
   * Select a ResourceContainer in the tree.
   *
   * @param ResourceContainer container
   */
  selectContainer: function(container) {
    if (this.selectedContainer === container) {
      return;
    }
    if (this.selectedContainer) {
      this.selectedContainer.selected = false;
    }
    this.selectedContainer = container;
    container.selected = true;
    emit(this, "selection", container.resource);
  },

  /**
   * Select a Resource in the tree.
   *
   * @param Resource resource
   */
  selectResource: function(resource) {
    this.selectContainer(this._containers.get(resource));
  },

  /**
   * Get the currently selected Resource
   *
   * @param Resource resource
   */
  getSelectedResource: function() {
    return this.selectedContainer.resource;
  },

  /**
   * Insert a Resource into the view.
   * Makes a new ResourceContainer if needed
   *
   * @param Resource resource
   */
  importResource: function(resource) {
    if (!resource) {
      return null;
    }

    if (this._containers.has(resource)) {
      return this._containers.get(resource);
    }
    var container = ResourceContainer(this, resource);
    this._containers.set(resource, container);
    this._updateChildren(container);

    on(this, resource, "children-changed", this.resourceChildrenChanged);
    on(this, resource, "label-change", this.updateResource);
    on(this, resource, "deleted", this.removeResource);

    return container;
  },

  /**
   * Remove a Resource (including children) from the view.
   *
   * @param Resource resource
   */
  removeResource: function(resource) {
    let toRemove = resource.allDescendants();
    toRemove.add(resource);
    for (let remove of toRemove) {
      this._removeResource(remove);
    }
  },

  /**
   * Remove an individual Resource (but not children) from the view.
   *
   * @param Resource resource
   */
  _removeResource: function(resource) {
    forget(this, resource);
    if (this._containers.get(resource)) {
      this._containers.get(resource).destroy();
      this._containers.delete(resource);
    }
    emit(this, "resource-removed", resource);
  },

  /**
   * Listener for when a resource has new children.
   * This can happen as files are being loaded in from FileSystem, for example.
   *
   * @param Resource resource
   */
  resourceChildrenChanged: function(resource) {
    this.updateResource(resource);
    this._updateChildren(this._containers.get(resource));
  },

  /**
   * Listener for when a label in the view has been updated.
   * For example, the 'dirty' plugin marks changed files with an '*'
   * next to the filename, and notifies with this event.
   *
   * @param Resource resource
   */
  updateResource: function(resource) {
    let container = this._containers.get(resource);
    container.update();
  },

  /**
   * Build necessary ResourceContainers for a Resource and its
   * children, then append them into the view.
   *
   * @param ResourceContainer container
   */
  _updateChildren: function(container) {
    let resource = container.resource;
    let fragment = this.doc.createDocumentFragment();
    if (resource.children) {
      for (let child of resource.childrenSorted) {
        let childContainer = this.importResource(child);
        fragment.appendChild(childContainer.elt);
      }
    }

    while (container.children.firstChild) {
      container.children.firstChild.remove();
    }

    container.children.appendChild(fragment);
  },
});

/**
 * ProjectTreeView is the implementation of TreeView
 * that is exported.  This is the class that is to be used
 * directly.
 */
var ProjectTreeView = Class({
  extends: TreeView,

  /**
   * See TreeView.initialize
   *
   * @param Document document
   * @param Object options
   */
  initialize: function(document, options) {
    TreeView.prototype.initialize.apply(this, arguments);
  },

  destroy: function() {
    this.forgetProject();
    TreeView.prototype.destroy.apply(this, arguments);
  },

  /**
   * Remove current project and empty the tree
   */
  forgetProject: function() {
    if (this.project) {
      forget(this, this.project);
      for (let store of this.project.allStores()) {
        this.removeModel(store);
      }
    }
  },

  /**
   * Show a project in the tree
   *
   * @param Project project
   *        The project to render into a tree
   */
  setProject: function(project) {
    this.forgetProject();
    this.project = project;
    if (this.project) {
      on(this, project, "store-added", this.addModel.bind(this));
      on(this, project, "store-removed", this.removeModel.bind(this));
      on(this, project, "project-saved", this.refresh.bind(this));
      this.refresh();
    }
  },

  /**
   * Refresh the tree with all of the current project stores
   */
  refresh: function() {
    for (let store of this.project.allStores()) {
      this.addModel(store);
    }
  }
});

exports.ProjectTreeView = ProjectTreeView;
