#include "tests/style/BadIncludes.h"    // bad: self-include
#include "tests/style/BadIncludes2.h"   // ok
#include "BadIncludes2.h"               // bad: not a full path
#include <tests/style/BadIncludes2.h>   // bad: <> form used for local file
#include "stdio.h"                      // bad: "" form used for system file
