let i = 10000;
oomTest(() => {
  let arr = [];
  arr[i++] = 1;
  for (var key in arr) {}
});
