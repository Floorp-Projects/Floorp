<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
        "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
  <title>Prototype Unit test file</title>
  <meta http-equiv="content-type" content="text/html; charset=utf-8" />
  <script src="../../dist/prototype.js" type="text/javascript"></script>
  <script src="../lib/unittest.js" type="text/javascript"></script>
  <link rel="stylesheet" href="../test.css" type="text/css" />
  <style type="text/css" media="screen">
  /* <![CDATA[ */
    #testcss1 { font-size:11px; color: #f00; }
    #testcss2 { font-size:12px; color: #0f0; display: none; }
  /* ]]> */
  </style>
</head>
<body>
<h1>Prototype Unit test file</h1>
<p>
  Test of utility functions in range.js
</p>

<!-- Log output -->
<div id="testlog"> </div>

<!-- Tests follow -->
<script type="text/javascript" language="javascript" charset="utf-8">
// <![CDATA[
  new Test.Unit.Runner({
    
    testInclude: function() {with(this) {
      assert(!$R(0, 0, true).include(0));
      assert($R(0, 0, false).include(0));

      assert($R(0, 5, true).include(0));
      assert($R(0, 5, true).include(4));
      assert(!$R(0, 5, true).include(5));

      assert($R(0, 5, false).include(0));
      assert($R(0, 5, false).include(5));
      assert(!$R(0, 5, false).include(6));
    }},

    testEach: function() {with(this) {
      var results = [];
      $R(0, 0, true).each(function(value) {
        results.push(value);
      });

      assertEnumEqual([], results);

      results = [];
      $R(0, 3, false).each(function(value) {
        results.push(value);
      });

      assertEnumEqual([0, 1, 2, 3], results);
    }},

    testAny: function() {with(this) {
      assert(!$R(1, 1, true).any());
      assert($R(0, 3, false).any(function(value) {
        return value == 3;
      }));
    }},

    testAll: function() {with(this) {
      assert($R(1, 1, true).all());
      assert($R(0, 3, false).all(function(value) {
        return value <= 3;
      }));
    }},

    testToArray: function() {with(this) {
      assertEnumEqual([], $R(0, 0, true).toArray());
      assertEnumEqual([0], $R(0, 0, false).toArray());
      assertEnumEqual([0], $R(0, 1, true).toArray());
      assertEnumEqual([0, 1], $R(0, 1, false).toArray());
      assertEnumEqual([-3, -2, -1, 0, 1, 2], $R(-3, 3, true).toArray());
      assertEnumEqual([-3, -2, -1, 0, 1, 2, 3], $R(-3, 3, false).toArray());
    }},
    
    testDefaultsToNotExclusive: function() {with(this) {
      assertEnumEqual(
        $R(-3,3), $R(-3,3,false)
      );
    }}
    
  }, 'testlog');
// ]]>
</script>
</body>
</html>