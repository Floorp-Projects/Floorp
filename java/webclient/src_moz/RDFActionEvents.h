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
 */

/*
 * nsActions.h
 */
 
#ifndef RDFActionEvents_h___
#define RDFActionEvents_h___

#include "nsActions.h"

struct WebShellInitContext;

class wsInitBookmarksEvent : public nsActionEvent {
public:
    wsInitBookmarksEvent(WebShellInitContext *yourInitContext);
    void * handleEvent(void);

protected:
    WebShellInitContext *mInitContext;
};

class wsNewRDFNodeEvent : public nsActionEvent {
public:
    wsNewRDFNodeEvent(WebShellInitContext *yourInitContext, 
                      const char *yourUrlString, PRBool yourIsFolder);
    void * handleEvent(void);

protected:
    WebShellInitContext *mInitContext;
    const char *mUrlString;
    PRBool mIsFolder;
};

class wsRDFIsContainerEvent : public nsActionEvent {
public:
    wsRDFIsContainerEvent(WebShellInitContext *yourInitContext, 
                          PRUint32 yourNativeRDFNode);
    void * handleEvent(void);

protected:
    WebShellInitContext *mInitContext;
    PRUint32 mNativeRDFNode;
};

class wsRDFGetChildAtEvent : public nsActionEvent {
public:
    wsRDFGetChildAtEvent(WebShellInitContext *yourInitContext, 
                         PRUint32 yourNativeRDFNode, PRUint32 childIndex);
    void * handleEvent(void);

protected:
    WebShellInitContext *mInitContext;
    PRUint32 mNativeRDFNode;
    PRUint32 mChildIndex;
};

class wsRDFGetChildCountEvent : public nsActionEvent {
public:
    wsRDFGetChildCountEvent(WebShellInitContext *yourInitContext, 
                            PRUint32 yourNativeRDFNode);
    void * handleEvent(void);

protected:
    WebShellInitContext *mInitContext;
    PRUint32 mNativeRDFNode;
};

class wsRDFGetChildIndexEvent : public nsActionEvent {
public:
    wsRDFGetChildIndexEvent(WebShellInitContext *yourInitContext, 
                            PRUint32 yourNativeRDFNode, 
                            PRUint32 yourChildRDFNode);
    void * handleEvent(void);

protected:
    WebShellInitContext *mInitContext;
    PRUint32 mNativeRDFNode;
    PRUint32 mChildRDFNode;
};

class wsRDFToStringEvent : public nsActionEvent {
public:
    wsRDFToStringEvent(WebShellInitContext *yourInitContext, 
                       PRUint32 yourNativeRDFNode);
    void * handleEvent(void);

protected:
    WebShellInitContext *mInitContext;
    PRUint32 mNativeRDFNode;
};

class wsRDFInsertElementAtEvent : public nsActionEvent {
public:
    wsRDFInsertElementAtEvent(WebShellInitContext *yourInitContext, 
                              PRUint32 yourParentRDFNode, 
                              PRUint32 yourChildRDFNode, 
                              void *yourChildProperties,
                              PRUint32 yourChildIndex);
    void * handleEvent(void);

protected:
    WebShellInitContext *mInitContext;
    PRUint32 mParentRDFNode;
    PRUint32 mChildRDFNode;
    void *mChildPropsJobject;
    PRUint32 mChildIndex;
};

class wsRDFNewFolderEvent : public nsActionEvent {
public:
    wsRDFNewFolderEvent(WebShellInitContext* yourInitContext, 
                        PRUint32 yourParentRDFNode, 
                        void *yourChildPropsJobject,
                        PRUint32 *yourRetVal);
    void * handleEvent(void);

protected:
    WebShellInitContext *mInitContext;
    PRUint32 mParentRDFNode;
    void *mChildPropsJobject;
    PRUint32 *mRetVal;
};

class wsRDFHasMoreElementsEvent : public nsActionEvent {
public:
    wsRDFHasMoreElementsEvent(WebShellInitContext *yourInitContext,
                              PRUint32 mNativeRDFNode,
                              void *yourJobject);
    void * handleEvent(void);

protected:
    WebShellInitContext *mInitContext;
    PRUint32 mNativeRDFNode;
    void *mJobject;
};

class wsRDFNextElementEvent : public nsActionEvent {
public:
    wsRDFNextElementEvent(WebShellInitContext *yourInitContext,
                          PRUint32 mNativeRDFNode,
                          void *yourJobject);
    void * handleEvent(void);

protected:
    WebShellInitContext *mInitContext;
    PRUint32 mNativeRDFNode;
    void *mJobject;
};

class wsRDFFinalizeEvent : public nsActionEvent {
public:
    wsRDFFinalizeEvent(void *yourJobject);
    void * handleEvent(void);

protected:
    void *mJobject;
};

#endif /* RDFActionEvents_h___ */

      
// EOF
