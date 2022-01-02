var obj = {valueOf: function() { "use strict"; undeclared = 7; }};
try { '' + obj; assertEq(true, false); } catch(e) { }
try { '' + obj; assertEq(true, false); } catch(e) { }
assertEq("undeclared" in this, false);
