var so = [];

function U(unusedV)
{
  for (var i = 0; i < so.length; ++i)
    return false;
  so.push(0);
}

function C(v)
{
  if (typeof v == "object" || typeof v == "function") {
    for (var i = 0; i < 10; ++i) {}
    U(v);
  }
}

function exploreProperties(obj)
{
  var props = [];
  for (var o = obj; o; o = Object.getPrototypeOf(o)) {
    props = props.concat(Object.getOwnPropertyNames(o));
  }
  for (var i = 0; i < props.length; ++i) {
    var p = props[i];
    try { 
      var v = obj[p];
      C(v);
    } catch(e) { }
  }
}

function boom()
{
  var a = [];
  var b = function(){};
  var c = [{}];
  exploreProperties(a);
  exploreProperties(b);
  exploreProperties(c);
  exploreProperties(c);
}

boom();

