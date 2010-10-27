// Second testPropagatedFunArgs test -- this is a crash-test.
(function () {
  var escapee;

  function testPropagatedFunArgs()
  {
    const magic = 42;

    var win = this;
    var res = [], q = [];
    function addEventListener(name, func, flag) {
      q.push(func);
    }

    var pageInfo = "pageInfo", obs;
    addEventListener("load", handleLoad, true);

    var observer = {
      observe: function(win, topic, data) {
        // obs.removeObserver(observer, "page-info-dialog-loaded");
        handlePageInfo();
      }
    };

    function handleLoad() {
      //pageInfo = { toString: function() { return "pageInfo"; } };
      obs = { addObserver: function (obs, topic, data) { obs.observe(win, topic, data); } };
      obs.addObserver(observer, "page-info-dialog-loaded", false);
    }

    function handlePageInfo() {
      res.push(pageInfo);
      function $(aId) {
        function notSafe() {
          return magic;
        }
        notSafe();
        res.push(pageInfo);
      };
      var feedTab = $("feedTab");
    }

    escapee = q[0];
    return res.join(',');
  }

  testPropagatedFunArgs();

  escapee();
})();

function testStringLengthNoTinyId()
{
  var x = "unset";
  var t = new String("");
  for (var i = 0; i < 5; i++)
    x = t["-1"];

  var r = "t['-1'] is " + x;
  t["-1"] = "foo";
  r += " when unset, '" + t["-1"] + "' when set";
  return r;
}
assertEq(testStringLengthNoTinyId(), "t['-1'] is undefined when unset, 'foo' when set");
