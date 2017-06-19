/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
class nsIPrincipal;

class FramingChecker {

public:
  // Determine if X-Frame-Options allows content to be framed
  // as a subdocument
  static bool CheckFrameOptions(nsIChannel* aChannel,
                                nsIDocShell* aDocShell,
                                nsIPrincipal* aPrincipal);

protected:
  enum XFOHeader
  {
    eDENY,
    eSAMEORIGIN,
    eALLOWFROM
  };

  static bool CheckOneFrameOptionsPolicy(nsIHttpChannel* aHttpChannel,
                                         const nsAString& aPolicy,
                                         nsIDocShell* aDocShell);

  static void ReportXFOViolation(nsIDocShellTreeItem* aTopDocShellItem,
                                 nsIURI* aThisURI,
                                 XFOHeader aHeader);
};

#endif /* mozilla_dom_FramingChecker_h */
