/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#include "xpctest_private.h"
#include "xpctest_in.h"
#include "nsISupports.h"
#define NS_IXPCTESTIN_IID \
  {0x318d6f6a, 0x5411, 0x11d3, \
    { 0x82, 0xef, 0x00, 0x60, 0xb0, 0xeb, 0x59, 0x6f }}


class xpcTestIn : public nsIXPCTestIn {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCTESTIN

  xpcTestIn();
};

NS_IMPL_ISUPPORTS1(xpcTestIn, nsIXPCTestIn);

xpcTestIn :: xpcTestIn() {
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
};

NS_IMETHODIMP xpcTestIn :: EchoLong(PRInt32 l, PRInt32 *_retval) {
    *_retval = l;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoShort(PRInt16 a, PRInt16 *_retval) {
    *_retval = a;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoChar(char c, char *_retval) {
    *_retval = c;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoBoolean(PRBool b, PRBool *_retval) {
    *_retval = b;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoOctet(PRUint8 o, PRUint8 *_retval) {
    *_retval = o;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoLongLong(PRInt64 ll, PRInt64 *_retval) {
    *_retval = ll;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoUnsignedShort(PRUint16 us, PRUint16 *_retval) {
    *_retval = us;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoUnsignedLong(PRUint32 ul, PRUint32 *_retval) {
    *_retval = ul;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoFloat(float f, float *_retval) {
    *_retval = f;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoDouble(double d, double *_retval) {
    *_retval = d;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoWchar(PRUnichar wc, PRUnichar *_retval) {
    *_retval = wc;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoString(const PRUnichar *ws, char **_retval) {
/*  const char s[] = *ws;
    **_retval= (char*) nsMemory::Clone(s, 
                                    sizeof(char)*(strlen(s)+1));
    return **_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
*/
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoPRBool(PRBool b, PRBool *_retval) {
    *_retval = b;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoPRInt32(PRInt32 l, PRInt32 *_retval) {
    *_retval = l;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoPRInt16(PRInt16 l, PRInt16 *_retval) {
    *_retval = l;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoPRInt64(PRInt64 i, PRInt64 *_retval) {
    *_retval = i;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoPRUint8(PRUint8 i, PRUint8 *_retval){
    *_retval = i;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoPRUint16(PRUint16 i, PRUint16 *_retval){
    *_retval = i;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoPRUint32(PRUint32 i, PRUint32 *_retval){
    *_retval = i;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoPRUint64(PRUint64 i, PRUint64 *_retval) {
    *_retval = i;
    return NS_OK;
};
NS_IMETHODIMP xpcTestIn :: EchoVoidPtr(void * vs, void * *_retval) {
    *_retval = vs;
    return NS_OK;
}; 

NS_IMETHODIMP xpcTestIn :: EchoCharPtr(char * cs, char * *_retval) {
    **_retval = *cs;
    return NS_OK;
};

NS_IMETHODIMP xpcTestIn :: EchoPRUint32_2(PRUint32 i, PRUint32 *_retval){
    *_retval = i;
    return NS_OK;
};
 /*
NS_IMETHODIMP xpcTestIn :: EchoNsIDRef(const nsID & r, nsID & *_retval) {
    &*_retval = r;
};
NS_IMETHODIMP xpcTestIn :: EchoNsCIDRef(const nsCID & r, nsCID & *_retval) {
    &*_retval = r;
};
NS_IMETHODIMP xpcTestIn :: EchoNsIDPtr(const nsID * p, nsID * *_retval) {
    **_retval = p;
};
NS_IMETHODIMP xpcTestIn :: EchoNsIIDPtr(const nsIID * p, nsIID * *_retval) {
    *_retval = p;
};
NS_IMETHODIMP xpcTestIn :: EchoNsCIDPtr(const nsCID * p, nsCID * *_retval) {
    *_retval = p;
};
NS_IMETHODIMP xpcTestIn :: EchoNsQIResult(void * r, void * *_retval) {
    **_retval = r;
};
*/
NS_IMETHODIMP xpcTestIn :: EchoVoid(void) {
    return NS_OK;
};

NS_IMETHODIMP
xpctest::ConstructXPCTestIn(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    NS_ASSERTION(aOuter == nsnull, "no aggregation");
    xpcTestIn *obj = new xpcTestIn();

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
};
