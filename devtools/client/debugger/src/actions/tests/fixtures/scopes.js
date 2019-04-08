// Program Scope

function outer() {
  function inner() {
    const x = 1;
  }

  const declaration = function() {
    const x = 1;
  };
}
