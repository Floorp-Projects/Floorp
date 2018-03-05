/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComposeTxtSrvFilter.h"
#include "nsError.h"                    // for NS_OK
#include "nsIContent.h"                 // for nsIContent
#include "nsIDOMNode.h"                 // for nsIDOMNode
#include "nsNameSpaceManager.h"        // for kNameSpaceID_None
#include "nsLiteralString.h"            // for NS_LITERAL_STRING
#include "nscore.h"                     // for NS_IMETHODIMP
#include "mozilla/dom/Element.h"                 // for nsIContent

nsComposeTxtSrvFilter::nsComposeTxtSrvFilter() :
  mIsForMail(false)
{
}

NS_IMPL_ISUPPORTS(nsComposeTxtSrvFilter, nsITextServicesFilter)

NS_IMETHODIMP
nsComposeTxtSrvFilter::Skip(nsIDOMNode* aNode, bool *_retval)
{
  *_retval = false;

  // Check to see if we can skip this node
  // For nodes that are blockquotes, we must make sure
  // their type is "cite"
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (content) {
    if (content->IsHTMLElement(nsGkAtoms::blockquote)) {
      if (mIsForMail) {
        *_retval = content->AsElement()->AttrValueIs(kNameSpaceID_None,
                                                     nsGkAtoms::type,
                                                     nsGkAtoms::cite,
                                                     eIgnoreCase);
      }
    } else if (content->IsHTMLElement(nsGkAtoms::span)) {
      if (mIsForMail) {
        *_retval = content->AsElement()->AttrValueIs(kNameSpaceID_None,
                                                     nsGkAtoms::mozquote,
                                                     nsGkAtoms::_true,
                                                     eIgnoreCase);
        if (!*_retval) {
          *_retval = content->AsElement()->AttrValueIs(kNameSpaceID_None,
                                                       nsGkAtoms::_class,
                                                       nsGkAtoms::mozsignature,
                                                       eCaseMatters);
        }
      }
    } else if (content->IsAnyOfHTMLElements(nsGkAtoms::script,
                                            nsGkAtoms::textarea,
                                            nsGkAtoms::select,
                                            nsGkAtoms::style,
                                            nsGkAtoms::map)) {
      *_retval = true;
    } else if (content->IsHTMLElement(nsGkAtoms::table)) {
      if (mIsForMail) {
        *_retval =
          content->AsElement()->AttrValueIs(kNameSpaceID_None,
                                            nsGkAtoms::_class,
                                            NS_LITERAL_STRING("moz-email-headers-table"),
                                            eCaseMatters);
      }
    }
  }

  return NS_OK;
}
