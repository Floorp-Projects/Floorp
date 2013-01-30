/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSSPropertiesQS_h__
#define nsCSSPropertiesQS_h__

#include "nsICSSDeclaration.h"

#define CSS_PROP_DOMPROP_PREFIXED(prop_) Moz ## prop_
#define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_, kwtable_, \
                 stylestruct_, stylestructoffset_, animtype_)                 \
static const nsCSSProperty QS_CSS_PROP_##method_ = eCSSProperty_##id_;

#define CSS_PROP_LIST_EXCLUDE_INTERNAL
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_, pref_)	\
  CSS_PROP(name_, id_, method_, flags_, pref_, X, X, X, X, X)
#include "nsCSSPropList.h"

#define CSS_PROP_ALIAS(aliasname_, propid_, aliasmethod_, pref_)  \
  CSS_PROP(X, propid_, aliasmethod_, X, pref_, X, X, X, X, X)
#include "nsCSSPropAliasList.h"
#undef CSS_PROP_ALIAS

#undef CSS_PROP_SHORTHAND
#undef CSS_PROP_LIST_EXCLUDE_INTERNAL
#undef CSS_PROP
#undef CSS_PROP_DOMPROP_PREFIXED

#endif /* nsCSSPropertiesQS_h__ */
