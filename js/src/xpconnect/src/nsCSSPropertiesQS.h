#ifndef nsCSSPropertiesQS_h__
#define nsCSSPropertiesQS_h__

#include "nsICSSDeclaration.h"

#define CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_,     \
                 kwtable_, stylestruct_, stylestructoffset_, animtype_)        \
static const nsCSSProperty QS_CSS_PROP_##method_ = eCSSProperty_##id_;

#define CSS_PROP_LIST_EXCLUDE_INTERNAL
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_) \
  CSS_PROP(name_, id_, method_, flags_, X, X, X, X, X, X, X)
#include "nsCSSPropList.h"

// Aliases
CSS_PROP(X, opacity, MozOpacity, 0, X, X, X, X, X, X, X)
CSS_PROP(X, outline, MozOutline, 0, X, X, X, X, X, X, X)
CSS_PROP(X, outline_color, MozOutlineColor, 0, X, X, X, X, X, X, X)
CSS_PROP(X, outline_style, MozOutlineStyle, 0, X, X, X, X, X, X, X)
CSS_PROP(X, outline_width, MozOutlineWidth, 0, X, X, X, X, X, X, X)
CSS_PROP(X, outline_offset, MozOutlineOffset, 0, X, X, X, X, X, X, X)

#undef CSS_PROP_SHORTHAND
#undef CSS_PROP_LIST_EXCLUDE_INTERNAL
#undef CSS_PROP

#endif /* nsCSSPropertiesQS_h__ */
