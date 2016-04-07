setJitCompilerOption("ion.warmup.trigger", 4);

function testBasic() {
  var f = function() {
    var result = "abc".match("b");
    assertEq(result.length, 1);
    assertEq(result.index, 1);
    assertEq(result[0], "b");
  };
  for (var i = 0; i < 40; i++) {
    f();
  }
}
testBasic();

function testMod(apply, unapply) {
  var f = function(applied) {
    var result = "abc".match("b");
    assertEq(result.length, 1);
    if (applied) {
      assertEq(result[0], "mod");
    } else {
      assertEq(result.index, 1);
      assertEq(result[0], "b");
    }
  };
  var applied = false;
  for (var i = 0; i < 120; i++) {
    f(applied);
    if (i == 40) {
      apply();
      applied = true;
    }
    if (i == 80) {
      unapply();
      applied = false;
    }
  }
}
testMod(() => {
  String.prototype[Symbol.match] = () => ["mod"];
}, () => {
  delete String.prototype[Symbol.match];
});
testMod(() => {
  Object.prototype[Symbol.match] = () => ["mod"];
}, () => {
  delete Object.prototype[Symbol.match];
});

testMod(() => {
  Object.setPrototypeOf(String.prototype, {
    [Symbol.match]: () => ["mod"]
  });
}, () => {
  Object.setPrototypeOf(String.prototype, Object.prototype);
});

var orig_exec = RegExp.prototype.exec;
testMod(() => {
  RegExp.prototype.exec = () => ["mod"];
}, () => {
  RegExp.prototype.exec = orig_exec;
});

var orig_match = RegExp.prototype[Symbol.match];
testMod(() => {
  RegExp.prototype[Symbol.match] = () => ["mod"];
}, () => {
  RegExp.prototype[Symbol.match] = orig_match;
});

var observed = false;
function testObserved(apply, unapply) {
  var f = function(applied) {
    observed = false;
    var result = "abc".match("b."); // Use meta char to avoid flat match.
    assertEq(result.length, 1);
    assertEq(result.index, 1);
    assertEq(result[0], "bc");
    assertEq(observed, applied);
  };
  var applied = false;
  for (var i = 0; i < 120; i++) {
    f(applied);
    if (i == 40) {
      apply();
      applied = true;
    }
    if (i == 80) {
      unapply();
      applied = false;
    }
  }
}

var orig_global = Object.getOwnPropertyDescriptor(RegExp.prototype, "global");
testObserved(() => {
  Object.defineProperty(RegExp.prototype, "global", {
    get: function() {
      observed = true;
      return false;
    }
  });
}, () => {
  Object.defineProperty(RegExp.prototype, "global", orig_global);
});

var orig_sticky = Object.getOwnPropertyDescriptor(RegExp.prototype, "sticky");
testObserved(() => {
  Object.defineProperty(RegExp.prototype, "sticky", {
    get: function() {
      observed = true;
      return false;
    }
  });
}, () => {
  Object.defineProperty(RegExp.prototype, "sticky", orig_sticky);
});
