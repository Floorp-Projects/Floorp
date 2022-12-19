/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getOriginalFrameScope,
  getGeneratedFrameScope,
  getInlinePreviews,
  getSelectedLocation,
} from "../../selectors";
import { features } from "../../utils/prefs";
import { validateThreadContext } from "../../utils/context";

// We need to display all variables in the current functional scope so
// include all data for block scopes until the first functional scope
function getLocalScopeLevels(originalAstScopes) {
  let levels = 0;
  while (
    originalAstScopes[levels] &&
    originalAstScopes[levels].type === "block"
  ) {
    levels++;
  }
  return levels;
}

export function generateInlinePreview(cx, frame) {
  return async function({ dispatch, getState, parser, client }) {
    if (!frame || !features.inlinePreview) {
      return null;
    }

    const { thread } = cx;

    // Avoid regenerating inline previews when we already have preview data
    if (getInlinePreviews(getState(), thread, frame.id)) {
      return null;
    }

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

    let scopes = originalFrameScopes?.scope || generatedFrameScopes?.scope;

    if (!scopes || !scopes.bindings) {
      return null;
    }

    // It's important to use selectedLocation, because we don't know
    // if we'll be viewing the original or generated frame location
    const selectedLocation = getSelectedLocation(getState());
    if (!selectedLocation) {
      return null;
    }

    const originalAstScopes = await parser.getScopes(selectedLocation);
    validateThreadContext(getState(), cx);
    if (!originalAstScopes) {
      return null;
    }

    const allPreviews = [];
    const pausedOnLine = selectedLocation.line;
    const levels = getLocalScopeLevels(originalAstScopes);

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

      const previewBindings = Object.keys(bindings).map(async name => {
        // We want to show values of properties of objects only and not
        // function calls on other data types like someArr.forEach etc..
        let properties = null;
        const objectGrip = bindings[name].value;
        if (objectGrip.actor && objectGrip.class === "Object") {
          properties = await client.loadObjectProperties(
            {
              name,
              path: name,
              contents: { value: objectGrip },
            },
            cx.thread
          );
        }

        const previewsFromBindings = getBindingValues(
          originalAstScopes,
          pausedOnLine,
          name,
          bindings[name].value,
          curLevel,
          properties
        );

        allPreviews.push(...previewsFromBindings);
      });
      await Promise.all(previewBindings);

      scopes = scopes.parent;
    }

    // Sort previews by line and column so they're displayed in the right order in the editor
    allPreviews.sort((previewA, previewB) => {
      if (previewA.line < previewB.line) {
        return -1;
      }
      if (previewA.line > previewB.line) {
        return 1;
      }
      // If we have the same line number
      return previewA.column < previewB.column ? -1 : 1;
    });

    const previews = {};
    for (const preview of allPreviews) {
      const { line } = preview;
      if (!previews[line]) {
        previews[line] = [];
      }
      previews[line].push(preview);
    }

    return dispatch({
      type: "ADD_INLINE_PREVIEW",
      thread,
      frame,
      previews,
    });
  };
}

function getBindingValues(
  originalAstScopes,
  pausedOnLine,
  name,
  value,
  curLevel,
  properties
) {
  const previews = [];

  const binding = originalAstScopes[curLevel]?.bindings[name];
  if (!binding) {
    return previews;
  }

  // Show a variable only once ( an object and it's child property are
  // counted as different )
  const identifiers = new Set();

  // We start from end as we want to show values besides variable
  // located nearest to the breakpoint
  for (let i = binding.refs.length - 1; i >= 0; i--) {
    const ref = binding.refs[i];
    // Subtracting 1 from line as codemirror lines are 0 indexed
    const line = ref.start.line - 1;
    const column = ref.start.column;
    // We don't want to render inline preview below the paused line
    if (line >= pausedOnLine - 1) {
      continue;
    }

    const { displayName, displayValue } = getExpressionNameAndValue(
      name,
      value,
      ref,
      properties
    );

    // Variable with same name exists, display value of current or
    // closest to the current scope's variable
    if (identifiers.has(displayName)) {
      continue;
    }
    identifiers.add(displayName);

    previews.push({
      line,
      column,
      name: displayName,
      value: displayValue,
    });
  }
  return previews;
}

function getExpressionNameAndValue(
  name,
  value,
  // TODO: Add data type to ref
  ref,
  properties
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
        const property = properties.find(prop => prop.name === meta.property);
        displayValue = property?.contents.value;
        displayName += `.${meta.property}`;
      } else if (displayValue?.preview?.ownProperties) {
        const { ownProperties } = displayValue.preview;
        Object.keys(ownProperties).forEach(prop => {
          if (prop === meta.property) {
            displayValue = ownProperties[prop].value;
            displayName += `.${meta.property}`;
          }
        });
      }
      meta = meta.parent;
    }
  }

  return { displayName, displayValue };
}
