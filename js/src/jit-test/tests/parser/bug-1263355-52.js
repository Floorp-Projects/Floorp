// |jit-test| error: ReferenceError

(function() {
  if ({}) {}
  else if (x) {}
  else {}
 
  return "" + x;
 
  let x;
})() 
