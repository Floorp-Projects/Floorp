/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { clientCommands } from "../../../client/firefox";

import { locColumn } from "./locColumn";
import { getOptimizedOutGrip } from "./optimizedOut";

export function buildGeneratedBindingList(
  scopes,
  generatedAstScopes,
  thisBinding
) {
  // The server's binding data doesn't include general 'this' binding
  // information, so we manually inject the one 'this' binding we have into
  // the normal binding data we are working with.
  const frameThisOwner = generatedAstScopes.find(
    generated => "this" in generated.bindings
  );

  let globalScope = null;
  const clientScopes = [];
  for (let s = scopes; s; s = s.parent) {
    const bindings = s.bindings
      ? Object.assign({}, ...s.bindings.arguments, s.bindings.variables)
      : {};

    clientScopes.push(bindings);
    globalScope = s;
  }

  const generatedMainScopes = generatedAstScopes.slice(0, -2);
  const generatedGlobalScopes = generatedAstScopes.slice(-2);

  const clientMainScopes = clientScopes.slice(0, generatedMainScopes.length);
  const clientGlobalScopes = clientScopes.slice(generatedMainScopes.length);

  // Map the main parsed script body using the nesting hierarchy of the
  // generated and client scopes.
  const generatedBindings = generatedMainScopes.reduce((acc, generated, i) => {
    const bindings = clientMainScopes[i];

    if (generated === frameThisOwner && thisBinding) {
      bindings.this = {
        value: thisBinding,
      };
    }

    for (const name of Object.keys(generated.bindings)) {
      // If there is no 'this' value, we exclude the binding entirely.
      // Otherwise it would pass through as found, but "(unscoped)", causing
      // the search logic to stop with a match.
      if (name === "this" && !bindings[name]) {
        continue;
      }

      const { refs } = generated.bindings[name];
      for (const loc of refs) {
        acc.push({
          name,
          loc,
          desc: () => Promise.resolve(bindings[name] || null),
        });
      }
    }
    return acc;
  }, []);

  // Bindings in the global/lexical global of the generated code may or
  // may not be the real global if the generated code is running inside
  // of an evaled context. To handle this, we just look up the client scope
  // hierarchy to find the closest binding with that name.
  for (const generated of generatedGlobalScopes) {
    for (const name of Object.keys(generated.bindings)) {
      const { refs } = generated.bindings[name];
      const bindings = clientGlobalScopes.find(b => name in b);

      for (const loc of refs) {
        if (bindings) {
          generatedBindings.push({
            name,
            loc,
            desc: () => Promise.resolve(bindings[name]),
          });
        } else {
          const globalGrip = globalScope?.object;
          if (globalGrip) {
            // Should always exist, just checking to keep Flow happy.

            generatedBindings.push({
              name,
              loc,
              desc: async () => {
                const objectFront = clientCommands.createObjectFront(
                  globalGrip
                );
                return (await objectFront.getProperty(name)).descriptor;
              },
            });
          }
        }
      }
    }
  }

  // Sort so we can binary-search.
  return sortBindings(generatedBindings);
}

export function buildFakeBindingList(generatedAstScopes) {
  // TODO if possible, inject real bindings for the global scope
  const generatedBindings = generatedAstScopes.reduce((acc, generated) => {
    for (const name of Object.keys(generated.bindings)) {
      if (name === "this") {
        continue;
      }
      const { refs } = generated.bindings[name];
      for (const loc of refs) {
        acc.push({
          name,
          loc,
          desc: () => Promise.resolve(getOptimizedOutGrip()),
        });
      }
    }
    return acc;
  }, []);
  return sortBindings(generatedBindings);
}

function sortBindings(generatedBindings) {
  return generatedBindings.sort((a, b) => {
    const aStart = a.loc.start;
    const bStart = b.loc.start;

    if (aStart.line === bStart.line) {
      return locColumn(aStart) - locColumn(bStart);
    }
    return aStart.line - bStart.line;
  });
}
