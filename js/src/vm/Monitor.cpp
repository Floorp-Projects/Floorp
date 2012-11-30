#include "Monitor.h"

namespace js {

Monitor::Monitor()
    : lock_(NULL), condVar_(NULL)
{
}

Monitor::~Monitor()
{
#ifdef JS_THREADSAFE
    PR_DestroyLock(lock_);
    PR_DestroyCondVar(condVar_);
#endif
}

bool
Monitor::init()
{
#ifdef JS_THREADSAFE
    lock_ = PR_NewLock();
    if (!lock_)
        return false;

    condVar_ = PR_NewCondVar(lock_);
    if (!condVar_)
        return false;
#endif

    return true;
}

}
