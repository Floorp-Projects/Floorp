
#include "prstubs.h"

PR_IMPLEMENT(PRLock*) PR_NewLock(void) { return NULL; }

PR_IMPLEMENT(void) PR_DestroyLock(PRLock *lock) { }
PR_IMPLEMENT(void) PR_Lock(PRLock *lock) { }
PR_IMPLEMENT(PRStatus) PR_Unlock(PRLock *lock) { return PR_SUCCESS;}

PRBool _pr_initialized;

void _PR_ImplicitInitialization(void)
{
    _pr_initialized = PR_TRUE;
}
