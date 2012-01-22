
function test() {
  try {
    for (var i = 0 in this) throw p;
  } catch (e) {
    if (i !== 94)
      return "what";
  }
}
test();
