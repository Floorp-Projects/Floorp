/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
* identity rules; i.e. doing a QI(NS_GET_IID(nsISupports)) on this interface is
* not required to return the same pointer as doing so on the object that
* presented this interface. Thus, it is allowable to implement only one
* nsIXPCScriptable instance per class if desired.
*/

class xpcoverloaded : public nsIXPCTestOverloaded, public nsIXPCScriptable
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCTESTOVERLOADED
    NS_DECL_NSIXPCSCRIPTABLE

    xpcoverloaded();
    virtual ~xpcoverloaded();
};

xpcoverloaded::xpcoverloaded()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

xpcoverloaded::~xpcoverloaded()
{
    // empty
}

NS_IMPL_ISUPPORTS2(xpcoverloaded, nsIXPCTestOverloaded, nsIXPCScriptable);

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

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           xpcoverloaded
#define XPC_MAP_QUOTED_CLASSNAME   "xpcoverloaded"
#define                             XPC_MAP_WANT_CREATE
#define XPC_MAP_FLAGS               0
#include "xpc_map_end.h" /* This will #undef the above */

// We implement this method ourselves


/* void create (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP 
xpcoverloaded::Create(nsIXPConnectWrappedNative *wrapper, 
                      JSContext * cx, JSObject * obj)
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

    static const char name[] = "__xpcoverloadedProto__";
    static const char source[] =
        "__xpcoverloadedProto__ = {"
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




