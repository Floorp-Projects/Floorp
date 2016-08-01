if (!('oomTest' in this))
    quit();
let s = saveStack();
oomTest(() => { saveStack(); });
