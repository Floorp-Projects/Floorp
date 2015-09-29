// |jit-test| --no-threads

load(libdir + 'oomTest.js');
oomTest(() => parseModule('import v from "mod";'));
