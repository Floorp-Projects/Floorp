/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *   David Hyatt <hyatt@netscape.com>
 *   Brendan Eich <brendan@mozilla.org>
 *   Mark Hammond <mhammond@skippinet.com.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsXULContentSink_h__
#define nsXULContentSink_h__

#include "nsIExpatSink.h"
#include "nsIXMLContentSink.h"
#include "nsAutoPtr.h"
#include "nsNodeInfoManager.h"
#include "nsWeakPtr.h"
#include "nsXULElement.h"
#include "nsIDTD.h"

class nsIDocument;
class nsIScriptSecurityManager;
class nsAttrName;
class nsXULPrototypeDocument;
class nsXULPrototypeElement;
class nsXULPrototypeNode;

class XULContentSinkImpl : public nsIXMLContentSink,
                           public nsIExpatSink
{
public:
    XULContentSinkImpl();
    virtual ~XULContentSinkImpl();

    // nsISupports
    NS_DECL_ISUPPORTS
    NS_DECL_NSIEXPATSINK

    // nsIContentSink
    NS_IMETHOD WillParse(void) { return NS_OK; }
    NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode);
    NS_IMETHOD DidBuildModel(bool aTerminated);
    NS_IMETHOD WillInterrupt(void);
    NS_IMETHOD WillResume(void);
    NS_IMETHOD SetParser(nsParserBase* aParser);
    virtual void FlushPendingNotifications(mozFlushType aType) { }
    NS_IMETHOD SetDocumentCharset(nsACString& aCharset);
    virtual nsISupports *GetTarget();

    /**
     * Initialize the content sink, giving it an nsIDocument object
     * with which to communicate with the outside world, and an
     * nsXULPrototypeDocument to build.
     */
    nsresult Init(nsIDocument* aDocument, nsXULPrototypeDocument* aPrototype);

protected:
    // pseudo-constants
    PRUnichar* mText;
    PRInt32 mTextLength;
    PRInt32 mTextSize;
    bool mConstrainSize;

    nsresult AddAttributes(const PRUnichar** aAttributes,
                           const PRUint32 aAttrLen,
                           nsXULPrototypeElement* aElement);

    nsresult OpenRoot(const PRUnichar** aAttributes,
                      const PRUint32 aAttrLen,
                      nsINodeInfo *aNodeInfo);

    nsresult OpenTag(const PRUnichar** aAttributes,
                     const PRUint32 aAttrLen,
                     const PRUint32 aLineNumber,
                     nsINodeInfo *aNodeInfo);

    // If OpenScript returns NS_OK and after it returns our state is eInScript,
    // that means that we created a prototype script and stuck it on
    // mContextStack.  If NS_OK is returned but the state is still
    // eInDocumentElement then we didn't create a prototype script (e.g. the
    // script had an unknown type), and the caller should create a prototype
    // element.
    nsresult OpenScript(const PRUnichar** aAttributes,
                        const PRUint32 aLineNumber);

    static bool IsDataInBuffer(PRUnichar* aBuffer, PRInt32 aLength);

    // Text management
    nsresult FlushText(bool aCreateTextNode = true);
    nsresult AddText(const PRUnichar* aText, PRInt32 aLength);


    nsRefPtr<nsNodeInfoManager> mNodeInfoManager;

    nsresult NormalizeAttributeString(const PRUnichar *aExpatName,
                                      nsAttrName &aName);
    nsresult CreateElement(nsINodeInfo *aNodeInfo,
                           nsXULPrototypeElement** aResult);


    public:
    enum State { eInProlog, eInDocumentElement, eInScript, eInEpilog };
    protected:

    State mState;

    // content stack management
    class ContextStack {
    protected:
        struct Entry {
            nsRefPtr<nsXULPrototypeNode> mNode;
            // a LOT of nodes have children; preallocate for 8
            nsPrototypeArray    mChildren;
            State               mState;
            Entry*              mNext;
            Entry() : mChildren(8) {}
        };

        Entry* mTop;
        PRInt32 mDepth;

    public:
        ContextStack();
        ~ContextStack();

        PRInt32 Depth() { return mDepth; }

        nsresult Push(nsXULPrototypeNode* aNode, State aState);
        nsresult Pop(State* aState);

        nsresult GetTopNode(nsRefPtr<nsXULPrototypeNode>& aNode);
        nsresult GetTopChildren(nsPrototypeArray** aChildren);

        void Clear();
    };

    friend class ContextStack;
    ContextStack mContextStack;

    nsWeakPtr              mDocument;             // [OWNER]
    nsCOMPtr<nsIURI>       mDocumentURL;          // [OWNER]

    nsRefPtr<nsXULPrototypeDocument> mPrototype;  // [OWNER]

    // We use regular pointer b/c of funky exports on nsIParser:
    nsParserBase*         mParser;               // [OWNER]
    nsCOMPtr<nsIScriptSecurityManager> mSecMan;
};

#endif /* nsXULContentSink_h__ */
