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

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include "gc_allocator.h"
#include "gc_container.h"

/*
namespace JavaScript {

template <class T>
typename gc_allocator<T>::pointer
gc_allocator<T>::allocate(gc_allocator<T>::size_type n, const void*)
{
	return static_cast<pointer>(GC_malloc(n*sizeof(T)));
}

template <class T>
void gc_allocator<T>::deallocate(gc_allocator<T>::pointer ptr, gc_allocator<T>::size_type)
{
	// this can really be a NO-OP with the GC.
	// ::GC_free(static_cast<void*>(ptr));
}

}
 */

// test driver for standalone GC development.

namespace JS = JavaScript;

template <class T>
void* operator new(std::size_t, const JS::gc_allocator<T>& alloc)
{
	return alloc.allocate(1);
}

/**
 * Define a C++ class that is garbage collectable, and wants to have its destructor
 * called when it is finalized.
 */
class A {
public:
	typedef JS::gc_traits_finalizable<A> traits;
	typedef JS::gc_allocator<A, traits> allocator;
	friend struct traits;
	
	static int instances;
	
	void* operator new(std::size_t)
	{
		return allocator::allocate(1);
	}
	
	A()
	{
		++instances;
		std::cout << "A::A() here." << std::endl;
	}

protected:	
	~A()
	{
		--instances;
		std::cout << "A::~A() here." << std::endl;
	}

private:
	// void operator delete(void*) {}
};

int A::instances = 0;

int main(int /* argc */, char* /* argv[] */)
{
	using namespace std;
	using namespace JS;

	cout << "testing the GC allocator." << endl;

#ifdef XP_MAC
	// allocate a string, using the GC, and owned by an auto_ptr, that knows how to correctly destroy the string.
	typedef gc_container<char>::string char_string;
	typedef gc_allocator<char_string> char_string_alloc;
	auto_ptr<char_string, char_string_alloc> ptr(new(char_string_alloc()) char_string("This is a garbage collectable string."));
	const char_string& str = *ptr;
	cout << str << endl;
#endif
	
	// question, how can we partially evaluate a template?
	// can we say, typedef template <class T> vector<typename T>.
	// typedef vector<int, gc_allocator<int> > int_vector;
	typedef gc_container<int>::vector int_vector;
	
	// generate 1000 random values.
	int_vector values;
	for (int i = 0; i < 1000; ++i) {
		int value = rand() % 32767;
		values.push_back(value);
		// allocate a random amount of garbage.
		if (!GC_malloc(static_cast<size_t>(value)))
			cerr << "GC_malloc failed." << endl;
		// allocate an object that has a finalizer to call its destructor.
		A* a = new A();
	}

	// run a collection.
	// gc_allocator<void>::collect();
	GC_gcollect();

	// print out instance count.
	cout << "A::instances = " << A::instances << endl;
	
	// sort the values.
	sort(values.begin(), values.end());
	
	// print the values.
	int_vector::iterator iter = values.begin(), last = values.end();
	cout << *iter++;
	while (iter < last)
		cout << ' ' << *iter++;
	cout << endl;

#ifdef XP_MAC	
	// finally, print the string again.
	cout << str << endl;
#endif

	return 0;
}
