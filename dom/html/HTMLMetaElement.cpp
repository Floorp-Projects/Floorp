/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/HTMLMetaElement.h"
#include "mozilla/dom/HTMLMetaElementBinding.h"
#include "mozilla/dom/nsCSPService.h"
#include "mozilla/Logging.h"
#include "nsContentUtils.h"
#include "nsStyleConsts.h"
#include "nsIContentSecurityPolicy.h"

static mozilla::LazyLogModule gMetaElementLog("nsMetaElement");
#define LOG(msg) MOZ_LOG(gMetaElementLog, mozilla::LogLevel::Debug, msg)
#define LOG_ENABLED() MOZ_LOG_TEST(gMetaElementLog, mozilla::LogLevel::Debug)

NS_IMPL_NS_NEW_HTML_ELEMENT(Meta)

namespace mozilla {
namespace dom {

HTMLMetaElement::HTMLMetaElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

HTMLMetaElement::~HTMLMetaElement()
{
}


NS_IMPL_ELEMENT_CLONE(HTMLMetaElement)


void
HTMLMetaElement::SetMetaReferrer(nsIDocument* aDocument)
{
  if (!aDocument ||
      !AttrValueIs(kNameSpaceID_None, nsGkAtoms::name, nsGkAtoms::referrer, eIgnoreCase)) {
    return;
  }
  nsAutoString content;
  GetContent(content);

  Element* headElt = aDocument->GetHeadElement();
  if (headElt && nsContentUtils::ContentIsDescendantOf(this, headElt)) {
      content = nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(content);
      aDocument->SetHeaderData(nsGkAtoms::referrer, content);
  }
}

nsresult
HTMLMetaElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                              const nsAttrValue* aValue,
                              const nsAttrValue* aOldValue,
                              nsIPrincipal* aSubjectPrincipal,
                              bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    nsIDocument *document = GetUncomposedDoc();
    if (aName == nsGkAtoms::content) {
      if (document && AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                  nsGkAtoms::viewport, eIgnoreCase)) {
        nsAutoString content;
        GetContent(content);
        nsContentUtils::ProcessViewportInfo(document, content);
      }
      CreateAndDispatchEvent(document, NS_LITERAL_STRING("DOMMetaChanged"));
    }
    // Update referrer policy when it got changed from JS
    SetMetaReferrer(document);
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                            aOldValue, aSubjectPrincipal, aNotify);
}

nsresult
HTMLMetaElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                            nsIContent* aBindingParent,
                            bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aDocument &&
      AttrValueIs(kNameSpaceID_None, nsGkAtoms::name, nsGkAtoms::viewport, eIgnoreCase)) {
    nsAutoString content;
    GetContent(content);
    nsContentUtils::ProcessViewportInfo(aDocument, content);
  }

  if (StaticPrefs::security_csp_enable() && aDocument &&
      !aDocument->IsLoadedAsData() &&
      AttrValueIs(kNameSpaceID_None, nsGkAtoms::httpEquiv, nsGkAtoms::headerCSP, eIgnoreCase)) {

    // only accept <meta http-equiv="Content-Security-Policy" content=""> if it appears
    // in the <head> element.
    Element* headElt = aDocument->GetHeadElement();
    if (headElt && nsContentUtils::ContentIsDescendantOf(this, headElt)) {

      nsAutoString content;
      GetContent(content);
      content = nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(content);

      nsIPrincipal* principal = aDocument->NodePrincipal();
      nsCOMPtr<nsIContentSecurityPolicy> csp;
      principal->EnsureCSP(aDocument, getter_AddRefs(csp));
      if (csp) {
        if (LOG_ENABLED()) {
          nsAutoCString documentURIspec;
          nsIURI* documentURI = aDocument->GetDocumentURI();
          if (documentURI) {
            documentURI->GetAsciiSpec(documentURIspec);
          }

          LOG(("HTMLMetaElement %p sets CSP '%s' on document=%p, document-uri=%s",
               this, NS_ConvertUTF16toUTF8(content).get(), aDocument, documentURIspec.get()));
        }

        // Multiple CSPs (delivered through either header of meta tag) need to be
        // joined together, see:
        // https://w3c.github.io/webappsec/specs/content-security-policy/#delivery-html-meta-element
        rv = csp->AppendPolicy(content,
                               false, // csp via meta tag can not be report only
                               true); // delivered through the meta tag
        NS_ENSURE_SUCCESS(rv, rv);
        aDocument->ApplySettingsFromCSP(false);
      }
    }
  }

  // Referrer Policy spec requires a <meta name="referrer" tag to be in the
  // <head> element.
  SetMetaReferrer(aDocument);
  CreateAndDispatchEvent(aDocument, NS_LITERAL_STRING("DOMMetaAdded"));
  return rv;
}

void
HTMLMetaElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsCOMPtr<nsIDocument> oldDoc = GetUncomposedDoc();
  CreateAndDispatchEvent(oldDoc, NS_LITERAL_STRING("DOMMetaRemoved"));
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

void
HTMLMetaElement::CreateAndDispatchEvent(nsIDocument* aDoc,
                                        const nsAString& aEventName)
{
  if (!aDoc)
    return;

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this,
                             aEventName,
                             CanBubble::eYes,
                             ChromeOnlyDispatch::eYes);
  asyncDispatcher->RunDOMEventWhenSafe();
}

JSObject*
HTMLMetaElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLMetaElement_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
