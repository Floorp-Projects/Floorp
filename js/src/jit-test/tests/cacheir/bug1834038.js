function foo() {
  const v4 = [];
  v4[1024] = {};
  v4.splice(0).push(0);
}

for (let v0 = 0; v0 < 1000; v0++) {
    foo();
}
