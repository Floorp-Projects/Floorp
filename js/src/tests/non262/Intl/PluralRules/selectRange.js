// |reftest| skip-if(!this.hasOwnProperty('Intl')||release_or_beta)

// Any combination returns "other" for "en-US".
{
  let numbers = [0, 0.5, 1.2, 1.5, 1.7, -1, 1, "1", 123456789.123456789, Infinity, -Infinity];

  const weirdCases = [
    NaN,
    "word",
    [0, 2],
    {},
  ];

  for (let type of ["ordinal", "cardinal"]) {
    let pr = new Intl.PluralRules("en-US", {type});
    for (let start of numbers) {
      for (let end of numbers) {
        assertEq(pr.selectRange(start, end), "other");
      }
    }

    for (let c of weirdCases) {
      assertThrowsInstanceOf(() => pr.selectRange(c, 0), RangeError);
      assertThrowsInstanceOf(() => pr.selectRange(0, c), RangeError);
      assertThrowsInstanceOf(() => pr.selectRange(c, c), RangeError);
    }
  }
}

// fr (French) returns different results.
{
  let ordinal = new Intl.PluralRules("fr", {type: "ordinal"});
  let cardinal = new Intl.PluralRules("fr", {type: "cardinal"});

  assertEq(ordinal.selectRange(1, 1), "one");
  assertEq(ordinal.selectRange(0, 1), "other");

  assertEq(cardinal.selectRange(1, 1), "one");
  assertEq(cardinal.selectRange(0, 1), "one");
}

// cy (Cymraeg) can return any combination.
{
  let ordinal = new Intl.PluralRules("cy", {type: "ordinal"});

  assertEq(ordinal.selectRange(0, 0), "other");
  assertEq(ordinal.selectRange(0, 1), "one");
  assertEq(ordinal.selectRange(0, 2), "two");
  assertEq(ordinal.selectRange(0, 3), "few");
  assertEq(ordinal.selectRange(0, 5), "many");
  assertEq(ordinal.selectRange(0, 10), "other");

  assertEq(ordinal.selectRange(1, 1), "other");
  assertEq(ordinal.selectRange(1, 2), "two");
  assertEq(ordinal.selectRange(1, 3), "few");
  assertEq(ordinal.selectRange(1, 5), "many");
  assertEq(ordinal.selectRange(1, 10), "other");

  assertEq(ordinal.selectRange(2, 2), "other");
  assertEq(ordinal.selectRange(2, 3), "few");
  assertEq(ordinal.selectRange(2, 5), "many");
  assertEq(ordinal.selectRange(2, 10), "other");

  assertEq(ordinal.selectRange(3, 3), "other");
  assertEq(ordinal.selectRange(3, 5), "many");
  assertEq(ordinal.selectRange(3, 10), "other");

  assertEq(ordinal.selectRange(5, 5), "other");
  assertEq(ordinal.selectRange(5, 10), "other");

  assertEq(ordinal.selectRange(10, 10), "other");
}

// BigInt inputs aren't allowed.
{
  let pr = new Intl.PluralRules("en-US");

  assertThrowsInstanceOf(() => pr.selectRange(0, 0n), TypeError);
  assertThrowsInstanceOf(() => pr.selectRange(0n, 0), TypeError);
  assertThrowsInstanceOf(() => pr.selectRange(0n, 0n), TypeError);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
