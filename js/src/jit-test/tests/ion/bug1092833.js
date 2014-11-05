// Test that lexicals work with functions with many bindings.

(function() {
    var a01
    var b02
    var c03
    var d04
    var e05
    var f06
    var g07
    var h08
    let i09
    var j10
    var k11
    var l12
    var m13
    var n14
    var o15
    (function n14() {
      assertEq(i09, undefined);
    })()
})();

try {
  (function() {
      var a01
      var b02
      var c03
      var d04
      var e05
      var f06
      var g07
      var h08
      let i09
      var j10
      var k11
      var l12
      var m13
      var n14
      var o15
      (function n14() {
        i12++
      })()
      let i12
  })()
} catch (e) {
  assertEq(e instanceof ReferenceError, true);
  assertEq(e.message.indexOf("i12") > 0, true);
}
