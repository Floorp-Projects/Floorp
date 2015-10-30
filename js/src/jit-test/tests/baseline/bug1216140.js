function newFunc(x) Function(x)()
newFunc(` 
  var BUGNUMBER = 8[ anonymous = true ]--;
  () => BUGNUMBER;
`);

