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
<div id="ensure_scrollbars" style="width:10000px; height:10000px; position:absolute" > </div>
  
<h1>Prototype Unit test file</h1>
<p>
  Test of functions in position.js
</p>

<!-- Log output -->
<div id="testlog"> </div>

<div id="body_absolute" style="position: absolute; top: 10px; left: 10px">
  <div id="absolute_absolute" style="position: absolute; top: 10px; left:10px"> </div>
  <div id="absolute_relative" style="position: relative; top: 10px; left:10px">
    <div style="height:10px">test<span id="inline">test</span></div>
    <div id="absolute_relative_undefined"> </div>
  </div>
</div>
 
<!-- Tests follow -->
<script type="text/javascript" language="javascript" charset="utf-8">
// <![CDATA[

  var testVar = 'to be updated';

  new Test.Unit.Runner({
    
    setup: function() {
      scrollTo(0,0);
      Position.prepare();
      Position.includeScrollOffsets = false;
    },
    
    teardown: function() {
      scrollTo(0,0);
      Position.prepare();
      Position.includeScrollOffsets = false;
    },
    
    testPrepare: function() {with(this) {
      Position.prepare();
      assertEqual(0, Position.deltaX);
      assertEqual(0, Position.deltaY);
      scrollTo(20,30);
      Position.prepare();
      assertEqual(20, Position.deltaX);
      assertEqual(30, Position.deltaY);
    }},
    
    testPositionedOffset: function() {with(this) {
      assertEnumEqual([10,10],
        Position.positionedOffset($('body_absolute')));
      assertEnumEqual([10,10],
        Position.positionedOffset($('absolute_absolute')));
      assertEnumEqual([10,10],
        Position.positionedOffset($('absolute_relative')));
      assertEnumEqual([0,10],
        Position.positionedOffset($('absolute_relative_undefined')));
    }},
    
    testPage: function() {with(this) {
      assertEnumEqual([10,10],
        Position.page($('body_absolute')));
      assertEnumEqual([20,20],
        Position.page($('absolute_absolute')));
      assertEnumEqual([20,20],
        Position.page($('absolute_relative')));
      assertEnumEqual([20,30],
        Position.page($('absolute_relative_undefined')));
    }},
    
    testOffsetParent: function() {with(this) {
      assertEqual('body_absolute', Position.offsetParent($('absolute_absolute')).id);
      assertEqual('body_absolute', Position.offsetParent($('absolute_relative')).id);
      assertEqual('absolute_relative', Position.offsetParent($('inline')).id);
      assertEqual('absolute_relative', Position.offsetParent($('absolute_relative_undefined')).id);
    }},
    
    testWithin: function() {with(this) {
      [true, false].each(function(withScrollOffsets) {
        Position.includeScrollOffsets = withScrollOffsets;
        assert(!Position.within($('body_absolute'), 9, 9), 'outside left/top');
        assert(Position.within($('body_absolute'), 10, 10), 'left/top corner');
        assert(Position.within($('body_absolute'), 10, 19), 'left/bottom corner');
        assert(!Position.within($('body_absolute'), 10, 20), 'outside bottom');
      });
      
      scrollTo(20,30);
      Position.prepare();
      Position.includeScrollOffsets = true;
      assert(!Position.within($('body_absolute'), 9, 9), 'outside left/top');
      assert(Position.within($('body_absolute'), 10, 10), 'left/top corner');
      assert(Position.within($('body_absolute'), 10, 19), 'left/bottom corner');
      assert(!Position.within($('body_absolute'), 10, 20), 'outside bottom');
    }}
    
  }, 'testlog');

// ]]>
</script>
</body>
</html>