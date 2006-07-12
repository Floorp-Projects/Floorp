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

// This must be before any #includes to enable logging in release builds
#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

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
#include "nsInterfaceHashtable.h"
#include "nsPtrHashKey.h"
#include "blapit.h"

class nsILocalFile;
class nsIDOMWindow;
class nsIDOMDocument;
class nsIDOMNode;
class nsIMetricsCollector;

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

  // Creates an EventItem in the default metrics namespace.
  nsresult CreateEventItem(const nsAString &name, nsIMetricsEventItem **item)
  {
    return CreateEventItem(NS_LITERAL_STRING(NS_METRICS_NAMESPACE),
                           name, item);
  }

  // More convenient non-scriptable version of GetWindowID().
  static PRUint32 GetWindowID(nsIDOMWindow *window);

  // VC6 needs this to be public :-(
  nsresult Init();

  // Returns the window id map (readonly)
  const nsDataHashtable< nsPtrHashKey<nsIDOMWindow>, PRUint32 >&
  WindowMap() const
  {
    return mWindowMap;
  }

  // Creates a one-way hash of the given string.
  nsresult HashUTF8(const nsCString &str, nsCString &hashed);

  // Convenience method for hashing UTF-16 strings.
  // The string is converted to UTF-8, then HashUTF8() is called.
  nsresult HashUTF16(const nsString &str, nsCString &hashed) {
    return HashUTF8(NS_ConvertUTF16toUTF8(str), hashed);
  }

private:
  nsMetricsService();
  ~nsMetricsService();

  // Post-profile-initialization startup code
  nsresult ProfileStartup();

  // Reads the config, starts a new session, and turns on collectors
  nsresult StartCollection();

  // Stops collectors and removes all metrics-related prefs and files
  nsresult StopCollection();

  // Starts and stops collectors based on the current configuration
  void EnableCollectors();
  
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

  // Initialize our timer for the next upload.
  // If immediate is true, the timer will be fired as soon as possible,
  // which is at the deferred-upload time if one exists, or immediately
  // if not.
  void InitUploadTimer(PRBool immediate);

  // A reference to the local file containing our current configuration
  void GetConfigFile(nsIFile **result);

  // A reference to the local file where we'll download the server response.
  // We don't replace the real config file until we know the new one is valid.
  void GetConfigTempFile(nsIFile **result);

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

  // Called to persist mEventCount.  Returns "true" if succeeded.
  PRBool PersistEventCount();

  // Hashes a byte buffer of the given length
  nsresult HashBytes(const PRUint8 *bytes, PRUint32 length,
                     nsACString &result);

  // Does the real work of GetWindowID().
  PRUint32 GetWindowIDInternal(nsIDOMWindow *window);

  // Tries to load a new config.  If successful, the old config file is
  // replaced with the new one.  If the new config couldn't be loaded,
  // a config file is written which disables collection and preserves the
  // upload interval from the old config.  Returns true if the new config
  // file was loaded successfully.
  PRBool LoadNewConfig(nsIFile *newConfig, nsIFile *oldConfig);

  // Removes the existing data file (metrics.xml)
  void RemoveDataFile();

  // Generates a random interval, in seconds, between 12 and 36 hours.
  PRInt32 GetRandomUploadInterval();

  static PLDHashOperator PR_CALLBACK
  PruneDisabledCollectors(const nsAString &key,
                          nsCOMPtr<nsIMetricsCollector> &value,
                          void *userData);

  static PLDHashOperator PR_CALLBACK
  DetachCollector(const nsAString &key,
                  nsIMetricsCollector *value, void *userData);

  static PLDHashOperator PR_CALLBACK
  NotifyNewLog(const nsAString &key,
               nsIMetricsCollector *value, void *userData);

  // Helpers to set a pref and then flush the pref file to disk.
  static nsresult FlushIntPref(const char *prefName, PRInt32 prefValue);
  static nsresult FlushCharPref(const char *prefName, const char *prefValue);
  static nsresult FlushClearPref(const char *prefName);

  // Returns true if the pref to enable collection is set to true
  static PRBool CollectionEnabled();

private:
  class BadCertListener;

  // Pointer to the metrics service singleton
  static nsMetricsService* sMetricsService;

  nsMetricsConfig mConfig;

  // This output stream is non-null when we are downloading the config file.
  nsCOMPtr<nsIOutputStream> mConfigOutputStream;

  // XML document containing events to be flushed.
  nsCOMPtr<nsIDOMDocument> mDocument;

  // Root element of the XML document
  nsCOMPtr<nsIDOMNode> mRoot;

  // MD5 hashing object for collectors to use
  MD5Context *mMD5Context;

  // Window to incrementing-id map.  The keys are nsIDOMWindow*.
  nsDataHashtable< nsPtrHashKey<nsIDOMWindow>, PRUint32 > mWindowMap;

  // All of the active observers, keyed by name.
  nsInterfaceHashtable<nsStringHashKey, nsIMetricsCollector> mCollectorMap;

  // Timer object used for uploads
  nsCOMPtr<nsITimer> mUploadTimer;

  // The max number of times we'll retry the upload before stopping
  // for several hours.
  static const PRUint32 kMaxRetries;

  PRInt32 mEventCount;
  PRInt32 mSuspendCount;
  PRBool mUploading;
  nsString mSessionID;
  // the next window id to hand out
  PRUint32 mNextWindowID;

  // The number of times we've tried to upload
  PRUint32 mRetryCount;
};

class nsMetricsUtils
{
public:
  // Creates a new nsIWritablePropertyBag2 instance.
  static nsresult NewPropertyBag(nsIWritablePropertyBag2 **result);

  // Creates a new item with the given properties, and appends it to the parent
  static nsresult AddChildItem(nsIMetricsEventItem *parent,
                               const nsAString &childName,
                               nsIPropertyBag *childProperties);

  // Loops until the given number of random bytes have been returned
  // from the OS.  Returns true on success, or false if no random
  // bytes are available
  static PRBool GetRandomNoise(void *buf, PRSize size);

  // Creates a new element in the metrics namespace, using the given
  // ownerDocument and tag.
  static nsresult CreateElement(nsIDOMDocument *ownerDoc,
                                const nsAString &tag, nsIDOMElement **element);
};

#endif  // nsMetricsService_h__
