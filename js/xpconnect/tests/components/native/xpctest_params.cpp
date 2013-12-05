/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpctest_private.h"
#include "xpctest_interfaces.h"
#include "js/Value.h"

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
    uint32_t elemSize = sizeof(type);                                         \
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
NS_IMETHODIMP nsXPCTestParams::TestBoolean(bool a, bool *b, bool *_retval)
{
    GENERIC_METHOD_IMPL;
}

/* octet testOctet (in octet a, inout octet b); */
NS_IMETHODIMP nsXPCTestParams::TestOctet(uint8_t a, uint8_t *b, uint8_t *_retval)
{
    GENERIC_METHOD_IMPL;
}

/* short testShort (in short a, inout short b); */
NS_IMETHODIMP nsXPCTestParams::TestShort(int16_t a, int16_t *b, int16_t *_retval)
{
    GENERIC_METHOD_IMPL;
}

/* long testLong (in long a, inout long b); */
NS_IMETHODIMP nsXPCTestParams::TestLong(int32_t a, int32_t *b, int32_t *_retval)
{
    GENERIC_METHOD_IMPL;
}

/* long long testLongLong (in long long a, inout long long b); */
NS_IMETHODIMP nsXPCTestParams::TestLongLong(int64_t a, int64_t *b, int64_t *_retval)
{
    GENERIC_METHOD_IMPL;
}

/* unsigned short testUnsignedShort (in unsigned short a, inout unsigned short b); */
NS_IMETHODIMP nsXPCTestParams::TestUnsignedShort(uint16_t a, uint16_t *b, uint16_t *_retval)
{
    GENERIC_METHOD_IMPL;
}

/* unsigned long testUnsignedLong (in unsigned long a, inout unsigned long b); */
NS_IMETHODIMP nsXPCTestParams::TestUnsignedLong(uint32_t a, uint32_t *b, uint32_t *_retval)
{
    GENERIC_METHOD_IMPL;
}

/* unsigned long long testUnsignedLongLong (in unsigned long long a, inout unsigned long long b); */
NS_IMETHODIMP nsXPCTestParams::TestUnsignedLongLong(uint64_t a, uint64_t *b, uint64_t *_retval)
{
    GENERIC_METHOD_IMPL;
}

/* float testFloat (in float a, inout float b); */
NS_IMETHODIMP nsXPCTestParams::TestFloat(float a, float *b, float *_retval)
{
    GENERIC_METHOD_IMPL;
}

/* double testDouble (in double a, inout float b); */
NS_IMETHODIMP nsXPCTestParams::TestDouble(double a, float *b, double *_retval)
{
    GENERIC_METHOD_IMPL;
}

/* char testChar (in char a, inout char b); */
NS_IMETHODIMP nsXPCTestParams::TestChar(char a, char *b, char *_retval)
{
    GENERIC_METHOD_IMPL;
}

/* string testString (in string a, inout string b); */
NS_IMETHODIMP nsXPCTestParams::TestString(const char * a, char * *b, char * *_retval)
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
NS_IMETHODIMP nsXPCTestParams::TestWchar(PRUnichar a, PRUnichar *b, PRUnichar *_retval)
{
    GENERIC_METHOD_IMPL;
}

/* wstring testWstring (in wstring a, inout wstring b); */
NS_IMETHODIMP nsXPCTestParams::TestWstring(const PRUnichar * a, PRUnichar * *b, PRUnichar * *_retval)
{
    nsDependentString aprime(a);
    nsDependentString bprime(*b);
    *_retval = ToNewUnicode(bprime);
    *b = ToNewUnicode(aprime);

    // XPCOM ownership rules dictate that overwritten inout params must be callee-freed.
    // See https://developer.mozilla.org/en/XPIDL
    NS_Free((void*)bprime.get());

    return NS_OK;
}

/* DOMString testDOMString (in DOMString a, inout DOMString b); */
NS_IMETHODIMP nsXPCTestParams::TestDOMString(const nsAString & a, nsAString & b, nsAString & _retval)
{
    STRING_METHOD_IMPL;
}


/* AString testAString (in AString a, inout AString b); */
NS_IMETHODIMP nsXPCTestParams::TestAString(const nsAString & a, nsAString & b, nsAString & _retval)
{
    STRING_METHOD_IMPL;
}

/* AUTF8String testAUTF8String (in AUTF8String a, inout AUTF8String b); */
NS_IMETHODIMP nsXPCTestParams::TestAUTF8String(const nsACString & a, nsACString & b, nsACString & _retval)
{
    STRING_METHOD_IMPL;
}

/* ACString testACString (in ACString a, inout ACString b); */
NS_IMETHODIMP nsXPCTestParams::TestACString(const nsACString & a, nsACString & b, nsACString & _retval)
{
    STRING_METHOD_IMPL;
}

/* jsval testJsval (in jsval a, inout jsval b); */
NS_IMETHODIMP nsXPCTestParams::TestJsval(const jsval & a, jsval & b, jsval *_retval)
{
    *_retval = b;
    b = a;
    return NS_OK;
}

/* void testShortArray (in unsigned long aLength, [array, size_is (aLength)] in short a,
 *                      inout unsigned long bLength, [array, size_is (bLength)] inout short b,
 *                      out unsigned long rvLength, [array, size_is (rvLength), retval] out short rv); */
NS_IMETHODIMP nsXPCTestParams::TestShortArray(uint32_t aLength, int16_t *a,
                                              uint32_t *bLength, int16_t **b,
                                              uint32_t *rvLength, int16_t **rv)
{
    BUFFER_METHOD_IMPL(int16_t, 0, TAKE_OWNERSHIP_NOOP);
}

/* void testDoubleArray (in unsigned long aLength, [array, size_is (aLength)] in double a,
 *                       inout unsigned long bLength, [array, size_is (bLength)] inout double b,
 *                       out unsigned long rvLength, [array, size_is (rvLength), retval] out double rv); */
NS_IMETHODIMP nsXPCTestParams::TestDoubleArray(uint32_t aLength, double *a,
                                               uint32_t *bLength, double **b,
                                               uint32_t *rvLength,  double **rv)
{
    BUFFER_METHOD_IMPL(double, 0, TAKE_OWNERSHIP_NOOP);
}

/* void testStringArray (in unsigned long aLength, [array, size_is (aLength)] in string a,
 *                       inout unsigned long bLength, [array, size_is (bLength)] inout string b,
 *                       out unsigned long rvLength, [array, size_is (rvLength), retval] out string rv); */
NS_IMETHODIMP nsXPCTestParams::TestStringArray(uint32_t aLength, const char * *a,
                                               uint32_t *bLength, char * **b,
                                               uint32_t *rvLength, char * **rv)
{
    BUFFER_METHOD_IMPL(char*, 0, TAKE_OWNERSHIP_STRING);
}

/* void testWstringArray (in unsigned long aLength, [array, size_is (aLength)] in wstring a,
 *                        inout unsigned long bLength, [array, size_is (bLength)] inout wstring b,
 *                        out unsigned long rvLength, [array, size_is (rvLength), retval] out wstring rv); */
NS_IMETHODIMP nsXPCTestParams::TestWstringArray(uint32_t aLength, const PRUnichar * *a,
                                                uint32_t *bLength, PRUnichar * **b,
                                                uint32_t *rvLength, PRUnichar * **rv)
{
    BUFFER_METHOD_IMPL(PRUnichar*, 0, TAKE_OWNERSHIP_WSTRING);
}

/* void testInterfaceArray (in unsigned long aLength, [array, size_is (aLength)] in nsIXPCTestInterfaceA a,
 *                          inout unsigned long bLength, [array, size_is (bLength)] inout nsIXPCTestInterfaceA b,
 *                          out unsigned long rvLength, [array, size_is (rvLength), retval] out nsIXPCTestInterfaceA rv); */
NS_IMETHODIMP nsXPCTestParams::TestInterfaceArray(uint32_t aLength, nsIXPCTestInterfaceA **a,
                                                  uint32_t *bLength, nsIXPCTestInterfaceA * **b,
                                                  uint32_t *rvLength, nsIXPCTestInterfaceA * **rv)
{
    BUFFER_METHOD_IMPL(nsIXPCTestInterfaceA*, 0, TAKE_OWNERSHIP_INTERFACE);
}

/* void testSizedString (in unsigned long aLength, [size_is (aLength)] in string a,
 *                       inout unsigned long bLength, [size_is (bLength)] inout string b,
 *                       out unsigned long rvLength, [size_is (rvLength), retval] out string rv); */
NS_IMETHODIMP nsXPCTestParams::TestSizedString(uint32_t aLength, const char * a,
                                               uint32_t *bLength, char * *b,
                                               uint32_t *rvLength, char * *rv)
{
    BUFFER_METHOD_IMPL(char, 1, TAKE_OWNERSHIP_NOOP);
}

/* void testSizedWstring (in unsigned long aLength, [size_is (aLength)] in wstring a,
 *                        inout unsigned long bLength, [size_is (bLength)] inout wstring b,
 *                        out unsigned long rvLength, [size_is (rvLength), retval] out wstring rv); */
NS_IMETHODIMP nsXPCTestParams::TestSizedWstring(uint32_t aLength, const PRUnichar * a,
                                                uint32_t *bLength, PRUnichar * *b,
                                                uint32_t *rvLength, PRUnichar * *rv)
{
    BUFFER_METHOD_IMPL(PRUnichar, 1, TAKE_OWNERSHIP_NOOP);
}

/* void testInterfaceIs (in nsIIDPtr aIID, [iid_is (aIID)] in nsQIResult a,
 *                       inout nsIIDPtr bIID, [iid_is (bIID)] inout nsQIResult b,
 *                       out nsIIDPtr rvIID, [iid_is (rvIID), retval] out nsQIResult rv); */
NS_IMETHODIMP nsXPCTestParams::TestInterfaceIs(const nsIID *aIID, void *a,
                                               nsIID **bIID, void **b,
                                               nsIID **rvIID, void **rv)
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
NS_IMETHODIMP nsXPCTestParams::TestInterfaceIsArray(uint32_t aLength, const nsIID *aIID,
                                                    void **a,
                                                    uint32_t *bLength, nsIID **bIID,
                                                    void ***b,
                                                    uint32_t *rvLength, nsIID **rvIID,
                                                    void ***rv)
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
