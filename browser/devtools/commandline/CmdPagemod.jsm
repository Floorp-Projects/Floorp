/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = [ ];

Cu.import("resource:///modules/devtools/gcli.jsm");

/**
 * 'pagemod' command
 */
gcli.addCommand({
  name: "pagemod",
  description: gcli.lookup("pagemodDesc"),
});

/**
 * The 'pagemod replace' command. This command allows the user to search and
 * replace within text nodes and attributes.
 */
gcli.addCommand({
  name: "pagemod replace",
  description: gcli.lookup("pagemodReplaceDesc"),
  params: [
    {
      name: "search",
      type: "string",
      description: gcli.lookup("pagemodReplaceSearchDesc"),
    },
    {
      name: "replace",
      type: "string",
      description: gcli.lookup("pagemodReplaceReplaceDesc"),
    },
    {
      name: "ignoreCase",
      type: "boolean",
      description: gcli.lookup("pagemodReplaceIgnoreCaseDesc"),
    },
    {
      name: "selector",
      type: "string",
      description: gcli.lookup("pagemodReplaceSelectorDesc"),
      defaultValue: "*:not(script):not(style):not(embed):not(object):not(frame):not(iframe):not(frameset)",
    },
    {
      name: "root",
      type: "node",
      description: gcli.lookup("pagemodReplaceRootDesc"),
      defaultValue: null,
    },
    {
      name: "attrOnly",
      type: "boolean",
      description: gcli.lookup("pagemodReplaceAttrOnlyDesc"),
    },
    {
      name: "contentOnly",
      type: "boolean",
      description: gcli.lookup("pagemodReplaceContentOnlyDesc"),
    },
    {
      name: "attributes",
      type: "string",
      description: gcli.lookup("pagemodReplaceAttributesDesc"),
      defaultValue: null,
    },
  ],
  exec: function(args, context) {
    let document = context.environment.contentDocument;
    let searchTextNodes = !args.attrOnly;
    let searchAttributes = !args.contentOnly;
    let regexOptions = args.ignoreCase ? 'ig' : 'g';
    let search = new RegExp(escapeRegex(args.search), regexOptions);
    let attributeRegex = null;
    if (args.attributes) {
      attributeRegex = new RegExp(args.attributes, regexOptions);
    }

    let root = args.root || document;
    let elements = root.querySelectorAll(args.selector);
    elements = Array.prototype.slice.call(elements);

    let replacedTextNodes = 0;
    let replacedAttributes = 0;

    function replaceAttribute() {
      replacedAttributes++;
      return args.replace;
    }
    function replaceTextNode() {
      replacedTextNodes++;
      return args.replace;
    }

    for (let i = 0; i < elements.length; i++) {
      let element = elements[i];
      if (searchTextNodes) {
        for (let y = 0; y < element.childNodes.length; y++) {
          let node = element.childNodes[y];
          if (node.nodeType == node.TEXT_NODE) {
            node.textContent = node.textContent.replace(search, replaceTextNode);
          }
        }
      }

      if (searchAttributes) {
        if (!element.attributes) {
          continue;
        }
        for (let y = 0; y < element.attributes.length; y++) {
          let attr = element.attributes[y];
          if (!attributeRegex || attributeRegex.test(attr.name)) {
            attr.value = attr.value.replace(search, replaceAttribute);
          }
        }
      }
    }

    return gcli.lookupFormat("pagemodReplaceResult",
                             [elements.length, replacedTextNodes,
                              replacedAttributes]);
  }
});

/**
 * 'pagemod remove' command
 */
gcli.addCommand({
  name: "pagemod remove",
  description: gcli.lookup("pagemodRemoveDesc"),
});


/**
 * The 'pagemod remove element' command.
 */
gcli.addCommand({
  name: "pagemod remove element",
  description: gcli.lookup("pagemodRemoveElementDesc"),
  params: [
    {
      name: "search",
      type: "string",
      description: gcli.lookup("pagemodRemoveElementSearchDesc"),
    },
    {
      name: "root",
      type: "node",
      description: gcli.lookup("pagemodRemoveElementRootDesc"),
      defaultValue: null,
    },
    {
      name: 'stripOnly',
      type: 'boolean',
      description: gcli.lookup("pagemodRemoveElementStripOnlyDesc"),
    },
    {
      name: 'ifEmptyOnly',
      type: 'boolean',
      description: gcli.lookup("pagemodRemoveElementIfEmptyOnlyDesc"),
    },
  ],
  exec: function(args, context) {
    let document = context.environment.contentDocument;
    let root = args.root || document;
    let elements = Array.prototype.slice.call(root.querySelectorAll(args.search));

    let removed = 0;
    for (let i = 0; i < elements.length; i++) {
      let element = elements[i];
      let parentNode = element.parentNode;
      if (!parentNode || !element.removeChild) {
        continue;
      }
      if (args.stripOnly) {
        while (element.hasChildNodes()) {
          parentNode.insertBefore(element.childNodes[0], element);
        }
      }
      if (!args.ifEmptyOnly || !element.hasChildNodes()) {
        element.parentNode.removeChild(element);
        removed++;
      }
    }

    return gcli.lookupFormat("pagemodRemoveElementResultMatchedAndRemovedElements",
                             [elements.length, removed]);
  }
});

/**
 * The 'pagemod remove attribute' command.
 */
gcli.addCommand({
  name: "pagemod remove attribute",
  description: gcli.lookup("pagemodRemoveAttributeDesc"),
  params: [
    {
      name: "searchAttributes",
      type: "string",
      description: gcli.lookup("pagemodRemoveAttributeSearchAttributesDesc"),
    },
    {
      name: "searchElements",
      type: "string",
      description: gcli.lookup("pagemodRemoveAttributeSearchElementsDesc"),
    },
    {
      name: "root",
      type: "node",
      description: gcli.lookup("pagemodRemoveAttributeRootDesc"),
      defaultValue: null,
    },
    {
      name: "ignoreCase",
      type: "boolean",
      description: gcli.lookup("pagemodRemoveAttributeIgnoreCaseDesc"),
    },
  ],
  exec: function(args, context) {
    let document = context.environment.contentDocument;

    let root = args.root || document;
    let regexOptions = args.ignoreCase ? 'ig' : 'g';
    let attributeRegex = new RegExp(args.searchAttributes, regexOptions);
    let elements = root.querySelectorAll(args.searchElements);
    elements = Array.prototype.slice.call(elements);

    let removed = 0;
    for (let i = 0; i < elements.length; i++) {
      let element = elements[i];
      if (!element.attributes) {
        continue;
      }

      var attrs = Array.prototype.slice.call(element.attributes);
      for (let y = 0; y < attrs.length; y++) {
        let attr = attrs[y];
        if (attributeRegex.test(attr.name)) {
          element.removeAttribute(attr.name);
          removed++;
        }
      }
    }

    return gcli.lookupFormat("pagemodRemoveAttributeResult",
                             [elements.length, removed]);
  }
});

/**
 * Make a given string safe to use  in a regular expression.
 *
 * @param string aString
 *        The string you want to use in a regex.
 * @return string
 *         The equivalent of |aString| but safe to use in a regex.
 */
function escapeRegex(aString) {
  return aString.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, "\\$&");
}
