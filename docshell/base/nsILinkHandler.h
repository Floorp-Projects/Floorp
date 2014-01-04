/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsILinkHandler_h___
#define nsILinkHandler_h___

#include "nsISupports.h"
#include "mozilla/EventForwards.h"

class nsIContent;
class nsIDocShell;
class nsIInputStream;
class nsIRequest;
class nsString;

// Interface ID for nsILinkHandler
#define NS_ILINKHANDLER_IID \
  { 0xceb9aade, 0x43da, 0x4f1a, \
    { 0xac, 0x8a, 0xc7, 0x09, 0xfb, 0x22, 0x46, 0x64 } }

/**
 * Interface used for handling clicks on links
 */
class nsILinkHandler : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ILINKHANDLER_IID)

  /**
   * Process a click on a link.
   *
   * @param aContent the content for the frame that generated the trigger
   * @param aURI a URI object that defines the destination for the link
   * @param aTargetSpec indicates where the link is targeted (may be an empty
   *        string)
   * @param aPostDataStream the POST data to send
   * @param aFileName non-null when the link should be downloaded as the given file
   * @param aHeadersDataStream ???
   * @param aIsTrusted false if the triggerer is an untrusted DOM event.
   */
  NS_IMETHOD OnLinkClick(nsIContent* aContent,
                         nsIURI* aURI,
                         const char16_t* aTargetSpec,
                         const nsAString& aFileName,
                         nsIInputStream* aPostDataStream,
                         nsIInputStream* aHeadersDataStream,
                         bool aIsTrusted) = 0;

  /**
   * Process a click on a link.
   *
   * Works the same as OnLinkClick() except it happens immediately rather than
   * through an event.
   *
   * @param aContent the content for the frame that generated the trigger
   * @param aURI a URI obect that defines the destination for the link
   * @param aTargetSpec indicates where the link is targeted (may be an empty
   *        string)
   * @param aFileName non-null when the link should be downloaded as the given file
   * @param aPostDataStream the POST data to send
   * @param aHeadersDataStream ???
   * @param aDocShell (out-param) the DocShell that the request was opened on
   * @param aRequest the request that was opened
   */
  NS_IMETHOD OnLinkClickSync(nsIContent* aContent,
                             nsIURI* aURI,
                             const char16_t* aTargetSpec,
                             const nsAString& aFileName,
                             nsIInputStream* aPostDataStream = 0,
                             nsIInputStream* aHeadersDataStream = 0,
                             nsIDocShell** aDocShell = 0,
                             nsIRequest** aRequest = 0) = 0;

  /**
   * Process a mouse-over a link.
   *
   * @param aContent the linked content.
   * @param aURI an URI object that defines the destination for the link
   * @param aTargetSpec indicates where the link is targeted (it may be an empty
   *        string)
   */
  NS_IMETHOD OnOverLink(nsIContent* aContent, 
                        nsIURI* aURLSpec,
                        const char16_t* aTargetSpec) = 0;

  /**
   * Process the mouse leaving a link.
   */
  NS_IMETHOD OnLeaveLink() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILinkHandler, NS_ILINKHANDLER_IID)

#endif /* nsILinkHandler_h___ */
