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
 *               Kyle Yuan <kyle.yuan@sun.com>
 */

/*
 * CurrentPageActionEvents.h
 */
 
#ifndef CurrentPageActionEvents_h___
#define CurrentPageActionEvents_h___

#include "nsActions.h"

#include "nsIContentViewerEdit.h"
#include "nsISHistory.h"
#include "ns_util.h"

class wsCopySelectionEvent : public nsActionEvent {
public:
    wsCopySelectionEvent(NativeBrowserControl *yourInitContext);
    void * handleEvent (void);

protected:
    NativeBrowserControl *mInitContext;
};

class wsFindEvent : public nsActionEvent {
public:
    wsFindEvent(NativeBrowserControl *yourInitContext, jstring searchString,
                jboolean forward, jboolean matchCase);
    wsFindEvent(NativeBrowserControl *yourInitContext);
    void * handleEvent (void);

protected:
    NativeBrowserControl *mInitContext;
    jstring mSearchString;
    jboolean mForward;
    jboolean mMatchCase;
};

class wsGetURLEvent : public nsActionEvent {
public:
                       wsGetURLEvent   (NativeBrowserControl *yourInitContext);
        void    *      handleEvent     (void);
protected:
    NativeBrowserControl *mInitContext;
};

class wsSelectAllEvent : public nsActionEvent {
public:
    wsSelectAllEvent(NativeBrowserControl *yourInitContext);
    void * handleEvent (void);

protected:
    NativeBrowserControl *mInitContext;
};

/* PENDING(ashuk): remove this from here and in the motif directory
class wsViewSourceEvent : public nsActionEvent {
public:
    wsViewSourceEvent (nsIDocShell * docShell, PRBool viewMode);
    void * handleEvent (void);

protected:
    nsIDocShell * mDocShell;
    PRBool mViewMode;
};
*/

class wsGetDOMEvent : public nsActionEvent {
public:
    wsGetDOMEvent (JNIEnv *env, jclass clz, jmethodID yourID, jlong yourDoc);
    void * handleEvent (void);

protected:
    JNIEnv * mEnv;
    jclass mClazz;
    jmethodID mID;
    jlong mDoc;
};

class wsPrintEvent : public nsActionEvent {
public:
    wsPrintEvent(NativeBrowserControl *yourInitContext);
    void * handleEvent (void);

protected:
    NativeBrowserControl *mInitContext;
};

class wsPrintPreviewEvent : public nsActionEvent {
public:
    wsPrintPreviewEvent(NativeBrowserControl *yourInitContext, jboolean preview);
    void * handleEvent (void);

protected:
    NativeBrowserControl *mInitContext;
    jboolean mInPreview;
};

class wsGetSelectionEvent: public nsActionEvent {
public:
    wsGetSelectionEvent (JNIEnv *yourEnv, NativeBrowserControl *yourInitContext, jobject yourSelection);
    void * handleEvent (void);

protected:
    JNIEnv * mEnv;
    NativeBrowserControl *mInitContext;
    jobject mSelection;
};


class wsHighlightSelectionEvent: public nsActionEvent {
public:
    wsHighlightSelectionEvent (JNIEnv *yourEnv, NativeBrowserControl *yourInitContext, jobject startContainer, jobject endContainer, PRInt32 startOffset, PRInt32 endOffset);
    void * handleEvent (void);

protected:
    JNIEnv *mEnv;
    NativeBrowserControl *mInitContext;
    jobject mStartContainer;
    jobject mEndContainer;
    PRInt32 mStartOffset;
    PRInt32 mEndOffset;
};


class wsClearAllSelectionEvent: public nsActionEvent {
public:
    wsClearAllSelectionEvent (NativeBrowserControl *yourInitContext);
    void * handleEvent (void);

protected:
    NativeBrowserControl *mInitContext;
};

#endif /* CurrentPageActionEvents_h___ */

      
// EOF
