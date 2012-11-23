#include "vm/monitor.h"

namespace js {

Monitor::Monitor()
    : lock_(NULL), condVar_(NULL)
{
}

Monitor::~Monitor()
{
    PR_DestroyLock(lock_);
    PR_DestroyCondVar(condVar_);
}

bool
Monitor::init()
{
    lock_ = PR_NewLock();
    if (!lock_)
        return false;

    condVar_ = PR_NewCondVar(lock_);
    if (!condVar_)
        return false;

    return true;
}

}
