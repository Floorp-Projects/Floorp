
#define _beginthreadex(security, stack_size, start_proc, arg, flags, pid) \
  CreateThread(security, stack_size ? stack_size : 65536L,(LPTHREAD_START_ROUTINE) start_proc, arg, flags | STACK_SIZE_PARAM_IS_A_RESERVATION, pid)
