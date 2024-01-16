/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This file contains utility functions which supports the structure & display of
// scopes information in Scopes panel.

import { objectInspector } from "devtools/client/shared/components/reps/index";
import { simplifyDisplayName } from "../pause/frames/index";

const {
  utils: {
    node: { NODE_TYPES },
  },
} = objectInspector;

// The heading that should be displayed for the scope
function _getScopeTitle(type, scope) {
  if (type === "block" && scope.block && scope.block.displayName) {
    return scope.block.displayName;
  }

  if (type === "function" && scope.function) {
    return scope.function.displayName
      ? simplifyDisplayName(scope.function.displayName)
      : L10N.getStr("anonymousFunction");
  }
  return L10N.getStr("scopes.block");
}

function _getThisVariable(this_, path) {
  if (!this_) {
    return null;
  }

  return {
    name: "<this>",
    path: `${path}/<this>`,
    contents: { value: this_ },
  };
}

/**
 * Builds a tree of nodes representing all the variables and arguments
 * for the bindings from a scope.
 *
 * Each binding => { variables: Array, arguments: Array }
 * Each binding argument => [name: string, contents: BindingContents]
 *
 * @param {Array} bindings
 * @param {String} parentName
 * @returns
 */
function _getBindingVariables(bindings, parentName) {
  if (!bindings) {
    return [];
  }

  const nodes = [];
  const addNode = (name, contents) =>
    nodes.push({ name, contents, path: `${parentName}/${name}` });

  for (const arg of bindings.arguments) {
    // `arg` is an object which only has a single property whose name is the name of the
    // argument. So here we can directly pick the first (and only) entry of `arg`
    const [name, contents] = Object.entries(arg)[0];
    addNode(name, contents);
  }

  for (const name in bindings.variables) {
    addNode(name, bindings.variables[name]);
  }

  return nodes;
}

/**
 * This generates the scope item for rendering in the scopes panel.
 *
 * @param {*} scope
 * @param {*} selectedFrame
 * @param {*} frameScopes
 * @param {*} why
 * @param {*} scopeIndex
 * @returns
 */
function _getScopeItem(scope, selectedFrame, frameScopes, why, scopeIndex) {
  const { type, actor } = scope;

  const isLocalScope = scope.actor === frameScopes.actor;

  const key = `${actor}-${scopeIndex}`;
  if (type === "function" || type === "block") {
    const { bindings } = scope;

    let vars = _getBindingVariables(bindings, key);

    // show exception, return, and this variables in innermost scope
    if (isLocalScope) {
      vars = vars.concat(_getFrameExceptionOrReturnedValueVariables(why, key));

      let thisDesc_ = selectedFrame.this;

      if (bindings && "this" in bindings) {
        // The presence of "this" means we're rendering a "this" binding
        // generated from mapScopes and this can override the binding
        // provided by the current frame.
        thisDesc_ = bindings.this ? bindings.this.value : null;
      }

      const this_ = _getThisVariable(thisDesc_, key);

      if (this_) {
        vars.push(this_);
      }
    }

    if (vars?.length) {
      const title = _getScopeTitle(type, scope) || "";
      vars.sort((a, b) => a.name.localeCompare(b.name));
      return {
        name: title,
        path: key,
        contents: vars,
        type: NODE_TYPES.BLOCK,
      };
    }
  } else if (type === "object" && scope.object) {
    let value = scope.object;
    // If this is the global window scope, mark it as such so that it will
    // preview Window: Global instead of Window: Window
    if (value.class === "Window") {
      value = { ...value, displayClass: "Global" };
    }
    return {
      name: scope.object.class,
      path: key,
      contents: { value },
    };
  }

  return null;
}
/**
 * Merge the scope bindings for lexical scopes and its parent function body scopes
 * Note: block scopes are not merged. See browser_dbg-merge-scopes.js for test examples
 * to better understand the scenario,
 *
 * @param {*} scope
 * @param {*} parentScope
 * @param {*} item
 * @param {*} parentItem
 * @returns
 */
export function _mergeLexicalScopesBindings(
  scope,
  parentScope,
  item,
  parentItem
) {
  if (scope.scopeKind == "function lexical" && parentScope.type == "function") {
    const contents = item.contents.concat(parentItem.contents);
    contents.sort((a, b) => a.name.localeCompare(b.name));

    return {
      name: parentItem.name,
      path: parentItem.path,
      contents,
      type: NODE_TYPES.BLOCK,
    };
  }
  return null;
}

/**
 * Returns a string path for an scope item which can be used
 * in different pauses for a thread.
 *
 * @param {Object} item
 * @returns
 */

export function getScopeItemPath(item) {
  // Calling toString() on item.path allows symbols to be handled.
  return item.path.toString();
}

// Generate variables when the function throws an exception or returned a value.
function _getFrameExceptionOrReturnedValueVariables(why, path) {
  const vars = [];

  if (why && why.frameFinished) {
    const { frameFinished } = why;

    // Always display a `throw` property if present, even if it is falsy.
    if (Object.prototype.hasOwnProperty.call(frameFinished, "throw")) {
      vars.push({
        name: "<exception>",
        path: `${path}/<exception>`,
        contents: { value: frameFinished.throw },
      });
    }

    if (Object.prototype.hasOwnProperty.call(frameFinished, "return")) {
      const returned = frameFinished.return;

      // Do not display undefined. Do display falsy values like 0 and false. The
      // protocol grip for undefined is a JSON object: { type: "undefined" }.
      if (typeof returned !== "object" || returned.type !== "undefined") {
        vars.push({
          name: "<return>",
          path: `${path}/<return>`,
          contents: { value: returned },
        });
      }
    }
  }

  return vars;
}

/**
 * Generates the scope items (for scopes related to selected frame) to be rendered in the scope panel
 * @param {*} why
 * @param {*} selectedFrame
 * @param {*} frameScopes
 * @returns
 */
export function getScopesItemsForSelectedFrame(
  why,
  selectedFrame,
  frameScopes
) {
  if (!why || !selectedFrame) {
    return null;
  }

  if (!frameScopes) {
    return null;
  }

  const scopes = [];

  let currentScope = frameScopes;
  let currentScopeIndex = 1;

  let prevScope = null,
    prevScopeItem = null;

  while (currentScope) {
    let currentScopeItem = _getScopeItem(
      currentScope,
      selectedFrame,
      frameScopes,
      why,
      currentScopeIndex
    );

    if (currentScopeItem) {
      const mergedItem =
        prevScope && prevScopeItem
          ? _mergeLexicalScopesBindings(
              prevScope,
              currentScope,
              prevScopeItem,
              currentScopeItem
            )
          : null;
      if (mergedItem) {
        currentScopeItem = mergedItem;
        scopes.pop();
      }
      scopes.push(currentScopeItem);
    }

    prevScope = currentScope;
    prevScopeItem = currentScopeItem;
    currentScopeIndex++;
    currentScope = currentScope.parent;
  }

  return scopes;
}
