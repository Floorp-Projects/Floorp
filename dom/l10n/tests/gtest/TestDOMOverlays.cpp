/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/l10n/DOMOverlays.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DOMOverlaysBinding.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/L10nUtilsBinding.h"
#include "mozilla/NullPrincipal.h"
#include "nsNetUtil.h"

using mozilla::NullPrincipal;
using namespace mozilla::dom;
using namespace mozilla::dom::l10n;

already_AddRefed<Document> SetUpDocument() {
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "about:blank");
  nsCOMPtr<nsIPrincipal> principal =
      NullPrincipal::CreateWithoutOriginAttributes();
  nsCOMPtr<Document> document;
  nsresult rv = NS_NewDOMDocument(getter_AddRefs(document),
                                  EmptyString(),  // aNamespaceURI
                                  EmptyString(),  // aQualifiedName
                                  nullptr,        // aDoctype
                                  uri, uri, principal,
                                  false,    // aLoadedAsData
                                  nullptr,  // aEventObject
                                  DocumentFlavorHTML);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  return document.forget();
}

/**
 * This test verifies that the basic C++ DOMOverlays API
 * works correctly.
 */
TEST(DOM_L10n_DOMOverlays, Initial)
{
  mozilla::ErrorResult rv;

  // 1. Set up an HTML document.
  nsCOMPtr<Document> doc = SetUpDocument();

  // 2. Create a simple Element with a child.
  //
  //   <div>
  //     <a data-l10n-name="link" href="https://www.mozilla.org"></a>
  //   </div>
  //
  RefPtr<Element> elem = doc->CreateHTMLElement(nsGkAtoms::div);
  RefPtr<Element> span = doc->CreateHTMLElement(nsGkAtoms::a);
  span->SetAttribute(NS_LITERAL_STRING("data-l10n-name"),
                     NS_LITERAL_STRING("link"), rv);
  span->SetAttribute(NS_LITERAL_STRING("href"),
                     NS_LITERAL_STRING("https://www.mozilla.org"), rv);
  elem->AppendChild(*span, rv);

  // 3. Create an L10nValue with a translation for the element.
  L10nValue translation;
  translation.mValue.AssignLiteral(
      "Hello <a data-l10n-name=\"link\">World</a>.");

  // 4. Translate the element.
  nsTArray<DOMOverlaysError> errors;
  DOMOverlays::TranslateElement(*elem, translation, errors, rv);

  nsAutoString textContent;
  elem->GetInnerHTML(textContent, rv);

  // 5. Verify that the innerHTML matches the expectations.
  ASSERT_STREQ(NS_ConvertUTF16toUTF8(textContent).get(),
               "Hello <a data-l10n-name=\"link\" "
               "href=\"https://www.mozilla.org\">World</a>.");
}
