// |jit-test| skip-if: !('oomTest' in this)

oomTest(new Function(`let a = grayRoot();`));
