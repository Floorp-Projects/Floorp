if (helperThreadCount() === 0)
    quit();

offThreadCompileModule("export { x };");
gcslice(10);
