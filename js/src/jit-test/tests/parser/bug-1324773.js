if (!('gczeal' in this))
    quit();
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
    gczeal(9, 1);
    dbg.findScripts({});
`);
