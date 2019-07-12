/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getScope, mergeScopes, type RenderableScope } from "./getScope";

import type { Frame, Why, BindingContents } from "../../../types";

export type NamedValue = {
  name: string,
  generatedName?: string,
  path: string,
  contents: BindingContents | NamedValue[],
};

export function getScopes(
  why: Why,
  selectedFrame: Frame,
  frameScopes: ?RenderableScope
): ?(NamedValue[]) {
  if (!why || !selectedFrame) {
    return null;
  }

  if (!frameScopes) {
    return null;
  }

  const scopes = [];

  let scope = frameScopes;
  let scopeIndex = 1;
  let prev = null,
    prevItem = null;

  while (scope) {
    let scopeItem = getScope(
      scope,
      selectedFrame,
      frameScopes,
      why,
      scopeIndex
    );

    if (scopeItem) {
      const mergedItem =
        prev && prevItem ? mergeScopes(prev, scope, prevItem, scopeItem) : null;
      if (mergedItem) {
        scopeItem = mergedItem;
        scopes.pop();
      }
      scopes.push(scopeItem);
    }
    prev = scope;
    prevItem = scopeItem;
    scopeIndex++;
    scope = scope.parent;
  }

  return scopes;
}
