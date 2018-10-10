// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => { let a = [2147483651]; [a[0], a[undefined]].sort(); });
