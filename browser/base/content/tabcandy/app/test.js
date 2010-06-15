// Title: test.js (revision-a)

(function(){

var testsRun = 0;
var testsFailed = 0;

// ----------
function ok(value, message) {
  testsRun++;
  if(!value) {
    Utils.log('test failed: ' + message);
    testsFailed ++;
  }
}

// ----------
function is(actual, expected, message) {
  ok(actual == expected, message + '; actual = ' + actual + '; expected = ' + expected);
}

// ----------
function isnot(actual, unexpected, message) {
  ok(actual != unexpected, message + '; actual = ' + actual + '; unexpected = ' + unexpected);
}

// ##########
function TabStub() {
  this.mirror = {
    addOnClose: function() {},
    addSubscriber: function() {},
    removeOnClose: function() {}
  };
  
  this.url = '';
  this.close = function() {};
  this.focus = function() {};
}

// ----------
function test() {
  try {
    Utils.log('unit tests starting -----------');
    
    // ___ iQ
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
    is($div.css('zIndex'), value, 'zIndex set');
  
    // ___ TabItem
    var box = new Rect(10, 10, 100, 100);
    $div = iQ('<div>')
      .addClass('tab')
      .css({
        left: box.left, 
        top: box.top,
        width: box.width,
        height: box.height
      })
      .appendTo('body');
      
    is($div.width(), box.width, 'widths match');
    is($div.height(), box.height, 'heights match');
    
    var tabItem = new TabItem($div.get(0), new TabStub());
    box = tabItem.getBounds();
    tabItem.setBounds(box); 
    ok(box.equals(tabItem.getBounds(), 'set/get match'));
    tabItem.reloadBounds();
    ok(box.equals(tabItem.getBounds(), 'reload match'));   
    
    // ___ done
    Utils.log('unit tests done', testsRun, (testsFailed ? testsFailed + ' tests failed!!' : ''));
  } catch(e) {
    Utils.log(e);
  }
}

test();

})();
