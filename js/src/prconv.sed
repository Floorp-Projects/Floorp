s/extern PR_PUBLIC_API/PR_EXTERN/g
s/PR_PUBLIC_API/PR_IMPLEMENT/g
s/prhashcode/PRHashNumber/g
s/include "prassert.h"/include "prlog.h"/g
s/include "prprintf.h"/include "prprf.h"/g
s/include "prtime.h"/include "prmjtime.h"/g
s/PRTime/PRMJTime/g
s/PR_ExplodeTime/PRMJ_ExplodeTime/g
s/PR_ComputeTime/PRMJ_ComputeTime/g
s/PR_ToLocal/PRMJ_ToLocal/g
s/PR_ToGMT/PRMJ_ToGMT/g
s/PR_DSTOffset/PRMJ_DSTOffset/g
s/PR_FormatTimeUSEnglish/PRMJ_FormatTimeUSEnglish/g
s/PR_FormatTime/PRMJ_FormatTime/g
s/PR_USEC_PER_SEC/PRMJ_USEC_PER_SEC/g
s/PR_USEC_PER_MSEC/PRMJ_USEC_PER_MSEC/g
s/PR_gmtime/PRMJ_gmtime/g
s/PR_Now/PRMJ_Now/g
s/PR_LocalGMTDifference/PRMJ_LocalGMTDifference/g

s/^#include "prarena.h"\(.*\)/#ifndef NSPR20\
&\
#else\
#include "plarena.h"\1\
#endif/

s/^#include "prhash.h"\(.*\)/#ifndef NSPR20\
&\
#else\
#include "plhash.h"\1\
#endif/

s@/\*JSSRC-SED-MAGIC-COMMENT\*/@#define NETSCAPE_INTERNAL 1\
#include "jscompat.h"@
