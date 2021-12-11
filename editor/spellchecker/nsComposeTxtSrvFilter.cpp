/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComposeTxtSrvFilter.h"
#include "nsError.h"              // for NS_OK
#include "nsIContent.h"           // for nsIContent
#include "nsLiteralString.h"      // for NS_LITERAL_STRING
#include "mozilla/dom/Element.h"  // for nsIContent

using namespace mozilla;

bool nsComposeTxtSrvFilter::Skip(nsINode* aNode) const {
  if (NS_WARN_IF(!aNode)) {
    return false;
  }

  // Check to see if we can skip this node

  if (aNode->IsAnyOfHTMLElements(nsGkAtoms::script, nsGkAtoms::textarea,
                                 nsGkAtoms::select, nsGkAtoms::style,
                                 nsGkAtoms::map)) {
    return true;
  }

  if (!mIsForMail) {
    return false;
  }

  // For nodes that are blockquotes, we must make sure
  // their type is "cite"
  if (aNode->IsHTMLElement(nsGkAtoms::blockquote)) {
    return aNode->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                           nsGkAtoms::cite, eIgnoreCase);
  }

  if (aNode->IsHTMLElement(nsGkAtoms::span)) {
    if (aNode->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::mozquote,
                                        nsGkAtoms::_true, eIgnoreCase)) {
      return true;
    }

    return aNode->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::_class,
                                           nsGkAtoms::mozsignature,
                                           eCaseMatters);
  }

  if (aNode->IsHTMLElement(nsGkAtoms::table)) {
    return aNode->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::_class,
                                           u"moz-email-headers-table"_ns,
                                           eCaseMatters);
  }

  return false;
}

// static
UniquePtr<nsComposeTxtSrvFilter> nsComposeTxtSrvFilter::CreateHelper(
    bool aIsForMail) {
  auto filter = MakeUnique<nsComposeTxtSrvFilter>();
  filter->Init(aIsForMail);
  return filter;
}
