/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* implement nsIXPCTestOverloaded as an example. */

#include "xpctest_private.h"
#include "nsIXPCScriptable.h"

/*
* This is an example of one way to reflect an interface into JavaScript such
* that one method name is overloaded to reflect multiple methods. This practice
* is strongly discouraged. But, some legacy JavaScript interfaces require this
* in order to support existing JavaScript code.
*/

/***************************************************************************/
/* This is a JS example of calling the object implemented below. */

/*
* // to run this in the shell...
* // put this in "foo.js" and the run "xpcshell foo.js"
*
*  var clazz = Components.classes.nsOverloaded;
*  var iface = Components.interfaces.nsIXPCTestOverloaded;
*
*  foo = clazz.createInstance(iface);
*
*  try {
*      print("foo.Foo1(1)...  ");  foo.Foo1(1)
*      print("foo.Foo2(1,2)...");  foo.Foo2(1,2)
*      print("foo.Foo(3)...   ");  foo.Foo(3)
*      print("foo.Foo(3,4)... ");  foo.Foo(3,4)
*      print("foo.Foo()...    ");  foo.Foo();
*  } catch(e) {
*      print("caught exception: "+e);
*  }
*
*/

/***************************************************************************/

/*
* This is the implementation of nsIXPCScriptable. This interface is used
* by xpconnect in order to allow wrapped native objects to gain greater
* control over how they are reflected into JavaScript. Most wrapped native
* objects do not need to implement this interface. It is useful for dynamic
* properties (those properties not explicitly mentioned in the .idl file).
* Here we are using the nsIXPCScriptable as a way to bootstrap some JS code
* to be run each time a wrapper is built around an instance of our object.
*
* xpconnect allows implementors of nsIXPCScriptable to bend the rules a bit...
* implementations of nsIXPCScriptable are not required to follow QueryInterface
* identity rules; i.e. doing a QI(nsISupports::GetIID()) on this interface is
* not required to return the same pointer as doing so on the object that
* presented this interface. Thus, it is allowable to implement only one
* nsIXPCScriptable instance per class if desired.
*/

class xpcoverloadedScriptable : public nsIXPCScriptable
{
public:
    NS_DECL_ISUPPORTS
    XPC_DECLARE_IXPCSCRIPTABLE
    xpcoverloadedScriptable();
    virtual ~xpcoverloadedScriptable();
};

xpcoverloadedScriptable::xpcoverloadedScriptable()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}
xpcoverloadedScriptable::~xpcoverloadedScriptable()
{
    // empty
}

static NS_DEFINE_IID(kxpcoverloadedScriptableIID, NS_IXPCSCRIPTABLE_IID);
NS_IMPL_ISUPPORTS(xpcoverloadedScriptable, kxpcoverloadedScriptableIID);

// These macros give default implementations for these methods.

//XPC_IMPLEMENT_FORWARD_CREATE(xpcoverloadedScriptable)
XPC_IMPLEMENT_IGNORE_GETFLAGS(xpcoverloadedScriptable);
XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(xpcoverloadedScriptable)
XPC_IMPLEMENT_FORWARD_DEFINEPROPERTY(xpcoverloadedScriptable)
XPC_IMPLEMENT_FORWARD_GETPROPERTY(xpcoverloadedScriptable)
XPC_IMPLEMENT_FORWARD_SETPROPERTY(xpcoverloadedScriptable)
XPC_IMPLEMENT_FORWARD_GETATTRIBUTES(xpcoverloadedScriptable)
XPC_IMPLEMENT_FORWARD_SETATTRIBUTES(xpcoverloadedScriptable)
XPC_IMPLEMENT_FORWARD_DELETEPROPERTY(xpcoverloadedScriptable)
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(xpcoverloadedScriptable)
XPC_IMPLEMENT_FORWARD_ENUMERATE(xpcoverloadedScriptable)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(xpcoverloadedScriptable)
XPC_IMPLEMENT_FORWARD_CALL(xpcoverloadedScriptable)
XPC_IMPLEMENT_FORWARD_CONSTRUCT(xpcoverloadedScriptable)
XPC_IMPLEMENT_FORWARD_FINALIZE(xpcoverloadedScriptable)

// we implement this method ourselves

NS_IMETHODIMP
xpcoverloadedScriptable::Create(JSContext *cx, JSObject *obj,
                                nsIXPConnectWrappedNative* wrapper,
                                nsIXPCScriptable* arbitrary)
{
/*
* Here are two implementations...
*
* The first uses a shared prototype object to implement the forwarding
* function.
*
* The second adds the forwarding function to each and every object
*/
#if 1
/*
* NOTE: in the future xpconnect is likely to build and maintain a
* 'per CLSID' prototype object. When we have flattened interfaces code will
* be able to ask the wrapper for the prototype object. The prototype object
* will be shared by all wrapped objects with the given CLSID.
*
* *** If anyone uses the code below to make their own prototype objects they
*     should be prepared to convert the code when the new scheme arrives. ***
*/

    static const char name[] = "__xpcoverloadedScriptableProto__";
    static const char source[] =
        "__xpcoverloadedScriptableProto__ = {"
        "   Foo : function() {"
        "     switch(arguments.length) {"
        "     case 1: return this.Foo1(arguments[0]);"
        "     case 2: return this.Foo2(arguments[0], arguments[1]);"
        "     default: throw '1 or 2 arguments required';"
        "     }"
        "   }"
        "};";

    jsval proto;

    if(!JS_GetProperty(cx, JS_GetGlobalObject(cx), name, &proto) ||
       JSVAL_IS_PRIMITIVE(proto))
    {
       if(!JS_EvaluateScript(cx, JS_GetGlobalObject(cx), source, strlen(source),
                          "builtin", 1, &proto) ||
          !JS_GetProperty(cx, JS_GetGlobalObject(cx), name, &proto)||
          JSVAL_IS_PRIMITIVE(proto))
            return NS_ERROR_UNEXPECTED;
    }
    if(!JS_SetPrototype(cx, obj, JSVAL_TO_OBJECT(proto)))
        return NS_ERROR_UNEXPECTED;
    return NS_OK;

#else
    // NOTE: this script is evaluated where the wrapped object is the current
    // 'this'.

    // here is a 'Foo' implementation that will forward to the appropriate
    // non-overloaded method.
    static const char source[] =
        "this.Foo = function() {"
        "  switch(arguments.length) {"
        "  case 1: return this.Foo1(arguments[0]);"
        "  case 2: return this.Foo2(arguments[0], arguments[1]);"
        "  default: throw '1 or 2 arguments required';"
        "  }"
        "};";

    jsval ignored;
    JS_EvaluateScript(cx, obj, source, strlen(source), "builtin", 1, &ignored);
    return NS_OK;
#endif
}

/***************************************************************************/

class xpcoverloaded : public nsIXPCTestOverloaded
{
public:
    NS_DECL_ISUPPORTS

    /* void Foo1 (in PRInt32 p1); */
    NS_IMETHOD Foo1(PRInt32 p1);

    /* void Foo2 (in PRInt32 p1, in PRInt32 p2); */
    NS_IMETHOD Foo2(PRInt32 p1, PRInt32 p2);

    xpcoverloaded();
    virtual ~xpcoverloaded();
private:
    xpcoverloadedScriptable* mScriptable;
};

xpcoverloaded::xpcoverloaded()
    : mScriptable(new xpcoverloadedScriptable())
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

xpcoverloaded::~xpcoverloaded()
{
    if(mScriptable)
        NS_RELEASE(mScriptable);
}

NS_IMPL_ADDREF(xpcoverloaded)
NS_IMPL_RELEASE(xpcoverloaded)
// this macro is a simple way to expose nsIXPCScriptable implementation
NS_IMPL_QUERY_INTERFACE_SCRIPTABLE(xpcoverloaded, mScriptable)


/* void Foo1 (in PRInt32 p1); */
NS_IMETHODIMP
xpcoverloaded::Foo1(PRInt32 p1)
{
    printf("xpcoverloaded::Foo1 called with p1 = %d\n", p1);
    return NS_OK;
}

/* void Foo2 (in PRInt32 p1, in PRInt32 p2); */
NS_IMETHODIMP
xpcoverloaded::Foo2(PRInt32 p1, PRInt32 p2)
{
    printf("xpcoverloaded::Foo2 called with p1 = %d and p2 = %d\n", p1, p2);
    return NS_OK;
}


/***************************************************************************/
// our standard generic factory helper.

// static
NS_IMETHODIMP
xpctest::ConstructOverloaded(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    NS_ASSERTION(aOuter == nsnull, "no aggregation");
    xpcoverloaded* obj = new xpcoverloaded();

    if(obj)
    {
        rv = obj->QueryInterface(aIID, aResult);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
        NS_RELEASE(obj);
    }
    else
    {
        *aResult = nsnull;
        rv = NS_ERROR_OUT_OF_MEMORY;
    }

    return rv;
}
/***************************************************************************/




