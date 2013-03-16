/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Intl_h___
#define Intl_h___

#include "js/RootingAPI.h"

struct JSContext;
class JSObject;

/*
 * The Intl module specified by standard ECMA-402,
 * ECMAScript Internationalization API Specification.
 */

/**
 * Initializes the Intl Object and its standard built-in properties.
 * Spec: ECMAScript Internationalization API Specification, 8.0, 8.1
 */
extern JSObject *
js_InitIntlClass(JSContext *cx, js::HandleObject obj);


namespace js {

/*
 * The following functions are for use by self-hosted code.
 */


/******************** Collator ********************/

/**
 * Returns an object indicating the supported locales for collation
 * by having a true-valued property for each such locale with the
 * canonicalized language tag as the property name. The object has no
 * prototype.
 *
 * Usage: availableLocales = intl_Collator_availableLocales()
 */
extern JSBool
intl_Collator_availableLocales(JSContext *cx, unsigned argc, Value *vp);

/**
 * Returns an array with the collation type identifiers per Unicode
 * Technical Standard 35, Unicode Locale Data Markup Language, for the
 * collations supported for the given locale. "standard" and "search" are
 * excluded.
 *
 * Usage: collations = intl_availableCollations(locale)
 */
extern JSBool
intl_availableCollations(JSContext *cx, unsigned argc, Value *vp);

/**
 * Compares x and y (which must be String values), and returns a number less
 * than 0 if x < y, 0 if x = y, or a number greater than 0 if x > y according
 * to the sort order for the locale and collation options of the given
 * Collator.
 *
 * Spec: ECMAScript Internationalization API Specification, 10.3.2.
 *
 * Usage: result = intl_CompareStrings(collator, x, y)
 */
extern JSBool
intl_CompareStrings(JSContext *cx, unsigned argc, Value *vp);


/******************** NumberFormat ********************/

/**
 * Returns an object indicating the supported locales for number formatting
 * by having a true-valued property for each such locale with the
 * canonicalized language tag as the property name. The object has no
 * prototype.
 *
 * Usage: availableLocales = intl_NumberFormat_availableLocales()
 */
extern JSBool
intl_NumberFormat_availableLocales(JSContext *cx, unsigned argc, Value *vp);

/**
 * Returns the numbering system type identifier per Unicode
 * Technical Standard 35, Unicode Locale Data Markup Language, for the
 * default numbering system for the given locale.
 *
 * Usage: defaultNumberingSystem = intl_numberingSystem(locale)
 */
extern JSBool
intl_numberingSystem(JSContext *cx, unsigned argc, Value *vp);

} // namespace js

#endif /* Intl_h___ */
