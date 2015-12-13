testcase();
function testcase() {
    var tokenCodes  = {
      get try() {
        try {
          super.actual();
        } catch (e) {}
      }
    };
    var arr = [
        'try',
    ];
    for (var i = 0; i < arr.length; i++) {
        if (tokenCodes[arr[i]] !== i) {};
    }
}

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
