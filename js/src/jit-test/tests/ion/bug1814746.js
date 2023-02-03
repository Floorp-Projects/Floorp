function testObjSetSelf(key) {
  let obj = {};
  obj[key] = obj;
}
for (let i = 0; i < 100; i++) {
  testObjSetSelf("a" + i);
}
