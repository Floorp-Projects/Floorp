// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Nickel rounding.
{
  let nf = new Intl.NumberFormat("en", {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2,
    roundingIncrement: 5,
  });

  assertEq(nf.format(1.22), "1.20");
  assertEq(nf.format(1.224), "1.20");
  assertEq(nf.format(1.225), "1.25");
  assertEq(nf.format(1.23), "1.25");
}

// Dime rounding.
{
  let nf = new Intl.NumberFormat("en", {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2,
    roundingIncrement: 10,
  });

  assertEq(nf.format(1.24), "1.20");
  assertEq(nf.format(1.249), "1.20");
  assertEq(nf.format(1.250), "1.30");
  assertEq(nf.format(1.25), "1.30");
}

// Rounding increment option is rounded down.
{
  let nf1 = new Intl.NumberFormat("en", {
    minimumFractionDigits: 0,
    maximumFractionDigits: 0,
    roundingIncrement: 10,
  });

  let nf2 = new Intl.NumberFormat("en", {
    minimumFractionDigits: 0,
    maximumFractionDigits: 0,
    roundingIncrement: 10.1,
  });

  let nf3 = new Intl.NumberFormat("en", {
    minimumFractionDigits: 0,
    maximumFractionDigits: 0,
    roundingIncrement: 10.9,
  });

  assertEq(nf1.resolvedOptions().roundingIncrement, 10);
  assertEq(nf2.resolvedOptions().roundingIncrement, 10);
  assertEq(nf3.resolvedOptions().roundingIncrement, 10);

  assertEq(nf1.format(123), "120");
  assertEq(nf2.format(123), "120");
  assertEq(nf3.format(123), "120");
}

// |minimumFractionDigits| must be equal to |maximumFractionDigits| when
// |roundingIncrement| is used.
//
// |minimumFractionDigits| defaults to zero.
{
  let nf = new Intl.NumberFormat("en", {
    roundingIncrement: 10,
    // minimumFractionDigits: 0, (default)
    maximumFractionDigits: 0,
  });

  let resolved = nf.resolvedOptions();
  assertEq(resolved.minimumFractionDigits, 0);
  assertEq(resolved.maximumFractionDigits, 0);
  assertEq(resolved.roundingIncrement, 10);

  assertEq(nf.format(123), "120");
  assertEq(nf.format(123.456), "120");
}

// |maximumFractionDigitsDefault| is set to |minimumFractionDigitsDefault| when
// roundingIncrement isn't equal to 1.
{
  let options = {
    roundingIncrement: 10,
    // minimumFractionDigits: 0, (default)
    // maximumFractionDigits: 0, (default)
  };
  let nf = new Intl.NumberFormat("en", options);
  assertEq(nf.resolvedOptions().minimumFractionDigits, 0);
  assertEq(nf.resolvedOptions().maximumFractionDigits, 0);
}

// |maximumFractionDigits| must be equal to |minimumFractionDigits| when
// roundingIncrement isn't equal to 1.
{
  let options = {
    roundingIncrement: 10,
    // minimumFractionDigits: 0, (default)
    maximumFractionDigits: 1,
  };
  assertThrowsInstanceOf(() => new Intl.NumberFormat("en", options), RangeError);
}

// Invalid values.
for (let roundingIncrement of [-1, 0, Infinity, NaN]){
  let options = {
    roundingIncrement,
    minimumFractionDigits: 0,
    maximumFractionDigits: 0,
  };
  assertThrowsInstanceOf(() => new Intl.NumberFormat("en", options), RangeError);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
