eval(`
  function a() {
    return b();
  }
  //# sourceURL=source-a.js
`);

eval(`
  function b() {
    return c();
  }
  //# sourceURL=source-b.js
`);

eval(`
  function c() {
    return Error().stack;
  }
  //# sourceURL=source-c.js
`);

let filenames = a().split(/\n/)
                   .map(f => f.slice(f.indexOf("@") + 1, f.indexOf(":")));
print(filenames.join("\n"));
assertEq(filenames[0], "source-c.js");
assertEq(filenames[1], "source-b.js");
assertEq(filenames[2], "source-a.js");
