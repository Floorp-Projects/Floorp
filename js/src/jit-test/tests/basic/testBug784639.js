evalcx("\
  Object.defineProperty(this, \"a\", {});\
  f = (function(j) {\
	  a = Proxy\
  });\
  Object.defineProperty(this, \"g\", {\
	  get: function() {\
		  return ({\
			  r: function() {},\
			  t: function() {}\
		  })\
	  }\
  });\
  for (p in g) {\
	  f(1)\
  }\
", newGlobal())
