test();
function test() {
  for (var i=0; i<2; ++i) {};
  try {}  catch ([ q ]) {
      function g() {}
  }     
}
