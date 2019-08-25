/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=4:expandtab:shiftwidth=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLayoutDebugCLH.h"
#include "nsArray.h"
#include "nsString.h"
#include "plstr.h"
#include "nsCOMPtr.h"
#include "nsIWindowWatcher.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindow.h"
#include "nsISupportsPrimitives.h"
#include "nsICommandLine.h"
#include "nsIURI.h"

nsLayoutDebugCLH::nsLayoutDebugCLH() {}

nsLayoutDebugCLH::~nsLayoutDebugCLH() {}

NS_IMPL_ISUPPORTS(nsLayoutDebugCLH, ICOMMANDLINEHANDLER)

static nsresult HandleFlagWithOptionalArgument(nsICommandLine* aCmdLine,
                                               const nsAString& aName,
                                               const nsAString& aDefaultValue,
                                               nsAString& aValue,
                                               bool& aFlagPresent) {
  aValue = EmptyString();
  aFlagPresent = false;

  nsresult rv;
  int32_t idx;

  rv = aCmdLine->FindFlag(aName, false, &idx);
  NS_ENSURE_SUCCESS(rv, rv);
  if (idx < 0) return NS_OK;

  aFlagPresent = true;

  int32_t length;
  aCmdLine->GetLength(&length);

  bool argPresent = false;

  if (idx + 1 < length) {
    rv = aCmdLine->GetArgument(idx + 1, aValue);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!aValue.IsEmpty() && aValue.CharAt(0) == '-') {
      aValue = EmptyString();
    } else {
      argPresent = true;
    }
  }

  if (!argPresent) {
    aValue = aDefaultValue;
  }

  return aCmdLine->RemoveArguments(idx, idx + argPresent);
}

static nsresult AppendArg(nsIMutableArray* aArray, const nsAString& aString) {
  nsCOMPtr<nsISupportsString> s =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
  NS_ENSURE_TRUE(s, NS_ERROR_FAILURE);
  s->SetData(aString);
  return aArray->AppendElement(s);
}

NS_IMETHODIMP
nsLayoutDebugCLH::Handle(nsICommandLine* aCmdLine) {
  nsresult rv;
  bool flagPresent;

  nsString url;

  rv =
      HandleFlagWithOptionalArgument(aCmdLine, NS_LITERAL_STRING("layoutdebug"),
                                     EmptyString(), url, flagPresent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!flagPresent) {
    return NS_OK;
  }

  nsCOMPtr<nsIMutableArray> argsArray = nsArray::Create();

  if (!url.IsEmpty()) {
    nsCOMPtr<nsIURI> uri;
    nsAutoCString resolvedSpec;

    rv = aCmdLine->ResolveURI(url, getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = uri->GetSpec(resolvedSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = AppendArg(argsArray, NS_ConvertUTF8toUTF16(resolvedSpec));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIWindowWatcher> wwatch =
      do_GetService(NS_WINDOWWATCHER_CONTRACTID);
  NS_ENSURE_TRUE(wwatch, NS_ERROR_FAILURE);

  nsCOMPtr<mozIDOMWindowProxy> opened;
  wwatch->OpenWindow(nullptr, "chrome://layoutdebug/content/", "_blank",
                     "chrome,dialog=no,all", argsArray, getter_AddRefs(opened));
  aCmdLine->SetPreventDefault(true);
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebugCLH::GetHelpInfo(nsACString& aResult) {
  aResult.AssignLiteral("  --layoutdebug [<url>] Start with Layout Debugger\n");
  return NS_OK;
}
