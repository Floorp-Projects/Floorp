function f(code) {
	    code.replace(/s/, "")
	    eval(code)
	}
	this.__defineGetter__("x", /x/)
	f("function a() {\
	    x = Proxy.createFunction((function () {\
	        return {\
defineProperty:	 function (name, desc) {\
	                Object.defineProperty(x, name, desc)\
	            },\
has:	 function () {},\
get:	 function (r, name) {\
	                return x[name]\
	            }\
	        }\
	    })(), Object.defineProperties).__defineGetter__(\"\",(Function(\"\")))} ;\
	a()\
	")

reportCompare("ok", "ok", "bug 566914");
