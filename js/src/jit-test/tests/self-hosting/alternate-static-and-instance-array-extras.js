// |jit-test| skip-if: !(getBuildConfiguration().release_or_beta)
actual = Array.indexOf([]);
actual += [].indexOf();
actual += Array.indexOf([]);