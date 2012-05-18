/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef qsObjectHelper_h
#define qsObjectHelper_h

#include "xpcObjectHelper.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsINode.h"
#include "nsWrapperCache.h"

inline nsISupports*
ToSupports(nsISupports *p)
{
    return p;
}

inline nsISupports*
ToCanonicalSupports(nsISupports *p)
{
    return NULL;
}

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 2) || \
    _MSC_FULL_VER >= 140050215

/* Use a compiler intrinsic if one is available. */

#define QS_CASTABLE_TO(_interface, _class) __is_base_of(_interface, _class)

#else

/* The generic version of this predicate relies on the overload resolution
 * rules.  If |_class| inherits from |_interface|, the |_interface*|
 * overload of DOMCI_CastableTo<_interface>::p() will be chosen, otherwise
 * the |void*| overload will be chosen.  There is no definition of these
 * functions; we determine which overload was selected by inspecting the
 * size of the return type.
 */

template <typename Interface> struct QS_CastableTo {
    struct false_type { int x[1]; };
    struct true_type { int x[2]; };
    static false_type p(void*);
    static true_type p(Interface*);
};

#define QS_CASTABLE_TO(_interface, _class)                                     \
    (sizeof(QS_CastableTo<_interface>::p(static_cast<_class*>(0))) ==          \
     sizeof(QS_CastableTo<_interface>::true_type))

#endif

#define QS_IS_NODE(_class)                                                     \
    QS_CASTABLE_TO(nsINode, _class) ||                                         \
    QS_CASTABLE_TO(nsIDOMNode, _class)

class qsObjectHelper : public xpcObjectHelper
{
public:
    template <class T>
    inline
    qsObjectHelper(T *aObject, nsWrapperCache *aCache)
        : xpcObjectHelper(ToSupports(aObject), ToCanonicalSupports(aObject),
                          aCache, QS_IS_NODE(T))
    {}

    template <class T>
    inline
    qsObjectHelper(nsCOMPtr<T>& aObject, nsWrapperCache *aCache)
        : xpcObjectHelper(ToSupports(aObject.get()),
                          ToCanonicalSupports(aObject.get()), aCache,
                          QS_IS_NODE(T))
    {
        if (mCanonical) {
            // Transfer the strong reference.
            mCanonicalStrong = dont_AddRef(mCanonical);
            aObject.forget();
        }
    }

    template <class T>
    inline
    qsObjectHelper(nsRefPtr<T>& aObject, nsWrapperCache *aCache)
        : xpcObjectHelper(ToSupports(aObject.get()),
                          ToCanonicalSupports(aObject.get()), aCache,
                          QS_IS_NODE(T))
    {
        if (mCanonical) {
            // Transfer the strong reference.
            mCanonicalStrong = dont_AddRef(mCanonical);
            aObject.forget();
        }
    }
};

#endif
