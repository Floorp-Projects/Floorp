function ok(aCondition, aMessage) {
  if (aCondition) {
    alert('OK: ' + aMessage);
  } else {
    alert('KO: ' + aMessage);
  }
}

function ready() {
  alert('READY');
}

function done() {
  alert('DONE');
}
