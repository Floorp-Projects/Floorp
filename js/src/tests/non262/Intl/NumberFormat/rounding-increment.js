// |reftest| skip-if(!this.hasOwnProperty("Intl")||release_or_beta)

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

// The default |maximumFractionDigits| value can lead to surprising results
// when |roundingIncrement| is used.
{
  let nf = new Intl.NumberFormat("en", {
    roundingIncrement: 10,
  });

  let resolved = nf.resolvedOptions();
  assertEq(resolved.minimumFractionDigits, 0);
  assertEq(resolved.maximumFractionDigits, 3);
  assertEq(resolved.roundingIncrement, 10);

  // |roundingIncrement| is relative to |maximumFractionDigits|, so when
  // |maximumFractionDigits| is unchanged, the rounding is applied to the
  // default maximum fraction digits value.
  assertEq(nf.format(123), "123.000");
  assertEq(nf.format(123.456), "123.460");
}

// Invalid values.
for (let roundingIncrement of [-1, 0, Infinity, NaN]){
  assertThrowsInstanceOf(() => new Intl.NumberFormat("en", {roundingIncrement}), RangeError);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
