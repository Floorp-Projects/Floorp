async function foo() {
  return new Promise(resolve => {
    setTimeout(resolve, 10);
  });
}

async function stuff() {
  await foo(1);
  await foo(2);
}
