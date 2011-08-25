// |jit-test| debug

// bug 658491
function f7() {
  try { y = w; } catch(y) {}
}
trap(f7, 14, '')
f7()
