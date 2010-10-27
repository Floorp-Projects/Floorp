function typeofTest()
{
  var values = ["hi", "hi", "hi", null, 5, 5.1, true, undefined, /foo/, typeofTest, [], {}], types = [];
  for (var i = 0; i < values.length; i++)
    types[i] = typeof values[i];
  return types.toString();
}
assertEq(typeofTest(), "string,string,string,object,number,number,boolean,undefined,object,function,object,object");
