// |jit-test| skip-if: !('oomTest' in this)
oomTest(() => eval("new WeakRef({});"));
