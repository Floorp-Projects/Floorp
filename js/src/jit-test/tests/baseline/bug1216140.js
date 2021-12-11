function newFunc(x) { return Function(x)(); }
newFunc(` 
  var BUGNUMBER = 8[ anonymous = true ]--;
  () => BUGNUMBER;
`);

