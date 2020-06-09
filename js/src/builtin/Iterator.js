/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function IteratorIdentity() {
  return this;
}

/* ECMA262 7.4.6 */
function IteratorClose(iteratorRecord, value) {
  // Step 3.
  const iterator = iteratorRecord.iterator;
  // Step 4.
  const returnMethod = iterator.return;
  // Step 5.
  if (returnMethod !== undefined && returnMethod !== null) {
    const result = callContentFunction(returnMethod, iterator);
    // Step 8.
    if (!IsObject(result)) {
      ThrowTypeError(JSMSG_OBJECT_REQUIRED, DecompileArg(0, result));
    }
  }
  // Step 5b & 9.
  return value;
}

/* Iterator Helpers proposal 1.1.1 */
function GetIteratorDirect(obj) {
  // Step 1.
  if (!IsObject(obj)) {
    ThrowTypeError(JSMSG_OBJECT_REQUIRED, DecompileArg(0, obj));
  }

  // Step 2.
  const nextMethod = obj.next;
  // Step 3.
  if (!IsCallable(nextMethod)) {
    ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, nextMethod));
  }

  // Steps 4-5.
  return {
    iterator: obj,
    nextMethod,
    done: false,
  };
}

function GetIteratorDirectWrapper(obj) {
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
    [std_iterator]: function IteratorMethod() {
      return this;
    },
    next(value) {
      return callContentFunction(nextMethod, obj, value);
    },
    return(value) {
      const returnMethod = obj.return;
      if (returnMethod !== undefined && returnMethod !== null) {
        return callContentFunction(returnMethod, obj, value);
      }
      return {done: true, value};
    },
  };
}

/* Iterator Helpers proposal 1.1.2 */
function IteratorStep(iteratorRecord, value) {
  // Steps 2-3.
  let result;
  if (arguments.length === 2) {
    result = callContentFunction(
      iteratorRecord.nextMethod,
      iteratorRecord.iterator,
      value
    );
  } else {
    result = callContentFunction(
      iteratorRecord.nextMethod,
      iteratorRecord.iterator
    );
  }

  // IteratorNext Step 3.
  if (!IsObject(result)) {
    ThrowTypeError(JSMSG_OBJECT_REQUIRED, DecompileArg(0, result));
  }

  // Steps 4-6.
  return result.done ? false : result;
}

/* Iterator Helpers proposal 2.1.3.3.1 */
function IteratorFrom(O) {
  // Step 1.
  const usingIterator = O[std_iterator];

  let iteratorRecord;
  // Step 2.
  if (usingIterator !== undefined && usingIterator !== null) {
    // Step a.
    // Inline call to GetIterator.
    const iterator = callContentFunction(usingIterator, O);
    iteratorRecord = GetIteratorDirect(iterator);
    // Step b-c.
    if (iteratorRecord.iterator instanceof GetBuiltinConstructor("Iterator")) {
      return iteratorRecord.iterator;
    }
  } else {
    // Step 3.
    iteratorRecord = GetIteratorDirect(O);
  }

  // Step 4.
  const wrapper = NewWrapForValidIterator();
  // Step 5.
  UnsafeSetReservedSlot(wrapper, ITERATED_SLOT, iteratorRecord);
  // Step 6.
  return wrapper;
}

/* Iterator Helpers proposal 2.1.3.3.1.1.1 */
function WrapForValidIteratorNext(value) {
  // Step 1-2.
  let O;
  if (!IsObject(this) || (O = GuardToWrapForValidIterator(this)) === null)
    ThrowTypeError(JSMSG_OBJECT_REQUIRED, DecompileArg(0, O));
  const iterated = UnsafeGetReservedSlot(O, ITERATED_SLOT);
  // Step 3.
  let result;
  if (arguments.length === 0) {
    result = callContentFunction(iterated.nextMethod, iterated.iterator);
  } else { // Step 4.
    result = callContentFunction(iterated.nextMethod, iterated.iterator, value);
  }
  // Inlined from IteratorNext.
  if (!IsObject(result))
    ThrowTypeError(JSMSG_OBJECT_REQUIRED, DecompileArg(0, result));
  return result;
}

/* Iterator Helpers proposal 2.1.3.3.1.1.2 */
function WrapForValidIteratorReturn(value) {
  // Step 1-2.
  let O;
  if (!IsObject(this) || (O = GuardToWrapForValidIterator(this)) === null)
    ThrowTypeError(JSMSG_OBJECT_REQUIRED, DecompileArg(0, O));
  const iterated = UnsafeGetReservedSlot(O, ITERATED_SLOT);

  // Step 3.
  // Inline call to IteratorClose.
  const iterator = iterated.iterator;
  const returnMethod = iterator.return;
  if (returnMethod !== undefined && returnMethod !== null) {
    let innerResult = callContentFunction(returnMethod, iterator);
    if (!IsObject(innerResult)) {
      ThrowTypeError(JSMSG_OBJECT_REQUIRED, DecompileArg(0, innerResult));
    }
  }
  // Step 4.
  return {
    done: true,
    value,
  };
}

/* Iterator Helpers proposal 2.1.3.3.1.1.3 */
function WrapForValidIteratorThrow(value) {
  // Step 1-2.
  let O;
  if (!IsObject(this) || (O = GuardToWrapForValidIterator(this)) === null) {
    ThrowTypeError(JSMSG_OBJECT_REQUIRED, DecompileArg(0, O));
  }
  const iterated = UnsafeGetReservedSlot(O, ITERATED_SLOT);
  // Step 3.
  const iterator = iterated.iterator;
  // Step 4.
  const throwMethod = iterator.throw;
  // Step 5.
  if (throwMethod === undefined || throwMethod === null) {
    throw value;
  }
  // Step 6.
  return callContentFunction(throwMethod, iterator, value);
}

/* Iterator Helpers proposal 2.1.5.8 */
function IteratorReduce(reducer/*, initialValue*/) {
  // Step 1.
  const iterated = GetIteratorDirectWrapper(this);

  // Step 2.
  if (!IsCallable(reducer)) {
    ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, reducer));
  }

  // Step 3.
  let accumulator;
  if (arguments.length === 1) {
    // Step a.
    const next = callContentFunction(iterated.next, iterated);
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
  for (const value of allowContentIter(iterated)) {
    accumulator = callContentFunction(reducer, undefined, accumulator, value);
  }
  return accumulator;
}

/* Iterator Helpers proposal 2.1.5.9 */
function IteratorToArray() {
  // Step 1.
  const iterated = {[std_iterator]: () => this};
  // Steps 2-3.
  return [...allowContentIter(iterated)];
}

/* Iterator Helpers proposal 2.1.5.10 */
function IteratorForEach(fn) {
  // Step 1.
  const iterated = GetIteratorDirectWrapper(this);

  // Step 2.
  if (!IsCallable(fn)) {
    ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, fn));
  }

  // Step 3.
  for (const value of allowContentIter(iterated)) {
    callContentFunction(fn, undefined, value);
  }
}

/* Iterator Helpers proposal 2.1.5.11 */
function IteratorSome(fn) {
  // Step 1.
  const iterated = GetIteratorDirectWrapper(this);

  // Step 2.
  if (!IsCallable(fn)) {
    ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, fn));
  }

  // Step 3.
  for (const value of allowContentIter(iterated)) {
    // Steps d-f.
    if (callContentFunction(fn, undefined, value)) {
      return true;
    }
  }
  // Step 3b.
  return false;
}

/* Iterator Helpers proposal 2.1.5.12 */
function IteratorEvery(fn) {
  // Step 1.
  const iterated = GetIteratorDirectWrapper(this);

  // Step 2.
  if (!IsCallable(fn)) {
    ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, fn));
  }

  // Step 3.
  for (const value of allowContentIter(iterated)) {
    // Steps d-f.
    if (!callContentFunction(fn, undefined, value)) {
      return false;
    }
  }
  // Step 3b.
  return true;
}

/* Iterator Helpers proposal 2.1.5.13 */
function IteratorFind(fn) {
  // Step 1.
  const iterated = GetIteratorDirectWrapper(this);

  // Step 2.
  if (!IsCallable(fn)) {
    ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, fn));
  }

  // Step 3.
  for (const value of allowContentIter(iterated)) {
    // Steps d-f.
    if (callContentFunction(fn, undefined, value)) {
      return value;
    }
  }
}
