#ifndef __JAVA_DOM_NATIVE_PROXY_LISTENER__
#define __JAVA_DOM_NATIVE_PROXY_LISTENER__

#include"nsIDOMEventListener.h"
#include"jni.h"

class NativeDOMProxyListener: public nsIDOMEventListener{

    private:
        JavaVM *vm;
        jobject listener;
        nsrefcnt mRefCnt;  

	//to be used only by Release()
	virtual ~NativeDOMProxyListener();
    public:

	NativeDOMProxyListener(JNIEnv *env, jobject jlistener);

	NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

	NS_IMETHOD QueryInterface(const nsIID &aIID,  void **aResult);  
	NS_IMETHOD_(nsrefcnt) AddRef(void);  
	NS_IMETHOD_(nsrefcnt) Release(void);  

};

#endif

