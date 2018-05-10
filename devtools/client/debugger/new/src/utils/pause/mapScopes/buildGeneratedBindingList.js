"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.buildGeneratedBindingList = buildGeneratedBindingList;

var _lodash = require("devtools/client/shared/vendor/lodash");

var _firefox = require("../../../client/firefox");

var _locColumn = require("./locColumn");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function buildGeneratedBindingList(scopes, generatedAstScopes, thisBinding) {
  // The server's binding data doesn't include general 'this' binding
  // information, so we manually inject the one 'this' binding we have into
  // the normal binding data we are working with.
  const frameThisOwner = generatedAstScopes.find(generated => "this" in generated.bindings);
  let globalScope = null;
  const clientScopes = [];

  for (let s = scopes; s; s = s.parent) {
    const bindings = s.bindings ? Object.assign({}, ...s.bindings.arguments, s.bindings.variables) : {};
    clientScopes.push(bindings);
    globalScope = s;
  }

  const generatedMainScopes = generatedAstScopes.slice(0, -2);
  const generatedGlobalScopes = generatedAstScopes.slice(-2);
  const clientMainScopes = clientScopes.slice(0, generatedMainScopes.length);
  const clientGlobalScopes = clientScopes.slice(generatedMainScopes.length); // Map the main parsed script body using the nesting hierarchy of the
  // generated and client scopes.

  const generatedBindings = generatedMainScopes.reduce((acc, generated, i) => {
    const bindings = clientMainScopes[i];

    if (generated === frameThisOwner && thisBinding) {
      bindings.this = {
        value: thisBinding
      };
    }

    for (const name of Object.keys(generated.bindings)) {
      // If there is no 'this' value, we exclude the binding entirely.
      // Otherwise it would pass through as found, but "(unscoped)", causing
      // the search logic to stop with a match.
      if (name === "this" && !bindings[name]) {
        continue;
      }

      const {
        refs
      } = generated.bindings[name];

      for (const loc of refs) {
        acc.push({
          name,
          loc,
          desc: () => Promise.resolve(bindings[name] || null)
        });
      }
    }

    return acc;
  }, []); // Bindings in the global/lexical global of the generated code may or
  // may not be the real global if the generated code is running inside
  // of an evaled context. To handle this, we just look up the client scope
  // hierarchy to find the closest binding with that name.

  for (const generated of generatedGlobalScopes) {
    for (const name of Object.keys(generated.bindings)) {
      const {
        refs
      } = generated.bindings[name];
      const bindings = clientGlobalScopes.find(b => (0, _lodash.has)(b, name));

      for (const loc of refs) {
        if (bindings) {
          generatedBindings.push({
            name,
            loc,
            desc: () => Promise.resolve(bindings[name])
          });
        } else {
          const globalGrip = globalScope && globalScope.object;

          if (globalGrip) {
            // Should always exist, just checking to keep Flow happy.
            generatedBindings.push({
              name,
              loc,
              desc: async () => {
                const objectClient = (0, _firefox.createObjectClient)(globalGrip);
                return (await objectClient.getProperty(name)).descriptor;
              }
            });
          }
        }
      }
    }
  } // Sort so we can binary-search.


  return generatedBindings.sort((a, b) => {
    const aStart = a.loc.start;
    const bStart = b.loc.start;

    if (aStart.line === bStart.line) {
      return (0, _locColumn.locColumn)(aStart) - (0, _locColumn.locColumn)(bStart);
    }

    return aStart.line - bStart.line;
  });
}