// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef gc_allocator_h
#define gc_allocator_h

#include <memory>

#ifndef _WIN32 // Microsoft VC6 bug: standard identifiers should be in std namespace
using std::size_t;
using std::ptrdiff_t;
#endif

namespace JavaScript {

	// An allocator that can be used to allocate objects in the
	// garbage collected heap.
	
	template <class T>
	class gc_allocator {
	public:
		typedef T value_type;
		typedef size_t size_type;
		typedef ptrdiff_t difference_type;
		typedef T *pointer;
		typedef const T *const_pointer;
		typedef T &reference;
		typedef const T &const_reference;
		
		pointer address(reference r) {return &r;}
		const_pointer address(const_reference r) {return &r;}
		
		gc_allocator() {}
		template<class U> gc_allocator(const gc_allocator<U> &u) {}
		~gc_allocator() {}
		
		pointer allocate(size_type n, const void *hint = 0);
		void deallocate(pointer, size_type);
		
		void construct(pointer p, const T &val) { new(p) T(val);}
		void destroy(pointer p) { p->~T(); }
		
		size_type max_size() { return std::numeric_limits<size_type>::max() / sizeof(T); }

		template<class U> struct rebind {typedef gc_allocator<U> other;};
	};

}

#endif /* gc_allocator_h */
