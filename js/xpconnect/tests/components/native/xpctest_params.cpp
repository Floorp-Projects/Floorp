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
#include "xpctest_interfaces.h"

NS_IMPL_ISUPPORTS1(nsXPCTestParams, nsIXPCTestParams)

nsXPCTestParams::nsXPCTestParams()
{
}

nsXPCTestParams::~nsXPCTestParams()
{
}

#define GENERIC_METHOD_IMPL {                                                 \
    *_retval = *b;                                                            \
    *b = a;                                                                   \
    return NS_OK;                                                             \
}

#define STRING_METHOD_IMPL {                                                  \
    _retval.Assign(b);                                                        \
    b.Assign(a);                                                              \
    return NS_OK;                                                             \
}

#define TAKE_OWNERSHIP_NOOP(val) {}
#define TAKE_OWNERSHIP_INTERFACE(val) {static_cast<nsISupports*>(val)->AddRef();}
#define TAKE_OWNERSHIP_STRING(val) {                                          \
    nsDependentCString vprime(val);                                           \
    val = ToNewCString(vprime);                                               \
}
#define TAKE_OWNERSHIP_WSTRING(val) {                                         \
    nsDependentString vprime(val);                                            \
    val = ToNewUnicode(vprime);                                               \
}

// Macro for our buffer-oriented types:
//   'type' is the type of element that the buffer contains.
//   'padding' is an offset added to length, allowing us to handle
//             null-terminated strings.
//   'TAKE_OWNERSHIP' is one of the macros above.
#define BUFFER_METHOD_IMPL(type, padding, TAKE_OWNERSHIP) {                   \
    PRUint32 elemSize = sizeof(type);                                         \
                                                                              \
    /* Copy b into rv. */                                                     \
    *rvLength = *bLength;                                                     \
    *rv = static_cast<type*>(NS_Alloc(elemSize * (*bLength + padding)));      \
    if (!*rv)                                                                 \
        return NS_ERROR_OUT_OF_MEMORY;                                        \
    memcpy(*rv, *b, elemSize * (*bLength + padding));                         \
                                                                              \
    /* Copy a into b. */                                                      \
    *bLength = aLength;                                                       \
    NS_Free(*b);                                                              \
    *b = static_cast<type*>(NS_Alloc(elemSize * (aLength + padding)));        \
    if (!*b)                                                                  \
        return NS_ERROR_OUT_OF_MEMORY;                                        \
    memcpy(*b, a, elemSize * (aLength + padding));                            \
                                                                              \
    /* We need to take ownership of the data we got from a,                   \
       since the caller owns it. */                                           \
    for (unsigned i = 0; i < *bLength + padding; ++i)                         \
        TAKE_OWNERSHIP((*b)[i]);                                              \
                                                                              \
    return NS_OK;                                                             \
}

/* boolean testBoolean (in boolean a, inout boolean b); */
NS_IMETHODIMP nsXPCTestParams::TestBoolean(bool a, bool *b NS_INOUTPARAM, bool *_retval NS_OUTPARAM)
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

    // XPCOM ownership rules dictate that overwritten inout params must be callee-freed.
    // See https://developer.mozilla.org/en/XPIDL
    NS_Free(const_cast<char*>(bprime.get()));

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

    // XPCOM ownership rules dictate that overwritten inout params must be callee-freed.
    // See https://developer.mozilla.org/en/XPIDL
    NS_Free(const_cast<PRUnichar*>(bprime.get()));

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

/* void testShortArray (in unsigned long aLength, [array, size_is (aLength)] in short a,
 *                      inout unsigned long bLength, [array, size_is (bLength)] inout short b,
 *                      out unsigned long rvLength, [array, size_is (rvLength), retval] out short rv); */
NS_IMETHODIMP nsXPCTestParams::TestShortArray(PRUint32 aLength, PRInt16 *a,
                                              PRUint32 *bLength NS_INOUTPARAM, PRInt16 **b NS_INOUTPARAM,
                                              PRUint32 *rvLength NS_OUTPARAM, PRInt16 **rv NS_OUTPARAM)
{
    BUFFER_METHOD_IMPL(PRInt16, 0, TAKE_OWNERSHIP_NOOP);
}

/* void testLongLongArray (in unsigned long aLength, [array, size_is (aLength)] in long long a,
 *                         inout unsigned long bLength, [array, size_is (bLength)] inout long long b,
 *                         out unsigned long rvLength, [array, size_is (rvLength), retval] out long long rv); */
NS_IMETHODIMP nsXPCTestParams::TestLongLongArray(PRUint32 aLength, PRInt64 *a,
                                                 PRUint32 *bLength NS_INOUTPARAM, PRInt64 **b NS_INOUTPARAM,
                                                 PRUint32 *rvLength NS_OUTPARAM, PRInt64 **rv NS_OUTPARAM)
{
    BUFFER_METHOD_IMPL(PRInt64, 0, TAKE_OWNERSHIP_NOOP);
}

/* void testStringArray (in unsigned long aLength, [array, size_is (aLength)] in string a,
 *                       inout unsigned long bLength, [array, size_is (bLength)] inout string b,
 *                       out unsigned long rvLength, [array, size_is (rvLength), retval] out string rv); */
NS_IMETHODIMP nsXPCTestParams::TestStringArray(PRUint32 aLength, const char * *a,
                                               PRUint32 *bLength NS_INOUTPARAM, char * **b NS_INOUTPARAM,
                                               PRUint32 *rvLength NS_OUTPARAM, char * **rv NS_OUTPARAM)
{
    BUFFER_METHOD_IMPL(char*, 0, TAKE_OWNERSHIP_STRING);
}

/* void testWstringArray (in unsigned long aLength, [array, size_is (aLength)] in wstring a,
 *                        inout unsigned long bLength, [array, size_is (bLength)] inout wstring b,
 *                        out unsigned long rvLength, [array, size_is (rvLength), retval] out wstring rv); */
NS_IMETHODIMP nsXPCTestParams::TestWstringArray(PRUint32 aLength, const PRUnichar * *a,
                                                PRUint32 *bLength NS_INOUTPARAM, PRUnichar * **b NS_INOUTPARAM,
                                                PRUint32 *rvLength NS_OUTPARAM, PRUnichar * **rv NS_OUTPARAM)
{
    BUFFER_METHOD_IMPL(PRUnichar*, 0, TAKE_OWNERSHIP_WSTRING);
}

/* void testInterfaceArray (in unsigned long aLength, [array, size_is (aLength)] in nsIXPCTestInterfaceA a,
 *                          inout unsigned long bLength, [array, size_is (bLength)] inout nsIXPCTestInterfaceA b,
 *                          out unsigned long rvLength, [array, size_is (rvLength), retval] out nsIXPCTestInterfaceA rv); */
NS_IMETHODIMP nsXPCTestParams::TestInterfaceArray(PRUint32 aLength, nsIXPCTestInterfaceA **a,
                                                  PRUint32 *bLength NS_INOUTPARAM, nsIXPCTestInterfaceA * **b NS_INOUTPARAM,
                                                  PRUint32 *rvLength NS_OUTPARAM, nsIXPCTestInterfaceA * **rv NS_OUTPARAM)
{
    BUFFER_METHOD_IMPL(nsIXPCTestInterfaceA*, 0, TAKE_OWNERSHIP_INTERFACE);
}

/* void testSizedString (in unsigned long aLength, [size_is (aLength)] in string a,
 *                       inout unsigned long bLength, [size_is (bLength)] inout string b,
 *                       out unsigned long rvLength, [size_is (rvLength), retval] out string rv); */
NS_IMETHODIMP nsXPCTestParams::TestSizedString(PRUint32 aLength, const char * a,
                                               PRUint32 *bLength NS_INOUTPARAM, char * *b NS_INOUTPARAM,
                                               PRUint32 *rvLength NS_OUTPARAM, char * *rv NS_OUTPARAM)
{
    BUFFER_METHOD_IMPL(char, 1, TAKE_OWNERSHIP_NOOP);
}

/* void testSizedWstring (in unsigned long aLength, [size_is (aLength)] in wstring a,
 *                        inout unsigned long bLength, [size_is (bLength)] inout wstring b,
 *                        out unsigned long rvLength, [size_is (rvLength), retval] out wstring rv); */
NS_IMETHODIMP nsXPCTestParams::TestSizedWstring(PRUint32 aLength, const PRUnichar * a,
                                                PRUint32 *bLength NS_INOUTPARAM, PRUnichar * *b NS_INOUTPARAM,
                                                PRUint32 *rvLength NS_OUTPARAM, PRUnichar * *rv NS_OUTPARAM)
{
    BUFFER_METHOD_IMPL(PRUnichar, 1, TAKE_OWNERSHIP_NOOP);
}

/* void testInterfaceIs (in nsIIDPtr aIID, [iid_is (aIID)] in nsQIResult a,
 *                       inout nsIIDPtr bIID, [iid_is (bIID)] inout nsQIResult b,
 *                       out nsIIDPtr rvIID, [iid_is (rvIID), retval] out nsQIResult rv); */
NS_IMETHODIMP nsXPCTestParams::TestInterfaceIs(const nsIID *aIID, void *a,
                                               nsIID **bIID NS_INOUTPARAM, void **b NS_INOUTPARAM,
                                               nsIID **rvIID NS_OUTPARAM, void **rv NS_OUTPARAM)
{
    //
    // Getting the buffers and ownership right here can be a little tricky.
    //

    // The interface pointers are heap-allocated, and b has been AddRef'd
    // by XPConnect for the duration of the call. If we snatch it away from b
    // and leave no trace, XPConnect won't Release it. Since we also need to
    // return an already-AddRef'd pointer in rv, we don't need to do anything
    // special here.
    *rv = *b;

    // rvIID is out-only, so nobody allocated an IID buffer for us. Do that now,
    // and store b's IID in the new buffer.
    *rvIID = static_cast<nsIID*>(NS_Alloc(sizeof(nsID)));
    if (!*rvIID)
        return NS_ERROR_OUT_OF_MEMORY;
    **rvIID = **bIID;

    // Copy the interface pointer from a to b. Since a is in-only, XPConnect will
    // release it upon completion of the call. AddRef it for b.
    *b = a;
    static_cast<nsISupports*>(*b)->AddRef();

    // We already had a buffer allocated for b's IID, so we can re-use it.
    **bIID = *aIID;

    return NS_OK;
}

/* void testInterfaceIsArray (in unsigned long aLength, in nsIIDPtr aIID,
 *                            [array, size_is (aLength), iid_is (aIID)] in nsQIResult a,
 *                            inout unsigned long bLength, inout nsIIDPtr bIID,
 *                            [array, size_is (bLength), iid_is (bIID)] inout nsQIResult b,
 *                            out unsigned long rvLength, out nsIIDPtr rvIID,
 *                            [retval, array, size_is (rvLength), iid_is (rvIID)] out nsQIResult rv); */
NS_IMETHODIMP nsXPCTestParams::TestInterfaceIsArray(PRUint32 aLength, const nsIID *aIID,
                                                    void **a,
                                                    PRUint32 *bLength NS_INOUTPARAM, nsIID **bIID NS_INOUTPARAM,
                                                    void ***b NS_INOUTPARAM,
                                                    PRUint32 *rvLength NS_OUTPARAM, nsIID **rvIID NS_OUTPARAM,
                                                    void ***rv NS_OUTPARAM)
{
    // Transfer the IIDs. See the comments in TestInterfaceIs (above) for an
    // explanation of what we're doing.
    *rvIID = static_cast<nsIID*>(NS_Alloc(sizeof(nsID)));
    if (!*rvIID)
        return NS_ERROR_OUT_OF_MEMORY;
    **rvIID = **bIID;
    **bIID = *aIID;

    // The macro is agnostic to the actual interface types, so we can re-use code here.
    //
    // Do this second, since the macro returns.
    BUFFER_METHOD_IMPL(void*, 0, TAKE_OWNERSHIP_INTERFACE);
}
