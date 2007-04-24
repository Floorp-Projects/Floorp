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
  Test the unit testing library (unittest.js)
</p>

<!-- Log output -->
<div id="testlog"> </div>

<!-- Test elements follow -->
<div id="test_1" class="a bbbbbbbbbbbb cccccccccc dddd"> </div>

<div id="test_2"> <span> </span> 



<div><div></div> </div><span> </span>
</div>

<ul id="tlist"><li id="tlist_1">x1</li><li id="tlist_2">x2</li></ul>
<ul id="tlist2"><li class="a" id="tlist2_1">x1</li><li id="tlist2_2">x2</li></ul>

<div id="testmoveby" style="background-color:#333;width:100px;">XXXX</div>

<div id="testcss1">testcss1<span id="testcss1_span" style="display:none;">blah</span></div><div id="testcss2">testcss1</div>

<!-- Tests follow -->
<script type="text/javascript" language="javascript" charset="utf-8">
// <![CDATA[

  var testObj = {
    isNice: function(){
      return true;
    },
    isBroken: function(){
      return false;
    }
  }

  new Test.Unit.Runner({
    
    testAssertEqual: function() { with(this) {
      assertEqual(0, 0);
      assertEqual(0, 0, "test");
      
      assertEqual(0,'0');
      assertEqual(65.0, 65);
      
      assertEqual("a", "a");
      assertEqual("a", "a", "test");
      
      assertNotEqual(0, 1);
      assertNotEqual("a","b");
      assertNotEqual({},{});
      assertNotEqual([],[]);
      assertNotEqual([],{});
    }},

    testAssertEnumEqual: function() { with(this) {
      assertEnumEqual([], []);
      assertEnumEqual(['a', 'b'], ['a', 'b']);
      assertEnumEqual(['1', '2'], [1, 2]);
      assertEnumNotEqual(['1', '2'], [1, 2, 3]);
    }},
    
    testAssertHashEqual: function() { with(this) {
      assertHashEqual({}, {});
      assertHashEqual({a:'b'}, {a:'b'});
      assertHashEqual({a:'b', c:'d'}, {c:'d', a:'b'});
      assertHashNotEqual({a:'b', c:'d'}, {c:'d', a:'boo!'});
    }},
    
    testAssertRespondsTo: function() { with(this) {
      assertRespondsTo('isNice', testObj);
      assertRespondsTo('isBroken', testObj);
    }},
    
    testAssertIdentical: function() { with(this) { 
      assertIdentical(0, 0); 
      assertIdentical(0, 0, "test"); 
      assertIdentical(1, 1); 
      assertIdentical('a', 'a'); 
      assertIdentical('a', 'a', "test"); 
      assertIdentical('', ''); 
      assertIdentical(undefined, undefined); 
      assertIdentical(null, null); 
      assertIdentical(true, true); 
      assertIdentical(false, false); 
      
      var obj = {a:'b'};
      assertIdentical(obj, obj);
      
      assertNotIdentical({1:2,3:4},{1:2,3:4});
      
      assertIdentical(1, 1.0); // both are typeof == 'number'
      
      assertNotIdentical(1, '1');
      assertNotIdentical(1, '1.0');
    }},
    
    testAssertNullAndAssertUndefined: function() { with(this) {
      assertNull(null);
      assertNotNull(undefined);
      assertNotNull(0);
      assertNotNull('');
      assertNotUndefined(null);
      assertUndefined(undefined);
      assertNotUndefined(0);
      assertNotUndefined('');
      assertNullOrUndefined(null);
      assertNullOrUndefined(undefined);
      assertNotNullOrUndefined(0);
      assertNotNullOrUndefined('');
    }},
    
    testAssertMatch: function() { with(this) {
      assertMatch(/knowmad.jpg$/, 'http://script.aculo.us/images/knowmad.jpg');
      assertMatch(/Fuc/, 'Thomas Fuchs');
      assertMatch(/^\$(\d{1,3}(\,\d{3})*|(\d+))(\.\d{2})?$/, '$19.95');
      assertMatch(/(\d{3}\) ?)|(\d{3}[- \.])?\d{3}[- \.]\d{4}(\s(x\d+)?){0,1}$/, '704-343-9330');
      assertMatch(/^(?:(?:(?:(?:(?:1[6-9]|[2-9]\d)?(?:0[48]|[2468][048]|[13579][26])|(?:(?:16|[2468][048]|[3579][26])00)))(\/|-|\.)(?:0?2\1(?:29)))|(?:(?:(?:1[6-9]|[2-9]\d)?\d{2})(\/|-|\.)(?:(?:(?:0?[13578]|1[02])\2(?:31))|(?:(?:0?[1,3-9]|1[0-2])\2(29|30))|(?:(?:0?[1-9])|(?:1[0-2]))\2(?:0?[1-9]|1\d|2[0-8]))))$/, '2001-06-16');
      assertMatch(/^((0?[123456789])|(1[012]))\s*:\s*([012345]\d)(\s*:\s*([012345]\d))?\s*[ap]m\s*-\s*((0?[123456789])|(1[012]))\s*:\s*([012345]\d)(\s*:\s*([012345]\d))?\s*[ap]m$/i, '2:00PM-2:15PM');
      assertNoMatch(/zubar/, 'foo bar');
    }},
    
    testAssertInstanceOf: function() { with(this) {
      assertInstanceOf(String, new String);
      assertInstanceOf(RegExp, /foo/);
      assertNotInstanceOf(String, {});
    }},
    
    testAssertVisible: function() { with(this) {
      assertVisible('testcss1');
      assertNotVisible('testcss1_span');
      //assertNotVisible('testcss2', "Due to a Safari bug, this test fails in Safari.");
      
      Element.hide('testcss1');
      assertNotVisible('testcss1');
      assertNotVisible('testcss1_span');
      Element.show('testcss1');
      assertVisible('testcss1');
      assertNotVisible('testcss1_span');
      
      Element.show('testcss1_span');
      assertVisible('testcss1_span');
      Element.hide('testcss1');
      assertNotVisible('testcss1_span'); // hidden by parent
    }}
      
  }, "testlog");
// ]]>
</script>
</body>
</html>
