/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

'use strict';

exports.clearResourceCache = function() {
  ResourceCache.clear();
};

/**
 * Resources are bits of CSS and JavaScript that the page either includes
 * directly or as a result of reading some remote resource.
 * Resource should not be used directly, but instead through a sub-class like
 * CssResource or ScriptResource.
 */
function Resource(name, type, inline, element) {
  this.name = name;
  this.type = type;
  this.inline = inline;
  this.element = element;
}

/**
 * Get the contents of the given resource as a string.
 * The base Resource leaves this unimplemented.
 */
Resource.prototype.loadContents = function() {
  throw new Error('not implemented');
};

Resource.TYPE_SCRIPT = 'text/javascript';
Resource.TYPE_CSS = 'text/css';

/**
 * A CssResource provides an implementation of Resource that works for both
 * [style] elements and [link type='text/css'] elements in the [head].
 */
function CssResource(domSheet) {
  this.name = domSheet.href;
  if (!this.name) {
    this.name = domSheet.ownerNode && domSheet.ownerNode.id ?
            'css#' + domSheet.ownerNode.id :
            'inline-css';
  }

  this.inline = (domSheet.href == null);
  this.type = Resource.TYPE_CSS;
  this.element = domSheet;
}

CssResource.prototype = Object.create(Resource.prototype);

CssResource.prototype.loadContents = function() {
  return new Promise((resolve, reject) => {
    resolve(this.element.ownerNode.innerHTML);
  });
};

CssResource._getAllStyles = function(context) {
  var resources = [];
  if (context.environment.window == null) {
    return resources;
  }

  var doc = context.environment.window.document;
  Array.prototype.forEach.call(doc.styleSheets, function(domSheet) {
    CssResource._getStyle(domSheet, resources);
  });

  dedupe(resources, function(clones) {
    for (var i = 0; i < clones.length; i++) {
      clones[i].name = clones[i].name + '-' + i;
    }
  });

  return resources;
};

CssResource._getStyle = function(domSheet, resources) {
  var resource = ResourceCache.get(domSheet);
  if (!resource) {
    resource = new CssResource(domSheet);
    ResourceCache.add(domSheet, resource);
  }
  resources.push(resource);

  // Look for imported stylesheets
  try {
    Array.prototype.forEach.call(domSheet.cssRules, function(domRule) {
      if (domRule.type == CSSRule.IMPORT_RULE && domRule.styleSheet) {
        CssResource._getStyle(domRule.styleSheet, resources);
      }
    }, this);
  }
  catch (ex) {
    // For system stylesheets
  }
};

/**
 * A ScriptResource provides an implementation of Resource that works for
 * [script] elements (both with a src attribute, and used directly).
 */
function ScriptResource(scriptNode) {
  this.name = scriptNode.src;
  if (!this.name) {
    this.name = scriptNode.id ?
            'script#' + scriptNode.id :
            'inline-script';
  }

  this.inline = (scriptNode.src === '' || scriptNode.src == null);
  this.type = Resource.TYPE_SCRIPT;
  this.element = scriptNode;
}

ScriptResource.prototype = Object.create(Resource.prototype);

ScriptResource.prototype.loadContents = function() {
  return new Promise((resolve, reject) => {
    if (this.inline) {
      resolve(this.element.innerHTML);
    }
    else {
      // It would be good if there was a better way to get the script source
      var xhr = new XMLHttpRequest();
      xhr.onreadystatechange = function() {
        if (xhr.readyState !== xhr.DONE) {
          return;
        }
        resolve(xhr.responseText);
      };
      xhr.open('GET', this.element.src, true);
      xhr.send();
    }
  });
};

ScriptResource._getAllScripts = function(context) {
  if (context.environment.window == null) {
    return [];
  }

  var doc = context.environment.window.document;
  var scriptNodes = doc.querySelectorAll('script');
  var resources = Array.prototype.map.call(scriptNodes, function(scriptNode) {
    var resource = ResourceCache.get(scriptNode);
    if (!resource) {
      resource = new ScriptResource(scriptNode);
      ResourceCache.add(scriptNode, resource);
    }
    return resource;
  });

  dedupe(resources, function(clones) {
    for (var i = 0; i < clones.length; i++) {
      clones[i].name = clones[i].name + '-' + i;
    }
  });

  return resources;
};

/**
 * Find resources with the same name, and call onDupe to change the names
 */
function dedupe(resources, onDupe) {
  // first create a map of name->[array of resources with same name]
  var names = {};
  resources.forEach(function(scriptResource) {
    if (names[scriptResource.name] == null) {
      names[scriptResource.name] = [];
    }
    names[scriptResource.name].push(scriptResource);
  });

  // Call the de-dupe function for each set of dupes
  Object.keys(names).forEach(function(name) {
    var clones = names[name];
    if (clones.length > 1) {
      onDupe(clones);
    }
  });
}

/**
 * A quick cache of resources against nodes
 * TODO: Potential memory leak when the target document has css or script
 * resources repeatedly added and removed. Solution might be to use a weak
 * hash map or some such.
 */
var ResourceCache = {
  _cached: [],

  /**
   * Do we already have a resource that was created for the given node
   */
  get: function(node) {
    for (var i = 0; i < ResourceCache._cached.length; i++) {
      if (ResourceCache._cached[i].node === node) {
        return ResourceCache._cached[i].resource;
      }
    }
    return null;
  },

  /**
   * Add a resource for a given node
   */
  add: function(node, resource) {
    ResourceCache._cached.push({ node: node, resource: resource });
  },

  /**
   * Drop all cache entries. Helpful to prevent memory leaks
   */
  clear: function() {
    ResourceCache._cached = [];
  }
};

/**
 * The resource type itself
 */
exports.items = [
  {
    item: 'type',
    name: 'resource',
    parent: 'selection',
    cacheable: false,
    include: null,

    constructor: function() {
      if (this.include !== Resource.TYPE_SCRIPT &&
          this.include !== Resource.TYPE_CSS &&
          this.include != null) {
        throw new Error('invalid include property: ' + this.include);
      }
    },

    lookup: function(context) {
      var resources = [];
      if (this.include !== Resource.TYPE_SCRIPT) {
        Array.prototype.push.apply(resources,
                                   CssResource._getAllStyles(context));
      }
      if (this.include !== Resource.TYPE_CSS) {
        Array.prototype.push.apply(resources,
                                   ScriptResource._getAllScripts(context));
      }

      return Promise.resolve(resources.map(function(resource) {
        return { name: resource.name, value: resource };
      }));
    }
  }
];
