#ifndef prstubs_h___
#define prstubs_h___

#include "prtypes.h"

PR_BEGIN_EXTERN_C

typedef struct PRLock {int dummy;} PRLock;

PR_EXTERN(PRLock*) PR_NewLock(void);
PR_EXTERN(void) PR_DestroyLock(PRLock *lock);
PR_EXTERN(void) PR_Lock(PRLock *lock);
PR_EXTERN(PRStatus) PR_Unlock(PRLock *lock);

extern PRBool _pr_initialized;
extern void _PR_ImplicitInitialization(void);

#define PR_SetError(x, y)

PR_END_EXTERN_C

#endif /* prstubs_h___ */
