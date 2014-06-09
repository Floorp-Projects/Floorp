/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global intl_Collator: false, */

/* ES6 Draft September 5, 2013 21.1.3.3 */
function String_codePointAt(pos) {
    // Steps 1-3.
    CheckObjectCoercible(this);
    var S = ToString(this);

    // Steps 4-5.
    var position = ToInteger(pos);

    // Step 6.
    var size = S.length;

    // Step 7.
    if (position < 0 || position >= size)
        return undefined;

    // Steps 8-9.
    var first = callFunction(std_String_charCodeAt, S, position);
    if (first < 0xD800 || first > 0xDBFF || position + 1 === size)
        return first;

    // Steps 10-11.
    var second = callFunction(std_String_charCodeAt, S, position + 1);
    if (second < 0xDC00 || second > 0xDFFF)
        return first;

    // Step 12.
    return (first - 0xD800) * 0x400 + (second - 0xDC00) + 0x10000;
}

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

/* ES6 Draft September 5, 2013 21.1.2.2 */
function String_static_fromCodePoint() {
    // Step 1. is not relevant
    // Step 2.
    var length = arguments.length;

    // Step 3.
    var elements = new List();

    // Step 4-5., 5g.
    for (var nextIndex = 0; nextIndex < length; nextIndex++) {
        // Step 5a.
        var next = arguments[nextIndex];
        // Step 5b-c.
        var nextCP = ToNumber(next);

        // Step 5d.
        if (nextCP !== ToInteger(nextCP) || std_isNaN(nextCP))
            ThrowError(JSMSG_NOT_A_CODEPOINT, ToString(nextCP));

        // Step 5e.
        if (nextCP < 0 || nextCP > 0x10FFFF)
            ThrowError(JSMSG_NOT_A_CODEPOINT, ToString(nextCP));

        // Step 5f.
        // Inlined UTF-16 Encoding
        if (nextCP <= 0xFFFF) {
            elements.push(nextCP);
            continue;
        }

        elements.push((((nextCP - 0x10000) / 0x400) | 0) + 0xD800);
        elements.push((nextCP - 0x10000) % 0x400 + 0xDC00);
    }

    // Step 6.
    return callFunction(std_Function_apply, std_String_fromCharCode, null, elements);
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

// ES6 draft 2014-04-27 B.2.3.3
function String_big() {
    CheckObjectCoercible(this);
    return "<big>" + ToString(this) + "</big>";
}

// ES6 draft 2014-04-27 B.2.3.4
function String_blink() {
    CheckObjectCoercible(this);
    return "<blink>" + ToString(this) + "</blink>";
}

// ES6 draft 2014-04-27 B.2.3.5
function String_bold() {
    CheckObjectCoercible(this);
    return "<b>" + ToString(this) + "</b>";
}

// ES6 draft 2014-04-27 B.2.3.6
function String_fixed() {
    CheckObjectCoercible(this);
    return "<tt>" + ToString(this) + "</tt>";
}

// ES6 draft 2014-04-27 B.2.3.9
function String_italics() {
    CheckObjectCoercible(this);
    return "<i>" + ToString(this) + "</i>";
}

// ES6 draft 2014-04-27 B.2.3.11
function String_small() {
    CheckObjectCoercible(this);
    return "<small>" + ToString(this) + "</small>";
}

// ES6 draft 2014-04-27 B.2.3.12
function String_strike() {
    CheckObjectCoercible(this);
    return "<strike>" + ToString(this) + "</strike>";
}

// ES6 draft 2014-04-27 B.2.3.13
function String_sub() {
    CheckObjectCoercible(this);
    return "<sub>" + ToString(this) + "</sub>";
}

// ES6 draft 2014-04-27 B.2.3.14
function String_sup() {
    CheckObjectCoercible(this);
    return "<sup>" + ToString(this) + "</sup>";
}

function EscapeAttributeValue(v) {
    var inputStr = ToString(v);
    var inputLen = inputStr.length;
    var outputStr = "";
    var chunkStart = 0;
    for (var i = 0; i < inputLen; i++) {
        if (inputStr[i] === '"') {
            outputStr += callFunction(std_String_substring, inputStr, chunkStart, i) + '&quot;';
            chunkStart = i + 1;
        }
    }
    if (chunkStart === 0)
        return inputStr;
    if (chunkStart < inputLen)
        outputStr += callFunction(std_String_substring, inputStr, chunkStart);
    return outputStr;
}

// ES6 draft 2014-04-27 B.2.3.2
function String_anchor(name) {
    CheckObjectCoercible(this);
    var S = ToString(this);
    return '<a name="' + EscapeAttributeValue(name) + '">' + S + "</a>";
}

// ES6 draft 2014-04-27 B.2.3.7
function String_fontcolor(color) {
    CheckObjectCoercible(this);
    var S = ToString(this);
    return '<font color="' + EscapeAttributeValue(color) + '">' + S + "</font>";
}

// ES6 draft 2014-04-27 B.2.3.8
function String_fontsize(size) {
    CheckObjectCoercible(this);
    var S = ToString(this);
    return '<font size="' + EscapeAttributeValue(size) + '">' + S + "</font>";
}

// ES6 draft 2014-04-27 B.2.3.10
function String_link(url) {
    CheckObjectCoercible(this);
    var S = ToString(this);
    return '<a href="' + EscapeAttributeValue(url) + '">' + S + "</a>";
}
