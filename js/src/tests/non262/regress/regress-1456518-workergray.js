if (typeof 'evalInWorder' == 'function') {
    evalInWorker(`
  addMarkObservers([grayRoot(), grayRoot().x, this, Object.create(null)]);
`);
}

reportCompare('do not crash', 'do not crash', 'did not crash!');
