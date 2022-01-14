/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// VarAndBindingsPair actually is [name: string, contents: BindingContents]

// Scope's bindings field which holds variables and arguments

// Create the tree nodes representing all the variables and arguments
// for the bindings from a scope.
export function getBindingVariables(bindings, parentName) {
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
