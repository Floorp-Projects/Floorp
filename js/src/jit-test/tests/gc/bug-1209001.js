if (!('oomTest' in this))
    quit();

oomTest(() => parseModule('import v from "mod";'));
