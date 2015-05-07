var log = [];
var resolvedPromise = Promise.resolve(null);
function schedulePromiseTask(f) {
  resolvedPromise.then(f);
}

setTimeout(function() {
  log.push('t1start');
  schedulePromiseTask(function() {
    log.push('promise');
  });
  log.push('t1end');
}, 10);

setTimeout(function() {
  log.push('t2');
  postMessage(log.join(', '));
}, 10);
