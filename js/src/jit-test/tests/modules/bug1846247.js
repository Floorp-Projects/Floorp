// |jit-test| skip-if: !('oomTest' in this); allow-unhandlable-oom
ignoreUnhandledRejections();
oomTest(() => {
  gc();
  import("javascript:0");
  drainJobQueue();
});
