if (helperThreadCount() === 0)
   quit();

var fe = "vv";
for (i = 0; i < 24; i++) fe += fe;
offThreadCompileScript(fe, {});
