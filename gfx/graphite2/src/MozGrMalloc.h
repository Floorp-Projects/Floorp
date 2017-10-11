/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef MOZ_GR_MALLOC_H
#define MOZ_GR_MALLOC_H

// Override malloc() and friends to call moz_xmalloc() etc, so that we get
// predictable, safe OOM crashes rather than relying on the code to handle
// allocation failures reliably.

#include "mozilla/mozalloc.h"

#if defined(XP_LINUX)

#define malloc moz_xmalloc
#define calloc moz_xcalloc
#define realloc moz_xrealloc

#else

// extern "C" is needed for the Solaris build, while the inline
// functions are needed for the MinGW build. They break gcc 5.4.0
// on Linux however, so keep the old #define's above for Linux

extern "C" inline void* malloc(size_t size)
{
    return moz_xmalloc(size);
}

extern "C" inline void* calloc(size_t nmemb, size_t size)
{
    return moz_xcalloc(nmemb, size);
}

extern "C" inline void* realloc(void *ptr, size_t size)
{
    return moz_xrealloc(ptr, size);
}

#endif // defined(XP_LINUX)

#endif // MOZ_GR_MALLOC_H
