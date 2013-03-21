(function() {
  __defineGetter__("x", /x/.constructor)
})()
for (var a = 0; a < 4; ++a) {
  if (a % 4 == 1) {
    gc()
  } else {
    print(x);
  }
}
