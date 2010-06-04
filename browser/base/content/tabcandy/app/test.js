// Title: test.js (revision-a)

(function(){

var testsRun = 0;

// ----------
function ok(value, message) {
  testsRun++;
  if(!value)
    Utils.log('test failed: ' + message);
}

// ----------
function is(actual, expected, message) {
  ok(actual == expected, message + '; actual = ' + actual + '; expected = ' + expected);
}

// ----------
function isnot(actual, unexpected, message) {
  ok(actual != unexpected, message + '; actual = ' + actual + '; unexpected = ' + unexpected);
}

// ----------
function test() {
  Utils.log('unit tests starting -----------');
  
  var $div = iQ('<div>');
  ok($div, '$div');
  
  is($div.css('padding-left'), '0px', 'padding-left initial');
  var value = '10px';
  $div.css('padding-left', value);
  is($div.css('padding-left'), value, 'padding-left set');

  is($div.css('z-index'), 'auto', 'z-index initial');
  value = 50;
  $div.css('zIndex', value);
  is($div.css('z-index'), value, 'z-index set');

  Utils.log('unit tests done', testsRun);
}

test();

})();
