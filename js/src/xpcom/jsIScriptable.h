#ifndef _jsIScriptable
#define _jsIScriptable
#include "jsapi.h"
#include "nsISupports.h"

#define JS_ISCRIPTABLE_IID \
    { 0, 0, 0, \
	{0, 0, 0, 0, 0, 0, 0, 0}}

class Meth {
public:
	virtual nsresult invoke(int argc, jsval* args, jsval* rval) =0;
	virtual	void SetJSContext(JSContext* cx) =0;
	virtual	JSContext* GetJSContext() =0;
	virtual	void SetJSVal(JSContext* cx, jsval v) =0;
	virtual jsval GetJSVal(JSContext* cx) =0;
	virtual int AddRef() =0;
	virtual int SubRef() =0;
};

#define Method(C) Method_##C
#define DefMethodClass(C)\
class C;\
class Method_##C : public Meth {\
	JSContext* cx;	\
	jsval mval;\
	int _ref;\
public:\
	C* thisptr;	\
	nsresult (C::*method)(JSContext* cx, int argc, jsval* args, jsval* rval);\
	virtual nsresult invoke(int argc, jsval* args, jsval* rval) {\
		return (thisptr->*method)(cx,argc,args,rval);\
	}\
	virtual void SetJSContext(JSContext* _cx) {\
        cx = _cx;\
	}\
	virtual JSContext* GetJSContext() {\
        return cx;\
	}\
	virtual void SetJSVal(JSContext* cx, jsval v) {\
		if (mval)\
				JS_RemoveRoot(cx,&mval);\
		mval = v;\
		JS_AddRoot(cx,&mval);\
	}\
	virtual jsval GetJSVal(JSContext* cx) {\
		return mval;\
	}\
	virtual int AddRef() {\
		return _ref++;\
	}\
	virtual int SubRef() {\
		return --_ref;\
	}\
	Method_##C() : cx(NULL), mval(0), _ref(0) {}\
	~Method_##C() {\
		if (mval)\
			JS_RemoveRoot(cx,&mval);\
	}\
}

class jsIScriptable : public nsISupports {
public:
	// static conversion between JS and C++
	static JS_PUBLIC_API(nsresult) FromJS(JSContext* cx, jsval v, double* d);
	static JS_PUBLIC_API(nsresult) FromJS(JSContext* cx, jsval v, char** s, size_t n);
	static JS_PUBLIC_API(nsresult) FromJS(JSContext* cx, jsval v, jsIScriptable** o);
	static JS_PUBLIC_API(nsresult) ToJS(JSContext* cx, double d, jsval* v);
	static JS_PUBLIC_API(nsresult) ToJS(JSContext* cx, char* s, jsval* v);
	static JS_PUBLIC_API(nsresult) ToJS(JSContext* cx, jsIScriptable* o, jsval* v);
	static JS_PUBLIC_API(nsresult) ToJS(JSContext* cx, Meth* m, jsval* v);
	virtual JSObject* GetJS() =0;			// returns the peer JS object
	virtual void SetJS(JSObject *o) =0;	// sets the peer
	virtual nsresult get(JSContext* cx, char* p, jsval* v) =0; // gets the property
	virtual nsresult put(JSContext* cx, char* p, jsval v) =0;	// sets ...
	virtual char** GetIds() =0;    // returns an array with the identifiers (ints or strings) that can be enumerated
	virtual JSObject *getParent(JSContext* cx) =0;
	virtual void setParent(JSContext* cx, JSObject *o) =0; 
	virtual JSObject* getProto(JSContext* cx) =0;
	virtual void setProto(JSContext* cx, JSObject *o) =0;
};

#define Scriptable(C) jsScriptable_##C
#define DefScriptableClass(C)\
DefMethodClass(C);\
class jsScriptable_##C : public jsIScriptable {\
	JSObject* obj;\
	int _ref;\
	int sz;\
	char** names;\
    Method(C)* meths;\
public:\
    NS_DECL_ISUPPORTS\
	jsScriptable_##C(int _sz=0) : _ref(0), sz(_sz), obj(NULL) {\
		names = new char*[sz];\
		meths = new Method(C)[sz];\
		for (int i=0; i<sz; i++)\
			names[i] = NULL;\
	}\
	~jsScriptable_##C() {\
		delete names;\
		delete meths;\
	}\
	nsresult AddMethod(char* nm, nsresult (C::*method)(JSContext*, int, jsval*, jsval*)) {\
		for (int i=0; i<sz; i++) \
			if (names[i] == NULL) {\
				names[i] = nm;\
				meths[i].method = method;\
				meths[i].thisptr = (C*)this;\
				return NS_OK;\
			}\
		return NS_ERROR_FAILURE;\
	}\
	nsresult GetMethod(char* nm, Meth** meth) {\
		for (int i=0; i<sz; i++) \
			if (strcmp(names[i],nm) == 0) {\
                *meth = &meths[i];\
				return NS_OK;\
			}\
		return NS_ERROR_FAILURE;\
	}\
	nsresult RemoveMethod(char* nm) {\
		for (int i=0; i<sz; i++) \
			if (strcmp(names[i],nm) == 0) {\
				names[i] = NULL;\
				return NS_OK;\
			}\
		return NS_ERROR_FAILURE;\
	}\
	virtual JSObject* GetJS() { \
		return obj;\
	}\
	virtual	void SetJS(JSObject *o) { \
		obj = o; \
	}\
	char** GetIds() {\
		return NULL;\
	}\
	virtual JSObject *getParent(JSContext* cx) {\
		return (obj ? JS_GetParent(cx,obj) : NULL);\
	}\
	virtual void setParent(JSContext* cx, JSObject *o) {\
		if (obj) JS_SetParent(cx,obj,o);\
	}\
	virtual JSObject* getPrototype(JSContext* cx) {\
		return (obj ? JS_GetPrototype(cx, obj) : NULL);\
	}\
	virtual void setPrototype(JSContext* cx, JSObject *o) {\
		if (obj) JS_SetPrototype(cx,obj,o);\
	}\
};\

#endif
