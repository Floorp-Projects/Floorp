// |jit-test| skip-if: !('oomAtAllocation' in this); error: Error

const THREAD_TYPE_WORKER = 10;

// OOM testing of worker threads is disallowed because it's not thread safe.
oomAtAllocation(11, THREAD_TYPE_WORKER);
