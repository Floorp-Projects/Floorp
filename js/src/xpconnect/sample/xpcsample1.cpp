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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
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

/* xpconnect api sample */

#include "xpcsample1.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

/***************************************************************************/
// forward declare the below classes for clarity
class nsXPCSample_ClassA;
class nsXPCSample_ClassB;
class nsXPCSample_ClassC;

/***************************************************************************/
// declare the classes

class nsXPCSample_ClassA : public nsIXPCSample_ClassA
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCSAMPLE_CLASSA

    nsXPCSample_ClassA(PRInt32 aValue);
    virtual ~nsXPCSample_ClassA();

private:
    PRInt32 mValue;
    nsCOMPtr<nsIXPCSample_ClassB> mClassB;
};

/********************************/

class nsXPCSample_ClassB : public nsIXPCSample_ClassB
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCSAMPLE_CLASSB

    nsXPCSample_ClassB(PRInt32 aValue);
    virtual ~nsXPCSample_ClassB();

private:
    PRInt32 mValue;
    nsCOMPtr<nsIXPCSample_ClassC> mClassC;
};

/********************************/

class nsXPCSample_ClassC : public nsIXPCSample_ClassC
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCSAMPLE_CLASSC

    nsXPCSample_ClassC(PRInt32 aValue);
    virtual ~nsXPCSample_ClassC();

private:
    PRInt32 mValue;
};

/***************************************************************************/
// implement the 'root' ClassA for this odd sample

NS_IMPL_ISUPPORTS1(nsXPCSample_ClassA, nsIXPCSample_ClassA)

nsXPCSample_ClassA::nsXPCSample_ClassA(PRInt32 aValue)
    :   mValue(aValue)
{
    NS_INIT_ISUPPORTS();
}

nsXPCSample_ClassA::~nsXPCSample_ClassA()
{
    // empty
}

/* attribute PRInt32 someValue; */
NS_IMETHODIMP nsXPCSample_ClassA::GetSomeValue(PRInt32 *aSomeValue)
{
    *aSomeValue = mValue;
    return NS_OK;
}

NS_IMETHODIMP nsXPCSample_ClassA::SetSomeValue(PRInt32 aSomeValue)
{
    mValue = aSomeValue;
    return NS_OK;
}

/* readonly attribute nsIXPCSample_ClassB B; */
NS_IMETHODIMP nsXPCSample_ClassA::GetB(nsIXPCSample_ClassB * *aB)
{
    if(!mClassB && !(mClassB = new nsXPCSample_ClassB(mValue)))
        return NS_ERROR_FAILURE;
    *aB = mClassB;
    NS_ADDREF(*aB);
    return NS_OK;
}

/***************************************************************************/
// implement ClassB for this odd sample

NS_IMPL_ISUPPORTS1(nsXPCSample_ClassB, nsIXPCSample_ClassB)

nsXPCSample_ClassB::nsXPCSample_ClassB(PRInt32 aValue)
    :   mValue(aValue)
{
    NS_INIT_ISUPPORTS();
}

nsXPCSample_ClassB::~nsXPCSample_ClassB()
{
    // empty
}

/* attribute PRInt32 someValue; */
NS_IMETHODIMP nsXPCSample_ClassB::GetSomeValue(PRInt32 *aSomeValue)
{
    *aSomeValue = mValue;
    return NS_OK;
}

NS_IMETHODIMP nsXPCSample_ClassB::SetSomeValue(PRInt32 aSomeValue)
{
    mValue = aSomeValue;
    return NS_OK;
}

/* readonly attribute nsIXPCSample_ClassC C; */
NS_IMETHODIMP nsXPCSample_ClassB::GetC(nsIXPCSample_ClassC * *aC)
{
    if(!mClassC && !(mClassC = new nsXPCSample_ClassC(mValue)))
        return NS_ERROR_FAILURE;
    *aC = mClassC;
    NS_ADDREF(*aC);
    return NS_OK;
}

/***************************************************************************/
// implement ClassC for this odd sample

NS_IMPL_ISUPPORTS1(nsXPCSample_ClassC, nsIXPCSample_ClassC)

nsXPCSample_ClassC::nsXPCSample_ClassC(PRInt32 aValue)
    :   mValue(aValue)
{
    NS_INIT_ISUPPORTS();
}

nsXPCSample_ClassC::~nsXPCSample_ClassC()
{
    // empty
}

/* attribute PRInt32 someValue; */
NS_IMETHODIMP nsXPCSample_ClassC::GetSomeValue(PRInt32 *aSomeValue)
{
    *aSomeValue = mValue;
    return NS_OK;
}

NS_IMETHODIMP nsXPCSample_ClassC::SetSomeValue(PRInt32 aSomeValue)
{
    mValue = aSomeValue;
    return NS_OK;
}

/***************************************************************************/
// implement the hooker-upper (see the comment in the xpcsample1.idl)

class nsXPCSample_HookerUpper : public nsIXPCSample_HookerUpper
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCSAMPLE_HOOKERUPPER

    nsXPCSample_HookerUpper();
    virtual ~nsXPCSample_HookerUpper();
};

NS_IMPL_ISUPPORTS1(nsXPCSample_HookerUpper, nsIXPCSample_HookerUpper)

nsXPCSample_HookerUpper::nsXPCSample_HookerUpper()
{
    NS_INIT_ISUPPORTS();
}

nsXPCSample_HookerUpper::~nsXPCSample_HookerUpper()
{
    // empty
}

/* void createSampleObjectAtGlobalScope (in string name, in PRInt32 value); */
NS_IMETHODIMP
nsXPCSample_HookerUpper::CreateSampleObjectAtGlobalScope(const char *name, PRInt32 value)
{
    // we use the xpconnect native call context stuff to get the current
    // JSContext. Again, this would not be necessary if we were setting up
    // our own JSContext instead of resonding to a call inside a running
    // context.

    // get the xpconnect service
    nsresult rv;
    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
    if(NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    // get the xpconnect native call context
    nsCOMPtr<nsIXPCNativeCallContext> callContext;
    xpc->GetCurrentNativeCallContext(getter_AddRefs(callContext));
    if(!callContext)
        return NS_ERROR_FAILURE;

    // verify that we are being called from JS (i.e. the current call is
    // to this object - though we don't verify that it is to this exact method)
    nsCOMPtr<nsISupports> callee;
    callContext->GetCallee(getter_AddRefs(callee));
    if(!callee || callee.get() != (nsISupports*)this)
        return NS_ERROR_FAILURE;

    // Get JSContext of current call
    JSContext* cx;
    rv = callContext->GetJSContext(&cx);
    if(NS_FAILED(rv) || !cx)
        return NS_ERROR_FAILURE;

    // Get the xpc wrapper (for 'this') so that we can get its JSObject
    nsCOMPtr<nsIXPConnectWrappedNative> calleeWrapper;
    callContext->GetCalleeWrapper(getter_AddRefs(calleeWrapper));
    if(!calleeWrapper)
        return NS_ERROR_FAILURE;

    // Get the JSObject of the wrapper
    JSObject* calleeJSObject;
    rv = calleeWrapper->GetJSObject(&calleeJSObject);
    if(NS_FAILED(rv) || !calleeJSObject)
        return NS_ERROR_FAILURE;

    // Now we walk the parent chain of the wrapper's JSObject to get at the
    // global object to which we'd like to attack the new property (which is
    // the point of this whole exercise!)
    JSObject* tempJSObject;
    JSObject* globalJSObject = calleeJSObject;
    while(tempJSObject = JS_GetParent(cx, globalJSObject))
        globalJSObject = tempJSObject;

    // --- if we did this all at JSContext setup time then only the following
    // is necessary...

    // Create the new native object
    nsXPCSample_ClassA* classA = new nsXPCSample_ClassA(value);
    if(!classA)
        return NS_ERROR_FAILURE;

    // Build an xpconnect wrapper around the object
    nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
    rv = xpc->WrapNative(cx, globalJSObject, classA,
                         NS_GET_IID(nsXPCSample_ClassA),
                         getter_AddRefs(wrapper));
    if(NS_FAILED(rv) || !wrapper)
        return NS_ERROR_FAILURE;

    // Get the JSObject of the new wrapper we just built around our ClassA obj
    JSObject* ourJSObject;
    rv = wrapper->GetJSObject(&ourJSObject);
    if(NS_FAILED(rv) || !ourJSObject)
        return NS_ERROR_FAILURE;

    // Now we can use the JSAPI to add this JSObject as a property of the
    // global object.

    if(!JS_DefineProperty(cx,
                          globalJSObject,       // add property to this object
                          "A",                  // give property this name
                          OBJECT_TO_JSVAL(ourJSObject),
                          nsnull, nsnull,
                          JSPROP_PERMANENT |    // these flags are optional
                          JSPROP_READONLY |
                          JSPROP_ENUMERATE))
        return NS_ERROR_FAILURE;

    // all is well - new object was added!
    return NS_OK;
}

/***************************************************************************/
// generic factory and component registration stuff...

NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPCSample_HookerUpper)

static nsModuleComponentInfo components[] = {
{ "sample xpc component",
  { 0x97380cf0, 0xc21b, 0x11d3, { 0x98, 0xc9, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } },
  NS_NSXPCSAMPLE_HOOKERUPPER_CONTRACTID,
  nsXPCSample_HookerUpperConstructor }
};

NS_IMPL_NSGETMODULE(xpconnect_samples, components)

