// |jit-test| slow; skip-if: !('oomTest' in this)

function main() {
  const v1 = this.newGlobal();
  function v2(v3,v4) {
    oomTest(() => {
      const v13 = this.parseModule("apply");
      const v14 = {};
      const v15 = v14.size;
      const v17 = Uint16Array !== v15;
      const v18 = v17 && Uint16Array;
      const v20 = this.objectGlobal(v18);
      const v21 = v20.newGlobal();
      const v24 = this.resumeProfilers();
      const v25 = v24 && v21;
      const v26 = v25.evalInWorker("9007199254740991");
      function v27(v28,v29) {
      }
      const v31 = new Promise(v27);
      const v33 = this.getModuleEnvironmentNames(v13);
    });
  }
  const v36 = new Promise(v2);
  const v37 = v1.Debugger;
  const v38 = v37();
  const v39 = v38.findAllGlobals();
  const v40 = v39.pop();
  const v41 = v40.getOwnPropertyDescriptor(v37);
  gc();
}
try {
  main();
} catch(exn) {
}
