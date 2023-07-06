// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
const iter = (value) => Iterator.from({
  next: () => value,
});

for (let value of [
  undefined,
  null,
  0,
  false,
  "test",
  Symbol(""),
]) {
  assertEq(iter(value).next(), value);
}

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
