/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 */
#include "nsLDAP.h"
#include "nspr.h"

// frees all elements of an XPIDL out array of a given size using
// freeFunc(), then frees the array itself using nsMemory::Free().
// Thanks to <alecf@netscape.com> for suggesting this form, which can be
// used to NS_RELEASE entire arrays before freeing as well.
//
#define NSLDAP_FREE_XPIDL_ARRAY(size, array, freeFunc) \
    for ( PRUint32 __iter__ = (size) ; __iter__ > 0 ; ) \
        freeFunc((array)[--__iter__]); \
    nsMemory::Free(array);

// XXXdmose should this really be DEBUG-only?
//
#ifdef DEBUG
extern PRLogModuleInfo *gLDAPLogModule;	   // defn in nsLDAPProtocolModule.cpp
#endif
