function testPropagatedFunArgs()
{
  var win = this;
  var res = [], q = [];
  function addEventListener(name, func, flag) {
    q.push(func);
  }

  var pageInfo, obs;
  addEventListener("load", handleLoad, true);

  var observer = {
    observe: function(win, topic, data) {
      // obs.removeObserver(observer, "page-info-dialog-loaded");
      handlePageInfo();
    }
  };

  function handleLoad() {
    pageInfo = { toString: function() { return "pageInfo"; } };
    obs = { addObserver: function (obs, topic, data) { obs.observe(win, topic, data); } };
    obs.addObserver(observer, "page-info-dialog-loaded", false);
  }

  function handlePageInfo() {
    res.push(pageInfo);
    function $(aId) { res.push(pageInfo); };
    var feedTab = $("feedTab");
  }

  q[0]();
  return res.join(',');
}
assertEq(testPropagatedFunArgs(), "pageInfo,pageInfo");
