/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global intl_Collator: false, */


var collatorCache = new Record();

/* ES6 20121122 draft 15.5.4.21. */
function String_repeat(count) {
    // Steps 1-3.
    CheckObjectCoercible(this);
    var S = ToString(this);

    // Steps 4-5.
    var n = ToInteger(count);

    // Steps 6-7.
    if (n < 0)
        ThrowError(JSMSG_NEGATIVE_REPETITION_COUNT); // a RangeError

    if (!(n * S.length < (1 << 28)))
        ThrowError(JSMSG_RESULTING_STRING_TOO_LARGE); // a RangeError

    // Communicate |n|'s possible range to the compiler.
    n = n & ((1 << 28) - 1);

    // Steps 8-9.
    var T = "";
    for (;;) {
        if (n & 1)
            T += S;
        n >>= 1;
        if (n)
            S += S;
        else
            break;
    }
    return T;
}

#define STRING_ITERATOR_SLOT_ITERATED_OBJECT 0
#define STRING_ITERATOR_SLOT_NEXT_INDEX 1

// ES6 draft specification, section 21.1.3.27, version 2013-09-27.
function String_iterator() {
    CheckObjectCoercible(this);
    var S = ToString(this);
    var iterator = NewStringIterator();
    UnsafeSetReservedSlot(iterator, STRING_ITERATOR_SLOT_ITERATED_OBJECT, S);
    UnsafeSetReservedSlot(iterator, STRING_ITERATOR_SLOT_NEXT_INDEX, 0);
    return iterator;
}

function StringIteratorIdentity() {
    return this;
}

function StringIteratorNext() {
    // FIXME: Cross-compartment wrapper StringIterator objects should pass this test.  Bug 924059.
    if (!IsObject(this) || !IsStringIterator(this))
        ThrowError(JSMSG_INCOMPATIBLE_METHOD, "StringIterator", "next", ToString(this));

    var S = UnsafeGetReservedSlot(this, STRING_ITERATOR_SLOT_ITERATED_OBJECT);
    var index = UnsafeGetReservedSlot(this, STRING_ITERATOR_SLOT_NEXT_INDEX);
    var size = S.length;

    if (index >= size) {
        return { value: undefined, done: true };
    }

    var charCount = 1;
    var first = callFunction(std_String_charCodeAt, S, index);
    if (first >= 0xD800 && first <= 0xDBFF && index + 1 < size) {
        var second = callFunction(std_String_charCodeAt, S, index + 1);
        if (second >= 0xDC00 && second <= 0xDFFF) {
            charCount = 2;
        }
    }

    UnsafeSetReservedSlot(this, STRING_ITERATOR_SLOT_NEXT_INDEX, index + charCount);
    var value = callFunction(std_String_substring, S, index, index + charCount);

    return { value: value, done: false };
}

/**
 * Compare this String against that String, using the locale and collation
 * options provided.
 *
 * Spec: ECMAScript Internationalization API Specification, 13.1.1.
 */
function String_localeCompare(that) {
    // Steps 1-3.
    CheckObjectCoercible(this);
    var S = ToString(this);
    var That = ToString(that);

    // Steps 4-5.
    var locales = arguments.length > 1 ? arguments[1] : undefined;
    var options = arguments.length > 2 ? arguments[2] : undefined;

    // Step 6.
    var collator;
    if (locales === undefined && options === undefined) {
        // This cache only optimizes for the old ES5 localeCompare without
        // locales and options.
        if (collatorCache.collator === undefined)
            collatorCache.collator = intl_Collator(locales, options);
        collator = collatorCache.collator;
    } else {
        collator = intl_Collator(locales, options);
    }

    // Step 7.
    return intl_CompareStrings(collator, S, That);
}

/**
 * Compare String str1 against String str2, using the locale and collation
 * options provided.
 *
 * Mozilla proprietary.
 * Spec: https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String#String_generic_methods
 */
function String_static_localeCompare(str1, str2) {
    if (arguments.length < 1)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, "String.localeCompare");
    var locales = arguments.length > 2 ? arguments[2] : undefined;
    var options = arguments.length > 3 ? arguments[3] : undefined;
    return callFunction(String_localeCompare, str1, str2, locales, options);
}
