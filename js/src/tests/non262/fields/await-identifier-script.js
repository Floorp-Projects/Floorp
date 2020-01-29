var await = 1;

async function getClass() {
  return class {
    x = await;
  };
}

getClass().then(cl => {
  assertEq(new cl().x, 1);
});

assertEq(raisesException(SyntaxError)(`
async () => class { [await] = 1 };
`), true);

assertEq(raisesException(SyntaxError)(`
  async () => class { x = await 1 };
`), true);

if (typeof reportCompare === "function")
  reportCompare(true, true);
