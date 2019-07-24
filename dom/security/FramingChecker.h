/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FramingChecker_h
#define mozilla_dom_FramingChecker_h

class nsIDocShell;
class nsIChannel;
class nsIHttpChannel;
class nsIDocShellTreeItem;
class nsIURI;
class nsIContentSecurityPolicy;

class FramingChecker {
 public:
  // Determine if X-Frame-Options allows content to be framed
  // as a subdocument
  static bool CheckFrameOptions(nsIChannel* aChannel, nsIDocShell* aDocShell,
                                nsIContentSecurityPolicy* aCSP);

 protected:
  enum XFOHeader { eDENY, eSAMEORIGIN };

  /**
   * Logs to the window about a X-Frame-Options error.
   *
   * @param aMessageTag the error message identifier to log
   * @param aParentDocShellItem the containing docshell that the frame is
   * loading into
   * @param aChildURI the URI of the frame attempting to load
   * @param aPolicy the header value string from the frame
   */
  static void ReportError(const char* aMessageTag,
                          nsIDocShellTreeItem* aParentDocShellItem,
                          nsIURI* aChildURI, const nsAString& aPolicy);

  static bool CheckOneFrameOptionsPolicy(nsIHttpChannel* aHttpChannel,
                                         const nsAString& aPolicy,
                                         nsIDocShell* aDocShell);
};

#endif /* mozilla_dom_FramingChecker_h */
