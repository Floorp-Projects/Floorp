// |jit-test| skip-if: typeof oomTest !== 'function' || typeof Intl !== 'object'

oomTest(() => {
    try {
        new Intl.DateTimeFormat;
        x1 = 0;
    }  catch (e) {
        switch (1) {
            case 0:
                let s;
            case 1:
                x;
        }
    }
})
