/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { objectInspector } from "devtools/client/shared/components/reps/index";
import { getBindingVariables } from "./getVariables";
import { getFramePopVariables, getThisVariable } from "./utils";
import { simplifyDisplayName } from "../../pause/frames";

const {
  utils: {
    node: { NODE_TYPES },
  },
} = objectInspector;

function getScopeTitle(type, scope) {
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

export function getScope(scope, selectedFrame, frameScopes, why, scopeIndex) {
  const { type, actor } = scope;

  const isLocalScope = scope.actor === frameScopes.actor;

  const key = `${actor}-${scopeIndex}`;
  if (type === "function" || type === "block") {
    const { bindings } = scope;

    let vars = getBindingVariables(bindings, key);

    // show exception, return, and this variables in innermost scope
    if (isLocalScope) {
      vars = vars.concat(getFramePopVariables(why, key));

      let thisDesc_ = selectedFrame.this;

      if (bindings && "this" in bindings) {
        // The presence of "this" means we're rendering a "this" binding
        // generated from mapScopes and this can override the binding
        // provided by the current frame.
        thisDesc_ = bindings.this ? bindings.this.value : null;
      }

      const this_ = getThisVariable(thisDesc_, key);

      if (this_) {
        vars.push(this_);
      }
    }

    if (vars?.length) {
      const title = getScopeTitle(type, scope) || "";
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
      value = { ...scope.object, displayClass: "Global" };
    }
    return {
      name: scope.object.class,
      path: key,
      contents: { value },
    };
  }

  return null;
}

export function mergeScopes(scope, parentScope, item, parentItem) {
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
