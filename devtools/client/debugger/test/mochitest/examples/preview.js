function empties() {
  const a = "";
  const b = false;
  const c = undefined;
  const d = null;
  debugger;
}

function smalls() {
  const a = "...";
  const b = true;
  const c = 1;
  const d = [];
  const e = {};
  debugger;
}

function objects() {
  const obj = {
    foo: 1
  };

  debugger;
}

function largeArray() {
  let bs = [];
  for (let i = 0; i <= 100; i++) {
    bs.push({ a: 2, b: { c: 3 } });
  }
  debugger;
}
