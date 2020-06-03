/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function IteratorIdentity() {
  return this;
}

/* Iterator Helpers proposal 1.1.1 */
function GetIteratorDirect(obj) {
  // Step 1.
  if (!IsObject(obj))
    ThrowTypeError(JSMSG_OBJECT_REQUIRED, DecompileArg(0, obj));

  // Step 2.
  const nextMethod = obj.next;
  // Step 3.
  if (!IsCallable(nextMethod))
    ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, nextMethod));

  // Steps 4-5.
  return {
    iterator: obj,
    nextMethod,
    done: false,
  };
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
    if (iteratorRecord.iterator instanceof GetBuiltinConstructor("Iterator"))
      return iteratorRecord.iterator;
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
    if (!IsObject(innerResult))
      ThrowTypeError(JSMSG_OBJECT_REQUIRED, DecompileArg(0, innerResult));
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
  if (!IsObject(this) || (O = GuardToWrapForValidIterator(this)) === null)
    ThrowTypeError(JSMSG_OBJECT_REQUIRED, DecompileArg(0, O));
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
