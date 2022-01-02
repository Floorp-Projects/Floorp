/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkMacros_DEFINED
#define SkMacros_DEFINED

/*
 *  Usage:  SK_MACRO_CONCAT(a, b)   to construct the symbol ab
 *
 *  SK_MACRO_CONCAT_IMPL_PRIV just exists to make this work. Do not use directly
 *
 */
#define SK_MACRO_CONCAT(X, Y)           SK_MACRO_CONCAT_IMPL_PRIV(X, Y)
#define SK_MACRO_CONCAT_IMPL_PRIV(X, Y)  X ## Y

/*
 *  Usage: SK_MACRO_APPEND_LINE(foo)    to make foo123, where 123 is the current
 *                                      line number. Easy way to construct
 *                                      unique names for local functions or
 *                                      variables.
 */
#define SK_MACRO_APPEND_LINE(name)  SK_MACRO_CONCAT(name, __LINE__)

/**
 * For some classes, it's almost always an error to instantiate one without a name, e.g.
 *   {
 *       SkAutoMutexAcquire(&mutex);
 *       <some code>
 *   }
 * In this case, the writer meant to hold mutex while the rest of the code in the block runs,
 * but instead the mutex is acquired and then immediately released.  The correct usage is
 *   {
 *       SkAutoMutexAcquire lock(&mutex);
 *       <some code>
 *   }
 *
 * To prevent callers from instantiating your class without a name, use SK_REQUIRE_LOCAL_VAR
 * like this:
 *   class classname {
 *       <your class>
 *   };
 *   #define classname(...) SK_REQUIRE_LOCAL_VAR(classname)
 *
 * This won't work with templates, and you must inline the class' constructors and destructors.
 * Take a look at SkAutoFree and SkAutoMalloc in this file for examples.
 */
#define SK_REQUIRE_LOCAL_VAR(classname) \
    static_assert(false, "missing name for " #classname)

////////////////////////////////////////////////////////////////////////////////

// Can be used to bracket data types that must be dense, e.g. hash keys.
#if defined(__clang__)  // This should work on GCC too, but GCC diagnostic pop didn't seem to work!
    #define SK_BEGIN_REQUIRE_DENSE _Pragma("GCC diagnostic push") \
                                   _Pragma("GCC diagnostic error \"-Wpadded\"")
    #define SK_END_REQUIRE_DENSE   _Pragma("GCC diagnostic pop")
#else
    #define SK_BEGIN_REQUIRE_DENSE
    #define SK_END_REQUIRE_DENSE
#endif

#define SK_INIT_TO_AVOID_WARNING    = 0

#endif  // SkMacros_DEFINED
