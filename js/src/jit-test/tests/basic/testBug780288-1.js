// |jit-test| need-for-each

s = newGlobal()
try {
  evalcx("\
	Object.defineProperty(this,\"i\",{enumerable:true,get:function(){t}});\
	for each(y in this)true\
  ", s)
} catch (e) {}
try {
  evalcx("\
	for(z=0,(7).watch(\"\",eval);;g){\
	  if(z=1){({t:function(){}})\
	}\
	", s)
} catch (e) {}
try {
  evalcx("\
	Object.defineProperty(this,\"g2\",{get:function(){return this}});\
	g2.y()\
  ", s)
} catch (e) {}
