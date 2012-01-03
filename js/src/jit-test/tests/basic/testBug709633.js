test();
function test() {
  var f;
  f = function() { (let(x) {y: z}) }
    let (f = function() {
        for (var t=0;t<6;++t) ++f;
    }) { f(); } //  {  }       
  actual = f + '';
}
