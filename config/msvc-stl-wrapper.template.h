/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_${HEADER}_h
#define mozilla_${HEADER}_h

#if _HAS_EXCEPTIONS
#  error "STL code can only be used with -fno-exceptions"
#endif

// Suppress windef.h min and max macros - they make std::min/max not compile.
#define NOMINMAX 1

// Code built with !_HAS_EXCEPTIONS calls std::_Throw(), but the win2k
// CRT doesn't export std::_Throw().  So we define it.
#ifndef mozilla_Throw_h
#  include "mozilla/throw_msvc.h"
#endif

// Code might include <new> before other wrapped headers, but <new>
// includes <exception> and so we want to wrap it.  But mozalloc.h
// wants <new> also, so we break the cycle by always explicitly
// including <new> here.
#include <${NEW_HEADER_PATH}>

// See if we're in code that can use mozalloc.  NB: this duplicates
// code in nscore.h because nscore.h pulls in prtypes.h, and chromium
// can't build with that being included before base/basictypes.h.
#if !defined(XPCOM_GLUE) && !defined(NS_NO_XPCOM) && !defined(MOZ_NO_MOZALLOC)
#  include "mozilla/mozalloc.h"
#else
#  error "STL code can only be used with infallible ::operator new()"
#endif

#ifdef _DEBUG
// From
//   http://msdn.microsoft.com/en-us/library/aa985982%28VS.80%29.aspx
// and
//   http://msdn.microsoft.com/en-us/library/aa985965%28VS.80%29.aspx
// there appear to be two types of STL container checking.  The
// former is enabled by -D_DEBUG (which is implied by -MDd or -MTd), and
// looks to be full generation/mutation checked iterators as done by
// _GLIBCXX_DEBUG.  The latter appears to just be bounds checking, and
// is enabled by the following macros.  It appears that the _DEBUG
// iterators subsume _SECURE_SCL, and the following settings are
// default anyway, so we'll just leave this commented out.
//#  define _SECURE_SCL 1
//#  define _SECURE_SCL_THROWS 0
#else
// Note: _SECURE_SCL iterators are on by default in opt builds.  We
// could leave them on, but since gcc doesn't, we might as well
// preserve that behavior for perf reasons.  nsTArray is in the same
// camp as gcc.  Can revisit later.
//
// FIXME/bug 551254: because we're not wrapping all the STL headers we
// use, undefining this here can cause some headers to be built with
// iterator checking and others not.  Turning this off until we have a
// better plan.
//#  undef _SECURE_SCL
#endif

// C4275: When _HAS_EXCEPTIONS is set to 0, system STL header
//        will generate the warning which we can't modify.
// C4530: We know that code won't be able to catch exceptions,
//        but that's OK because we're not throwing them.
#pragma warning( push )
#pragma warning( disable : 4275 4530 )

#include <${HEADER_PATH}>

#pragma warning( pop )

#endif  // if mozilla_${HEADER}_h
