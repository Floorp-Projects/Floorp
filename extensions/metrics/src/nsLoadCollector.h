/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Metrics extension.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsLoadCollector_h_
#define nsLoadCollector_h_

// This file defines the load collector class, which monitors requests using
// the document loader service and records the events into the metrics service.
//
// The load collector logs <document action="load"/> events.
// This event has the following attributes:
//
// origin: The action which initiated the load (string).  Possible values are:
//         "typed": The user typed or pasted the URI into the location bar.
//         "link": The user followed a link to the URI.
//         "reload": The user reloaded the URI.
//         "refresh": A meta-refresh caused the URI to be loaded.
//         "session-history": The user used back/forward to load the URI.
//         "global-history": The user selected the URI from global history.
//         "bookmark": The user selected the URI from bookmarks.
//         "external": An external application passed in the URI to load.
//         "other": Any origin not listed above.
//
// bfCacheHit: The document presentation was restored from the
//             session history cache (boolean).
//
// window: The id of the window where the document loaded (uint16).
// loadtime: The elapsed time for the load, in milliseconds (uint32).

#include "nsIMetricsCollector.h"
#include "nsIWebProgressListener.h"
#include "nsIWritablePropertyBag2.h"
#include "nsWeakReference.h"
#include "nsDataHashtable.h"
#include "nsAutoPtr.h"
#include "nsIDocumentObserver.h"
#include "nsPtrHashKey.h"

class nsIDocument;

class nsLoadCollector : public nsIMetricsCollector,
                        public nsIWebProgressListener,
                        public nsIDocumentObserver,
                        public nsSupportsWeakReference
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMETRICSCOLLECTOR
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSIDOCUMENTOBSERVER

  nsLoadCollector();
  nsresult Init();

 private:
  struct RequestEntry {
    nsCOMPtr<nsIWritablePropertyBag2> properties;
    PRTime startTime;
  };

  struct DocumentEntry {
    PRUint32 docID;
    PRUint32 windowID;
  };

  ~nsLoadCollector();

  // Callback for removing a document observer and map entry
  static PLDHashOperator PR_CALLBACK
  RemoveDocumentFromMap(const nsIDocument *document,
                        DocumentEntry &entry, void *userData);

  // Hash table mapping nsIRequest objects to their event properties.
  nsDataHashtable<nsISupportsHashKey, RequestEntry> mRequestMap;

  // Documents we're currently listening to, and their associated ids
  nsDataHashtable< nsPtrHashKey<nsIDocument>, DocumentEntry > mDocumentMap;

  // The next document id we'll assign
  PRUint32 mNextDocID;
};

#define NS_LOADCOLLECTOR_CLASSNAME "Load Collector"
#define NS_LOADCOLLECTOR_CID \
{ 0xa97357a0, 0xa2f3, 0x4b1f, {0x93, 0xd3, 0x36, 0xdc, 0xb7, 0xee, 0x24, 0x63}}

#endif // nsLoadCollector_h_
