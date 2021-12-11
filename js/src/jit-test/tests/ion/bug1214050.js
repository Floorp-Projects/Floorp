eval(`
    with ({}) {
      var f = function() {};
    }
    function f() {}
`);
