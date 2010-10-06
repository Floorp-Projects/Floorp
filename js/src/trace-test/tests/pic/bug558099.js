(function()[function() function() function() function() function() function() {}]);
foo = [{
  text: "(function(){if(d){(1)}})",
  s: function() {},
  test: function() {
    try {
      f
    } catch(e) {}
  }
},
{
  text: "(function(){t})",
  s: function() {},
  test: function() {}
},
{
  text: "(function(){if(0){}})",
  s: function() {},
  test: function() {}
},
{
  text: "(function(){if(1){}(2)})",
  s: function() {},
  test: function() {}
},
{
  text: "(function(){g})",
  b: function() {},
  test: function() {}
},
{
  text: "(function(){})",
  s: function() {},
  test: function() {}
},
{
  text: "(function(){1})",
  s: function() {},
  test: function() {}
}]; (function() {
  for (i = 0; i < foo.length; ++i) {
    a = foo[i]
    text = a.text
    eval(text.replace(/@/, ""));
    if (a.test()) {}
  }
} ());
s = [function() function() function() function() function() function() {}]
[function() function() function() function() {}];
(function() { [function() function() {}] });
(function() {});
(eval("\
  (function(){\
    for each(d in[\
      0,0,0,0,0,0,0,0,0,0,0,0,0,null,NaN,1,Boolean(false),Boolean(false)\
    ]){\
      [].filter(new Function,gczeal(2))\
    }\
  })\
"))();
