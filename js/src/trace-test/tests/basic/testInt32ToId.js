function testInt32ToId()
{
  // Ensure that a property which is a negative integer that does not fit in a
  // jsval is properly detected by the 'in' operator.
  var obj = { "-1073741828": 17 };
  var index = -1073741819;
  var a = [];
  for (var i = 0; i < 10; i++)
  {
    a.push(index in obj);
    index--;
  }

  // Ensure that a property which is a negative integer that does not fit in a
  // jsval is properly *not* detected by the 'in' operator.  In this case
  // wrongly applying INT_TO_JSID to -2147483648 will shift off the sign bit
  // (the only bit set in that number) and bitwise-or that value with 1,
  // producing jsid(1) -- which actually represents "0", not "-2147483648".
  // Thus 'in' will report a "-2147483648" property when none exists, because
  // it thinks the request was really whether the object had property "0".
  var obj2 = { 0: 17 };
  var b = [];
  var index = -(1 << 28);
  for (var i = 0; i < 10; i++)
  {
    b.push(index in obj2);
    index = index - (1 << 28);
  }

  return a.join(",") + b.join(",");
}

assertEq(testInt32ToId(),   
	 "false,false,false,false,false,false,false,false,false,true" +
	 "false,false,false,false,false,false,false,false,false,false");

checkStats({
  sideExitIntoInterpreter: 2
});
