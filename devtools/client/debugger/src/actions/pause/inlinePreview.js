/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import { getOriginalFrameScope, getGeneratedFrameScope } from "../../selectors";
import { features } from "../../utils/prefs";
import { getValue } from "../../utils/pause";

import type { OriginalScope } from "../../utils/pause/mapScopes";
import type { ThreadId, Frame, Scope, Preview } from "../../types";
import type { ThunkArgs } from "../types";

// We need to display all variables in the current functional scope so
// include all data for block scopes until the first functional scope
function getLocalScopeLevels(originalAstScopes): number {
  let levels = 0;
  while (
    originalAstScopes[levels] &&
    originalAstScopes[levels].type === "block"
  ) {
    levels++;
  }
  return levels;
}

export function generateInlinePreview(thread: ThreadId, frame: ?Frame) {
  return async function({ dispatch, getState, parser, client }: ThunkArgs) {
    if (!frame || !features.inlinePreview) {
      return;
    }

    const originalAstScopes = await parser.getScopes(frame.location);

    const originalFrameScopes = getOriginalFrameScope(
      getState(),
      thread,
      frame.location.sourceId,
      frame.id
    );

    const generatedFrameScopes = getGeneratedFrameScope(
      getState(),
      thread,
      frame.id
    );

    let scopes: ?OriginalScope | Scope | null =
      (originalFrameScopes && originalFrameScopes.scope) ||
      (generatedFrameScopes && generatedFrameScopes.scope);

    if (!scopes || !scopes.bindings || !originalAstScopes) {
      return;
    }

    const previewData: Preview = {};
    const pausedOnLine: number = frame.location.line;

    const levels: number = getLocalScopeLevels(originalAstScopes);

    for (
      let curLevel = 0;
      curLevel <= levels && scopes && scopes.bindings;
      curLevel++
    ) {
      const bindings = { ...scopes.bindings.variables };
      scopes.bindings.arguments.forEach(argument => {
        Object.keys(argument).forEach(key => {
          bindings[key] = argument[key];
        });
      });
      for (const name in bindings) {
        // We want to show values of properties of objects only and not
        // function calls on other data types like someArr.forEach etc..
        let properties = null;
        if (bindings[name].value.class === "Object") {
          const root = {
            name: name,
            path: name,
            contents: { value: bindings[name].value },
          };
          properties = await client.loadObjectProperties(root);
        }

        const preview: Preview = getBindingValues(
          originalAstScopes,
          pausedOnLine,
          name,
          bindings[name].value,
          curLevel,
          properties
        );

        for (const line in preview) {
          previewData[line] = { ...previewData[line], ...preview[line] };
        }
      }

      scopes = scopes.parent;
    }

    return dispatch({
      type: "ADD_INLINE_PREVIEW",
      thread,
      frame,
      previewData,
    });
  };
}

function getBindingValues(
  originalAstScopes: Object,
  pausedOnLine: number,
  name: string,
  value: any,
  curLevel: number,
  properties: Array<Object> | null
): Preview {
  const preview: Preview = {};

  const binding =
    originalAstScopes[curLevel] && originalAstScopes[curLevel].bindings[name];
  if (!binding) {
    return preview;
  }

  // Show a variable only once ( an object and it's child property are
  // counted as different )
  const identifiers = new Set();

  // We start from end as we want to show values besides variable
  // located nearest to the breakpoint
  for (let i = binding.refs.length - 1; i >= 0; i--) {
    const ref = binding.refs[i];
    // Subtracting 1 from line as codemirror lines are 0 indexed
    let line = ref.start.line - 1;
    // We don't want to render inline preview below the paused line
    if (line >= pausedOnLine - 1) {
      continue;
    }

    // Converting to string as all iterators on object keys ( eg: Object.keys,
    // for..in ) will return string
    line = line.toString();

    const { displayName, displayValue } = getExpressionNameAndValue(
      name,
      value,
      ref,
      properties
    );

    // Variable with same name exists, display value of current or
    // closest to the current scope's variable
    if (identifiers.has(displayName)) {
      return preview;
    }

    if (!preview[line]) {
      preview[line] = {};
    }

    identifiers.add(displayName);

    preview[line][displayName] = getValue(displayValue);
  }
  return preview;
}

function getExpressionNameAndValue(
  name: string,
  value: any,
  // TODO: Add data type to ref
  ref: Object,
  properties: Array<Object> | null
) {
  let displayName = name;
  let displayValue = value;

  // Only variables of type Object will have properties
  if (properties) {
    let { meta } = ref;
    // Presence of meta property means expression contains child property
    // reference eg: objName.propName
    while (meta) {
      // Initially properties will be an array, after that it will be an object
      if (displayValue === value) {
        const property: Object = properties.find(
          prop => prop.name === meta.property
        );
        displayValue = property.contents.value;
        displayName += `.${meta.property}`;
      } else {
        const { ownProperties } = displayValue.preview;
        for (const prop in ownProperties) {
          if (prop === meta.property) {
            displayValue = ownProperties[prop].value;
            displayName += `.${meta.property}`;
          }
        }
      }
      meta = meta.parent;
    }
  }

  return { displayName, displayValue };
}
