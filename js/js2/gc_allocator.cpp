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

#include "gc_allocator.h"

#include <iostream>
#include <string>
#include <vector>

namespace JavaScript {

/*
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
 */

}

// test driver for standalone GC development.

using namespace std;
using namespace JavaScript;

template <class T> struct gc_types {
	typedef vector<T, gc_allocator<T> > gc_vector;
};

void main(int /* argc */, char* /* argv[] */)
{
	cout << "testing the GC allocator." << endl;
	
	typedef basic_string< char, char_traits<char>, gc_allocator<char> > GC_string;
	GC_string str("This is a garbage collectable string.");
	cout << str << endl;
	
	// question, how can we partially evaluate a template?
	// can we say, typedef template <class T> vector<typename T>.
	// typedef vector<int, gc_allocator<int> > int_vector;
	typedef gc_types<int>::gc_vector int_vector;
	
	// generate 1000 random values.
	int_vector values;
	for (int i = 0; i < 1000; ++i) {
		int value = rand();
		values.push_back(value);
		// allocate a random amount of garbage.
		if (!GC_malloc(static_cast<size_t>(value)))
			cerr << "GC_malloc failed." << endl;
	}
	
	// run a collection.
	GC_gcollect();
	
	// sort the values.
	sort(values.begin(), values.end());
	
	// print the values.
	int_vector::iterator iter = values.begin(), last = values.end();
	cout << *iter++;
	while (iter < last)
		cout << ' ' << *iter++;
	cout << endl;
	
	// finally, print the string again.
	cout << str << endl;
}
