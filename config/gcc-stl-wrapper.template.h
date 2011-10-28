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

// For some reason, Apple's GCC refuses to honor -fno-exceptions when
// compiling ObjC.
#if __EXCEPTIONS && !(__OBJC__ && __GNUC__ && XP_IOS)
#  error "STL code can only be used with -fno-exceptions"
#endif

// Silence "warning: #include_next is a GCC extension"
#pragma GCC system_header

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
