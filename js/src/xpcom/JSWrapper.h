#ifndef _JSWrapper
#define _JSWrapper

#include "jsIScriptable.h"
extern const JS_PUBLIC_DATA(nsIID) kIScriptableIID;

// this class makes a JS object be scriptable
DefScriptableClass(JSWrapper);

class JSWrapper : public Scriptable(JSWrapper) {
	JSContext* cx;
	JSObject* obj;
public:
	JSWrapper(JSContext* _cx, JSObject* _obj) : obj(_obj), Scriptable(JSWrapper)() {
		jsval v = OBJECT_TO_JSVAL(obj);
		JS_AddRoot(cx,&v);
	}
	~JSWrapper() {
		jsval v = OBJECT_TO_JSVAL(obj);
		JS_RemoveRoot(cx,&v);
	}
	JSObject* GetJS() { 
		return obj;
	}
	void SetJS(JSObject *o) { 
		obj = o; 
	}
	nsresult get(JSContext* cx, char* p, jsval* vp) {
		return (JS_GetProperty(cx,obj,p,vp) == JS_TRUE ? NS_OK : NS_ERROR_FAILURE);
	}
	nsresult put(JSContext* cx, char* p, jsval v) {
		return (JS_SetProperty(cx,obj,p,&v) == JS_TRUE ? NS_OK : NS_ERROR_FAILURE);
	}
	char** GetIds() {
		return NULL;
	}
	JSObject *getParent(JSContext* cx) {
		return JS_GetParent(cx,obj);
	}
	void setParent(JSContext* cx, JSObject *o) {
		JS_SetParent(cx,obj,o);
	}
	JSObject* getProto(JSContext* cx) {
		return JS_GetPrototype(cx, obj);
	}
	void setProto(JSContext* cx, JSObject *o) {
		JS_SetPrototype(cx,obj,o);
	}
};

#endif
