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
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *
 */
#ifndef webshell____h
#define webshell____h

#include "nsError.h"
#include "nsIWebShellServices.h"
#include "nsIWebShell.h"
#include "nsILinkHandler.h"
#include "nsIClipboardCommands.h"
#include "nsDocShell.h"

class nsIController;
struct PRThread;

typedef enum {
    eCharsetReloadInit,
    eCharsetReloadRequested,
    eCharsetReloadStopOrigional
} eCharsetReloadState;

#define NS_ERROR_WEBSHELL_REQUEST_REJECTED  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GENERAL,1001)

class nsWebShell : public nsDocShell,
                   public nsIWebShell,
                   public nsIWebShellContainer,
                   public nsIWebShellServices,
                   public nsILinkHandler,
                   public nsIClipboardCommands
{
public:
    nsWebShell();
    virtual ~nsWebShell();

    NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSICLIPBOARDCOMMANDS
    NS_DECL_NSIWEBSHELLSERVICES

    NS_IMETHOD SetupNewViewer(nsIContentViewer* aViewer);

    // nsIContentViewerContainer
    NS_IMETHOD Embed(nsIContentViewer* aDocViewer,
                   const char* aCommand,
                   nsISupports* aExtraInfo);

    // nsIWebShell
    NS_IMETHOD SetContainer(nsIWebShellContainer* aContainer);
    NS_IMETHOD GetContainer(nsIWebShellContainer*& aResult);
    NS_IMETHOD GetTopLevelWindow(nsIWebShellContainer** aWebShellWindow);
    NS_IMETHOD GetRootWebShell(nsIWebShell*& aResult);
    /*NS_IMETHOD SetParent(nsIWebShell* aParent);
    NS_IMETHOD GetParent(nsIWebShell*& aParent);*/
    NS_IMETHOD GetReferrer(nsIURI **aReferrer);

    // Document load api's
    NS_IMETHOD GetDocumentLoader(nsIDocumentLoader*& aResult);

    void SetReferrer(const PRUnichar* aReferrer);

    // History api's
    NS_IMETHOD GoTo(PRInt32 aHistoryIndex);
    NS_IMETHOD GetHistoryLength(PRInt32& aResult);
    NS_IMETHOD GetHistoryIndex(PRInt32& aResult);
    NS_IMETHOD GetURL(PRInt32 aHistoryIndex, const PRUnichar** aURLResult);

    // nsIWebShellContainer
    NS_IMETHOD SetHistoryState(nsISupports* aLayoutHistoryState);

    // nsILinkHandler
    NS_IMETHOD OnLinkClick(nsIContent* aContent,
        nsLinkVerb aVerb,
        const PRUnichar* aURLSpec,
        const PRUnichar* aTargetSpec,
        nsIInputStream* aPostDataStream = 0,
        nsIInputStream* aHeadersDataStream = 0);
    NS_IMETHOD OnOverLink(nsIContent* aContent,
        const PRUnichar* aURLSpec,
        const PRUnichar* aTargetSpec);
    NS_IMETHOD GetLinkState(const char* aLinkURI, nsLinkState& aState);

    NS_IMETHOD FindNext(const PRUnichar * aSearchStr, PRBool aMatchCase,
        PRBool aSearchDown, PRBool &aIsFound);

    // nsIBaseWindow 
    NS_IMETHOD SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
        PRInt32 cy, PRBool fRepaint);
    NS_IMETHOD GetPositionAndSize(PRInt32* x, PRInt32* y, 
        PRInt32* cx, PRInt32* cy);
    NS_IMETHOD Create();
    NS_IMETHOD Destroy();

  // nsWebShell
    nsresult GetEventQueue(nsIEventQueue **aQueue);
    void HandleLinkClickEvent(nsIContent *aContent,
        nsLinkVerb aVerb,
        const PRUnichar* aURLSpec,
        const PRUnichar* aTargetSpec,
        nsIInputStream* aPostDataStream = 0,
        nsIInputStream* aHeadersDataStream = 0);

    static nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent);

    NS_IMETHOD SetURL(const PRUnichar* aURL);

protected:
    void GetRootWebShellEvenIfChrome(nsIWebShell** aResult);
    void InitFrameData();

    // helpers for executing commands
    virtual nsresult GetControllerForCommand ( const nsAReadableString & inCommand, nsIController** outController );
    virtual nsresult IsCommandEnabled ( const nsAReadableString & inCommand, PRBool* outEnabled );
    virtual nsresult DoCommand ( const nsAReadableString & inCommand );

    //
    // Helper method that is called when a new document (including any
    // sub-documents - ie. frames) has been completely loaded.
    //
    virtual nsresult EndPageLoad(nsIWebProgress *aProgress,
        nsIChannel* channel,
        nsresult aStatus);

    PRThread *mThread;

    nsIWebShellContainer* mContainer;
    nsIDocumentLoader* mDocLoader;

    nsRect   mBounds;

    eCharsetReloadState mCharsetReloadState;

    nsISupports* mHistoryState; // Weak reference.  Session history owns this.

    nsresult CreateViewer(nsIRequest* request,
        const char* aContentType,
        const char* aCommand,
        nsIStreamListener** aResult);

#ifdef DEBUG
private:
    // We're counting the number of |nsWebShells| to help find leaks
    static unsigned long gNumberOfWebShells;
#endif /* DEBUG */
};

#endif /* webshell____h */

