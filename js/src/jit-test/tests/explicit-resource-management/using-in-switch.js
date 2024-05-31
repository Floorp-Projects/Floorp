// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

let disposedInSwitchCase = [];
function testDisposalsInSwitchCase(caseNumber) {
  using a = {
    [Symbol.dispose]: () => disposedInSwitchCase.push("a")
  };

  switch (caseNumber) {
    case 1:
      using b = {
        [Symbol.dispose]: () => disposedInSwitchCase.push("b")
      };
      break;
    case 2:
      using c = {
        [Symbol.dispose]: () => disposedInSwitchCase.push("c")
      };
      break;
  }

  using d = {
    [Symbol.dispose]: () => disposedInSwitchCase.push("d")
  };

  disposedInSwitchCase.push("e");
}
testDisposalsInSwitchCase(1);
assertArrayEq(disposedInSwitchCase, ["b", "e", "d", "a"]);
disposedInSwitchCase = [];
testDisposalsInSwitchCase(2);
assertArrayEq(disposedInSwitchCase, ["c", "e", "d", "a"]);
