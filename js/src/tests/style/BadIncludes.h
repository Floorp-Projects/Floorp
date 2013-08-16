// Note:  the #if/#elif conditions are to get past the #include order checking.
#if A
#include "tests/style/BadIncludes.h"    // bad: self-include
#include "tests/style/BadIncludes2.h"   // ok
#elif B
#include "BadIncludes2.h"               // bad: not a full path
#elif C
#include <tests/style/BadIncludes2.h>   // bad: <> form used for local file
#elif D
#include "stdio.h"                      // bad: "" form used for system file
#endif
