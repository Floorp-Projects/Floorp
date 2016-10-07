if (helperThreadCount() == 0)
    quit();

m = 'x';
for (var i = 0; i < 10; i++)
    m += m;
offThreadCompileScript("", ({elementAttributeName: m}));
var n = newGlobal();
gczeal(2,1);
n.runOffThreadScript();
