// |jit-test| allow-unhandlable-oom; allow-oom; skip-if: !('oomTest' in this)
oomTest(() => evaluate(`meta: { with({}) {} }`));
