/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "wrapper.h"
#include "nsAString.h"
#include "nsString.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace ipdl {
namespace wrapper {

static uint8_t
source_string_loader(const nsACString* aFileName, nsACString* aRet)
{
  nsCOMPtr<nsIChannel> chan;
  nsCOMPtr<nsIInputStream> instream;
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIIOService> serv = do_GetService(NS_IOSERVICE_CONTRACTID);
  nsresult rv;

  rv = NS_NewURI(getter_AddRefs(uri), *aFileName);

  if (NS_FAILED(rv)) {
    NS_WARNING("FAILED TO GET URI");
    return 0;
  }

  rv = NS_NewChannel(getter_AddRefs(chan),
                    uri,
                    nsContentUtils::GetSystemPrincipal(),
                    nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                    nsIContentPolicy::TYPE_OTHER,
                    nullptr,  // PerformanceStorage
                    nullptr,  // aLoadGroup
                    nullptr,  // aCallbacks
                    nsIRequest::LOAD_NORMAL,
                    serv);

  if (NS_SUCCEEDED(rv)) {
    chan->SetContentType(NS_LITERAL_CSTRING("application/x-ipdl"));
    rv = chan->Open2(getter_AddRefs(instream));
  }

  if (NS_FAILED(rv)) {
    NS_WARNING("LOAD_ERROR_NOSTREAM");
    return 0;
  }

  int64_t len = -1;

  rv = chan->GetContentLength(&len);
  if (NS_FAILED(rv) || len == -1) {
    NS_WARNING("LOAD_ERROR_NOCONTENT");
    return 0;
  }

  if (len > INT32_MAX) {
    NS_WARNING("LOAD_ERROR_CONTENTTOOBIG");
    return 0;
  }

  rv = NS_ReadInputStreamToString(instream, *aRet, len);

  if (NS_FAILED(rv)) {
    NS_WARNING("FAILED TO READ");
    return 0;
  }

  return 1;
}

static uint8_t
resolve_relative_path(const nsACString* aFileName, const nsACString* aRelativePath, nsACString* aRet)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv;

  rv = NS_NewURI(getter_AddRefs(uri), *aFileName);

  if (NS_FAILED(rv)) {
    NS_WARNING("FAILED TO GET URI");
    return 0;
  }

  rv = uri->Resolve(*aRelativePath, *aRet);

  if (NS_FAILED(rv)) {
    NS_WARNING("FAILED TO RESOLVE URI");
    return 0;
  }

  return 1;
}

static uint8_t
uri_equals(const nsACString* aFileName1, const nsACString* aFileName2)
{
  nsCOMPtr<nsIURI> uri1;
  nsCOMPtr<nsIURI> uri2;
  nsresult rv;

  rv = NS_NewURI(getter_AddRefs(uri1), *aFileName1);

  if (NS_FAILED(rv)) {
    NS_WARNING("FAILED TO GET URI 1");
  }

  rv = NS_NewURI(getter_AddRefs(uri2), *aFileName2);

  if (NS_FAILED(rv)) {
    NS_WARNING("FAILED TO GET URI 2");
  }

  bool retval;

  rv = uri1->Equals(uri2, &retval);

  if (NS_FAILED(rv)) {
    NS_WARNING("FAILED TO COMPARE URIs");
  }

  return retval ? 1 : 0;
}

const ffi::AST*
Parse(const nsACString& aIPDLFile, nsACString& errorString)
{
  return ffi::ipdl_parse_file(&aIPDLFile, &errorString, source_string_loader, resolve_relative_path, uri_equals);
}

void
FreeAST(const ffi::AST* aAST)
{
  ffi::ipdl_free_ast(aAST);
}

const ffi::TranslationUnit*
GetTU(const ffi::AST* aAST, ffi::TUId aTUID)
{
  return ffi::ipdl_ast_get_tu(aAST, aTUID);
}

ffi::TUId
GetMainTUId(const ffi::AST* aAST)
{
  return ffi::ipdl_ast_main_tuid(aAST);
}

} // wrapper
} // ipdl
} // mozilla
