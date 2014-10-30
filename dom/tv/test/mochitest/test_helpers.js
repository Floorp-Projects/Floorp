"use strict";

function is(a, b, msg) {
  alert((a === b ? 'OK' : 'KO') + ' ' + msg);
}

function ok(a, msg) {
  alert((a ? 'OK' : 'KO')+ ' ' + msg);
}

function info(msg) {
  alert('INFO ' + msg);
}

function finish() {
  alert('DONE');
}
