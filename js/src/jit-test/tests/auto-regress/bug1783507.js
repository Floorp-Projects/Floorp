function main(x) {
  var ta = new Int32Array(10);

  for (var i = 0; i < 100; ++i) {
    // MCompare::Compare_UIntPtr with result only used on bailout.
    var r = 0 in ta;

    // Path only entered through bailout condition.
    if (x) {
      // |r| must be recovered on bailout here.
      x(r);
    }
  }
}

main();
main(() => {});
