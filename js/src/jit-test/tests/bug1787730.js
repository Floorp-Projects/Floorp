// |jit-test| --delazification-mode=concurrent-df+on-demand; skip-if: !('oomTest' in this) || isLcovEnabled()

oomTest(() => evalcx(0));
