foo = {}
foo.y = 3;
foo.y = function () {}
Object.defineProperty(foo, "y", { set:function(){} })
gc()
delete foo.y
gc();
