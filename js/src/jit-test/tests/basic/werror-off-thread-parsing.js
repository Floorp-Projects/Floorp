// |jit-test| error:toggle werror
if (helperThreadCount() === 0)
    throw "toggle werror";
options("werror");
offThreadCompileScript("function f(){return 1;''}");
options("werror");
runOffThreadScript();
