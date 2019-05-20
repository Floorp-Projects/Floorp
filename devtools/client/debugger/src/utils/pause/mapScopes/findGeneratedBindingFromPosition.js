/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { locColumn } from "./locColumn";
import { mappingContains } from "./mappingContains";

import type { BindingContents } from "../../../types";
// eslint-disable-next-line max-len
import type { ApplicableBinding } from "./getApplicableBindingsForOriginalPosition";

import { createObjectClient } from "../../../client/firefox";

export type GeneratedDescriptor = {
  name: string,
  // Falsy if the binding itself matched a location, but the location didn't
  // have a value descriptor attached. Happens if the binding was 'this'
  // or if there was a mismatch between client and generated scopes.
  desc: ?BindingContents,

  expression: string,
};

/**
 * Given a mapped range over the generated source, attempt to resolve a real
 * binding descriptor that can be used to access the value.
 */
export async function findGeneratedReference(
  applicableBindings: Array<ApplicableBinding>
): Promise<GeneratedDescriptor | null> {
  // We can adjust this number as we go, but these are a decent start as a
  // general heuristic to assume the bindings were bad or just map a chunk of
  // whole line or something.
  if (applicableBindings.length > 4) {
    // Babel's for..of generates at least 3 bindings inside one range for
    // block-scoped loop variables, so we shouldn't go below that.
    applicableBindings = [];
  }

  for (const applicable of applicableBindings) {
    const result = await mapBindingReferenceToDescriptor(applicable);
    if (result) {
      return result;
    }
  }
  return null;
}

export async function findGeneratedImportReference(
  applicableBindings: Array<ApplicableBinding>
): Promise<GeneratedDescriptor | null> {
  // When wrapped, for instance as `Object(ns.default)`, the `Object` binding
  // will be the first in the list. To avoid resolving `Object` as the
  // value of the import itself, we potentially skip the first binding.
  applicableBindings = applicableBindings.filter((applicable, i) => {
    if (
      !applicable.firstInRange ||
      applicable.binding.loc.type !== "ref" ||
      applicable.binding.loc.meta
    ) {
      return true;
    }

    const next =
      i + 1 < applicableBindings.length ? applicableBindings[i + 1] : null;

    return !next || next.binding.loc.type !== "ref" || !next.binding.loc.meta;
  });

  // We can adjust this number as we go, but these are a decent start as a
  // general heuristic to assume the bindings were bad or just map a chunk of
  // whole line or something.
  if (applicableBindings.length > 2) {
    // Babel's for..of generates at least 3 bindings inside one range for
    // block-scoped loop variables, so we shouldn't go below that.
    applicableBindings = [];
  }

  for (const applicable of applicableBindings) {
    const result = await mapImportReferenceToDescriptor(applicable);
    if (result) {
      return result;
    }
  }

  return null;
}

/**
 * Given a mapped range over the generated source and the name of the imported
 * value that is referenced, attempt to resolve a binding descriptor for
 * the import's value.
 */
export async function findGeneratedImportDeclaration(
  applicableBindings: Array<ApplicableBinding>,
  importName: string
): Promise<GeneratedDescriptor | null> {
  // We can adjust this number as we go, but these are a decent start as a
  // general heuristic to assume the bindings were bad or just map a chunk of
  // whole line or something.
  if (applicableBindings.length > 10) {
    // Import declarations tend to have a large number of bindings for
    // for things like 'require' and 'interop', so this number is larger
    // than other binding count checks.
    applicableBindings = [];
  }

  let result = null;

  for (const { binding } of applicableBindings) {
    if (binding.loc.type === "ref") {
      continue;
    }

    const namespaceDesc = await binding.desc();
    if (isPrimitiveValue(namespaceDesc)) {
      continue;
    }
    if (!isObjectValue(namespaceDesc)) {
      // We want to handle cases like
      //
      //   var _mod = require(...);
      //   var _mod2 = _interopRequire(_mod);
      //
      // where "_mod" is optimized out because it is only referenced once. To
      // allow that, we track the optimized-out value as a possible result,
      // but allow later binding values to overwrite the result.
      result = {
        name: binding.name,
        desc: namespaceDesc,
        expression: binding.name,
      };
      continue;
    }

    const desc = await readDescriptorProperty(namespaceDesc, importName);
    const expression = `${binding.name}.${importName}`;

    if (desc) {
      result = {
        name: binding.name,
        desc,
        expression,
      };
      break;
    }
  }

  return result;
}

/**
 * Given a generated binding, and a range over the generated code, statically
 * check if the given binding matches the range.
 */
async function mapBindingReferenceToDescriptor({
  binding,
  range,
  firstInRange,
  firstOnLine,
}: ApplicableBinding): Promise<GeneratedDescriptor | null> {
  // Allow the mapping to point anywhere within the generated binding
  // location to allow for less than perfect sourcemaps. Since you also
  // need at least one character between identifiers, we also give one
  // characters of space at the front the generated binding in order
  // to increase the probability of finding the right mapping.
  if (
    range.start.line === binding.loc.start.line &&
    // If a binding is the first on a line, Babel will extend the mapping to
    // include the whitespace between the newline and the binding. To handle
    // that, we skip the range requirement for starting location.
    (firstInRange ||
      firstOnLine ||
      locColumn(range.start) >= locColumn(binding.loc.start)) &&
    locColumn(range.start) <= locColumn(binding.loc.end)
  ) {
    return {
      name: binding.name,
      desc: await binding.desc(),
      expression: binding.name,
    };
  }

  return null;
}

/**
 * Given an generated binding, and a range over the generated code, statically
 * evaluate accessed properties within the mapped range to resolve the actual
 * imported value.
 */
async function mapImportReferenceToDescriptor({
  binding,
  range,
}: ApplicableBinding): Promise<GeneratedDescriptor | null> {
  if (binding.loc.type !== "ref") {
    return null;
  }

  // Expression matches require broader searching because sourcemaps usage
  // varies in how they map certain things. For instance given
  //
  //   import { bar } from "mod";
  //   bar();
  //
  // The "bar()" expression is generally expanded into one of two possibly
  // forms, both of which map the "bar" identifier in different ways. See
  // the "^^" markers below for the ranges.
  //
  //   (0, foo.bar)()    // Babel
  //       ^^^^^^^       // mapping
  //       ^^^           // binding
  // vs
  //
  //   __webpack_require__.i(foo.bar)() // Webpack 2
  //   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^   // mapping
  //                         ^^^        // binding
  // vs
  //
  //   Object(foo.bar)() // Webpack >= 3
  //   ^^^^^^^^^^^^^^^   // mapping
  //          ^^^        // binding
  //
  // Unfortunately, Webpack also has a tendancy to over-map past the call
  // expression to the start of the next line, at least when there isn't
  // anything else on that line that is mapped, e.g.
  //
  //   Object(foo.bar)()
  //   ^^^^^^^^^^^^^^^^^
  //   ^                 // wrapped to column 0 of next line

  if (!mappingContains(range, binding.loc)) {
    return null;
  }

  // Webpack 2's import declarations wrap calls with an identity fn, so we
  // need to make sure to skip that binding because it is mapped to the
  // location of the original binding usage.
  if (
    binding.name === "__webpack_require__" &&
    binding.loc.meta &&
    binding.loc.meta.type === "member" &&
    binding.loc.meta.property === "i"
  ) {
    return null;
  }

  let expression = binding.name;
  let desc = await binding.desc();

  if (binding.loc.type === "ref") {
    const { meta } = binding.loc;

    // Limit to 2 simple property or inherits operartions, since it would
    // just be more work to search more and it is very unlikely that
    // bindings would be mapped to more than a single member + inherits
    // wrapper.
    for (
      let op = meta, index = 0;
      op && mappingContains(range, op) && desc && index < 2;
      index++, op = op && op.parent
    ) {
      // Calling could potentially trigger side-effects, which would not
      // be ideal for this case.
      if (op.type === "call") {
        return null;
      }

      if (op.type === "inherit") {
        continue;
      }

      desc = await readDescriptorProperty(desc, op.property);
      expression += `.${op.property}`;
    }
  }

  return desc
    ? {
        name: binding.name,
        desc,
        expression,
      }
    : null;
}

function isPrimitiveValue(desc: ?BindingContents) {
  return desc && (!desc.value || typeof desc.value !== "object");
}
function isObjectValue(desc: ?BindingContents) {
  return (
    desc &&
    !isPrimitiveValue(desc) &&
    desc.value.type === "object" &&
    // Note: The check for `.type` might already cover the optimizedOut case
    // but not 100% sure, so just being cautious.
    !desc.value.optimizedOut
  );
}

async function readDescriptorProperty(
  desc: ?BindingContents,
  property: string
): Promise<?BindingContents> {
  if (!desc) {
    return null;
  }

  if (typeof desc.value !== "object" || !desc.value) {
    // If accessing a property on a primitive type, just return 'undefined'
    // as the value.
    return {
      value: {
        type: "undefined",
      },
    };
  }

  if (!isObjectValue(desc)) {
    // If we got a non-primitive descriptor but it isn't an object, then
    // it's definitely not the namespace and it is probably an error.
    return desc;
  }

  const objectClient = createObjectClient(desc.value);
  return (await objectClient.getProperty(property)).descriptor;
}
