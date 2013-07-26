function add(a, b, k) {
  var result = a + b;
  return k(result);
}

function sub(a, b, k) {
  var result = a - b;
  return k(result);
}

function mul(a, b, k) {
  var result = a * b;
  return k(result);
}

function div(a, b, k) {
  var result = a / b;
  return k(result);
}

function arithmetic() {
  add(4, 4, function (a) {
    // 8
    sub(a, 2, function (b) {
      // 6
      mul(b, 3, function (c) {
        // 18
        div(c, 2, function (d) {
          // 9
          console.log(d);
        });
      });
    });
  });
}

// Compile with closure compiler and the following flags:
//
//     --compilation_level WHITESPACE_ONLY
//     --source_map_format V3
//     --create_source_map math.map
//     --js_output_file    math.min.js
//
// And then append the sourceMappingURL comment directive to math.min.js
// manually.
