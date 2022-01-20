// |jit-test| skip-if: helperThreadCount() === 0

offThreadCompileModuleToStencil("export { x };");
gcslice(10);
