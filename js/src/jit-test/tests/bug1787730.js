// |jit-test| --delazification-mode=concurrent-df+on-demand; skip-if: isLcovEnabled()

oomTest(() => evalcx(0));
