
try { with( {"a":1} ) {
    (function () {
        for (;;) {
            t
        }
    })()
} } catch (e) {}

with( {"b":2} ) {
  (function () {
    for (b = 0; b < 18; ++b) {}
  })();
}
