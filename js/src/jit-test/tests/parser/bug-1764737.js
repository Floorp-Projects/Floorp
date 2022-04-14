// |jit-test| skip-if: !('oomTest' in this); --fuzzing-safe; --ion-offthread-compile=off

function r(src) {
  oomTest(function() {
      parseModule(src);
  });
}
r("export * from 'y';");
r("export * from 'y';");
