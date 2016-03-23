// |reftest| skip-if(!xulRuntime.shell) -- needs drainJobQueue

if (!this.Promise) {
    this.reportCompare && reportCompare(true,true);
    quit(0);
}

let results = [];

let p1 = new Promise(res=>res('result'))
  .then(val=>{results.push('then ' + val); return 'first then rval';})
  .then(val=>{results.push('chained then with val: ' + val); return 'p1 then, then'});

let p2 = new Promise((res, rej)=>rej('rejection'))
  .catch(val=> {results.push('catch ' + val); return results.length;})
  .then(val=>{results.push('then after catch with val: ' + val); return 'p2 catch, then'},
        val=>{throw new Error("mustn't be called")});

Promise.all([p1, p2]).then(res => results.push(res + ''));

drainJobQueue();

assertEq(results.length, 5);
assertEq(results[0], 'then result');
assertEq(results[1], 'catch rejection');
assertEq(results[2], 'chained then with val: first then rval');
assertEq(results[3], 'then after catch with val: 2');
assertEq(results[4], 'p1 then, then,p2 catch, then');

this.reportCompare && reportCompare(true,true);
