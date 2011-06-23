// |jit-test| debug

function f(){
  this.zzz.zzz;
  for(let d in []);
}
trap(f, 18, '')
try {
    f()
} catch(e) {
    caught = true;
    assertEq(""+e, "TypeError: this.zzz is undefined");
}
assertEq(caught, true);
