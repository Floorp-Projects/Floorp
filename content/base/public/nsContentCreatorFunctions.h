/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentCreatorFunctions_h__
#define nsContentCreatorFunctions_h__

#include "nscore.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/FromParser.h"

/**
 * Functions to create content, to be used only inside Gecko
 * (mozilla/content and mozilla/layout).
 */

class nsAString;
class nsIContent;
class nsINodeInfo;
class imgRequestProxy;
class nsNodeInfoManager;
class nsGenericHTMLElement;

nsresult
NS_NewElement(nsIContent** aResult,
              already_AddRefed<nsINodeInfo> aNodeInfo,
              mozilla::dom::FromParser aFromParser);

nsresult
NS_NewXMLElement(nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo);

nsresult
NS_NewHTMLElement(nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo,
                  mozilla::dom::FromParser aFromParser);

// First argument should be nsHTMLTag, but that adds dependency to parser
// for a bunch of files.
already_AddRefed<nsGenericHTMLElement>
CreateHTMLElement(uint32_t aNodeType, already_AddRefed<nsINodeInfo> aNodeInfo,
                  mozilla::dom::FromParser aFromParser);

nsresult
NS_NewMathMLElement(nsIContent** aResult,
                     already_AddRefed<nsINodeInfo> aNodeInfo);

#ifdef MOZ_XUL
nsresult
NS_NewXULElement(nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo);

void
NS_TrustedNewXULElement(nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo);
#endif

nsresult
NS_NewSVGElement(nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo,
                 mozilla::dom::FromParser aFromParser);

nsresult
NS_NewGenConImageContent(nsIContent** aResult,
                         already_AddRefed<nsINodeInfo> aNodeInfo,
                         imgRequestProxy* aImageRequest);

#endif // nsContentCreatorFunctions_h__
