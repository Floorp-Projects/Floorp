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

"use strict";

/* globals document */

/**
 * For full documentation, see:
 * https://github.com/mozilla/domtemplate/blob/master/README.md
 */

/**
 * Begin a new templating process.
 * @param node A DOM element or string referring to an element's id
 * @param data Data to use in filling out the template
 * @param options Options to customize the template processing. One of:
 * - allowEval: boolean (default false) Basic template interpolations are
 *   either property paths (e.g. ${a.b.c.d}), or if allowEval=true then we
 *   allow arbitrary JavaScript
 * - stack: string or array of strings (default empty array) The template
 *   engine maintains a stack of tasks to help debug where it is. This allows
 *   this stack to be prefixed with a template name
 * - blankNullUndefined: By default DOMTemplate exports null and undefined
 *   values using the strings 'null' and 'undefined', which can be helpful for
 *   debugging, but can introduce unnecessary extra logic in a template to
 *   convert null/undefined to ''. By setting blankNullUndefined:true, this
 *   conversion is handled by DOMTemplate
 */
var template = function (node, data, options) {
  let state = {
    options: options || {},
    // We keep a track of the nodes that we've passed through so we can keep
    // data.__element pointing to the correct node
    nodes: []
  };

  state.stack = state.options.stack;

  if (!Array.isArray(state.stack)) {
    if (typeof state.stack === "string") {
      state.stack = [ options.stack ];
    } else {
      state.stack = [];
    }
  }

  processNode(state, node, data);
};

if (typeof exports !== "undefined") {
  exports.template = template;
}
this.template = template;

/**
 * Helper for the places where we need to act asynchronously and keep track of
 * where we are right now
 */
function cloneState(state) {
  return {
    options: state.options,
    stack: state.stack.slice(),
    nodes: state.nodes.slice()
  };
}

/**
 * Regex used to find ${...} sections in some text.
 * Performance note: This regex uses ( and ) to capture the 'script' for
 * further processing. Not all of the uses of this regex use this feature so
 * if use of the capturing group is a performance drain then we should split
 * this regex in two.
 */
var TEMPLATE_REGION = /\$\{([^}]*)\}/g;

/**
 * Recursive function to walk the tree processing the attributes as it goes.
 * @param node the node to process. If you pass a string in instead of a DOM
 * element, it is assumed to be an id for use with document.getElementById()
 * @param data the data to use for node processing.
 */
function processNode(state, node, data) {
  if (typeof node === "string") {
    node = document.getElementById(node);
  }
  if (data == null) {
    data = {};
  }
  state.stack.push(node.nodeName + (node.id ? "#" + node.id : ""));
  let pushedNode = false;
  try {
    // Process attributes
    if (node.attributes && node.attributes.length) {
      // We need to handle 'foreach' and 'if' first because they might stop
      // some types of processing from happening, and foreach must come first
      // because it defines new data on which 'if' might depend.
      if (node.hasAttribute("foreach")) {
        processForEach(state, node, data);
        return;
      }
      if (node.hasAttribute("if")) {
        if (!processIf(state, node, data)) {
          return;
        }
      }
      // Only make the node available once we know it's not going away
      state.nodes.push(data.__element);
      data.__element = node;
      pushedNode = true;
      // It's good to clean up the attributes when we've processed them,
      // but if we do it straight away, we mess up the array index
      let attrs = Array.prototype.slice.call(node.attributes);
      for (let i = 0; i < attrs.length; i++) {
        let value = attrs[i].value;
        let name = attrs[i].name;

        state.stack.push(name);
        try {
          if (name === "save") {
            // Save attributes are a setter using the node
            value = stripBraces(state, value);
            property(state, value, data, node);
            node.removeAttribute("save");
          } else if (name.substring(0, 2) === "on") {
            // If this attribute value contains only an expression
            if (value.substring(0, 2) === "${" && value.slice(-1) === "}" &&
                    value.indexOf("${", 2) === -1) {
              value = stripBraces(state, value);
              let func = property(state, value, data);
              if (typeof func === "function") {
                node.removeAttribute(name);
                let capture = node.hasAttribute("capture" + name.substring(2));
                node.addEventListener(name.substring(2), func, capture);
                if (capture) {
                  node.removeAttribute("capture" + name.substring(2));
                }
              } else {
                // Attribute value is not a function - use as a DOM-L0 string
                node.setAttribute(name, func);
              }
            } else {
              // Attribute value is not a single expression use as DOM-L0
              node.setAttribute(name, processString(state, value, data));
            }
          } else {
            node.removeAttribute(name);
            // Remove '_' prefix of attribute names so the DOM won't try
            // to use them before we've processed the template
            if (name.charAt(0) === "_") {
              name = name.substring(1);
            }

            // Async attributes can only work if the whole attribute is async
            let replacement;
            if (value.indexOf("${") === 0 &&
                value.charAt(value.length - 1) === "}") {
              replacement = envEval(state, value.slice(2, -1), data, value);
              if (replacement && typeof replacement.then === "function") {
                node.setAttribute(name, "");
                /* jshint loopfunc:true */
                replacement.then(function (newValue) {
                  node.setAttribute(name, newValue);
                }).catch(console.error);
              } else {
                if (state.options.blankNullUndefined && replacement == null) {
                  replacement = "";
                }
                node.setAttribute(name, replacement);
              }
            } else {
              node.setAttribute(name, processString(state, value, data));
            }
          }
        } finally {
          state.stack.pop();
        }
      }
    }

    // Loop through our children calling processNode. First clone them, so the
    // set of nodes that we visit will be unaffected by additions or removals.
    let childNodes = Array.prototype.slice.call(node.childNodes);
    for (let j = 0; j < childNodes.length; j++) {
      processNode(state, childNodes[j], data);
    }

    /* 3 === Node.TEXT_NODE */
    if (node.nodeType === 3) {
      processTextNode(state, node, data);
    }
  } finally {
    if (pushedNode) {
      data.__element = state.nodes.pop();
    }
    state.stack.pop();
  }
}

/**
 * Handle attribute values where the output can only be a string
 */
function processString(state, value, data) {
  return value.replace(TEMPLATE_REGION, function (path) {
    let insert = envEval(state, path.slice(2, -1), data, value);
    return state.options.blankNullUndefined && insert == null ? "" : insert;
  });
}

/**
 * Handle <x if="${...}">
 * @param node An element with an 'if' attribute
 * @param data The data to use with envEval()
 * @returns true if processing should continue, false otherwise
 */
function processIf(state, node, data) {
  state.stack.push("if");
  try {
    let originalValue = node.getAttribute("if");
    let value = stripBraces(state, originalValue);
    let recurse = true;
    try {
      let reply = envEval(state, value, data, originalValue);
      recurse = !!reply;
    } catch (ex) {
      handleError(state, "Error with '" + value + "'", ex);
      recurse = false;
    }
    if (!recurse) {
      node.remove();
    }
    node.removeAttribute("if");
    return recurse;
  } finally {
    state.stack.pop();
  }
}

/**
 * Handle <x foreach="param in ${array}"> and the special case of
 * <loop foreach="param in ${array}">.
 * This function is responsible for extracting what it has to do from the
 * attributes, and getting the data to work on (including resolving promises
 * in getting the array). It delegates to processForEachLoop to actually
 * unroll the data.
 * @param node An element with a 'foreach' attribute
 * @param data The data to use with envEval()
 */
function processForEach(state, node, data) {
  state.stack.push("foreach");
  try {
    let originalValue = node.getAttribute("foreach");
    let value = originalValue;

    let paramName = "param";
    if (value.charAt(0) === "$") {
      // No custom loop variable name. Use the default: 'param'
      value = stripBraces(state, value);
    } else {
      // Extract the loop variable name from 'NAME in ${ARRAY}'
      let nameArr = value.split(" in ");
      paramName = nameArr[0].trim();
      value = stripBraces(state, nameArr[1].trim());
    }
    node.removeAttribute("foreach");
    try {
      let evaled = envEval(state, value, data, originalValue);
      let cState = cloneState(state);
      handleAsync(evaled, node, function (reply, siblingNode) {
        processForEachLoop(cState, reply, node, siblingNode, data, paramName);
      });
      node.remove();
    } catch (ex) {
      handleError(state, "Error with " + value + "'", ex);
    }
  } finally {
    state.stack.pop();
  }
}

/**
 * Called by processForEach to handle looping over the data in a foreach loop.
 * This works with both arrays and objects.
 * Calls processForEachMember() for each member of 'set'
 * @param set The object containing the data to loop over
 * @param templNode The node to copy for each set member
 * @param sibling The sibling node to which we add things
 * @param data the data to use for node processing
 * @param paramName foreach loops have a name for the parameter currently being
 * processed. The default is 'param'. e.g. <loop foreach="param in ${x}">...
 */
function processForEachLoop(state, set, templNode, sibling, data, paramName) {
  if (Array.isArray(set)) {
    set.forEach(function (member, i) {
      processForEachMember(state, member, templNode, sibling,
                           data, paramName, "" + i);
    });
  } else {
    for (let member in set) {
      if (set.hasOwnProperty(member)) {
        processForEachMember(state, member, templNode, sibling,
                             data, paramName, member);
      }
    }
  }
}

/**
 * Called by processForEachLoop() to resolve any promises in the array (the
 * array itself can also be a promise, but that is resolved by
 * processForEach()). Handle <LOOP> elements (which are taken out of the DOM),
 * clone the template node, and pass the processing on to processNode().
 * @param member The data item to use in templating
 * @param templNode The node to copy for each set member
 * @param siblingNode The parent node to which we add things
 * @param data the data to use for node processing
 * @param paramName The name given to 'member' by the foreach attribute
 * @param frame A name to push on the stack for debugging
 */
function processForEachMember(state, member, templNode, siblingNode, data,
                              paramName, frame) {
  state.stack.push(frame);
  try {
    let cState = cloneState(state);
    handleAsync(member, siblingNode, function (reply, node) {
      // Clone data because we can't be sure that we can safely mutate it
      let newData = Object.create(null);
      Object.keys(data).forEach(function (key) {
        newData[key] = data[key];
      });
      newData[paramName] = reply;
      if (node.parentNode != null) {
        let clone;
        if (templNode.nodeName.toLowerCase() === "loop") {
          for (let i = 0; i < templNode.childNodes.length; i++) {
            clone = templNode.childNodes[i].cloneNode(true);
            node.parentNode.insertBefore(clone, node);
            processNode(cState, clone, newData);
          }
        } else {
          clone = templNode.cloneNode(true);
          clone.removeAttribute("foreach");
          node.parentNode.insertBefore(clone, node);
          processNode(cState, clone, newData);
        }
      }
    });
  } finally {
    state.stack.pop();
  }
}

/**
 * Take a text node and replace it with another text node with the ${...}
 * sections parsed out. We replace the node by altering node.parentNode but
 * we could probably use a DOM Text API to achieve the same thing.
 * @param node The Text node to work on
 * @param data The data to use in calls to envEval()
 */
function processTextNode(state, node, data) {
  // Replace references in other attributes
  let value = node.data;
  // We can't use the string.replace() with function trick (see generic
  // attribute processing in processNode()) because we need to support
  // functions that return DOM nodes, so we can't have the conversion to a
  // string.
  // Instead we process the string as an array of parts. In order to split
  // the string up, we first replace '${' with '\uF001$' and '}' with '\uF002'
  // We can then split using \uF001 or \uF002 to get an array of strings
  // where scripts are prefixed with $.
  // \uF001 and \uF002 are just unicode chars reserved for private use.
  value = value.replace(TEMPLATE_REGION, "\uF001$$$1\uF002");
  // Split a string using the unicode chars F001 and F002.
  let parts = value.split(/\uF001|\uF002/);
  if (parts.length > 1) {
    parts.forEach(function (part) {
      if (part === null || part === undefined || part === "") {
        return;
      }
      if (part.charAt(0) === "$") {
        part = envEval(state, part.slice(1), data, node.data);
      }
      let cState = cloneState(state);
      handleAsync(part, node, function (reply, siblingNode) {
        let doc = siblingNode.ownerDocument;
        if (reply == null) {
          reply = cState.options.blankNullUndefined ? "" : "" + reply;
        }
        if (typeof reply.cloneNode === "function") {
          // i.e. if (reply instanceof Element) { ...
          reply = maybeImportNode(cState, reply, doc);
          siblingNode.parentNode.insertBefore(reply, siblingNode);
        } else if (typeof reply.item === "function" && reply.length) {
          // NodeLists can be live, in which case maybeImportNode can
          // remove them from the document, and thus the NodeList, which in
          // turn breaks iteration. So first we clone the list
          let list = Array.prototype.slice.call(reply, 0);
          list.forEach(function (child) {
            let imported = maybeImportNode(cState, child, doc);
            siblingNode.parentNode.insertBefore(imported, siblingNode);
          });
        } else {
          // if thing isn't a DOM element then wrap its string value in one
          reply = doc.createTextNode(reply.toString());
          siblingNode.parentNode.insertBefore(reply, siblingNode);
        }
      });
    });
    node.remove();
  }
}

/**
 * Return node or a import of node, if it's not in the given document
 * @param node The node that we want to be properly owned
 * @param doc The document that the given node should belong to
 * @return A node that belongs to the given document
 */
function maybeImportNode(state, node, doc) {
  return node.ownerDocument === doc ? node : doc.importNode(node, true);
}

/**
 * A function to handle the fact that some nodes can be promises, so we check
 * and resolve if needed using a marker node to keep our place before calling
 * an inserter function.
 * @param thing The object which could be real data or a promise of real data
 * we use it directly if it's not a promise, or resolve it if it is.
 * @param siblingNode The element before which we insert new elements.
 * @param inserter The function to to the insertion. If thing is not a promise
 * then handleAsync() is just 'inserter(thing, siblingNode)'
 */
function handleAsync(thing, siblingNode, inserter) {
  if (thing != null && typeof thing.then === "function") {
    // Placeholder element to be replaced once we have the real data
    let tempNode = siblingNode.ownerDocument.createElement("span");
    siblingNode.parentNode.insertBefore(tempNode, siblingNode);
    thing.then(function (delayed) {
      inserter(delayed, tempNode);
      if (tempNode.parentNode != null) {
        tempNode.remove();
      }
    }).catch(function (error) {
      console.error(error.stack);
    });
  } else {
    inserter(thing, siblingNode);
  }
}

/**
 * Warn of string does not begin '${' and end '}'
 * @param str the string to check.
 * @return The string stripped of ${ and }, or untouched if it does not match
 */
function stripBraces(state, str) {
  if (!str.match(TEMPLATE_REGION)) {
    handleError(state, "Expected " + str + " to match ${...}");
    return str;
  }
  return str.slice(2, -1);
}

/**
 * Combined getter and setter that works with a path through some data set.
 * For example:
 * <ul>
 * <li>property(state, 'a.b', { a: { b: 99 }}); // returns 99
 * <li>property(state, 'a', { a: { b: 99 }}); // returns { b: 99 }
 * <li>property(state, 'a', { a: { b: 99 }}, 42); // returns 99 and alters the
 * input data to be { a: { b: 42 }}
 * </ul>
 * @param path An array of strings indicating the path through the data, or
 * a string to be cut into an array using <tt>split('.')</tt>
 * @param data the data to use for node processing
 * @param newValue (optional) If defined, this value will replace the
 * original value for the data at the path specified.
 * @return The value pointed to by <tt>path</tt> before any
 * <tt>newValue</tt> is applied.
 */
function property(state, path, data, newValue) {
  try {
    if (typeof path === "string") {
      path = path.split(".");
    }
    let value = data[path[0]];
    if (path.length === 1) {
      if (newValue !== undefined) {
        data[path[0]] = newValue;
      }
      if (typeof value === "function") {
        return value.bind(data);
      }
      return value;
    }
    if (!value) {
      handleError(state, "\"" + path[0] + "\" is undefined");
      return null;
    }
    return property(state, path.slice(1), value, newValue);
  } catch (ex) {
    handleError(state, "Path error with '" + path + "'", ex);
    return "${" + path + "}";
  }
}

/**
 * Like eval, but that creates a context of the variables in <tt>env</tt> in
 * which the script is evaluated.
 * @param script The string to be evaluated.
 * @param data The environment in which to eval the script.
 * @param frame Optional debugging string in case of failure.
 * @return The return value of the script, or the error message if the script
 * execution failed.
 */
function envEval(state, script, data, frame) {
  try {
    state.stack.push(frame.replace(/\s+/g, " "));
    // Detect if a script is capable of being interpreted using property()
    if (/^[_a-zA-Z0-9.]*$/.test(script)) {
      return property(state, script, data);
    }
    if (!state.options.allowEval) {
      handleError(state, "allowEval is not set, however '" + script + "'" +
                  " can not be resolved using a simple property path.");
      return "${" + script + "}";
    }

    // What we're looking to do is basically:
    //   with(data) { return eval(script); }
    // except in strict mode where 'with' is banned.
    // So we create a function which has a parameter list the same as the
    // keys in 'data' and with 'script' as its function body.
    // We then call this function with the values in 'data'
    let keys = allKeys(data);
    let func = Function.apply(null, keys.concat("return " + script));

    let values = keys.map((key) => data[key]);
    return func.apply(null, values);

    // TODO: The 'with' method is different from the code above in the value
    // of 'this' when calling functions. For example:
    //   envEval(state, 'foo()', { foo: function () { return this; } }, ...);
    // The global for 'foo' when using 'with' is the data object. However the
    // code above, the global is null. (Using 'func.apply(data, values)'
    // changes 'this' in the 'foo()' frame, but not in the inside the body
    // of 'foo', so that wouldn't help)
  } catch (ex) {
    handleError(state, "Template error evaluating '" + script + "'", ex);
    return "${" + script + "}";
  } finally {
    state.stack.pop();
  }
}

/**
 * Object.keys() that respects the prototype chain
 */
function allKeys(data) {
  let keys = [];
  for (let key in data) {
    keys.push(key);
  }
  return keys;
}

/**
 * A generic way of reporting errors, for easy overloading in different
 * environments.
 * @param message the error message to report.
 * @param ex optional associated exception.
 */
function handleError(state, message, ex) {
  logError(message + " (In: " + state.stack.join(" > ") + ")");
  if (ex) {
    logError(ex);
  }
}

/**
 * A generic way of reporting errors, for easy overloading in different
 * environments.
 * @param message the error message to report.
 */
function logError(message) {
  console.error(message);
}

exports.template = template;
