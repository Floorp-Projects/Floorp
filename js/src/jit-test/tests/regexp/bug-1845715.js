// |jit-test| skip-if: !('oomTest' in this)
oomTest(() => { gc(); /./d.exec(); });
