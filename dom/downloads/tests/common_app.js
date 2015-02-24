function is(a, b, msg) {
  alert((a === b ? 'OK' : 'KO') + ' ' + a + ' should equal ' + b + ': ' + msg);
}

function ok(a, msg) {
  alert((a ? 'OK' : 'KO')+ ' ' + msg);
}

function info(msg) {
  alert('INFO ' + msg);
}

function cbError() {
  alert('KO error');
}

function finish() {
  alert('DONE');
}
