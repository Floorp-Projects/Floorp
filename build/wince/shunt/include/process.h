
#define _beginthreadex(security, stack_size, start_proc, arg, flags, pid) \
        CreateThread(security, stack_size,(LPTHREAD_START_ROUTINE) start_proc, arg, flags, pid)
