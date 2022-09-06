// |jit-test| --delazification-mode=concurrent-df+on-demand; skip-if: !('oomTest' in this)

oomTest(() => evalcx(0));
