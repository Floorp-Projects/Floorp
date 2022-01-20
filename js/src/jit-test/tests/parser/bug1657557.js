setImmutablePrototype(Function.__proto__);

if (helperThreadCount() > 0) {
    offThreadCompileScript("function f() {}");
    runOffThreadScript();
}
