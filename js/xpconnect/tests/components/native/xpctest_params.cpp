/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpctest_private.h"
#include "xpctest_interfaces.h"
#include "js/Value.h"

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIURI.h"

NS_IMPL_ISUPPORTS(nsXPCTestParams, nsIXPCTestParams)

nsXPCTestParams::nsXPCTestParams() = default;

nsXPCTestParams::~nsXPCTestParams() = default;

#define GENERIC_METHOD_IMPL \
  {                         \
    *_retval = *b;          \
    *b = a;                 \
    return NS_OK;           \
  }

#define STRING_METHOD_IMPL \
  {                        \
    _retval.Assign(b);     \
    b.Assign(a);           \
    return NS_OK;          \
  }

#define SEQUENCE_METHOD_IMPL(TAKE_OWNERSHIP)                        \
  {                                                                 \
    _retval.SwapElements(b);                                        \
    b = a.Clone();                                                  \
    for (uint32_t i = 0; i < b.Length(); ++i) TAKE_OWNERSHIP(b[i]); \
    return NS_OK;                                                   \
  }

#define TAKE_OWNERSHIP_NOOP(val) \
  {}
#define TAKE_OWNERSHIP_INTERFACE(val) \
  { static_cast<nsISupports*>(val)->AddRef(); }
#define TAKE_OWNERSHIP_STRING(val)  \
  {                                 \
    nsDependentCString vprime(val); \
    val = ToNewCString(vprime);     \
  }
#define TAKE_OWNERSHIP_WSTRING(val) \
  {                                 \
    nsDependentString vprime(val);  \
    val = ToNewUnicode(vprime);     \
  }

// Macro for our buffer-oriented types:
//   'type' is the type of element that the buffer contains.
//   'padding' is an offset added to length, allowing us to handle
//             null-terminated strings.
//   'TAKE_OWNERSHIP' is one of the macros above.
#define BUFFER_METHOD_IMPL(type, padding, TAKE_OWNERSHIP)                      \
  {                                                                            \
    uint32_t elemSize = sizeof(type);                                          \
                                                                               \
    /* Copy b into rv. */                                                      \
    *rvLength = *bLength;                                                      \
    *rv = static_cast<type*>(moz_xmalloc(elemSize * (*bLength + padding)));    \
    memcpy(*rv, *b, elemSize*(*bLength + padding));                            \
                                                                               \
    /* Copy a into b. */                                                       \
    *bLength = aLength;                                                        \
    free(*b);                                                                  \
    *b = static_cast<type*>(moz_xmalloc(elemSize * (aLength + padding)));      \
    memcpy(*b, a, elemSize*(aLength + padding));                               \
                                                                               \
    /* We need to take ownership of the data we got from a,                    \
       since the caller owns it. */                                            \
    for (unsigned i = 0; i < *bLength + padding; ++i) TAKE_OWNERSHIP((*b)[i]); \
                                                                               \
    return NS_OK;                                                              \
  }

NS_IMETHODIMP nsXPCTestParams::TestBoolean(bool a, bool* b, bool* _retval) {
  GENERIC_METHOD_IMPL;
}

NS_IMETHODIMP nsXPCTestParams::TestOctet(uint8_t a, uint8_t* b,
                                         uint8_t* _retval) {
  GENERIC_METHOD_IMPL;
}

NS_IMETHODIMP nsXPCTestParams::TestShort(int16_t a, int16_t* b,
                                         int16_t* _retval) {
  GENERIC_METHOD_IMPL;
}

NS_IMETHODIMP nsXPCTestParams::TestLong(int32_t a, int32_t* b,
                                        int32_t* _retval) {
  GENERIC_METHOD_IMPL;
}

NS_IMETHODIMP nsXPCTestParams::TestLongLong(int64_t a, int64_t* b,
                                            int64_t* _retval) {
  GENERIC_METHOD_IMPL;
}

NS_IMETHODIMP nsXPCTestParams::TestUnsignedShort(uint16_t a, uint16_t* b,
                                                 uint16_t* _retval) {
  GENERIC_METHOD_IMPL;
}

NS_IMETHODIMP nsXPCTestParams::TestUnsignedLong(uint32_t a, uint32_t* b,
                                                uint32_t* _retval) {
  GENERIC_METHOD_IMPL;
}

NS_IMETHODIMP nsXPCTestParams::TestUnsignedLongLong(uint64_t a, uint64_t* b,
                                                    uint64_t* _retval) {
  GENERIC_METHOD_IMPL;
}

NS_IMETHODIMP nsXPCTestParams::TestFloat(float a, float* b, float* _retval) {
  GENERIC_METHOD_IMPL;
}

NS_IMETHODIMP nsXPCTestParams::TestDouble(double a, float* b, double* _retval) {
  GENERIC_METHOD_IMPL;
}

NS_IMETHODIMP nsXPCTestParams::TestChar(char a, char* b, char* _retval) {
  GENERIC_METHOD_IMPL;
}

NS_IMETHODIMP nsXPCTestParams::TestString(const char* a, char** b,
                                          char** _retval) {
  nsDependentCString aprime(a);
  nsDependentCString bprime(*b);
  *_retval = ToNewCString(bprime);
  *b = ToNewCString(aprime);

  // XPCOM ownership rules dictate that overwritten inout params must be
  // callee-freed. See https://developer.mozilla.org/en/XPIDL
  free(const_cast<char*>(bprime.get()));

  return NS_OK;
}

NS_IMETHODIMP nsXPCTestParams::TestWchar(char16_t a, char16_t* b,
                                         char16_t* _retval) {
  GENERIC_METHOD_IMPL;
}

NS_IMETHODIMP nsXPCTestParams::TestWstring(const char16_t* a, char16_t** b,
                                           char16_t** _retval) {
  nsDependentString aprime(a);
  nsDependentString bprime(*b);
  *_retval = ToNewUnicode(bprime);
  *b = ToNewUnicode(aprime);

  // XPCOM ownership rules dictate that overwritten inout params must be
  // callee-freed. See https://developer.mozilla.org/en/XPIDL
  free((void*)bprime.get());

  return NS_OK;
}

NS_IMETHODIMP nsXPCTestParams::TestAString(const nsAString& a, nsAString& b,
                                           nsAString& _retval) {
  STRING_METHOD_IMPL;
}

NS_IMETHODIMP nsXPCTestParams::TestAUTF8String(const nsACString& a,
                                               nsACString& b,
                                               nsACString& _retval) {
  STRING_METHOD_IMPL;
}

NS_IMETHODIMP nsXPCTestParams::TestACString(const nsACString& a, nsACString& b,
                                            nsACString& _retval) {
  STRING_METHOD_IMPL;
}

NS_IMETHODIMP nsXPCTestParams::TestJsval(JS::Handle<JS::Value> a,
                                         JS::MutableHandle<JS::Value> b,
                                         JS::MutableHandle<JS::Value> _retval) {
  _retval.set(b);
  b.set(a);
  return NS_OK;
}

NS_IMETHODIMP nsXPCTestParams::TestShortArray(uint32_t aLength, int16_t* a,
                                              uint32_t* bLength, int16_t** b,
                                              uint32_t* rvLength,
                                              int16_t** rv) {
  BUFFER_METHOD_IMPL(int16_t, 0, TAKE_OWNERSHIP_NOOP);
}

NS_IMETHODIMP nsXPCTestParams::TestDoubleArray(uint32_t aLength, double* a,
                                               uint32_t* bLength, double** b,
                                               uint32_t* rvLength,
                                               double** rv) {
  BUFFER_METHOD_IMPL(double, 0, TAKE_OWNERSHIP_NOOP);
}

NS_IMETHODIMP nsXPCTestParams::TestStringArray(uint32_t aLength, const char** a,
                                               uint32_t* bLength, char*** b,
                                               uint32_t* rvLength, char*** rv) {
  BUFFER_METHOD_IMPL(char*, 0, TAKE_OWNERSHIP_STRING);
}

NS_IMETHODIMP nsXPCTestParams::TestWstringArray(
    uint32_t aLength, const char16_t** a, uint32_t* bLength, char16_t*** b,
    uint32_t* rvLength, char16_t*** rv) {
  BUFFER_METHOD_IMPL(char16_t*, 0, TAKE_OWNERSHIP_WSTRING);
}

NS_IMETHODIMP nsXPCTestParams::TestInterfaceArray(
    uint32_t aLength, nsIXPCTestInterfaceA** a, uint32_t* bLength,
    nsIXPCTestInterfaceA*** b, uint32_t* rvLength, nsIXPCTestInterfaceA*** rv) {
  BUFFER_METHOD_IMPL(nsIXPCTestInterfaceA*, 0, TAKE_OWNERSHIP_INTERFACE);
}

NS_IMETHODIMP nsXPCTestParams::TestJsvalArray(uint32_t aLength, JS::Value* a,
                                              uint32_t* bLength, JS::Value** b,
                                              uint32_t* rvLength,
                                              JS::Value** rv) {
  BUFFER_METHOD_IMPL(JS::Value, 0, TAKE_OWNERSHIP_NOOP);
}

NS_IMETHODIMP nsXPCTestParams::TestSizedString(uint32_t aLength, const char* a,
                                               uint32_t* bLength, char** b,
                                               uint32_t* rvLength, char** rv) {
  BUFFER_METHOD_IMPL(char, 1, TAKE_OWNERSHIP_NOOP);
}

NS_IMETHODIMP nsXPCTestParams::TestSizedWstring(uint32_t aLength,
                                                const char16_t* a,
                                                uint32_t* bLength, char16_t** b,
                                                uint32_t* rvLength,
                                                char16_t** rv) {
  BUFFER_METHOD_IMPL(char16_t, 1, TAKE_OWNERSHIP_NOOP);
}

NS_IMETHODIMP nsXPCTestParams::TestInterfaceIs(const nsIID* aIID, void* a,
                                               nsIID** bIID, void** b,
                                               nsIID** rvIID, void** rv) {
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
  *rvIID = static_cast<nsIID*>(moz_xmalloc(sizeof(nsID)));
  **rvIID = **bIID;

  // Copy the interface pointer from a to b. Since a is in-only, XPConnect will
  // release it upon completion of the call. AddRef it for b.
  *b = a;
  static_cast<nsISupports*>(*b)->AddRef();

  // We already had a buffer allocated for b's IID, so we can re-use it.
  **bIID = *aIID;

  return NS_OK;
}

NS_IMETHODIMP nsXPCTestParams::TestInterfaceIsArray(
    uint32_t aLength, const nsIID* aIID, void** a, uint32_t* bLength,
    nsIID** bIID, void*** b, uint32_t* rvLength, nsIID** rvIID, void*** rv) {
  // Transfer the IIDs. See the comments in TestInterfaceIs (above) for an
  // explanation of what we're doing.
  *rvIID = static_cast<nsIID*>(moz_xmalloc(sizeof(nsID)));
  **rvIID = **bIID;
  **bIID = *aIID;

  // The macro is agnostic to the actual interface types, so we can re-use code
  // here.
  //
  // Do this second, since the macro returns.
  BUFFER_METHOD_IMPL(void*, 0, TAKE_OWNERSHIP_INTERFACE);
}

NS_IMETHODIMP nsXPCTestParams::TestOutAString(nsAString& o) {
  o.AssignLiteral("out");
  return NS_OK;
}

NS_IMETHODIMP nsXPCTestParams::TestStringArrayOptionalSize(const char** a,
                                                           uint32_t length,
                                                           nsACString& out) {
  out.Truncate();
  for (uint32_t i = 0; i < length; ++i) {
    out.Append(a[i]);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXPCTestParams::TestShortSequence(const nsTArray<short>& a, nsTArray<short>& b,
                                   nsTArray<short>& _retval) {
  SEQUENCE_METHOD_IMPL(TAKE_OWNERSHIP_NOOP);
}

NS_IMETHODIMP
nsXPCTestParams::TestDoubleSequence(const nsTArray<double>& a,
                                    nsTArray<double>& b,
                                    nsTArray<double>& _retval) {
  SEQUENCE_METHOD_IMPL(TAKE_OWNERSHIP_NOOP);
}

NS_IMETHODIMP
nsXPCTestParams::TestInterfaceSequence(
    const nsTArray<RefPtr<nsIXPCTestInterfaceA>>& a,
    nsTArray<RefPtr<nsIXPCTestInterfaceA>>& b,
    nsTArray<RefPtr<nsIXPCTestInterfaceA>>& _retval) {
  SEQUENCE_METHOD_IMPL(TAKE_OWNERSHIP_NOOP);
}

NS_IMETHODIMP
nsXPCTestParams::TestAStringSequence(const nsTArray<nsString>& a,
                                     nsTArray<nsString>& b,
                                     nsTArray<nsString>& _retval) {
  SEQUENCE_METHOD_IMPL(TAKE_OWNERSHIP_NOOP);
}

NS_IMETHODIMP
nsXPCTestParams::TestACStringSequence(const nsTArray<nsCString>& a,
                                      nsTArray<nsCString>& b,
                                      nsTArray<nsCString>& _retval) {
  SEQUENCE_METHOD_IMPL(TAKE_OWNERSHIP_NOOP);
}

NS_IMETHODIMP
nsXPCTestParams::TestJsvalSequence(const nsTArray<JS::Value>& a,
                                   nsTArray<JS::Value>& b,
                                   nsTArray<JS::Value>& _retval) {
  SEQUENCE_METHOD_IMPL(TAKE_OWNERSHIP_NOOP);
}

NS_IMETHODIMP
nsXPCTestParams::TestSequenceSequence(const nsTArray<nsTArray<short>>& a,
                                      nsTArray<nsTArray<short>>& b,
                                      nsTArray<nsTArray<short>>& _retval) {
  _retval = std::move(b);
  for (const auto& element : a) {
    b.AppendElement(element.Clone());
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXPCTestParams::TestInterfaceIsSequence(const nsIID* aIID,
                                         const nsTArray<void*>& a, nsIID** bIID,
                                         nsTArray<void*>& b, nsIID** rvIID,
                                         nsTArray<void*>& _retval) {
  // Shuffle around our nsIIDs
  *rvIID = (*bIID)->Clone();
  *bIID = aIID->Clone();

  // Perform the generic sequence shuffle.
  SEQUENCE_METHOD_IMPL(TAKE_OWNERSHIP_INTERFACE);
}

NS_IMETHODIMP
nsXPCTestParams::TestOptionalSequence(const nsTArray<uint8_t>& aInArr,
                                      nsTArray<uint8_t>& aReturnArr) {
  aReturnArr = aInArr.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsXPCTestParams::TestOmittedOptionalOut(nsIURI** aOut) {
  MOZ_ASSERT(!(*aOut), "Unexpected value received");
  // Call the js component, to check XPConnect won't crash when passing nullptr
  // as the optional out parameter, and that the out object is built regardless.
  nsresult rv;
  nsCOMPtr<nsIXPCTestParams> jsComponent =
      do_CreateInstance("@mozilla.org/js/xpc/test/js/Params;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  // Invoke it directly passing nullptr.
  rv = jsComponent->TestOmittedOptionalOut(nullptr);
  NS_ENSURE_SUCCESS(rv, rv);
  // Also invoke it with a ref pointer.
  nsCOMPtr<nsIURI> someURI;
  rv = jsComponent->TestOmittedOptionalOut(getter_AddRefs(someURI));
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString spec;
  rv = someURI->GetSpec(spec);
  if (!spec.EqualsLiteral("http://example.com/")) {
    return NS_ERROR_UNEXPECTED;
  }
  someURI.forget(aOut);
  return NS_OK;
}
