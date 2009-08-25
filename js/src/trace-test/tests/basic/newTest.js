function MyConstructor(i)
{
  this.i = i;
}
MyConstructor.prototype.toString = function() {return this.i + ""};

function newTest()
{
  var a = [];
  for (var i = 0; i < 10; i++)
    a[i] = new MyConstructor(i);
  return a.join("");
}
assertEq(newTest(), "0123456789");
