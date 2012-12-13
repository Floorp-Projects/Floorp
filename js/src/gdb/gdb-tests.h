/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Support for C++ fragments to be used by Python unit tests for SpiderMonkey's
// GDB support.
//
// That is:
// - js/src/gdb/mozilla holds the actual GDB SpiderMonkey support code.
// - Each '.py' file in js/src/gdb/tests is a unit test for the above.
// - Each '.cpp' file in js/src/gdb/tests is C++ code for one of the unit tests
//   to run.
//
// (So the .cpp files are two steps removed from being anything one would
// actually run.)

#include "jsapi.h"

void breakpoint();

struct GDBFragment {
    GDBFragment() {
        next = allFragments;
        allFragments = this;
    }

    // The name of this fragment. gdb-tests.cpp runs the fragments whose names
    // are passed to it on the command line.
    virtual const char *name() = 0;

    // Run the fragment code. |argv| is a reference to the pointer into the
    // command-line argument vector, referring to the argument immediately
    // following this fragment's name. The fragment can consume arguments and
    // advance argv if it wishes.
    virtual void run(JSContext *cx, const char **&argv) = 0;

    // We declare one instance of this type for each fragment to run. The
    // constructor adds each instance to a linked list, of which this is
    // the head.
    static GDBFragment *allFragments;

    // The link in the list of all instances.
    GDBFragment *next;
};

// Macro for declaring a C++ fragment for some Python unit test to call. Usage:
//
//   FRAGMENT(<category>, <name>) { <body of fragment function> }
//
// where <category> and <name> are identifiers. The gdb-tests executable
// takes a series of fragment names as command-line arguments and runs them in
// turn; each fragment is named <category>.<name> on the command line.
//
// The body runs in a scope where 'cx' is a usable JSContext *.

#define FRAGMENT(category, subname)                                                             \
class FRAGMENT_CLASS_NAME(category, subname): public GDBFragment {                              \
    void run(JSContext *cx, const char **&argv);                                                \
    const char *name() { return FRAGMENT_STRING_NAME(category, subname); }                      \
    static FRAGMENT_CLASS_NAME(category, subname) singleton;                                    \
};                                                                                              \
FRAGMENT_CLASS_NAME(category, subname) FRAGMENT_CLASS_NAME(category, subname)::singleton;       \
void FRAGMENT_CLASS_NAME(category, subname)::run(JSContext *cx, const char **&argv)

#define FRAGMENT_STRING_NAME(category, subname) (#category "." #subname)
#define FRAGMENT_CLASS_NAME(category, subname) Fragment_ ## category ## _ ## subname

