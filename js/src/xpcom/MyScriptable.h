#include "jsIScriptable.h"
extern const PR_IMPORT_DATA(nsIID) kIScriptableIID;

// this implements an instance of jsIScriptable to
// be loaded into JS.

DefScriptableClass(MyScriptable);

class MyScriptable : public Scriptable(MyScriptable) {
public:
	int x,y;
	int m(int a) {
		return a*a;
	}
	nsresult m(JSContext* cx, int argc, jsval* args, jsval* rval);
	MyScriptable(int _x=42,int _y=99) : 
		x(_x), y(_y), Scriptable(MyScriptable)(1) {
		AddMethod("m",&MyScriptable::m);
	}
	nsresult get(JSContext* cx, char* p, jsval* vp);
	nsresult put(JSContext* cx, char* p, jsval v);
};

