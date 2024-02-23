oomTest(new Function(`
  var a = ['p', 'q', 'r', 's', 't'];
  var o = {p:1, q:2, r:3, s:4, t:5};
  for (var i in o)
    delete o[i];
  for (var i of a)
    o.hasOwnProperty(undefined + this, false);
`));
