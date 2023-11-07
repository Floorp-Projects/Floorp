/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  debuggerToSourceMapLocation,
  sourceMapToDebuggerLocation,
} from "../../location";
import { locColumn } from "./locColumn";
import { loadRangeMetadata, findMatchingRange } from "./rangeMetadata";

// eslint-disable-next-line max-len
import {
  findGeneratedReference,
  findGeneratedImportReference,
  findGeneratedImportDeclaration,
} from "./findGeneratedBindingFromPosition";
import {
  buildGeneratedBindingList,
  buildFakeBindingList,
} from "./buildGeneratedBindingList";
import {
  originalRangeStartsInside,
  getApplicableBindingsForOriginalPosition,
} from "./getApplicableBindingsForOriginalPosition";
import { getOptimizedOutGrip } from "./optimizedOut";

import { log } from "../../log";

// Create real location objects for all location start and end.
//
// Parser worker returns scopes with location having a sourceId
// instead of a source object as it doesn't know about main thread source objects.
function updateLocationsInScopes(state, scopes) {
  for (const item of scopes) {
    for (const name of Object.keys(item.bindings)) {
      for (const ref of item.bindings[name].refs) {
        const locs = [ref];
        if (ref.type !== "ref") {
          locs.push(ref.declaration);
        }
        for (const loc of locs) {
          loc.start = sourceMapToDebuggerLocation(state, loc.start);
          loc.end = sourceMapToDebuggerLocation(state, loc.end);
        }
      }
    }
  }
}

export async function buildMappedScopes(
  source,
  content,
  frame,
  generatedScopes,
  thunkArgs
) {
  const { getState, parserWorker } = thunkArgs;
  if (!parserWorker.isLocationSupported(frame.location)) {
    return null;
  }
  const originalAstScopes = await parserWorker.getScopes(frame.location);
  updateLocationsInScopes(getState(), originalAstScopes);
  const generatedAstScopes = await parserWorker.getScopes(
    frame.generatedLocation
  );
  updateLocationsInScopes(getState(), generatedAstScopes);

  if (!originalAstScopes || !generatedAstScopes) {
    return null;
  }

  const originalRanges = await loadRangeMetadata(
    frame.location,
    originalAstScopes,
    thunkArgs
  );

  if (hasLineMappings(originalRanges)) {
    // Fallback to generated scopes as there are no clear mappings to original scopes
    // This means the scope variable names are likely the same for both the original
    // generated sources.
    return { scope: generatedScopes };
  }

  let generatedAstBindings;
  if (generatedScopes) {
    generatedAstBindings = buildGeneratedBindingList(
      generatedScopes,
      generatedAstScopes,
      frame.this
    );
  } else {
    generatedAstBindings = buildFakeBindingList(generatedAstScopes);
  }

  const { mappedOriginalScopes, expressionLookup } =
    await mapOriginalBindingsToGenerated(
      source,
      content,
      originalRanges,
      originalAstScopes,
      generatedAstBindings,
      thunkArgs
    );

  const globalLexicalScope = generatedScopes
    ? getGlobalFromScope(generatedScopes)
    : generateGlobalFromAst(generatedAstScopes);
  const mappedGeneratedScopes = generateClientScope(
    globalLexicalScope,
    mappedOriginalScopes
  );

  return isReliableScope(mappedGeneratedScopes)
    ? { mappings: expressionLookup, scope: mappedGeneratedScopes }
    : { scope: generatedScopes };
}

async function mapOriginalBindingsToGenerated(
  source,
  content,
  originalRanges,
  originalAstScopes,
  generatedAstBindings,
  thunkArgs
) {
  const expressionLookup = {};
  const mappedOriginalScopes = [];

  const cachedSourceMaps = batchScopeMappings(
    originalAstScopes,
    source,
    thunkArgs
  );
  // Override sourceMapLoader attribute with the special cached SourceMapLoader instance
  // in order to make it used by all functions used in this method.
  thunkArgs = { ...thunkArgs, sourceMapLoader: cachedSourceMaps };

  for (const item of originalAstScopes) {
    const generatedBindings = {};

    for (const name of Object.keys(item.bindings)) {
      const binding = item.bindings[name];

      const result = await findGeneratedBinding(
        source,
        content,
        name,
        binding,
        originalRanges,
        generatedAstBindings,
        thunkArgs
      );

      if (result) {
        generatedBindings[name] = result.grip;

        if (
          binding.refs.length !== 0 &&
          // These are assigned depth-first, so we don't want shadowed
          // bindings in parent scopes overwriting the expression.
          !Object.prototype.hasOwnProperty.call(expressionLookup, name)
        ) {
          expressionLookup[name] = result.expression;
        }
      }
    }

    mappedOriginalScopes.push({
      ...item,
      generatedBindings,
    });
  }

  return {
    mappedOriginalScopes,
    expressionLookup,
  };
}

/**
 * Consider a scope and its parents reliable if the vast majority of its
 * bindings were successfully mapped to generated scope bindings.
 */
function isReliableScope(scope) {
  let totalBindings = 0;
  let unknownBindings = 0;

  for (let s = scope; s; s = s.parent) {
    const vars = s.bindings?.variables || {};
    for (const key of Object.keys(vars)) {
      const binding = vars[key];

      totalBindings += 1;
      if (
        binding.value &&
        typeof binding.value === "object" &&
        (binding.value.type === "unscoped" || binding.value.type === "unmapped")
      ) {
        unknownBindings += 1;
      }
    }
  }

  // As determined by fair dice roll.
  return totalBindings === 0 || unknownBindings / totalBindings < 0.25;
}

function hasLineMappings(ranges) {
  return ranges.every(
    range => range.columnStart === 0 && range.columnEnd === Infinity
  );
}

/**
 * Build a special SourceMapLoader instance, based on the one passed in thunkArgs,
 * which will both:
 *   - preload generated ranges/locations for original locations mentioned
 *     in originalAstScopes
 *   - cache the requests to fetch these genereated ranges/locations
 */
function batchScopeMappings(originalAstScopes, source, thunkArgs) {
  const { sourceMapLoader } = thunkArgs;
  const precalculatedRanges = new Map();
  const precalculatedLocations = new Map();

  // Explicitly dispatch all of the sourcemap requests synchronously up front so
  // that they will be batched into a single request for the worker to process.
  for (const item of originalAstScopes) {
    for (const name of Object.keys(item.bindings)) {
      for (const ref of item.bindings[name].refs) {
        const locs = [ref];
        if (ref.type !== "ref") {
          locs.push(ref.declaration);
        }

        for (const loc of locs) {
          precalculatedRanges.set(
            buildLocationKey(loc.start),
            sourceMapLoader.getGeneratedRanges(
              debuggerToSourceMapLocation(loc.start)
            )
          );
          precalculatedLocations.set(
            buildLocationKey(loc.start),
            sourceMapLoader.getGeneratedLocation(
              debuggerToSourceMapLocation(loc.start)
            )
          );
          precalculatedLocations.set(
            buildLocationKey(loc.end),
            sourceMapLoader.getGeneratedLocation(
              debuggerToSourceMapLocation(loc.end)
            )
          );
        }
      }
    }
  }

  return {
    async getGeneratedRanges(pos) {
      const key = buildLocationKey(pos);

      if (!precalculatedRanges.has(key)) {
        log("Bad precalculated mapping");
        return sourceMapLoader.getGeneratedRanges(
          debuggerToSourceMapLocation(pos)
        );
      }
      return precalculatedRanges.get(key);
    },

    async getGeneratedLocation(pos) {
      const key = buildLocationKey(pos);

      if (!precalculatedLocations.has(key)) {
        log("Bad precalculated mapping");
        return sourceMapLoader.getGeneratedLocation(
          debuggerToSourceMapLocation(pos)
        );
      }
      return precalculatedLocations.get(key);
    },
  };
}
function buildLocationKey(loc) {
  return `${loc.line}:${locColumn(loc)}`;
}

function generateClientScope(globalLexicalScope, originalScopes) {
  // Build a structure similar to the client's linked scope object using
  // the original AST scopes, but pulling in the generated bindings
  // linked to each scope.
  const result = originalScopes
    .slice(0, -2)
    .reverse()
    .reduce((acc, orig, i) => {
      const {
        // The 'this' binding data we have is handled independently, so
        // the binding data is not included here.
        // eslint-disable-next-line no-unused-vars
        this: _this,
        ...variables
      } = orig.generatedBindings;

      return {
        parent: acc,
        actor: `originalActor${i}`,
        type: orig.type,
        scopeKind: orig.scopeKind,
        bindings: {
          arguments: [],
          variables,
        },
        ...(orig.type === "function"
          ? {
              function: {
                displayName: orig.displayName,
              },
            }
          : null),
        ...(orig.type === "block"
          ? {
              block: {
                displayName: orig.displayName,
              },
            }
          : null),
      };
    }, globalLexicalScope);

  // The rendering logic in getScope 'this' bindings only runs on the current
  // selected frame scope, so we pluck out the 'this' binding that was mapped,
  // and put it in a special location
  const thisScope = originalScopes.find(scope => scope.bindings.this);
  if (result.bindings && thisScope) {
    result.bindings.this = thisScope.generatedBindings.this || null;
  }

  return result;
}

function getGlobalFromScope(scopes) {
  // Pull the root object scope and root lexical scope to reuse them in
  // our mapped scopes. This assumes that file being processed is
  // a CommonJS or ES6 module, which might not be ideal. Potentially
  // should add some logic to try to detect those cases?
  let globalLexicalScope = null;
  for (let s = scopes; s.parent; s = s.parent) {
    globalLexicalScope = s;
  }
  if (!globalLexicalScope) {
    throw new Error("Assertion failure - there should always be a scope");
  }
  return globalLexicalScope;
}

function generateGlobalFromAst(generatedScopes) {
  const globalLexicalAst = generatedScopes[generatedScopes.length - 2];
  if (!globalLexicalAst) {
    throw new Error("Assertion failure - there should always be a scope");
  }
  return {
    actor: "generatedActor1",
    type: "block",
    scopeKind: "",
    bindings: {
      arguments: [],
      variables: Object.fromEntries(
        Object.keys(globalLexicalAst).map(key => [key, getOptimizedOutGrip()])
      ),
    },
    parent: {
      actor: "generatedActor0",
      object: getOptimizedOutGrip(),
      scopeKind: "",
      type: "object",
    },
  };
}

function hasValidIdent(range, pos) {
  return (
    range.type === "match" ||
    // For declarations, we allow the range on the identifier to be a
    // more general "contains" to increase the chances of a match.
    (pos.type !== "ref" && range.type === "contains")
  );
}

// eslint-disable-next-line complexity
async function findGeneratedBinding(
  source,
  content,
  name,
  originalBinding,
  originalRanges,
  generatedAstBindings,
  thunkArgs
) {
  // If there are no references to the implicits, then we have no way to
  // even attempt to map it back to the original since there is no location
  // data to use. Bail out instead of just showing it as unmapped.
  if (
    originalBinding.type === "implicit" &&
    !originalBinding.refs.some(item => item.type === "ref")
  ) {
    return null;
  }

  const loadApplicableBindings = async (pos, locationType) => {
    let applicableBindings = await getApplicableBindingsForOriginalPosition(
      generatedAstBindings,
      source,
      pos,
      originalBinding.type,
      locationType,
      thunkArgs
    );
    if (applicableBindings.length) {
      hadApplicableBindings = true;
    }
    if (locationType === "ref") {
      // Some tooling creates ranges that map a line as a whole, which is useful
      // for step-debugging, but can easily lead to finding the wrong binding.
      // To avoid these false-positives, we entirely ignore bindings matched
      // by ranges that cover full lines.
      applicableBindings = applicableBindings.filter(
        ({ range }) =>
          !(range.start.column === 0 && range.end.column === Infinity)
      );
    }
    if (
      locationType !== "ref" &&
      !(await originalRangeStartsInside(pos, thunkArgs))
    ) {
      applicableBindings = [];
    }
    return applicableBindings;
  };

  const { refs } = originalBinding;

  let hadApplicableBindings = false;
  let genContent = null;
  for (const pos of refs) {
    const applicableBindings = await loadApplicableBindings(pos, pos.type);

    const range = findMatchingRange(originalRanges, pos);
    if (range && hasValidIdent(range, pos)) {
      if (originalBinding.type === "import") {
        genContent = await findGeneratedImportReference(applicableBindings);
      } else {
        genContent = await findGeneratedReference(applicableBindings);
      }
    }

    if (
      (pos.type === "class-decl" || pos.type === "class-inner") &&
      content.contentType &&
      content.contentType.match(/\/typescript/)
    ) {
      const declRange = findMatchingRange(originalRanges, pos.declaration);
      if (declRange && declRange.type !== "multiple") {
        const applicableDeclBindings = await loadApplicableBindings(
          pos.declaration,
          pos.type
        );

        // Resolve to first binding in the range
        const declContent = await findGeneratedReference(
          applicableDeclBindings
        );

        if (declContent) {
          // Prefer the declaration mapping in this case because TS sometimes
          // maps class declaration names to "export.Foo = Foo;" or to
          // the decorator logic itself
          genContent = declContent;
        }
      }
    }

    if (
      !genContent &&
      pos.type === "import-decl" &&
      typeof pos.importName === "string"
    ) {
      const { importName } = pos;
      const declRange = findMatchingRange(originalRanges, pos.declaration);

      // The import declaration should have an original position mapping,
      // but otherwise we don't really have preferences on the range type
      // because it can have multiple bindings, but we do want to make sure
      // that all of the bindings that match the range are part of the same
      // import declaration.
      if (declRange?.singleDeclaration) {
        const applicableDeclBindings = await loadApplicableBindings(
          pos.declaration,
          pos.type
        );

        // match the import declaration location
        genContent = await findGeneratedImportDeclaration(
          applicableDeclBindings,
          importName
        );
      }
    }

    if (genContent) {
      break;
    }
  }

  if (genContent && genContent.desc) {
    return {
      grip: genContent.desc,
      expression: genContent.expression,
    };
  } else if (genContent) {
    // If there is no descriptor for 'this', then this is not the top-level
    // 'this' that the server gave us a binding for, and we can just ignore it.
    if (name === "this") {
      return null;
    }

    // If the location is found but the descriptor is not, then it
    // means that the server scope information didn't match the scope
    // information from the DevTools parsed scopes.
    return {
      grip: {
        configurable: false,
        enumerable: true,
        writable: false,
        value: {
          type: "unscoped",
          unscoped: true,

          // HACK: Until support for "unscoped" lands in devtools-reps,
          // this will make these show as (unavailable).
          missingArguments: true,
        },
      },
      expression: null,
    };
  } else if (!hadApplicableBindings && name !== "this") {
    // If there were no applicable bindings to consider while searching for
    // matching bindings, then the source map for this file didn't make any
    // attempt to map the binding, and that most likely means that the
    // code was entirely emitted from the output code.
    return {
      grip: getOptimizedOutGrip(),
      expression: `
        (() => {
          throw new Error('"' + ${JSON.stringify(
            name
          )} + '" has been optimized out.');
        })()
      `,
    };
  }

  // If no location mapping is found, then the map is bad, or
  // the map is okay but it original location is inside
  // of some scope, but the generated location is outside, leading
  // us to search for bindings that don't technically exist.
  return {
    grip: {
      configurable: false,
      enumerable: true,
      writable: false,
      value: {
        type: "unmapped",
        unmapped: true,

        // HACK: Until support for "unmapped" lands in devtools-reps,
        // this will make these show as (unavailable).
        missingArguments: true,
      },
    },
    expression: null,
  };
}
