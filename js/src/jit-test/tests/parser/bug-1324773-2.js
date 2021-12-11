// |jit-test| skip-if: !('gczeal' in this)

var lfGlobal = newGlobal();
lfGlobal.evaluate(`
    for (var i = 0; i < 600; i++)
        eval('function f' + i + '() {}');
`);
var lfGlobal = newGlobal();
lfGlobal.evaluate(`
    if (!('gczeal' in this))
        quit();
    var dbg = new Debugger();
    gczeal(9, 10);
    dbg.findScripts({});
`);
