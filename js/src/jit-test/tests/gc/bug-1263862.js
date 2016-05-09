if (!('oomTest' in this))
    quit();

function loadFile(lfVarx) {
  oomTest(() => eval(lfVarx));
}
for (var i = 0; i < 10; ++i) {
  loadFile(`"use strict"; const s = () => s;`);
}
