/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object holding utility CSS functions */

#include "CSS.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ServoBindings.h"
#include "nsGlobalWindow.h"
#include "nsIDocument.h"
#include "nsIURI.h"
#include "nsStyleUtil.h"
#include "xpcpublic.h"

namespace mozilla {
namespace dom {

struct SupportsParsingInfo
{
  nsIURI* mDocURI;
  nsIURI* mBaseURI;
  nsIPrincipal* mPrincipal;
};

static nsresult
GetParsingInfo(const GlobalObject& aGlobal,
               SupportsParsingInfo& aInfo)
{
  nsGlobalWindowInner* win = xpc::WindowOrNull(aGlobal.Get());
  if (!win) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc = win->GetDoc();
  if (!doc) {
    return NS_ERROR_FAILURE;
  }

  aInfo.mDocURI = nsCOMPtr<nsIURI>(doc->GetDocumentURI()).get();
  aInfo.mBaseURI = nsCOMPtr<nsIURI>(doc->GetBaseURI()).get();
  aInfo.mPrincipal = win->GetPrincipal();
  return NS_OK;
}

/* static */ bool
CSS::Supports(const GlobalObject& aGlobal,
              const nsAString& aProperty,
              const nsAString& aValue,
              ErrorResult& aRv)
{
  SupportsParsingInfo info;

  nsresult rv = GetParsingInfo(aGlobal, info);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  NS_ConvertUTF16toUTF8 property(aProperty);
  NS_ConvertUTF16toUTF8 value(aValue);
  return Servo_CSSSupports2(&property, &value);
}

/* static */ bool
CSS::Supports(const GlobalObject& aGlobal,
              const nsAString& aCondition,
              ErrorResult& aRv)
{
  SupportsParsingInfo info;

  nsresult rv = GetParsingInfo(aGlobal, info);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  NS_ConvertUTF16toUTF8 cond(aCondition);
  return Servo_CSSSupports(&cond);
}

/* static */ void
CSS::Escape(const GlobalObject& aGlobal,
            const nsAString& aIdent,
            nsAString& aReturn)
{
  nsStyleUtil::AppendEscapedCSSIdent(aIdent, aReturn);
}

} // namespace dom
} // namespace mozilla
