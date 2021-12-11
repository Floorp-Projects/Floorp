var arr = [];
Object.defineProperty(arr, 4, {
  configurable: true,
  enumerable: false,
  writable: false,
  value: undefined
});
for (var y = 0; y < 2; y++)
  arr.length = 0;
