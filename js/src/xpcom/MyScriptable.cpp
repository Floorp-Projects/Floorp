#include "MyScriptable.h"

NS_IMPL_ISUPPORTS(Scriptable(MyScriptable), kIScriptableIID);

nsresult
MyScriptable::m(JSContext* cx, int argc, jsval* args, jsval* rval) 
{
	if (argc<1)
		return NS_ERROR_FAILURE;
	double d;
	if (FromJS(cx,args[0],&d) != NS_OK)
		return NS_ERROR_FAILURE;
	return ToJS(cx,m(d),rval);
}

nsresult
MyScriptable::get(JSContext* cx, char* p, jsval* vp) 
{
	Meth* meth;
	if (strcmp(p,"x") == 0)
		return ToJS(cx,x,vp);
	else if (strcmp(p,"y") == 0)
		return ToJS(cx,y,vp);
	else if (GetMethod("m",&meth) == NS_OK)
		return ToJS(cx,meth,vp);
	else
		return NS_ERROR_FAILURE;
}

nsresult
MyScriptable::put(JSContext* cx, char* p, jsval v) 
{
	double d;
	if (FromJS(cx,v,&d) != NS_OK)
		return NS_ERROR_FAILURE;
	if (strcmp(p,"x") == 0) {
		x = d;
		return NS_OK;
	}
	else if (strcmp(p,"y") == 0) {
		y = d;
		return NS_OK;
	}
	else
		return NS_ERROR_FAILURE;
}

extern "C" {
JS_PUBLIC_API(jsIScriptable*) 
CreateInstance(JSContext* cx)
{
	jsIScriptable* jsi = new MyScriptable();
	return jsi;
}
}
