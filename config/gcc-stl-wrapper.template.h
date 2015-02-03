/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_${HEADER}_h
#define mozilla_${HEADER}_h

// For some reason, Apple's GCC refuses to honor -fno-exceptions when
// compiling ObjC.
#if defined(__EXCEPTIONS) && __EXCEPTIONS && !(__OBJC__ && __GNUC__ && XP_IOS)
#  error "STL code can only be used with -fno-exceptions"
#endif

// Silence "warning: #include_next is a GCC extension"
#pragma GCC system_header

#ifdef _WIN32
// Suppress windef.h min and max macros - they make std::min/max not compile.
#define NOMINMAX 1
#endif

// mozalloc.h wants <new>; break the cycle by always explicitly
// including <new> here.  NB: this is a tad sneaky.  Sez the gcc docs:
//
//    `#include_next' does not distinguish between <file> and "file"
//    inclusion, nor does it check that the file you specify has the
//    same name as the current file. It simply looks for the file
//    named, starting with the directory in the search path after the
//    one where the current file was found.
#include_next <new>

// See if we're in code that can use mozalloc.  NB: this duplicates
// code in nscore.h because nscore.h pulls in prtypes.h, and chromium
// can't build with that being included before base/basictypes.h.
#if !defined(XPCOM_GLUE) && !defined(NS_NO_XPCOM) && !defined(MOZ_NO_MOZALLOC)
#  include "mozilla/mozalloc.h"
#else
#  error "STL code can only be used with infallible ::operator new()"
#endif

#if defined(DEBUG) && !defined(_GLIBCXX_DEBUG)
// Enable checked iterators and other goodies
//
// FIXME/bug 551254: gcc's debug STL implementation requires -frtti.
// Figure out how to resolve this with -fno-rtti.  Maybe build with
// -frtti in DEBUG builds?
//
//  # define _GLIBCXX_DEBUG 1
#endif

#pragma GCC visibility push(default)
#include_next <${HEADER}>
#pragma GCC visibility pop

// gcc calls a __throw_*() function from bits/functexcept.h when it
// wants to "throw an exception".  functexcept exists nominally to
// support -fno-exceptions, but since we'll always use the system
// libstdc++, and it's compiled with exceptions, then in practice
// these __throw_*() functions will always throw exceptions (shades of
// -fshort-wchar).  We don't want that and so define our own inlined
// __throw_*().
#ifndef mozilla_throw_gcc_h
#  include "mozilla/throw_gcc.h"
#endif

#endif  // if mozilla_${HEADER}_h
