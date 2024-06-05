// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

const disposedInFor = [];
function testDisposalsInForWithBreak() {
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
    if (disposable.value === "c") {
      break;
    }
    disposedInFor.push(disposable.value);
  }
}

testDisposalsInForWithBreak();
assertArrayEq(disposedInFor, ["a", "disposed a", "b", "disposed b", "disposed c"]);


const disposedInWhile = [];
function testDisposalsInWhileWithBreak() {
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
    if (disposable.value === "c") {
      break;
    }
    disposedInWhile.push(disposable.value);
    i++;
  }
}

testDisposalsInWhileWithBreak();
assertArrayEq(disposedInWhile, ["a", "disposed a", "b", "disposed b", "disposed c"]);

const disposedInForOf = [];
function testDisposalsInForOfWithBreak() {
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
    if (disposable.value === "c") {
      break;
    }
    disposedInForOf.push(disposable.value);
  }
}

testDisposalsInForOfWithBreak();
assertArrayEq(disposedInForOf, ["a", "disposed a", "b", "disposed b", "disposed c"]);
