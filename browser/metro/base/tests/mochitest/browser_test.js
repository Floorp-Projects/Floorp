// Tests for the test functions in head.js

function test() {
  waitForExplicitFinish();
  runTests();
}

gTests.push({
  desc: "task sanity check",
  run: function() {
    let sum2plus2 = yield asyncSum(2, 2);
    ok(sum2plus2 == 4, "asyncSum responded 2+2=4");

    function asyncSum(a, b) {
      var defd = Promise.defer();
      setTimeout(function(){
        defd.resolve(a+b);
      }, 25);
      return defd.promise;
    }
  }
});

gTests.push({
  desc: "addTab",
  run: function testAddTab() {
    let tab = yield addTab("http://example.com/");
    is(tab, Browser.selectedTab, "The new tab is selected");
  }
});
