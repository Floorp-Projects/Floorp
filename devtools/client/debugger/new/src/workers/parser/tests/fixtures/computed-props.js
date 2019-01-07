(function(key) {
  let obj = {
    b: 5
  };
  obj[key] = 0;
  const c = obj.b;
  return obj;
})("a");
