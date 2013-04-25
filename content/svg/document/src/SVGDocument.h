/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGDocument_h
#define mozilla_dom_SVGDocument_h

#include "mozilla/dom/XMLDocument.h"
#include "nsIDOMSVGDocument.h"

class nsSVGElement;

namespace mozilla {
namespace dom {

class SVGDocument : public XMLDocument,
                    public nsIDOMSVGDocument
{
public:
  using nsDocument::GetElementById;
  using nsDocument::SetDocumentURI;
  SVGDocument();
  virtual ~SVGDocument();

  NS_DECL_NSIDOMSVGDOCUMENT
  NS_FORWARD_NSIDOMDOCUMENT(mozilla::dom::XMLDocument::)
  // And explicitly import the things from nsDocument that we just shadowed
  using nsDocument::GetImplementation;
  using nsDocument::GetTitle;
  using nsDocument::SetTitle;
  using nsDocument::GetLastStyleSheetSet;
  using nsDocument::MozSetImageElement;
  using nsDocument::GetMozFullScreenElement;
  using nsIDocument::GetLocation;

  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_DECL_ISUPPORTS_INHERITED
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // WebIDL API
  void GetDomain(nsAString& aDomain, ErrorResult& aRv);
  nsSVGElement* GetRootElement(ErrorResult& aRv);

protected:
  virtual JSObject* WrapNode(JSContext *aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGDocument_h
