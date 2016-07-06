if (!('oomTest' in this))
    quit();

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
  oomTest(function() eval(lfVarx))
}
