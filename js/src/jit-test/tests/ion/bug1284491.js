// |jit-test| skip-if: !('oomTest' in this)

loadFile(`
  function SwitchTest(){
    switch(value) {
      case 0:break
      case isNaN: break
    }
  }
  SwitchTest();
`)
function loadFile(lfVarx) {
  oomTest(function() { return eval(lfVarx); })
}
