/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Brian Satterfield <bsatterf@atl.lmco.com>
 *               Anthony Sizer <sizera@yahoo.com>
 */

/*
 * nsActions.h
 */
 
#ifndef NavigationActionEvents_h___
#define NavigationActionEvents_h___

#include "nsActions.h"
#include "nsIWebNavigation.h"
#include "nsString.h"
#include "nsIURI.h"
#include "ns_util.h"

struct NativeBrowserControl;
class InputStreamShim;


class wsLoadURLEvent : public nsActionEvent {
public:
                        wsLoadURLEvent (nsIWebNavigation* webNavigation, PRUnichar * urlString, PRInt32 urlLength);
                       ~wsLoadURLEvent ();
        void    *       handleEvent    (void);

protected:
        nsIWebNavigation *   mWebNavigation;
        nsString    *   mURL;
};

class wsLoadFromStreamEvent : public nsActionEvent {
public:
    wsLoadFromStreamEvent(NativeBrowserControl *yourInitContext, 
                          void *globalStream,
                          nsString &uriToCopy,
                          const char *contentTypeToCopy,
                          PRInt32 contentLength, void *globalLoadProperties);
    virtual ~wsLoadFromStreamEvent();
    void * handleEvent(void);

private:
    wsLoadFromStreamEvent(NativeBrowserControl *yourDocShell, 
                          InputStreamShim *yourShim);

protected:

    NativeBrowserControl *mInitContext;
    nsString mUriString;
    char *mContentType;       // MUST be delete'd in destructor
    void * mProperties;       // MUST be util_deleteGlobalRef'd in destructor.
    InputStreamShim *mShim;   // DO NOT delete this in the destructor
};


class wsPostEvent : public nsActionEvent {
public:
    wsPostEvent(NativeBrowserControl *yourInitContext, 
                nsIURI              *absoluteUrl,
                const PRUnichar      *targetToCopy,
                PRInt32              targetLength,
                PRInt32              postDataLength,
                const char          *postData,  
                PRInt32              postHeadersLength,
                const char          *postHeaders);

    virtual ~wsPostEvent();
    void * handleEvent(void);

private:

protected:

    NativeBrowserControl *mInitContext;
    nsCOMPtr<nsIURI>     mAbsoluteURI;          
    nsString            *mTarget;          
    const char          *mPostData;      
    const char          *mPostHeaders;   
    PRInt32              mPostDataLength;
    PRInt32              mPostHeaderLength;
};


class wsStopEvent : public nsActionEvent {
public:
                        wsStopEvent    (nsIWebNavigation* webNavigation);
        void    *       handleEvent    (void);

protected:
        nsIWebNavigation *   mWebNavigation;
};

// added by Mark Goddard OTMP 9/2/1999 
class wsRefreshEvent : public nsActionEvent {
public:
                        wsRefreshEvent (nsIWebNavigation* webNavigation, 
                                        PRInt32 reloadType);
        void    *       handleEvent    (void);

protected:
        nsIWebNavigation *   mWebNavigation;
        PRInt32 mReloadType;
};


class wsSetPromptEvent : public nsActionEvent {
public:
                        wsSetPromptEvent (wcIBrowserContainer* aBrowserContainer, jobject aUserPrompt);
        void    *       handleEvent    (void);

protected:
        wcIBrowserContainer *   mBrowserContainer;
        jobject mUserPrompt;
};



#endif /* NavigationActionEvents_h___ */

      
// EOF
