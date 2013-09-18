/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Template mechanism based on Object Emitters.
 *
 * The data used to expand the templates comes from
 * a ObjectEmitter object. The templates are automatically
 * updated as the ObjectEmitter is updated (via the "set"
 * event). See documentation in observable-object.js.
 *
 * Templates are used this way:
 *
 * (See examples in browser/devtools/app-manager/content/*.xhtml)
 *
 * <div template="{JSON Object}">
 *
 * {
 *  type: "attribute"
 *  name: name of the attribute
 *  path: location of the attribute value in the ObjectEmitter
 * }
 *
 * {
 *  type: "textContent"
 *  path: location of the textContent value in the ObjectEmitter
 * }
 *
 * {
 *  type: "localizedContent"
 *  paths: array of locations of the value of the arguments of the property
 *  property: l10n property
 * }
 *
 * <div template-loop="{JSON Object}">
 *
 * {
 *  arrayPath: path of the array in the ObjectEmitter to loop from
 *  childSelector: selector of the element to duplicate in the loop
 * }
 *
 */

const NOT_FOUND_STRING = "n/a";

/**
 * let t = new Template(root, store, l10nResolver);
 * t.start();
 *
 * @param DOMNode root.
 *        Node from where templates are expanded.
 * @param ObjectEmitter store.
 *        ObjectEmitter object.
 * @param function (property, args). l10nResolver
 *        A function that returns localized content.
 */
function Template(root, store, l10nResolver) {
  this._store = store;
  this._l10n = l10nResolver;

  // Listeners are stored in Maps.
  // path => Set(node1, node2, ..., nodeN)
  // For example: "foo.bar.4.name" => Set(div1,div2)

  this._nodeListeners = new Map();
  this._loopListeners = new Map();
  this._forListeners = new Map();
  this._root = root;
  this._doc = this._root.ownerDocument;

  this._store.on("set", (event,path,value) => this._storeChanged(path,value));
}

Template.prototype = {
  start: function() {
    this._processTree(this._root);
  },

  _resolvePath: function(path, defaultValue=null) {

    // From the store, get the value of an object located
    // at @path.
    //
    // For example, if the store is designed as:
    //
    // {
    //   foo: {
    //     bar: [
    //       {},
    //       {},
    //       {a: 2}
    //   }
    // }
    //
    // _resolvePath("foo.bar.2.a") will return "2".
    //
    // Array indexes are not surrounded by brackets.

    let chunks = path.split(".");
    let obj = this._store.object;
    for (let word of chunks) {
      if ((typeof obj) == "object" &&
          (word in obj)) {
        obj = obj[word];
      } else {
        return defaultValue;
      }
    }
    return obj;
  },

  _storeChanged: function(path, value) {

    // The store has changed (a "set" event has been emitted).
    // We need to invalidate and rebuild the affected elements.

    let strpath = path.join(".");
    this._invalidate(strpath);

    for (let [registeredPath, set] of this._nodeListeners) {
      if (strpath != registeredPath &&
          registeredPath.indexOf(strpath) > -1) {
        this._invalidate(registeredPath);
      }
    }
  },

  _invalidate: function(path) {
    // Loops:
    let set = this._loopListeners.get(path);
    if (set) {
      for (let elt of set) {
        this._processLoop(elt);
      }
    }

    // For:
    set = this._forListeners.get(path);
    if (set) {
      for (let elt of set) {
        this._processFor(elt);
      }
    }

    // Nodes:
    set = this._nodeListeners.get(path);
    if (set) {
      for (let elt of set) {
        this._processNode(elt);
      }
    }
  },

  _registerNode: function(path, element) {

    // We map a node to a path.
    // If the value behind this path is updated,
    // we get notified from the ObjectEmitter,
    // and then we know which objects to update.

    if (!this._nodeListeners.has(path)) {
      this._nodeListeners.set(path, new Set());
    }
    let set = this._nodeListeners.get(path);
    set.add(element);
  },

  _unregisterNodes: function(nodes) {
    for (let [registeredPath, set] of this._nodeListeners) {
      for (let e of nodes) {
        set.delete(e);
      }
      if (set.size == 0) {
        this._nodeListeners.delete(registeredPath);
      }
    }
  },

  _registerLoop: function(path, element) {
    if (!this._loopListeners.has(path)) {
      this._loopListeners.set(path, new Set());
    }
    let set = this._loopListeners.get(path);
    set.add(element);
  },

  _registerFor: function(path, element) {
    if (!this._forListeners.has(path)) {
      this._forListeners.set(path, new Set());
    }
    let set = this._forListeners.get(path);
    set.add(element);
  },

  _processNode: function(element, rootPath="") {
    // The actual magic.
    // The element has a template attribute.
    // The value is supposed to be a JSON string.
    // rootPath is the prefex to the path used by
    // these elements (if children of template-loop);

    let e = element;
    let str = e.getAttribute("template");

    if (rootPath) {
      // We will prefix paths with this rootPath.
      // It needs to end with a dot.
      rootPath = rootPath + ".";
    }

    try {
      let json = JSON.parse(str);
      // Sanity check
      if (!("type" in json)) {
        throw new Error("missing property");
      }
      if (json.rootPath) {
        // If node has been generated through a loop, we stored
        // previously its rootPath.
        rootPath = json.rootPath;
      }

      // paths is an array that will store all the paths we needed
      // to expand the node. We will then, via _registerNode, link
      // this element to these paths.

      let paths = [];

      switch (json.type) {
        case "attribute": {
          if (!("name" in json) ||
              !("path" in json)) {
            throw new Error("missing property");
          }
          e.setAttribute(json.name, this._resolvePath(rootPath + json.path, NOT_FOUND_STRING));
          paths.push(rootPath + json.path);
          break;
        }
        case "textContent": {
          if (!("path" in json)) {
            throw new Error("missing property");
          }
          e.textContent = this._resolvePath(rootPath + json.path, NOT_FOUND_STRING);
          paths.push(rootPath + json.path);
          break;
        }
        case "localizedContent": {
          if (!("property" in json) ||
              !("paths" in json)) {
            throw new Error("missing property");
          }
          let params = json.paths.map((p) => {
            paths.push(rootPath + p);
            let str = this._resolvePath(rootPath + p, NOT_FOUND_STRING);
            return str;
          });
          e.textContent = this._l10n(json.property, params);
          break;
        }
      }
      if (rootPath) {
        // We save the rootPath if any.
        json.rootPath = rootPath;
        e.setAttribute("template", JSON.stringify(json));
      }
      if (paths.length > 0) {
        for (let path of paths) {
          this._registerNode(path, e);
        }
      }
    } catch(exception) {
      console.error("Invalid template: " + e.outerHTML + " (" + exception + ")");
    }
  },

  _processLoop: function(element, rootPath="") {
    // The element has a template-loop attribute.
    // The related path must be an array. We go
    // through the array, and build one child per
    // item. The template for this child is pointed
    // by the childSelector property.
    let e = element;
    try {
      let template, count;
      let str = e.getAttribute("template-loop");
      let json = JSON.parse(str);
      if (!("arrayPath" in json)     ||
          !("childSelector" in json)) {
        throw new Error("missing property");
      }
      if (rootPath) {
        json.arrayPath = rootPath + "." + json.arrayPath;
      }
      let templateParent = this._doc.querySelector(json.childSelector);
      if (!templateParent) {
        throw new Error("can't find child");
      }
      template = this._doc.createElement("div");
      template.innerHTML = templateParent.innerHTML;
      template = template.firstElementChild;
      let array = this._resolvePath(json.arrayPath, []);
      if (!Array.isArray(array)) {
        console.error("referenced array is not an array");
      }
      count = array.length;

      let fragment = this._doc.createDocumentFragment();
      for (let i = 0; i < count; i++) {
        let node = template.cloneNode(true);
        this._processTree(node, json.arrayPath + "." + i);
        fragment.appendChild(node);
      }
      this._registerLoop(json.arrayPath, e);
      this._registerLoop(json.arrayPath + ".length", e);
      this._unregisterNodes(e.querySelectorAll("[template]"));
      e.innerHTML = "";
      e.appendChild(fragment);
    } catch(exception) {
      console.error("Invalid template: " + e.outerHTML + " (" + exception + ")");
    }
  },

  _processFor: function(element, rootPath="") {
    let e = element;
    try {
      let template;
      let str = e.getAttribute("template-for");
      let json = JSON.parse(str);
      if (!("path" in json) ||
          !("childSelector" in json)) {
        throw new Error("missing property");
      }

      if (rootPath) {
        json.path = rootPath + "." + json.path;
      }

      if (!json.path) {
        // Nothing to show.
        this._unregisterNodes(e.querySelectorAll("[template]"));
        e.innerHTML = "";
        return;
      }

      let templateParent = this._doc.querySelector(json.childSelector);
      if (!templateParent) {
        throw new Error("can't find child");
      }
      let content = this._doc.createElement("div");
      content.innerHTML = templateParent.innerHTML;
      content = content.firstElementChild;

      this._processTree(content, json.path);

      this._unregisterNodes(e.querySelectorAll("[template]"));
      this._registerFor(json.path, e);

      e.innerHTML = "";
      e.appendChild(content);

    } catch(exception) {
      console.error("Invalid template: " + e.outerHTML + " (" + exception + ")");
    }
  },

  _processTree: function(parent, rootPath="") {
    let loops = parent.querySelectorAll(":not(template) [template-loop]");
    let fors = parent.querySelectorAll(":not(template) [template-for]");
    let nodes = parent.querySelectorAll(":not(template) [template]");
    for (let e of loops) {
      this._processLoop(e, rootPath);
    }
    for (let e of fors) {
      this._processFor(e, rootPath);
    }
    for (let e of nodes) {
      this._processNode(e, rootPath);
    }
    if (parent.hasAttribute("template")) {
      this._processNode(parent, rootPath);
    }
  },
}
