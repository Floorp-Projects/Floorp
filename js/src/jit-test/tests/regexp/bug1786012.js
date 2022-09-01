var repeated = "A".repeat(65536);
var src = "^(?:" + repeated + ")\$";

for (var i = 0; i < 100; i++) {
  try {
    RegExp(src).test();
  } catch {}
}
