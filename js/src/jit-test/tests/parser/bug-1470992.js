// |jit-test| skip-if: helperThreadCount() === 0

offThreadCompileModule("export { x };");
gcslice(10);
