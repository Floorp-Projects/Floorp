// |jit-test| allow-unhandlable-oom
ignoreUnhandledRejections();
oomTest(() => {
  gc();
  import("javascript:0");
  drainJobQueue();
});
