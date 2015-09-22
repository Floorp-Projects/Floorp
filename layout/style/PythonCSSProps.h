/* A file meant as input to the preprocessor only */

/* DO_PROP serves as an extra level of indirection to allow expansion
   of CSS_PROP_DOMPROP_PREFIXED */

[

#define PROP_STRINGIFY_INTERNAL(X) #X
#define PROP_STRINGIFY(X) PROP_STRINGIFY_INTERNAL(X)

#define DO_PROP(name, method, id, flags, pref, proptype) \
  [ #name, #method, #id, PROP_STRINGIFY(flags), pref, proptype ],
#define CSS_PROP(name, id, method, flags, pref, parsevariant, kwtable, \
		 stylestruct, stylestructoffset, animtype) \
  DO_PROP(name, method, id, flags, pref, "longhand")
#define CSS_PROP_SHORTHAND(name, id, method, flags, pref) \
  DO_PROP(name, method, id, flags, pref, "shorthand")
#define CSS_PROP_LOGICAL(name, id, method, flags, pref, parsevariant, kwtable, \
     group, stylestruct, stylestructoffset, animtype) \
  DO_PROP(name, method, id, flags, pref, "logical")
#define CSS_PROP_PUBLIC_OR_PRIVATE(publicname_, privatename_) publicname_

#include "nsCSSPropList.h"

#undef CSS_PROP_PUBLIC_OR_PRIVATE
#undef CSS_PROP_LOGICAL
#undef CSS_PROP_SHORTHAND
#undef CSS_PROP

#define CSS_PROP_ALIAS(name, id, method, pref) \
  DO_PROP(name, method, id, 0, pref, "alias")

#include "nsCSSPropAliasList.h"

#undef CSS_PROP_ALIAS

#undef DO_PROP
#undef PROP_STRINGIFY
#undef PROP_STRINGIFY_INTERNAL

]
