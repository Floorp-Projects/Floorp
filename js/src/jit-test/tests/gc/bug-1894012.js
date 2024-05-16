// Exercise the generateMissingAllocSites feature by enabling it an allocating
// a bunch of GC things.
//
// Run this with JS_GC_REPORT_PRETENURE=0 to see data about the generated sites.

gcparam('generateMissingAllocSites', 1);
gcparam('minNurseryBytes', 1024 * 1024);
gcparam('maxNurseryBytes', 1024 * 1024);
gc();

let a = [];

for (let i = 0; i < 10000; i++) {
  let n = new Number(i);
  let s = n.toString();
  let r = s + s;
  a.push([n, s, r]);
}

gcparam('generateMissingAllocSites', 0);
