x = <x/>
try {
  Function("\
    for(var a = 0; a < 6; a++) {\
      Number(x.u)\
    }\
  ")()
} catch (e) {
  assertEq(e.message, "too much recursion");
}
