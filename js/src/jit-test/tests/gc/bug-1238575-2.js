// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => evalInWorker("1"));
