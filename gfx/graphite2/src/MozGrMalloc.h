/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef MOZ_GR_MALLOC_H
#define MOZ_GR_MALLOC_H

// Override malloc() and friends to call moz_malloc() etc, so that we get
// predictable, safe OOM crashes rather than relying on the code to handle
// allocation failures reliably.

#include "mozilla/mozalloc.h"

#define malloc moz_xmalloc
#define calloc moz_xcalloc
#define realloc moz_xrealloc
#define free moz_free

#endif // MOZ_GR_MALLOC_H
