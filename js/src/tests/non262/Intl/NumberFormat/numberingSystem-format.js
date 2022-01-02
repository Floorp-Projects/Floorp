for (let [numberingSystem, {digits, algorithmic}] of Object.entries(numberingSystems)) {
  if (algorithmic) {
    // We don't yet support algorithmic numbering systems.
    continue;
  }

  let nf = new Intl.NumberFormat("en", {numberingSystem});

  assertEq([...digits].length, 10, "expect exactly ten digits for each numbering system");

  let i = 0;
  for (let digit of digits) {
    assertEq(nf.format(i++), digit);
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
