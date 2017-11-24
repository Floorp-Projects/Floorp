/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentCreatorFunctions_h__
#define nsContentCreatorFunctions_h__

#include "nsError.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/FromParser.h"

/**
 * Functions to create content, to be used only inside Gecko
 * (mozilla/content and mozilla/layout).
 */

class nsIContent;
class imgRequestProxy;
class nsGenericHTMLElement;

namespace mozilla {
namespace dom {
class Element;
class NodeInfo;
struct CustomElementDefinition;
} // namespace dom
} // namespace mozilla

nsresult
NS_NewElement(mozilla::dom::Element** aResult,
              already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
              mozilla::dom::FromParser aFromParser,
              const nsAString* aIs = nullptr);

nsresult
NS_NewXMLElement(mozilla::dom::Element** aResult,
                 already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

nsresult
NS_NewHTMLElement(mozilla::dom::Element** aResult,
                  already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                  mozilla::dom::FromParser aFromParser,
                  nsAtom* aIsAtom = nullptr,
                  mozilla::dom::CustomElementDefinition* aDefinition = nullptr);

// First argument should be nsHTMLTag, but that adds dependency to parser
// for a bunch of files.
already_AddRefed<nsGenericHTMLElement>
CreateHTMLElement(uint32_t aNodeType,
                  already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                  mozilla::dom::FromParser aFromParser);

nsresult
NS_NewMathMLElement(mozilla::dom::Element** aResult,
                    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

#ifdef MOZ_XUL
nsresult
NS_NewXULElement(mozilla::dom::Element** aResult,
                 already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                 mozilla::dom::FromParser aFromParser);

void
NS_TrustedNewXULElement(mozilla::dom::Element** aResult,
                        already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
#endif

nsresult
NS_NewSVGElement(mozilla::dom::Element** aResult,
                 already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                 mozilla::dom::FromParser aFromParser);

nsresult
NS_NewGenConImageContent(nsIContent** aResult,
                         already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                         imgRequestProxy* aImageRequest);

#endif // nsContentCreatorFunctions_h__
