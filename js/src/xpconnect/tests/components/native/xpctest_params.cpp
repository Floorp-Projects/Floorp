/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is XPConnect Test Code.
 *
 * The Initial Developer of the Original Code is The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bobby Holley <bobbyholley@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

NS_IMPL_ISUPPORTS1(nsXPCTestParams, nsIXPCTestParams)

nsXPCTestParams::nsXPCTestParams()
{
}

nsXPCTestParams::~nsXPCTestParams()
{
}

#define GENERIC_METHOD_IMPL { \
    *_retval = *b; \
    *b = a; \
    return NS_OK; \
}

#define STRING_METHOD_IMPL { \
    _retval.Assign(b); \
    b.Assign(a); \
    return NS_OK; \
}

/* boolean testBoolean (in boolean a, inout boolean b); */
NS_IMETHODIMP nsXPCTestParams::TestBoolean(PRBool a, PRBool *b NS_INOUTPARAM, PRBool *_retval NS_OUTPARAM)
{
    GENERIC_METHOD_IMPL;
}

/* octet testOctet (in octet a, inout octet b); */
NS_IMETHODIMP nsXPCTestParams::TestOctet(PRUint8 a, PRUint8 *b NS_INOUTPARAM, PRUint8 *_retval NS_OUTPARAM)
{
    GENERIC_METHOD_IMPL;
}

/* short testShort (in short a, inout short b); */
NS_IMETHODIMP nsXPCTestParams::TestShort(PRInt16 a, PRInt16 *b NS_INOUTPARAM, PRInt16 *_retval NS_OUTPARAM)
{
    GENERIC_METHOD_IMPL;
}

/* long testLong (in long a, inout long b); */
NS_IMETHODIMP nsXPCTestParams::TestLong(PRInt32 a, PRInt32 *b NS_INOUTPARAM, PRInt32 *_retval NS_OUTPARAM)
{
    GENERIC_METHOD_IMPL;
}

/* long long testLongLong (in long long a, inout long long b); */
NS_IMETHODIMP nsXPCTestParams::TestLongLong(PRInt64 a, PRInt64 *b NS_INOUTPARAM, PRInt64 *_retval NS_OUTPARAM)
{
    GENERIC_METHOD_IMPL;
}

/* unsigned short testUnsignedShort (in unsigned short a, inout unsigned short b); */
NS_IMETHODIMP nsXPCTestParams::TestUnsignedShort(PRUint16 a, PRUint16 *b NS_INOUTPARAM, PRUint16 *_retval NS_OUTPARAM)
{
    GENERIC_METHOD_IMPL;
}

/* unsigned long testUnsignedLong (in unsigned long a, inout unsigned long b); */
NS_IMETHODIMP nsXPCTestParams::TestUnsignedLong(PRUint32 a, PRUint32 *b NS_INOUTPARAM, PRUint32 *_retval NS_OUTPARAM)
{
    GENERIC_METHOD_IMPL;
}

/* unsigned long long testUnsignedLongLong (in unsigned long long a, inout unsigned long long b); */
NS_IMETHODIMP nsXPCTestParams::TestUnsignedLongLong(PRUint64 a, PRUint64 *b NS_INOUTPARAM, PRUint64 *_retval NS_OUTPARAM)
{
    GENERIC_METHOD_IMPL;
}

/* float testFloat (in float a, inout float b); */
NS_IMETHODIMP nsXPCTestParams::TestFloat(float a, float *b NS_INOUTPARAM, float *_retval NS_OUTPARAM)
{
    GENERIC_METHOD_IMPL;
}

/* double testDouble (in double a, inout float b); */
NS_IMETHODIMP nsXPCTestParams::TestDouble(double a, float *b NS_INOUTPARAM, double *_retval NS_OUTPARAM)
{
    GENERIC_METHOD_IMPL;
}

/* char testChar (in char a, inout char b); */
NS_IMETHODIMP nsXPCTestParams::TestChar(char a, char *b NS_INOUTPARAM, char *_retval NS_OUTPARAM)
{
    GENERIC_METHOD_IMPL;
}

/* string testString (in string a, inout string b); */
NS_IMETHODIMP nsXPCTestParams::TestString(const char * a, char * *b NS_INOUTPARAM, char * *_retval NS_OUTPARAM)
{
    nsDependentCString aprime(a);
    nsDependentCString bprime(*b);
    *_retval = ToNewCString(bprime);
    *b = ToNewCString(aprime);
    return NS_OK;
}

/* wchar testWchar (in wchar a, inout wchar b); */
NS_IMETHODIMP nsXPCTestParams::TestWchar(PRUnichar a, PRUnichar *b NS_INOUTPARAM, PRUnichar *_retval NS_OUTPARAM)
{
    GENERIC_METHOD_IMPL;
}

/* wstring testWstring (in wstring a, inout wstring b); */
NS_IMETHODIMP nsXPCTestParams::TestWstring(const PRUnichar * a, PRUnichar * *b NS_INOUTPARAM, PRUnichar * *_retval NS_OUTPARAM)
{
    nsDependentString aprime(a);
    nsDependentString bprime(*b);
    *_retval = ToNewUnicode(bprime);
    *b = ToNewUnicode(aprime);
    return NS_OK;
}

/* DOMString testDOMString (in DOMString a, inout DOMString b); */
NS_IMETHODIMP nsXPCTestParams::TestDOMString(const nsAString & a, nsAString & b NS_INOUTPARAM, nsAString & _retval NS_OUTPARAM)
{
    STRING_METHOD_IMPL;
}


/* AString testAString (in AString a, inout AString b); */
NS_IMETHODIMP nsXPCTestParams::TestAString(const nsAString & a, nsAString & b NS_INOUTPARAM, nsAString & _retval NS_OUTPARAM)
{
    STRING_METHOD_IMPL;
}

/* AUTF8String testAUTF8String (in AUTF8String a, inout AUTF8String b); */
NS_IMETHODIMP nsXPCTestParams::TestAUTF8String(const nsACString & a, nsACString & b NS_INOUTPARAM, nsACString & _retval NS_OUTPARAM)
{
    STRING_METHOD_IMPL;
}

/* ACString testACString (in ACString a, inout ACString b); */
NS_IMETHODIMP nsXPCTestParams::TestACString(const nsACString & a, nsACString & b NS_INOUTPARAM, nsACString & _retval NS_OUTPARAM)
{
    STRING_METHOD_IMPL;
}

/* jsval testJsval (in jsval a, inout jsval b); */
NS_IMETHODIMP nsXPCTestParams::TestJsval(const jsval & a, jsval & b NS_INOUTPARAM, jsval *_retval NS_OUTPARAM)
{
    *_retval = b;
    b = a;
    return NS_OK;
}
