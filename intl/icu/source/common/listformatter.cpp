/*
*******************************************************************************
*
*   Copyright (C) 2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  listformatter.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2012aug27
*   created by: Umesh P. Nair
*/

#include "unicode/listformatter.h"
#include "mutex.h"
#include "hash.h"
#include "cstring.h"
#include "ulocimp.h"
#include "charstr.h"
#include "ucln_cmn.h"

U_NAMESPACE_BEGIN

static Hashtable* listPatternHash = NULL;
static UMutex listFormatterMutex = U_MUTEX_INITIALIZER;
static UChar FIRST_PARAMETER[] = { 0x7b, 0x30, 0x7d };  // "{0}"
static UChar SECOND_PARAMETER[] = { 0x7b, 0x31, 0x7d };  // "{0}"

U_CDECL_BEGIN
static UBool U_CALLCONV uprv_listformatter_cleanup() {
    delete listPatternHash;
    listPatternHash = NULL;
    return TRUE;
}

static void U_CALLCONV
uprv_deleteListFormatData(void *obj) {
    delete static_cast<ListFormatData *>(obj);
}

U_CDECL_END

void ListFormatter::initializeHash(UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return;
    }

    listPatternHash = new Hashtable();
    if (listPatternHash == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    listPatternHash->setValueDeleter(uprv_deleteListFormatData);
    ucln_common_registerCleanup(UCLN_COMMON_LIST_FORMATTER, uprv_listformatter_cleanup);

    addDataToHash("af", "{0} en {1}", "{0}, {1}", "{0}, {1}", "{0} en {1}", errorCode);
    addDataToHash("am", "{0} \\u12a5\\u1293 {1}", "{0}, {1}", "{0}, {1}", "{0}, \\u12a5\\u1293 {1}", errorCode);
    addDataToHash("ar", "{0} \\u0648 {1}", "{0}\\u060c {1}", "{0}\\u060c {1}", "{0}\\u060c \\u0648 {1}", errorCode);
    addDataToHash("bg", "{0} \\u0438 {1}", "{0}, {1}", "{0}, {1}", "{0} \\u0438 {1}", errorCode);
    addDataToHash("bn", "{0} \\u098f\\u09ac\\u0982 {1}", "{0}, {1}", "{0}, {1}", "{0}, \\u098f\\u09ac\\u0982 {1}", errorCode);
    addDataToHash("bs", "{0} i {1}", "{0}, {1}", "{0}, {1}", "{0} i {1}", errorCode);
    addDataToHash("ca", "{0} i {1}", "{0}, {1}", "{0}, {1}", "{0} i {1}", errorCode);
    addDataToHash("cs", "{0} a {1}", "{0}, {1}", "{0}, {1}", "{0} a {1}", errorCode);
    addDataToHash("da", "{0} og {1}", "{0}, {1}", "{0}, {1}", "{0} og {1}", errorCode);
    addDataToHash("de", "{0} und {1}", "{0}, {1}", "{0}, {1}", "{0} und {1}", errorCode);
    addDataToHash("ee", "{0} kple {1}", "{0}, {1}", "{0}, {1}", "{0}, kple {1}", errorCode);
    addDataToHash("el", "{0} \\u03ba\\u03b1\\u03b9 {1}", "{0}, {1}", "{0}, {1}", "{0} \\u03ba\\u03b1\\u03b9 {1}", errorCode);
    addDataToHash("en", "{0} and {1}", "{0}, {1}", "{0}, {1}", "{0}, and {1}", errorCode);
    addDataToHash("es", "{0} y {1}", "{0}, {1}", "{0}, {1}", "{0} y {1}", errorCode);
    addDataToHash("et", "{0} ja {1}", "{0}, {1}", "{0}, {1}", "{0} ja {1}", errorCode);
    addDataToHash("eu", "{0} eta {1}", "{0}, {1}", "{0}, {1}", "{0} eta {1}", errorCode);
    addDataToHash("fa", "{0} \\u0648 {1}", "{0}\\u060c\\u200f {1}", "{0}\\u060c\\u200f {1}", "{0}\\u060c \\u0648 {1}", errorCode);
    addDataToHash("fi", "{0} ja {1}", "{0}, {1}", "{0}, {1}", "{0} ja {1}", errorCode);
    addDataToHash("fil", "{0} at {1}", "{0}, {1}", "{0}, {1}", "{0} at {1}", errorCode);
    addDataToHash("fo", "{0} og {1}", "{0}, {1}", "{0}, {1}", "{0} og {1}", errorCode);
    addDataToHash("fr", "{0} et {1}", "{0}, {1}", "{0}, {1}", "{0} et {1}", errorCode);
    addDataToHash("fur", "{0} e {1}", "{0}, {1}", "{0}, {1}", "{0} e {1}", errorCode);
    addDataToHash("gd", "{0} agus {1}", "{0}, {1}", "{0}, {1}", "{0}, agus {1}", errorCode);
    addDataToHash("gl", "{0} e {1}", "{0}, {1}", "{0}, {1}", "{0} e {1}", errorCode);
    addDataToHash("gsw", "{0} und {1}", "{0}, {1}", "{0}, {1}", "{0} und {1}", errorCode);
    addDataToHash("gu", "{0} \\u0a85\\u0aa8\\u0ac7 {1}", "{0}, {1}", "{0}, {1}", "{0} \\u0a85\\u0aa8\\u0ac7 {1}", errorCode);
    addDataToHash("he", "{0} \\u05d5-{1}", "{0}, {1}", "{0}, {1}", "{0} \\u05d5-{1}", errorCode);
    addDataToHash("hi", "{0} \\u0914\\u0930 {1}", "{0}, {1}", "{0}, {1}", "{0}, \\u0914\\u0930 {1}", errorCode);
    addDataToHash("hr", "{0} i {1}", "{0}, {1}", "{0}, {1}", "{0} i {1}", errorCode);
    addDataToHash("hu", "{0} \\u00e9s {1}", "{0}, {1}", "{0}, {1}", "{0} \\u00e9s {1}", errorCode);
    addDataToHash("id", "{0} dan {1}", "{0}, {1}", "{0}, {1}", "{0}, dan {1}", errorCode);
    addDataToHash("is", "{0} og {1}", "{0}, {1}", "{0}, {1}", "{0} og {1}", errorCode);
    addDataToHash("it", "{0} e {1}", "{0}, {1}", "{0}, {1}", "{0}, e {1}", errorCode);
    addDataToHash("ja", "{0}\\u3001{1}", "{0}\\u3001{1}", "{0}\\u3001{1}", "{0}\\u3001{1}", errorCode);
    addDataToHash("ka", "{0} \\u10d3\\u10d0 {1}", "{0}, {1}", "{0}, {1}", "{0} \\u10d3\\u10d0 {1}", errorCode);
    addDataToHash("kea", "{0} y {1}", "{0}, {1}", "{0}, {1}", "{0} y {1}", errorCode);
    addDataToHash("kl", "{0} aamma {1}", "{0} aamma {1}", "{0}, {1}", "{0}, {1}", errorCode);
    addDataToHash("kn", "{0} \\u0cae\\u0ca4\\u0ccd\\u0ca4\\u0cc1 {1}", "{0}, {1}", "{0}, {1}",
                  "{0}, \\u0cae\\u0ca4\\u0ccd\\u0ca4\\u0cc1 {1}", errorCode);
    addDataToHash("ko", "{0} \\ubc0f {1}", "{0}, {1}", "{0}, {1}", "{0} \\ubc0f {1}", errorCode);
    addDataToHash("ksh", "{0} un {1}", "{0}, {1}", "{0}, {1}", "{0} un {1}", errorCode);
    addDataToHash("lt", "{0} ir {1}", "{0}, {1}", "{0}, {1}", "{0} ir {1}", errorCode);
    addDataToHash("lv", "{0} un {1}", "{0}, {1}", "{0}, {1}", "{0} un {1}", errorCode);
    addDataToHash("ml", "{0} \\u0d15\\u0d42\\u0d1f\\u0d3e\\u0d24\\u0d46 {1}", "{0}, {1}", "{0}, {1}",
                  "{0}, {1} \\u0d0e\\u0d28\\u0d4d\\u0d28\\u0d3f\\u0d35", errorCode);
    addDataToHash("mr", "{0} \\u0906\\u0923\\u093f {1}", "{0}, {1}", "{0}, {1}", "{0} \\u0906\\u0923\\u093f {1}", errorCode);
    addDataToHash("ms", "{0} dan {1}", "{0}, {1}", "{0}, {1}", "{0}, dan {1}", errorCode);
    addDataToHash("nb", "{0} og {1}", "{0}, {1}", "{0}, {1}", "{0} og {1}", errorCode);
    addDataToHash("nl", "{0} en {1}", "{0}, {1}", "{0}, {1}", "{0} en {1}", errorCode);
    addDataToHash("nn", "{0} og {1}", "{0}, {1}", "{0}, {1}", "{0} og {1}", errorCode);
    addDataToHash("pl", "{0} i {1}", "{0}; {1}", "{0}; {1}", "{0} i {1}", errorCode);
    addDataToHash("pt", "{0} e {1}", "{0}, {1}", "{0}, {1}", "{0} e {1}", errorCode);
    addDataToHash("ro", "{0} \\u015fi {1}", "{0}, {1}", "{0}, {1}", "{0} \\u015fi {1}", errorCode);
    addDataToHash("", "{0}, {1}", "{0}, {1}", "{0}, {1}", "{0}, {1}", errorCode); // root
    addDataToHash("ru", "{0} \\u0438 {1}", "{0}, {1}", "{0}, {1}", "{0} \\u0438 {1}", errorCode);
    addDataToHash("se", "{0} ja {1}", "{0}, {1}", "{0}, {1}", "{0} ja {1}", errorCode);
    addDataToHash("sk", "{0} a {1}", "{0}, {1}", "{0}, {1}", "{0} a {1}", errorCode);
    addDataToHash("sl", "{0} in {1}", "{0}, {1}", "{0}, {1}", "{0} in {1}", errorCode);
    addDataToHash("sr", "{0} \\u0438 {1}", "{0}, {1}", "{0}, {1}", "{0} \\u0438 {1}", errorCode);
    addDataToHash("sr_Cyrl", "{0} \\u0438 {1}", "{0}, {1}", "{0}, {1}", "{0} \\u0438 {1}", errorCode);
    addDataToHash("sr_Latn", "{0} i {1}", "{0}, {1}", "{0}, {1}", "{0} i {1}", errorCode);
    addDataToHash("sv", "{0} och {1}", "{0}, {1}", "{0}, {1}", "{0} och {1}", errorCode);
    addDataToHash("sw", "{0} na {1}", "{0}, {1}", "{0}, {1}", "{0}, na {1}", errorCode);
    addDataToHash("ta", "{0} \\u0bae\\u0bb1\\u0bcd\\u0bb1\\u0bc1\\u0bae\\u0bcd {1}", "{0}, {1}", "{0}, {1}",
                  "{0} \\u0bae\\u0bb1\\u0bcd\\u0bb1\\u0bc1\\u0bae\\u0bcd {1}", errorCode);
    addDataToHash("te", "{0} \\u0c2e\\u0c30\\u0c3f\\u0c2f\\u0c41 {1}", "{0}, {1}", "{0}, {1}",
                  "{0} \\u0c2e\\u0c30\\u0c3f\\u0c2f\\u0c41 {1}", errorCode);
    addDataToHash("th", "{0}\\u0e41\\u0e25\\u0e30{1}", "{0} {1}", "{0} {1}", "{0} \\u0e41\\u0e25\\u0e30{1}", errorCode);
    addDataToHash("tr", "{0} ve {1}", "{0}, {1}", "{0}, {1}", "{0} ve {1}", errorCode);
    addDataToHash("uk", "{0} \\u0442\\u0430 {1}", "{0}, {1}", "{0}, {1}", "{0} \\u0442\\u0430 {1}", errorCode);
    addDataToHash("ur", "{0} \\u0627\\u0648\\u0631 {1}", "{0}\\u060c {1}", "{0}\\u060c {1}",
                  "{0}\\u060c \\u0627\\u0648\\u0631 {1}", errorCode);
    addDataToHash("vi", "{0} v\\u00e0 {1}", "{0}, {1}", "{0}, {1}", "{0} v\\u00e0 {1}", errorCode);
    addDataToHash("wae", "{0} und {1}", "{0}, {1}", "{0}, {1}", "{0} und {1}", errorCode);
    addDataToHash("zh", "{0}\\u548c{1}", "{0}\\u3001{1}", "{0}\\u3001{1}", "{0}\\u548c{1}", errorCode);
    addDataToHash("zu", "I-{0} ne-{1}", "{0}, {1}", "{0}, {1}", "{0}, no-{1}", errorCode);
}

void ListFormatter::addDataToHash(
    const char* locale,
    const char* two,
    const char* start,
    const char* middle,
    const char* end,
    UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return;
    }
    UnicodeString key(locale, -1, US_INV);
    ListFormatData* value = new ListFormatData(
        UnicodeString(two, -1, US_INV).unescape(),
        UnicodeString(start, -1, US_INV).unescape(),
        UnicodeString(middle, -1, US_INV).unescape(),
        UnicodeString(end, -1, US_INV).unescape());

    if (value == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    listPatternHash->put(key, value, errorCode);
}

const ListFormatData* ListFormatter::getListFormatData(
        const Locale& locale, UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return NULL;
    }
    {
        Mutex m(&listFormatterMutex);
        if (listPatternHash == NULL) {
            initializeHash(errorCode);
            if (U_FAILURE(errorCode)) {
                return NULL;
            }
        }
    }

    UnicodeString key(locale.getName(), -1, US_INV);
    return static_cast<const ListFormatData*>(listPatternHash->get(key));
}

ListFormatter* ListFormatter::createInstance(UErrorCode& errorCode) {
    Locale locale;  // The default locale.
    return createInstance(locale, errorCode);
}

ListFormatter* ListFormatter::createInstance(const Locale& locale, UErrorCode& errorCode) {
    Locale tempLocale = locale;
    for (;;) {
        const ListFormatData* listFormatData = getListFormatData(tempLocale, errorCode);
        if (U_FAILURE(errorCode)) {
            return NULL;
        }
        if (listFormatData != NULL) {
            ListFormatter* p = new ListFormatter(*listFormatData);
            if (p == NULL) {
                errorCode = U_MEMORY_ALLOCATION_ERROR;
                return NULL;
            }
            return p;
        }
        errorCode = U_ZERO_ERROR;
        Locale correctLocale;
        getFallbackLocale(tempLocale, correctLocale, errorCode);
        if (U_FAILURE(errorCode)) {
            return NULL;
        }
        if (correctLocale.isBogus()) {
            return createInstance(Locale::getRoot(), errorCode);
        }
        tempLocale = correctLocale;
    }
}

ListFormatter::ListFormatter(const ListFormatData& listFormatterData) : data(listFormatterData) {
}

ListFormatter::~ListFormatter() {}

void ListFormatter::getFallbackLocale(const Locale& in, Locale& out, UErrorCode& errorCode) {
    if (uprv_strcmp(in.getName(), "zh_TW") == 0) {
        out = Locale::getTraditionalChinese();
    } else {
        const char* localeString = in.getName();
        const char* extStart = locale_getKeywordsStart(localeString);
        if (extStart == NULL) {
            extStart = uprv_strchr(localeString, 0);
        }
        const char* last = extStart;

        // TODO: Check whether uloc_getParent() will work here.
        while (last > localeString && *(last - 1) != '_') {
            --last;
        }

        // Truncate empty segment.
        while (last > localeString) {
            if (*(last-1) != '_') {
                break;
            }
            --last;
        }

        size_t localePortionLen = last - localeString;
        CharString fullLocale;
        fullLocale.append(localeString, localePortionLen, errorCode).append(extStart, errorCode);

        if (U_FAILURE(errorCode)) {
            return;
        }
        out = Locale(fullLocale.data());
    }
}

UnicodeString& ListFormatter::format(const UnicodeString items[], int32_t nItems,
                      UnicodeString& appendTo, UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return appendTo;
    }

    if (nItems > 0) {
        UnicodeString newString = items[0];
        if (nItems == 2) {
            addNewString(data.twoPattern, newString, items[1], errorCode);
        } else if (nItems > 2) {
            addNewString(data.startPattern, newString, items[1], errorCode);
            int i;
            for (i = 2; i < nItems - 1; ++i) {
                addNewString(data.middlePattern, newString, items[i], errorCode);
            }
            addNewString(data.endPattern, newString, items[nItems - 1], errorCode);
        }
        if (U_SUCCESS(errorCode)) {
            appendTo += newString;
        }
    }
    return appendTo;
}

/**
 * Joins originalString and nextString using the pattern pat and puts the result in
 * originalString.
 */
void ListFormatter::addNewString(const UnicodeString& pat, UnicodeString& originalString,
                                 const UnicodeString& nextString, UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return;
    }

    int32_t p0Offset = pat.indexOf(FIRST_PARAMETER, 3, 0);
    if (p0Offset < 0) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    int32_t p1Offset = pat.indexOf(SECOND_PARAMETER, 3, 0);
    if (p1Offset < 0) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    int32_t i, j;

    const UnicodeString* firstString;
    const UnicodeString* secondString;
    if (p0Offset < p1Offset) {
        i = p0Offset;
        j = p1Offset;
        firstString = &originalString;
        secondString = &nextString;
    } else {
        i = p1Offset;
        j = p0Offset;
        firstString = &nextString;
        secondString = &originalString;
    }

    UnicodeString result = UnicodeString(pat, 0, i) + *firstString;
    result += UnicodeString(pat, i+3, j-i-3);
    result += *secondString;
    result += UnicodeString(pat, j+3);
    originalString = result;
}

UOBJECT_DEFINE_NO_RTTI_IMPLEMENTATION(ListFormatter)

U_NAMESPACE_END
