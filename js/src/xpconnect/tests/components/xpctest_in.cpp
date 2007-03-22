/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "xpctest_private.h"
#include "xpctest_in.h"
#include "nsISupports.h"
#define NS_IXPCTESTIN_IID \
  {0xa3cab49d, 0xae83, 0x4e63, \
    { 0xa7, 0x35, 0x00, 0x9b, 0x9a, 0x75, 0x92, 0x04 }}


class xpcTestIn : public nsIXPCTestIn {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCTESTIN

  xpcTestIn();
};

NS_IMPL_ISUPPORTS1(xpcTestIn, nsIXPCTestIn)

xpcTestIn :: xpcTestIn() {
    NS_ADDREF_THIS();
}

NS_IMETHODIMP xpcTestIn :: EchoLong(PRInt32 l, PRInt32 *_retval) {
    *_retval = l;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoShort(PRInt16 a, PRInt16 *_retval) {
    *_retval = a;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoChar(char c, char *_retval) {
    *_retval = c;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoBoolean(PRBool b, PRBool *_retval) {
    *_retval = b;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoOctet(PRUint8 o, PRUint8 *_retval) {
    *_retval = o;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoLongLong(PRInt64 ll, PRInt64 *_retval) {
    *_retval = ll;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoUnsignedShort(PRUint16 us, PRUint16 *_retval) {
    *_retval = us;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoUnsignedLong(PRUint32 ul, PRUint32 *_retval) {
    *_retval = ul;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoFloat(float f, float *_retval) {
    *_retval = f;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoDouble(double d, double *_retval) {
    *_retval = d;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoWchar(PRUnichar wc, PRUnichar *_retval) {
    *_retval = wc;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoString(const PRUnichar *ws, PRUnichar **_retval) {
/*  const char s[] = *ws;
    **_retval= (char*) nsMemory::Clone(s, 
                                    sizeof(char)*(strlen(s)+1));
    return **_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
*/
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoPRBool(PRBool b, PRBool *_retval) {
    *_retval = b;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoPRInt32(PRInt32 l, PRInt32 *_retval) {
    *_retval = l;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoPRInt16(PRInt16 l, PRInt16 *_retval) {
    *_retval = l;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoPRInt64(PRInt64 i, PRInt64 *_retval) {
    *_retval = i;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoPRUint8(PRUint8 i, PRUint8 *_retval){
    *_retval = i;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoPRUint16(PRUint16 i, PRUint16 *_retval){
    *_retval = i;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoPRUint32(PRUint32 i, PRUint32 *_retval){
    *_retval = i;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoPRUint64(PRUint64 i, PRUint64 *_retval) {
    *_retval = i;
    return NS_OK;
}
NS_IMETHODIMP xpcTestIn :: EchoVoidPtr(void * vs, void * *_retval) {
    *_retval = vs;
    return NS_OK;
} 

NS_IMETHODIMP xpcTestIn :: EchoCharPtr(char * cs, char * *_retval) {
    **_retval = *cs;
    return NS_OK;
}

NS_IMETHODIMP xpcTestIn :: EchoPRUint32_2(PRUint32 i, PRUint32 *_retval){
    *_retval = i;
    return NS_OK;
}
 /*
NS_IMETHODIMP xpcTestIn :: EchoNsIDRef(const nsID & r, nsID & *_retval) {
    &*_retval = r;
}
NS_IMETHODIMP xpcTestIn :: EchoNsCIDRef(const nsCID & r, nsCID & *_retval) {
    &*_retval = r;
}
NS_IMETHODIMP xpcTestIn :: EchoNsIDPtr(const nsID * p, nsID * *_retval) {
    **_retval = p;
}
NS_IMETHODIMP xpcTestIn :: EchoNsIIDPtr(const nsIID * p, nsIID * *_retval) {
    *_retval = p;
}
NS_IMETHODIMP xpcTestIn :: EchoNsCIDPtr(const nsCID * p, nsCID * *_retval) {
    *_retval = p;
}
NS_IMETHODIMP xpcTestIn :: EchoNsQIResult(void * r, void * *_retval) {
    **_retval = r;
}
*/
NS_IMETHODIMP xpcTestIn :: EchoVoid(void) {
    return NS_OK;
}

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
}
