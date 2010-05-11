/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_${HEADER}_h
#define mozilla_${HEADER}_h

#if _HAS_EXCEPTIONS
#  error "STL code can only be used with -fno-exceptions"
#endif

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

#ifdef DEBUG
// From
//   http://msdn.microsoft.com/en-us/library/aa985982%28VS.80%29.aspx
// and
//   http://msdn.microsoft.com/en-us/library/aa985965%28VS.80%29.aspx
// there appear to be two types of STL container checking.  The
// former is enabled by -D_DEBUG (which is implied by -DDEBUG), and
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

// We know that code won't be able to catch exceptions, but that's OK
// because we're not throwing them.
#pragma warning( push )
#pragma warning( disable : 4530 )

#include <${HEADER_PATH}>

#pragma warning( pop )

#endif  // if mozilla_${HEADER}_h
