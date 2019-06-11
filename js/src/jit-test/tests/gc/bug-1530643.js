// |jit-test| skip-if: !('oomAtAllocation' in this); error: Error

// OOM testing of worker threads is disallowed because it's not thread safe.
oomAtAllocation(11, 11);
