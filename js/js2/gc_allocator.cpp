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

extern "C" {
void* GC_malloc(std::size_t bytes);
void GC_free(void* ptr);
}

namespace JavaScript {

template <class T>
typename gc_allocator<T>::pointer
gc_allocator<T>::allocate(gc_allocator<T>::size_type n, const void *hint)
{
	return static_cast<pointer>(GC_malloc(n*sizeof(T)));
}

template <class T>
void gc_allocator<T>::deallocate(gc_allocator<T>::pointer, gc_allocator<T>::size_type)
{
	// this can really be a NO-OP with the GC.
	::GC_free(static_cast<void*>(ptr));
}

}
