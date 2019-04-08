// Program Scope

function outer() {
  function inner() {
    const x = 1;
  }

  const arrow = () => {
    const x = 1;
  };

  const declaration = function() {
    const x = 1;
  };

  assignment = (function() {
    const x = 1;
  })();

  const iifeDeclaration = (function() {
    const x = 1;
  })();

  return function() {
    const x = 1;
  };
}

function exclude() {
  function another() {
    const x = 1;
  }
}

const globalArrow = () => {
  const x = 1;
};

const globalDeclaration = function() {
  const x = 1;
};

globalAssignment = (function() {
  const x = 1;
})();

const globalIifeDeclaration = (function() {
  const x = 1;
})();

function parentFunc() {
  let MAX = 3;
  let nums = [0, 1, 2, 3];
  let x = 1;
  let y = nums.find(function(n) {
    return n == x;
  });
  function innerFunc(a) {
    return Math.max(a, MAX);
  }
  return innerFunc(y);
}
