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
#include "xpctest_out.h"
#include "nsISupports.h"

#define NS_IXPCTESTOUT_IID \
  {0x4105ae88, 0x5599, 0x11d3, \
    { 0x82, 0xef, 0x00, 0x60, 0xb0, 0xeb, 0x59, 0x6f }}

class xpcTestOut : public nsIXPCTestOut {
public:
NS_DECL_ISUPPORTS
NS_DECL_NSIXPCTESTOUT
xpcTestOut();

private:
    PRInt32   longProperty;
    PRInt16  shortProperty;
    char   charProperty;
    float  floatProperty;
    double doubleProperty;
    PRUnichar * stringProperty;
    PRBool booleanProperty;
    PRUint8 octetProperty;
    PRUint16 unsignedShortProperty;
    PRUint32 unsignedLongProperty;
    PRInt64 longLongProperty;
    PRUnichar wcharProperty;
//	PRUnichar *wsStringProperty;

    PRBool PRBoolProperty;
    PRInt32 PRInt32Property;
    PRInt16 PRInt16Property;
    PRInt64 PRInt64Property;
    PRUint8 PRUint8Property;
    PRUint16 PRUint16Property;
    PRUint32 PRUint32Property;
    PRUint64 PRUint64Property;
};


NS_IMPL_ISUPPORTS1(xpcTestOut, nsIXPCTestOut)

xpcTestOut :: xpcTestOut() {
    NS_ADDREF_THIS();
}

NS_IMETHODIMP xpcTestOut :: GetLong(PRInt32 *l){
    *l = longProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetLong(PRInt32 l){
    longProperty = l;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetShort(PRInt16 *s){
    *s  = shortProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetShort(PRInt16 s){
    shortProperty = s;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetChar(char c){
    charProperty = c;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetChar(char *c){
    *c = charProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetBoolean(PRBool *b){
    *b = booleanProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetBoolean(PRBool b){
    booleanProperty = b;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetOctet(PRUint8 *o){
    *o = octetProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetOctet(PRUint8 o){
    octetProperty = o;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetLongLong(PRInt64 *ll){
    *ll = longLongProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetLongLong(PRInt64 ll){
    longLongProperty = ll;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetUnsignedShort(PRUint16 *us){
    *us = unsignedShortProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetUnsignedShort(PRUint16 us){
    unsignedShortProperty = us;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetUnsignedLong(PRUint32 *ul){
    *ul = unsignedLongProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetUnsignedLong(PRUint32 ul){
    unsignedLongProperty = ul;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetFloat(float *f){
    *f = floatProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetFloat(float f){
    floatProperty = f;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetDouble(double *d){
    *d = doubleProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetDouble(double d){
    doubleProperty = d;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetWchar(PRUnichar *wc){
    *wc = wcharProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetWchar(PRUnichar wc){
    wcharProperty = wc;
    return NS_OK;
}
/*
NS_IMETHODIMP xpcTestOut :: GetString(PRUnichar **ws){
    **ws = *stringProperty;

    if(!stringProperty)
        return NS_ERROR_NULL_POINTER;
    *ws = (PRUnichar *) nsMemory::Clone(stringProperty, 
                    sizeof(char) *(strlen(stringProperty)+1));
    return **ws? NS_OK : NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetString(const PRUnichar *ws){
    //stringProperty = ws;
    return NS_OK;
}
*/
NS_IMETHODIMP xpcTestOut :: GetPRBool(PRBool *b){
    *b = PRBoolProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetPRBool(PRBool b){
    PRBoolProperty = b;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetPRInt32(PRInt32 *l){
    *l = PRInt32Property;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetPRInt32(PRInt32 l){
    PRInt32Property = l;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetPRInt16(PRInt16 *l){
    *l = PRInt16Property;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetPRInt16(PRInt16 l){
    PRInt16Property = l;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetPRInt64(PRInt64 *i){
    *i = PRInt64Property;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetPRInt64(PRInt64 i){
    PRInt64Property = i;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetPRUint8(PRUint8 *i){
    *i = PRUint8Property;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetPRUint8(PRUint8 i){
    PRUint8Property = i;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetPRUint16(PRUint16 *i){
    *i = PRUint16Property;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetPRUint16(PRUint16 i){
    PRUint16Property = i;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetPRUint32(PRUint32 *i){
    *i = PRUint32Property;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetPRUint32(PRUint32 i){
    PRUint32Property = i;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetPRUint64(PRUint64 *i){
    *i = PRUint64Property;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetPRUint64(PRUint64 i){
    PRUint64Property = i;
    return NS_OK;
}
/*
NS_IMETHODIMP xpcTestOut :: GetVoidStar(void * *vs){
    **vs = voidStarProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetVoidStar(void * vs){
    voidStarProperty = vs;
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetCharStar(char * *cs){
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetCharStar(char * cs){
    return NS_OK;
}

NS_IMETHODIMP xpcTestOut :: GetNsIDRef(nsID & *r){
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetNsIDRef(const nsID & r){
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetNsCIDRef(nsCID & *r){
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetNsCIDRef(const nsCID & r){
    return NS_OK;
}

NS_IMETHODIMP xpcTestOut :: GetNsIDPtr(nsID * *p){
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetNsIDPtr(const nsID * p){
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetNsIIDPtr(nsIID * *p){
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetNsIIDPtr(const nsIID * p){
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetNsCIDPtr(nsCID * *p){
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetNsCIDPtr(const nsCID * p){
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetNsQIResult(void * *r){
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetNsQIResult(void * r){
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: GetVoid(void){
    return NS_OK;
}
NS_IMETHODIMP xpcTestOut :: SetVoid(void){
    return NS_OK;
}
*/
nsresult
xpctest::ConstructXPCTestOut(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    NS_ASSERTION(aOuter == nsnull, "no aggregation");
    xpcTestOut *obj = new xpcTestOut();

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
