/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSVGDocument_h__
#define nsSVGDocument_h__

#include "nsXMLDocument.h"
#include "nsIDOMSVGDocument.h"

class nsSVGDocument : public nsXMLDocument,
                      public nsIDOMSVGDocument
{
public:
  using nsDocument::GetElementById;
  using nsDocument::SetDocumentURI;
  nsSVGDocument();
  virtual ~nsSVGDocument();

  NS_DECL_NSIDOMSVGDOCUMENT
  NS_FORWARD_NSIDOMDOCUMENT(nsXMLDocument::)
  // And explicitly import the things from nsDocument that we just shadowed
  using nsDocument::GetImplementation;
  using nsDocument::GetTitle;
  using nsDocument::SetTitle;
  using nsDocument::GetLastStyleSheetSet;
  using nsDocument::MozSetImageElement;
  using nsDocument::GetMozFullScreenElement;

  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_DECL_ISUPPORTS_INHERITED
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsXPCClassInfo* GetClassInfo();
};

#endif
