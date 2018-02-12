// Note: Each #if scope gets checked separately.

// These are in reverse order!
#if A
# include "vm/Interpreter-inl.h"
# include "jsscriptinlines.h"
# include "js/Value.h"
# include "ds/LifoAlloc.h"
# include "jsapi.h"
# include <stdio.h>
# include "mozilla/HashFunctions.h"
#endif

// These are in reverse order, but it's ok due to the #if scopes.
#if B
# include "vm/Interpreter-inl.h"
# if C
#  include "js/Value.h"
#  if D
#   include "jsapi.h"
#  endif
#  include <stdio.h>
# endif
# include "mozilla/HashFunctions.h"
#endif

#include "jsscript.h"
#include "vm/JSObject.h"  // out of order
#include "jstypes.h"
#include "vm/JSFunction.h"
