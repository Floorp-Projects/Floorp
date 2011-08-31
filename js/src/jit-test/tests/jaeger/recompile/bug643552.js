var x = Proxy.create( {get:function(r,name){Proxy = 0;}} );
try { x.watch('e', function(){}); } catch (e) {}
