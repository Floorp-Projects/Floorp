function comparer(a, b) {
  const ma = /.(\d+)\W*$/.exec(a);
  const mb = /.(\d+)\W*$/.exec(b);
  if (ma == null || mb == null || ma[1] == mb[1]) {
    return a < b ? -1 : a > b ? 1 : 0;
  } else {
    const na = +ma[1],
      nb = +mb[1];
    return na < nb ? -1 : na > nb ? 1 : 0;
  }
}

function binaryLookup(ar, i, comparer) {
  if (ar.length == 0) {
    return { found: false, index: 0 };
  }
  let l = 0,
    r = ar.length - 1;
  while (l < r) {
    const mid = Math.floor((l + r) / 2);
    if (comparer(ar[mid], i) < 0) {
      l = mid + 1;
    } else {
      r = mid;
    }
  }
  const result = comparer(ar[l], i);
  if (result === 0) {
    return { found: true, index: l };
  }
  return {
    found: false,
    index: result < 0 ? l + 1 : l
  };
}

export function fancySort(input) {
  return input.reduce((ar, i) => {
    const { index } = binaryLookup(ar, i, comparer);
    ar.splice(index, 0, i);
    return ar;
  }, []);
}
