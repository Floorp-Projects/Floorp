var proxy = new Proxy(['a', 'b', 'c'], {});
assertEq([x for each (x in proxy)].toString(), 'a,b,c');
