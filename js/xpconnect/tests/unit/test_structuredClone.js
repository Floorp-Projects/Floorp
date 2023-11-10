function run_test() {
  var sb = new Cu.Sandbox("http://www.example.com", {
    wantGlobalProperties: ["structuredClone"],
  });

  sb.equal = equal;

  sb.testing = Cu.cloneInto({ xyz: 123 }, sb);
  Cu.evalInSandbox(
    `
    equal(structuredClone("abc"), "abc");

    var obj = { a: 1 };
    obj.self = obj;
    var clone = structuredClone(obj);
    equal(clone.a, 1);
    equal(clone.self, clone);

    var ab = new ArrayBuffer(1);
    clone = structuredClone(ab, { transfer: [ab] });
    equal(clone.byteLength, 1);
    equal(ab.byteLength, 0);

    clone = structuredClone(testing);
    equal(clone.xyz, 123);
    `,
    sb
  );

  const clone = structuredClone({ b: 2 });
  Assert.equal(clone.b, 2);
}
