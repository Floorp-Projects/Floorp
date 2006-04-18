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
 *  Darin Fisher <darin@meer.net>
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

#ifndef nsMetricsService_h__
#define nsMetricsService_h__

#include "nsIMetricsService.h"
#include "nsMetricsModule.h"
#include "nsMetricsConfig.h"
#include "nsIAboutModule.h"
#include "nsIStreamListener.h"
#include "nsIOutputStream.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "prio.h"
#include "prlog.h"
#include "nsIWritablePropertyBag2.h"
#include "nsDataHashtable.h"

class nsILocalFile;
class nsIDOMWindow;
class nsIDOMDocument;
class nsIDOMNode;

#ifdef PR_LOGGING
// Shared log for the metrics service and collectors.
// (NSPR_LOG_MODULES=nsMetricsService:5)
extern PRLogModuleInfo *gMetricsLog;
#endif
#define MS_LOG(args) PR_LOG(gMetricsLog, PR_LOG_DEBUG, args)
#define MS_LOG_ENABLED() PR_LOG_TEST(gMetricsLog, PR_LOG_DEBUG)

// This is the namespace for the built-in metrics events.
#define NS_METRICS_NAMESPACE "http://www.mozilla.org/metrics"

// This class manages the metrics datastore.  It is responsible for persisting
// the data when the browser closes and for uploading it to the metrics server
// periodically.

class nsMetricsService : public nsIMetricsService
                       , public nsIAboutModule
                       , public nsIStreamListener
                       , public nsIObserver
                       , public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMETRICSSERVICE
  NS_DECL_NSIABOUTMODULE
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK
  
  // Get the metrics service singleton.  This method will call do_GetService if
  // necessary to fetch the metrics service.  It relies on the service manager
  // to keep the singleton instance alive.  This method may return null!
  static nsMetricsService* get();

  // Create the metrics service singleton, called only by the XPCOM factory for
  // this class.
  static NS_METHOD Create(nsISupports *outer, const nsIID &iid, void **result);

  // Helper function for logging events in the default namespace
  nsresult LogEvent(const nsAString &eventName,
                    nsIWritablePropertyBag2 *eventProperties)
  {
    nsCOMPtr<nsIPropertyBag> bag = do_QueryInterface(eventProperties);
    NS_ENSURE_STATE(bag);
    return LogSimpleEvent(NS_LITERAL_STRING(NS_METRICS_NAMESPACE), eventName,
                          bag);
  }

  // Get the window id for the given DOMWindow.  If a window id has not
  // yet been assigned for the window, the next id will be used.
  static PRUint32 GetWindowID(nsIDOMWindow *window);

  // VC6 needs this to be public :-(
  nsresult Init();

private:
  nsMetricsService();
  ~nsMetricsService();

  // Post-profile-initialization startup code
  nsresult ProfileStartup();

  // Starts and stops collectors based on the current configuration
  nsresult EnableCollectors();
  
  // Creates a new root element to hold event nodes
  nsresult CreateRoot();

  nsresult UploadData();
  nsresult GetDataFile(nsCOMPtr<nsILocalFile> *result);
  nsresult OpenDataFile(PRUint32 flags, PRFileDesc **result);
  nsresult GetDataFileForUpload(nsCOMPtr<nsILocalFile> *result);

  // This method returns an input stream containing the complete XML for the
  // data to upload.
  nsresult OpenCompleteXMLStream(nsILocalFile *dataFile,
                                 nsIInputStream **result);

  // Hook ourselves up to the timer manager.
  void RegisterUploadTimer();

  // A reference to the local file containing our current configuration
  void GetConfigFile(nsIFile **result);

  // Generate a new random client id string
  nsresult GenerateClientID(nsCString &clientID);

  // Check if a built-in event is enabled
  PRBool IsEventEnabled(const nsAString &event) const
  {
    return mConfig.IsEventEnabled(NS_LITERAL_STRING(NS_METRICS_NAMESPACE),
                                  event);
  }

  // Builds up a DOMElement tree from the given item and its children
  nsresult BuildEventItem(nsIMetricsEventItem *item,
                          nsIDOMElement **itemElement);

private:
  // Pointer to the metrics service singleton
  static nsMetricsService* sMetricsService;

  nsMetricsConfig mConfig;

  // This output stream is non-null when we are downloading the config file.
  nsCOMPtr<nsIOutputStream> mConfigOutputStream;

  // XML document containing events to be flushed.
  nsCOMPtr<nsIDOMDocument> mDocument;
  // Root element of the XML document
  nsCOMPtr<nsIDOMNode> mRoot;

  // Window to incrementing-id map.  The keys are nsIDOMWindow*.
  nsDataHashtable<nsVoidPtrHashKey, PRUint32> mWindowMap;

  PRInt32 mEventCount;
  PRInt32 mSuspendCount;
  PRBool mUploading;
  nsString mSessionID;
  // the next window id to hand out
  PRUint32 mNextWindowID;
};

class nsMetricsUtils
{
public:
  // Creates a new nsIWritablePropertyBag2 instance.
  static nsresult NewPropertyBag(nsIWritablePropertyBag2 **result);
};

#endif  // nsMetricsService_h__
