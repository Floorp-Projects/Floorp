/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULContentSink_h__
#define nsXULContentSink_h__

#include "mozilla/Attributes.h"
#include "nsIExpatSink.h"
#include "nsIXMLContentSink.h"
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

class XULContentSinkImpl final : public nsIXMLContentSink,
                                 public nsIExpatSink
{
public:
    XULContentSinkImpl();

    // nsISupports
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIEXPATSINK

    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(XULContentSinkImpl, nsIXMLContentSink)

    // nsIContentSink
    NS_IMETHOD WillParse(void) override { return NS_OK; }
    NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode) override;
    NS_IMETHOD DidBuildModel(bool aTerminated) override;
    NS_IMETHOD WillInterrupt(void) override;
    NS_IMETHOD WillResume(void) override;
    NS_IMETHOD SetParser(nsParserBase* aParser) override;
    virtual void FlushPendingNotifications(mozFlushType aType) override { }
    NS_IMETHOD SetDocumentCharset(nsACString& aCharset) override;
    virtual nsISupports *GetTarget() override;

    /**
     * Initialize the content sink, giving it an nsIDocument object
     * with which to communicate with the outside world, and an
     * nsXULPrototypeDocument to build.
     */
    nsresult Init(nsIDocument* aDocument, nsXULPrototypeDocument* aPrototype);

protected:
    virtual ~XULContentSinkImpl();

    // pseudo-constants
    char16_t* mText;
    int32_t mTextLength;
    int32_t mTextSize;
    bool mConstrainSize;

    nsresult AddAttributes(const char16_t** aAttributes,
                           const uint32_t aAttrLen,
                           nsXULPrototypeElement* aElement);

    nsresult OpenRoot(const char16_t** aAttributes,
                      const uint32_t aAttrLen,
                      mozilla::dom::NodeInfo *aNodeInfo);

    nsresult OpenTag(const char16_t** aAttributes,
                     const uint32_t aAttrLen,
                     const uint32_t aLineNumber,
                     mozilla::dom::NodeInfo *aNodeInfo);

    // If OpenScript returns NS_OK and after it returns our state is eInScript,
    // that means that we created a prototype script and stuck it on
    // mContextStack.  If NS_OK is returned but the state is still
    // eInDocumentElement then we didn't create a prototype script (e.g. the
    // script had an unknown type), and the caller should create a prototype
    // element.
    nsresult OpenScript(const char16_t** aAttributes,
                        const uint32_t aLineNumber);

    static bool IsDataInBuffer(char16_t* aBuffer, int32_t aLength);

    // Text management
    nsresult FlushText(bool aCreateTextNode = true);
    nsresult AddText(const char16_t* aText, int32_t aLength);


    RefPtr<nsNodeInfoManager> mNodeInfoManager;

    nsresult NormalizeAttributeString(const char16_t *aExpatName,
                                      nsAttrName &aName);
    nsresult CreateElement(mozilla::dom::NodeInfo *aNodeInfo,
                           nsXULPrototypeElement** aResult);


    public:
    enum State { eInProlog, eInDocumentElement, eInScript, eInEpilog };
    protected:

    State mState;

    // content stack management
    class ContextStack {
    protected:
        struct Entry {
            RefPtr<nsXULPrototypeNode> mNode;
            // a LOT of nodes have children; preallocate for 8
            nsPrototypeArray    mChildren;
            State               mState;
            Entry*              mNext;
            Entry() : mChildren(8) {}
        };

        Entry* mTop;
        int32_t mDepth;

    public:
        ContextStack();
        ~ContextStack();

        int32_t Depth() { return mDepth; }

        nsresult Push(nsXULPrototypeNode* aNode, State aState);
        nsresult Pop(State* aState);

        nsresult GetTopNode(RefPtr<nsXULPrototypeNode>& aNode);
        nsresult GetTopChildren(nsPrototypeArray** aChildren);

        void Clear();

        void Traverse(nsCycleCollectionTraversalCallback& aCallback);
    };

    friend class ContextStack;
    ContextStack mContextStack;

    nsWeakPtr              mDocument;             // [OWNER]
    nsCOMPtr<nsIURI>       mDocumentURL;          // [OWNER]

    RefPtr<nsXULPrototypeDocument> mPrototype;  // [OWNER]

    RefPtr<nsParserBase> mParser;
    nsCOMPtr<nsIScriptSecurityManager> mSecMan;
};

#endif /* nsXULContentSink_h__ */
