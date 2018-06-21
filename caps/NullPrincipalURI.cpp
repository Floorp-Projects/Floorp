/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NullPrincipalURI.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"

#include "mozilla/ipc/URIParams.h"

#include "nsEscape.h"
#include "nsCRT.h"
#include "nsIUUIDGenerator.h"

////////////////////////////////////////////////////////////////////////////////
//// NullPrincipalURI

NullPrincipalURI::NullPrincipalURI()
{
}

NullPrincipalURI::NullPrincipalURI(const NullPrincipalURI& aOther)
{
  mPath.Assign(aOther.mPath);
}

nsresult
NullPrincipalURI::Init()
{
  // FIXME: bug 327161 -- make sure the uuid generator is reseeding-resistant.
  nsCOMPtr<nsIUUIDGenerator> uuidgen = services::GetUUIDGenerator();
  NS_ENSURE_TRUE(uuidgen, NS_ERROR_NOT_AVAILABLE);

  nsID id;
  nsresult rv = uuidgen->GenerateUUIDInPlace(&id);
  NS_ENSURE_SUCCESS(rv, rv);

  mPath.SetLength(NSID_LENGTH - 1); // -1 because NSID_LENGTH counts the '\0'
  id.ToProvidedString(
    *reinterpret_cast<char(*)[NSID_LENGTH]>(mPath.BeginWriting()));

  MOZ_ASSERT(mPath.Length() == NSID_LENGTH - 1);
  MOZ_ASSERT(strlen(mPath.get()) == NSID_LENGTH - 1);

  return NS_OK;
}

/* static */
already_AddRefed<NullPrincipalURI>
NullPrincipalURI::Create()
{
  RefPtr<NullPrincipalURI> uri = new NullPrincipalURI();
  nsresult rv = uri->Init();
  NS_ENSURE_SUCCESS(rv, nullptr);
  return uri.forget();
}

static NS_DEFINE_CID(kNullPrincipalURIImplementationCID,
                     NS_NULLPRINCIPALURI_IMPLEMENTATION_CID);

NS_IMPL_ADDREF(NullPrincipalURI)
NS_IMPL_RELEASE(NullPrincipalURI)

NS_INTERFACE_MAP_BEGIN(NullPrincipalURI)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURI)
  if (aIID.Equals(kNullPrincipalURIImplementationCID))
    foundInterface = static_cast<nsIURI*>(this);
  else
  NS_INTERFACE_MAP_ENTRY(nsIURI)
  NS_INTERFACE_MAP_ENTRY(nsISizeOf)
  NS_INTERFACE_MAP_ENTRY(nsIIPCSerializableURI)
NS_INTERFACE_MAP_END

////////////////////////////////////////////////////////////////////////////////
//// nsIURI

NS_IMETHODIMP
NullPrincipalURI::GetAsciiHost(nsACString& _host)
{
  _host.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
NullPrincipalURI::GetAsciiHostPort(nsACString& _hostport)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullPrincipalURI::GetAsciiSpec(nsACString& _spec)
{
  nsAutoCString buffer;
  // Ignore the return value -- NullPrincipalURI::GetSpec() is infallible.
  Unused << GetSpec(buffer);
  // This uses the infallible version of |NS_EscapeURL| as |GetSpec| is
  // already infallible.
  NS_EscapeURL(buffer, esc_OnlyNonASCII | esc_AlwaysCopy, _spec);
  return NS_OK;
}

NS_IMETHODIMP
NullPrincipalURI::GetHost(nsACString& _host)
{
  _host.Truncate();
  return NS_OK;
}

nsresult
NullPrincipalURI::SetHost(const nsACString& aHost)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullPrincipalURI::GetHostPort(nsACString& _host)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NullPrincipalURI::SetHostPort(const nsACString& aHost)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullPrincipalURI::GetPassword(nsACString& _password)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NullPrincipalURI::SetPassword(const nsACString& aPassword)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullPrincipalURI::GetPathQueryRef(nsACString& _path)
{
  _path = mPath;
  return NS_OK;
}

nsresult
NullPrincipalURI::SetPathQueryRef(const nsACString& aPath)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullPrincipalURI::GetFilePath(nsACString& aFilePath)
{
  aFilePath.Truncate();
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NullPrincipalURI::SetFilePath(const nsACString& aFilePath)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullPrincipalURI::GetQuery(nsACString& aQuery)
{
  aQuery.Truncate();
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NullPrincipalURI::SetQuery(const nsACString& aQuery)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NullPrincipalURI::SetQueryWithEncoding(const nsACString& aQuery,
                                       const Encoding* aEncoding)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullPrincipalURI::GetRef(nsACString& _ref)
{
  _ref.Truncate();
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NullPrincipalURI::SetRef(const nsACString& aRef)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullPrincipalURI::GetPrePath(nsACString& _prePath)
{
  _prePath = NS_LITERAL_CSTRING(NS_NULLPRINCIPAL_SCHEME ":");
  return NS_OK;
}

NS_IMETHODIMP
NullPrincipalURI::GetPort(int32_t* _port)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NullPrincipalURI::SetPort(int32_t aPort)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullPrincipalURI::GetScheme(nsACString& _scheme)
{
  _scheme = NS_LITERAL_CSTRING(NS_NULLPRINCIPAL_SCHEME);
  return NS_OK;
}

nsresult
NullPrincipalURI::SetScheme(const nsACString& aScheme)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullPrincipalURI::GetSpec(nsACString& _spec)
{
  _spec = NS_LITERAL_CSTRING(NS_NULLPRINCIPAL_SCHEME ":") + mPath;
  return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
NullPrincipalURI::GetSpecIgnoringRef(nsACString& _result)
{
  return GetSpec(_result);
}

NS_IMETHODIMP
NullPrincipalURI::GetHasRef(bool* _result)
{
  *_result = false;
  return NS_OK;
}

nsresult
NullPrincipalURI::SetSpecInternal(const nsACString& aSpec)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullPrincipalURI::GetUsername(nsACString& _username)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NullPrincipalURI::SetUsername(const nsACString& aUsername)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NullPrincipalURI::GetUserPass(nsACString& _userPass)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NullPrincipalURI::SetUserPass(const nsACString& aUserPass)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NullPrincipalURI::Clone(nsIURI** _newURI)
{
  nsCOMPtr<nsIURI> uri = new NullPrincipalURI(*this);
  uri.forget(_newURI);
  return NS_OK;
}

NS_IMETHODIMP
NullPrincipalURI::CloneIgnoringRef(nsIURI** _newURI)
{
  // GetRef/SetRef not supported by NullPrincipalURI, so
  // CloneIgnoringRef() is the same as Clone().
  return Clone(_newURI);
}

NS_IMETHODIMP
NullPrincipalURI::CloneWithNewRef(const nsACString& newRef, nsIURI** _newURI)
{
  // GetRef/SetRef not supported by NullPrincipalURI, so
  // CloneWithNewRef() is the same as Clone().
  return Clone(_newURI);
}

NS_IMPL_ISUPPORTS(NullPrincipalURI::Mutator, nsIURISetters, nsIURIMutator)

NS_IMETHODIMP
NullPrincipalURI::Mutate(nsIURIMutator** aMutator)
{
    RefPtr<NullPrincipalURI::Mutator> mutator = new NullPrincipalURI::Mutator();
    nsresult rv = mutator->InitFromURI(this);
    if (NS_FAILED(rv)) {
        return rv;
    }
    mutator.forget(aMutator);
    return NS_OK;
}

NS_IMETHODIMP
NullPrincipalURI::Equals(nsIURI* aOther, bool* _equals)
{
  *_equals = false;
  RefPtr<NullPrincipalURI> otherURI;
  nsresult rv = aOther->QueryInterface(kNullPrincipalURIImplementationCID,
                                       getter_AddRefs(otherURI));
  if (NS_SUCCEEDED(rv)) {
    *_equals = mPath == otherURI->mPath;
  }
  return NS_OK;
}

NS_IMETHODIMP
NullPrincipalURI::EqualsExceptRef(nsIURI* aOther, bool* _equals)
{
  // GetRef/SetRef not supported by NullPrincipalURI, so
  // EqualsExceptRef() is the same as Equals().
  return Equals(aOther, _equals);
}

NS_IMETHODIMP
NullPrincipalURI::Resolve(const nsACString& aRelativePath,
                            nsACString& _resolvedURI)
{
  _resolvedURI = aRelativePath;
  return NS_OK;
}

NS_IMETHODIMP
NullPrincipalURI::SchemeIs(const char* aScheme, bool* _schemeIs)
{
  *_schemeIs = (0 == nsCRT::strcasecmp(NS_NULLPRINCIPAL_SCHEME, aScheme));
  return NS_OK;
}

NS_IMETHODIMP
NullPrincipalURI::GetDisplaySpec(nsACString &aUnicodeSpec)
{
  return GetSpec(aUnicodeSpec);
}

NS_IMETHODIMP
NullPrincipalURI::GetDisplayHostPort(nsACString &aUnicodeHostPort)
{
  return GetHostPort(aUnicodeHostPort);
}

NS_IMETHODIMP
NullPrincipalURI::GetDisplayHost(nsACString &aUnicodeHost)
{
  return GetHost(aUnicodeHost);
}

NS_IMETHODIMP
NullPrincipalURI::GetDisplayPrePath(nsACString &aPrePath)
{
    return GetPrePath(aPrePath);
}

////////////////////////////////////////////////////////////////////////////////
//// nsIIPCSerializableURI

void
NullPrincipalURI::Serialize(mozilla::ipc::URIParams& aParams)
{
  aParams = mozilla::ipc::NullPrincipalURIParams();
}

bool
NullPrincipalURI::Deserialize(const mozilla::ipc::URIParams& aParams)
{
  if (aParams.type() != mozilla::ipc::URIParams::TNullPrincipalURIParams) {
    MOZ_ASSERT_UNREACHABLE("unexpected URIParams type");
    return false;
  }

  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
//// nsISizeOf

size_t
NullPrincipalURI::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return mPath.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

size_t
NullPrincipalURI::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}
