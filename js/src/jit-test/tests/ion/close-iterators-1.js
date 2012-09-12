// |jit-test| error: InternalError
// from bug 729797, bug732852 as well
var patterns = new Array(); 
patterns[0] = '';
test();
function test() {
  for (i in patterns) {
    s = patterns[i];
    status =(test)(s);  
  }
}
