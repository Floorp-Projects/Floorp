// -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// The contents of this file are subject to the Netscape Public
// License Version 1.1 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of
// the License at http://www.mozilla.org/NPL/
//
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
// implied. See the License for the specific language governing
// rights and limitations under the License.
//
// The Original Code is the JavaScript 2 Prototype.
//
// The Initial Developer of the Original Code is Netscape
// Communications Corporation.  Portions created by Netscape are
// Copyright (C) 2000 Netscape Communications Corporation. All
// Rights Reserved.

#ifndef gc_container_h
#define gc_container_h

#include "gc_allocator.h"

#include <list>
#include <vector>
#include <string>

#define LIST std::list
#define VECTOR std::vector

#if defined(__GNUC__)
// grr, what kind of standard is this?
#define STRING basic_string
#define CHAR_TRAITS string_char_traits
#else
#define STRING std::basic_string
#define CHAR_TRAITS std::char_traits
#endif

namespace JavaScript {
	/**
	 * Rebind some of the basic container types to use a GC_allocator.
	 * What I really want is something more general, something like:
	 * template <typename Container, typename T> class gc_rebind {
	 *   typedef typename Container<T, gc_allocator<T> > other;
	 * };
	 * But I can't figure out how to do that with C++ templates.
	 */
	template <class T> struct gc_container {
		typedef typename LIST<T, gc_allocator<T> > list;
		typedef typename VECTOR<T, gc_allocator<T> > vector;
		typedef typename STRING<T, CHAR_TRAITS<T>, gc_allocator<T> > string;
	};

	/**
	 * But, it's pretty easy to do with macros:
	 */
	#define GC_CONTAINER(container, type) container<T, gc_allocator<T> >
}

#endif /* gc_container_h */
