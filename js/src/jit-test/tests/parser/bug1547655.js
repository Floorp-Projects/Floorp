// |jit-test| allow-unhandlable-oom; allow-oom
oomTest(() => evaluate(`meta: { with({}) {} }`));
