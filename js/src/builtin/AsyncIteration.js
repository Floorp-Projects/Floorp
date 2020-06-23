/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function AsyncIteratorIdentity() {
    return this;
}

function AsyncGeneratorNext(val) {
    assert(IsAsyncGeneratorObject(this),
           "ThisArgument must be a generator object for async generators");
    return resumeGenerator(this, val, "next");
}

function AsyncGeneratorThrow(val) {
    assert(IsAsyncGeneratorObject(this),
           "ThisArgument must be a generator object for async generators");
    return resumeGenerator(this, val, "throw");
}

function AsyncGeneratorReturn(val) {
    assert(IsAsyncGeneratorObject(this),
           "ThisArgument must be a generator object for async generators");
    return resumeGenerator(this, val, "return");
}

/* Iterator Helpers proposal 1.1.1 */
function GetAsyncIteratorDirectWrapper(obj) {
  // Step 1.
  if (!IsObject(obj)) {
    ThrowTypeError(JSMSG_OBJECT_REQUIRED, obj);
  }

  // Step 2.
  const nextMethod = obj.next;
  // Step 3.
  if (!IsCallable(nextMethod)) {
    ThrowTypeError(JSMSG_NOT_FUNCTION, nextMethod);
  }

  // Steps 4-5.
  return {
    // Use a named function expression instead of a method definition, so
    // we don't create an inferred name for this function at runtime.
    [std_asyncIterator]: function AsyncIteratorMethod() {
      return this;
    },
    next(value) {
      return callContentFunction(nextMethod, obj, value);
    },
    async return(value) {
      const returnMethod = obj.return;
      if (returnMethod !== undefined && returnMethod !== null) {
        return callContentFunction(returnMethod, obj, value);
      }
      return {done: true, value};
    },
  };
}

/* Iterator Helpers proposal 2.1.6.8 */
async function AsyncIteratorReduce(reducer/*, initialValue*/) {
  // Step 1.
  const iterated = GetAsyncIteratorDirectWrapper(this);

  // Step 2.
  if (!IsCallable(reducer)) {
    ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, reducer));
  }

  // Step 3.
  let accumulator;
  if (arguments.length === 1) {
    // Step a.
    const next = await callContentFunction(iterated.next, iterated);
    if (!IsObject(next)) {
      ThrowTypeError(JSMSG_OBJECT_REQUIRED, DecompileArg(0, next));
    }
    // Step b.
    if (next.done) {
      ThrowTypeError(JSMSG_EMPTY_ITERATOR_REDUCE);
    }
    // Step c.
    accumulator = next.value;
  } else {
    // Step 4.
    accumulator = arguments[1];
  }

  // Step 5.
  for await (const value of allowContentIter(iterated)) {
    // Steps d-h.
    accumulator = await callContentFunction(reducer, undefined, accumulator, value);
  }
  // Step 5b.
  return accumulator;
}

/* Iterator Helpers proposal 2.1.6.9 */
async function AsyncIteratorToArray() {
  // Step 1.
  const iterated = {[std_asyncIterator]: () => this};
  // Step 2.
  const items = [];
  let index = 0;
  // Step 3.
  for await (const value of allowContentIter(iterated)) {
    // Step d.
    _DefineDataProperty(items, index++, value);
  }
  // Step 3b.
  return items;
}

/* Iterator Helpers proposal 2.1.6.10 */
async function AsyncIteratorForEach(fn) {
  // Step 1.
  const iterated = GetAsyncIteratorDirectWrapper(this);

  // Step 2.
  if (!IsCallable(fn)) {
    ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, fn));
  }

  // Step 3.
  for await (const value of allowContentIter(iterated)) {
    // Steps d-g.
    await callContentFunction(fn, undefined, value);
  }
}

/* Iterator Helpers proposal 2.1.6.11 */
async function AsyncIteratorSome(fn) {
  // Step 1.
  const iterated = GetAsyncIteratorDirectWrapper(this);

  // Step 2.
  if (!IsCallable(fn)) {
    ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, fn));
  }

  // Step 3.
  for await (const value of allowContentIter(iterated)) {
    // Steps d-h.
    if (await callContentFunction(fn, undefined, value)) {
      return true;
    }
  }
  // Step 3b.
  return false;
}

/* Iterator Helpers proposal 2.1.6.12 */
async function AsyncIteratorEvery(fn) {
  // Step 1.
  const iterated = GetAsyncIteratorDirectWrapper(this);

  // Step 2.
  if (!IsCallable(fn)) {
    ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, fn));
  }

  // Step 3.
  for await (const value of allowContentIter(iterated)) {
    // Steps d-h.
    if (!await callContentFunction(fn, undefined, value)) {
      return false;
    }
  }
  // Step 3b.
  return true;
}

/* Iterator Helpers proposal 2.1.6.13 */
async function AsyncIteratorFind(fn) {
  // Step 1.
  const iterated = GetAsyncIteratorDirectWrapper(this);

  // Step 2.
  if (!IsCallable(fn)) {
    ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, fn));
  }

  // Step 3.
  for await (const value of allowContentIter(iterated)) {
    // Steps d-h.
    if (await callContentFunction(fn, undefined, value)) {
      return value;
    }
  }
}
