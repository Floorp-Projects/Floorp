/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsMsgStatusFeedback_h
#define _nsMsgStatusFeedback_h

#include "nsIWebProgressListener.h"
#include "nsIDOMWindowInternal.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsIMsgStatusFeedback.h"
#include "nsIProgressEventSink.h"
#include "nsIStringBundle.h"

class nsMsgStatusFeedback : public nsIMsgStatusFeedback,
                            public nsIProgressEventSink,
                            public nsIWebProgressListener
{
public:
	nsMsgStatusFeedback();
	virtual ~nsMsgStatusFeedback();

	NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGSTATUSFEEDBACK
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSIPROGRESSEVENTSINK

protected:
	nsIDOMWindowInternal			*mWindow;
	PRBool					m_meteorsSpinning;
	PRInt32					m_lastPercent;
	PRInt64					m_lastProgressTime;

  PRInt32 mQueuedMeteorStarts;
  PRInt32 mQueuedMeteorStops;
  nsCOMPtr<nsITimer> mStartTimer;
  nsCOMPtr<nsITimer> mStopTimer;

  void BeginObserving();
  void EndObserving();

  // timer callbacks
  static void notifyStartMeteors(nsITimer *aTimer, void *aClosure);
  static void notifyStopMeteors(nsITimer *aTimer, void *aClosure);

  // timer callbacks w/resolved closure
  void NotifyStartMeteors(nsITimer *aTimer);
  void NotifyStopMeteors(nsITimer *aTimer);

  // the JS status feedback implementation object...eventually this object
  // will replace this very C++ class you are looking at.
  nsCOMPtr<nsIMsgStatusFeedback> mStatusFeedback;

  static nsrefcnt gInstanceCount;
  static nsCOMPtr<nsIStringBundle> mBundle;
};

#endif // _nsMsgStatusFeedback_h
