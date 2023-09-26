function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/css", false);
  response.write(gResponses[request.queryString]);
}

let gResponses = {
  // 1
  A: "@import 'generateCss.sjs?B';",
  B: "",

  // 2
  C: "@import 'generateCss.sjs?D';",
  D: "",

  // 3
  E: "@import 'generateCss.sjs?F';",
  F: "",

  // 4
  G: "@import 'generateCss.sjs?H'; @import 'http://example.org/tests/dom/tests/mochitest/general/generateCss.sjs?K';",
  H: "@import 'http://example.com/tests/dom/tests/mochitest/general/generateCss.sjs?I';",
  I: "@import 'generateCss.sjs?J",
  J: "",
  K: "@import 'generateCss.sjs?L';",
  L: "@import 'generateCss.sjs?M",
  M: "",

  // 6
  O: ".c2 { background-image: url('/image/test/mochitest/red.png');}",

  // 7
  P: "@font-face { font-family: Ahem; src: url('/tests/dom/base/test/Ahem.ttf'); } .c3 { font-family: Ahem; font-size: 20px; }",

  // 8
  Q: ".c4 { cursor:  url('/image/test/mochitest/over.png') 2 2, auto; } ",

  // 9
  R: "#image { mask: url('/tests/dom/base/test/file_use_counter_svg_fill_pattern_data.svg'); }",
};
