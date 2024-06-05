// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

const disposedInFor = [];
function testDisposalsInForWithContinue() {
  const disposables = [
    {
      value: "a",
      [Symbol.dispose]: () => disposedInFor.push("disposed a")
    },
    {
      value: "b",
      [Symbol.dispose]: () => disposedInFor.push("disposed b")
    },
    {
      value: "c",
      [Symbol.dispose]: () => disposedInFor.push("disposed c")
    }
  ];

  for (let i = 0; i < disposables.length; i++) {
    using disposable = disposables[i];
    if (disposable.value === "b") {
      continue;
    }
    disposedInFor.push(disposable.value);
  }
}

testDisposalsInForWithContinue();
assertArrayEq(disposedInFor, ["a", "disposed a", "disposed b", "c", "disposed c"]);

const disposedInWhile = [];
function testDisposalsInWhileWithContinue() {
  const disposables = [
    {
      value: "a",
      [Symbol.dispose]: () => disposedInWhile.push("disposed a")
    },
    {
      value: "b",
      [Symbol.dispose]: () => disposedInWhile.push("disposed b")
    },
    {
      value: "c",
      [Symbol.dispose]: () => disposedInWhile.push("disposed c")
    }
  ];

  let i = 0;
  while (i < disposables.length) {
    using disposable = disposables[i];
    if (disposable.value === "b") {
      i++;
      continue;
    }
    disposedInWhile.push(disposable.value);
    i++;
  }
}

testDisposalsInWhileWithContinue();
assertArrayEq(disposedInWhile, ["a", "disposed a", "disposed b", "c", "disposed c"]);

const disposedInForOf = [];
function testDisposalsInForOfWithContinue() {
  const disposables = [
    {
      value: "a",
      [Symbol.dispose]: () => disposedInForOf.push("disposed a")
    },
    {
      value: "b",
      [Symbol.dispose]: () => disposedInForOf.push("disposed b")
    },
    {
      value: "c",
      [Symbol.dispose]: () => disposedInForOf.push("disposed c")
    }
  ];

  for (using disposable of disposables) {
    if (disposable.value === "b") {
      continue;
    }
    disposedInForOf.push(disposable.value);
  }
}

testDisposalsInForOfWithContinue();
assertArrayEq(disposedInForOf, ["a", "disposed a", "disposed b", "c", "disposed c"]);
