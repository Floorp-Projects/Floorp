// |reftest| skip-if(!this.hasOwnProperty("Intl")||release_or_beta)

// "stripIfInteger" with fractional digits.
{
  let nf1 = new Intl.NumberFormat("en", {
    minimumFractionDigits: 2,
  });

  let nf2 = new Intl.NumberFormat("en", {
    minimumFractionDigits: 2,
    trailingZeroDisplay: "auto",
  });

  let nf3 = new Intl.NumberFormat("en", {
    minimumFractionDigits: 2,
    trailingZeroDisplay: "stripIfInteger",
  });

  assertEq(nf1.resolvedOptions().trailingZeroDisplay, "auto");
  assertEq(nf2.resolvedOptions().trailingZeroDisplay, "auto");
  assertEq(nf3.resolvedOptions().trailingZeroDisplay, "stripIfInteger");

  assertEq(nf1.format(123), "123.00");
  assertEq(nf2.format(123), "123.00");
  assertEq(nf3.format(123), "123");
}

// "stripIfInteger" with significand digits.
{
  let nf1 = new Intl.NumberFormat("en", {
    minimumSignificantDigits: 2,
  });

  let nf2 = new Intl.NumberFormat("en", {
    minimumSignificantDigits: 2,
    trailingZeroDisplay: "auto",
  });

  let nf3 = new Intl.NumberFormat("en", {
    minimumSignificantDigits: 2,
    trailingZeroDisplay: "stripIfInteger",
  });

  assertEq(nf1.resolvedOptions().trailingZeroDisplay, "auto");
  assertEq(nf2.resolvedOptions().trailingZeroDisplay, "auto");
  assertEq(nf3.resolvedOptions().trailingZeroDisplay, "stripIfInteger");

  assertEq(nf1.format(1), "1.0");
  assertEq(nf2.format(1), "1.0");
  assertEq(nf3.format(1), "1");
}

// "stripIfInteger" with rounding increment.
{
  let nf1 = new Intl.NumberFormat("en", {
    maximumFractionDigits: 2,
    roundingIncrement: 5,
  });
  let nf2 = new Intl.NumberFormat("en", {
    maximumFractionDigits: 2,
    roundingIncrement: 5,
    trailingZeroDisplay: "auto",
  });
  let nf3 = new Intl.NumberFormat("en", {
    maximumFractionDigits: 2,
    roundingIncrement: 5,
    trailingZeroDisplay: "stripIfInteger",
  });

  assertEq(nf1.resolvedOptions().trailingZeroDisplay, "auto");
  assertEq(nf2.resolvedOptions().trailingZeroDisplay, "auto");
  assertEq(nf3.resolvedOptions().trailingZeroDisplay, "stripIfInteger");

  // NB: Tests 1.975 twice b/c of <https://unicode-org.atlassian.net/browse/ICU-xxx>.

  assertEq(nf1.format(1.975), "2.00");
  assertEq(nf1.format(1.97), "1.95");
  assertEq(nf1.format(1.975), "2.00");

  assertEq(nf2.format(1.975), "2.00");
  assertEq(nf2.format(1.97), "1.95");
  assertEq(nf2.format(1.975), "2.00");

  assertEq(nf3.format(1.975), "2");
  assertEq(nf3.format(1.97), "1.95");
  assertEq(nf3.format(1.975), "2");
}

// Invalid values.
for (let trailingZeroDisplay of ["", "true", true, "none", "yes", "no"]){
  assertThrowsInstanceOf(() => new Intl.NumberFormat("en", {trailingZeroDisplay}), RangeError);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
