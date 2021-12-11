// |jit-test| skip-if: !('oomTest' in this)

enableGeckoProfiling();
oomTest(() => {
    try {
        for (quit of ArrayBuffer);
    } catch (e) {
        switch (1) {
            case 0:
                let x
            case 1:
                (function() { return x; })()
        }
    }
})
