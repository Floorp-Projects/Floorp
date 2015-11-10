// |reftest| skip-if(!xulRuntime.shell) -- needs drainJobQueue

if (!this.Promise) {
    this.reportCompare && reportCompare(true,true);
    quit(0);
}

let results = [];

class SubPromise extends Promise {
  constructor(executor) {
    results.push('SubPromise ctor called');
    super(executor);
  }
  then(res, rej) {
    results.push('SubPromise#then called');
    return intermediatePromise = super.then(res, rej);
  }
}

let subPromise = new SubPromise(function(res, rej) {
  results.push('SubPromise ctor called executor');
  res('result');
});

let intermediatePromise;
let allSubPromise = SubPromise.all([subPromise]);

assertEq(subPromise instanceof SubPromise, true);
assertEq(allSubPromise instanceof SubPromise, true);
assertEq(intermediatePromise instanceof SubPromise, true);

expected = [
'SubPromise ctor called',
'SubPromise ctor called executor',
'SubPromise ctor called',
'SubPromise#then called',
'SubPromise ctor called',
];

assertEq(results.length, expected.length);
expected.forEach((expected,i) => assertEq(results[i], expected));

subPromise.then(val=>results.push('subPromise.then with val ' + val));
allSubPromise.then(val=>results.push('allSubPromise.then with val ' + val));

expected.forEach((expected,i) => assertEq(results[i], expected));
expected = expected.concat([
'SubPromise#then called',
'SubPromise ctor called',
'SubPromise#then called',
'SubPromise ctor called',
]);

assertEq(results.length, expected.length);
expected.forEach((expected,i) => assertEq(results[i], expected));

drainJobQueue();

expected = expected.concat([
'subPromise.then with val result',
'allSubPromise.then with val result',
]);

assertEq(results.length, expected.length);

this.reportCompare && reportCompare(0, 0, "ok");
