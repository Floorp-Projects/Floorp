/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*

  The base XUL element class and associates.

*/

#ifndef nsXULElement_h__
#define nsXULElement_h__

// XXX because nsIEventListenerManager has broken includes
#include "nslayout.h" 
#include "nsIDOMEvent.h"

#include "nsForwardReference.h"
#include "nsHTMLValue.h"
#include "nsIAtom.h"
#include "nsIControllers.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULTreeElement.h"
#include "nsIEventListenerManager.h"
#include "nsIFocusableContent.h"
#include "nsIJSScriptObject.h"
#include "nsINameSpace.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFResource.h"
#include "nsIScriptObjectOwner.h"
#include "nsIStyleRule.h"
#include "nsIStyledContent.h"
#include "nsIXMLContent.h"
#include "nsIXULContent.h"

class nsClassList;
class nsIDocument;
class nsIRDFService;
class nsISupportsArray;
class nsIXULContentUtils;
class nsRDFDOMNodeList;
class nsString;
class nsVoidArray;
class nsXULAttributes;
class nsXULPrototypeDocument;

////////////////////////////////////////////////////////////////////////

/**

  A prototype attribute for an nsXULPrototypeElement.

 */

struct nsXULPrototypeAttribute
{
    PRInt32     mNameSpaceID;
    nsIAtom*    mName;
    PRUnichar*  mValue;
};


/**

  A prototype content model element that holds the "primordial" values
  that have been parsed from the original XUL document. A
  'lightweight' nsXULElement may delegate its representation to this
  structure, which is shared.

 */

struct nsXULPrototypeElement
{
    nsXULPrototypeDocument*  mDocument;           // [OWNER] because doc is refcounted
    PRInt32                  mNumChildren;
    nsXULPrototypeElement*   mChildren;           // [OWNER]

    nsINameSpace*            mNameSpace;          // [OWNER]
    nsIAtom*                 mNameSpacePrefix;    // [OWNER]
    PRInt32                  mNameSpaceID;
    nsIAtom*                 mTag;                // [OWNER]

    PRInt32                  mNumAttributes;
    nsXULPrototypeAttribute* mAttributes;         // [OWNER]

    nsIStyleRule*            mInlineStyleRule;    // [OWNER]
    nsClassList*             mClassList;
};


////////////////////////////////////////////////////////////////////////

/**

  This class serves as a base for aggregates that will implement a
  per-element XUL API.

 */

class nsXULAggregateElement : public nsISupports
{
protected:
    nsIDOMXULElement* mOuter;
    nsXULAggregateElement(nsIDOMXULElement* aOuter) : mOuter(aOuter) {}
    
public:
    virtual ~nsXULAggregateElement() {};

    // nsISupports interface. Subclasses should use the
    // NS_DECL/IMPL_ISUPPORTS_INHERITED macros to implement the
    // nsISupports interface.
    NS_IMETHOD_(nsrefcnt) AddRef() {
        return mOuter->AddRef();
    }

    NS_IMETHOD_(nsrefcnt) Release() {
        return mOuter->Release();
    }

    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aResult) {
        return mOuter->QueryInterface(aIID, aResult);
    }
};


////////////////////////////////////////////////////////////////////////

/**

  The XUL element.

 */

class nsXULElement : public nsIStyledContent,
                     public nsIXMLContent,
                     public nsIXULContent,
                     public nsIFocusableContent,
                     public nsIDOMXULElement,
                     public nsIDOMEventReceiver,
                     public nsIScriptObjectOwner,
                     public nsIJSScriptObject,
                     public nsIStyleRule
{
public:
protected:
    // pseudo-constants
    static nsrefcnt             gRefCnt;
    static nsIRDFService*       gRDFService;
    static nsINameSpaceManager* gNameSpaceManager;
    static nsIXULContentUtils*  gXULUtils;
    static PRInt32              kNameSpaceID_RDF;
    static PRInt32              kNameSpaceID_XUL;

    static nsIAtom*             kClassAtom;
    static nsIAtom*             kContextAtom;
    static nsIAtom*             kIdAtom;
    static nsIAtom*             kObservesAtom;
    static nsIAtom*             kPopupAtom;
    static nsIAtom*             kRefAtom;
    static nsIAtom*             kSelectedAtom;
    static nsIAtom*             kStyleAtom;
    static nsIAtom*             kTitledButtonAtom;
    static nsIAtom*             kTooltipAtom;
    static nsIAtom*             kTreeAtom;
    static nsIAtom*             kTreeCellAtom;
    static nsIAtom*             kTreeChildrenAtom;
    static nsIAtom*             kTreeColAtom;
    static nsIAtom*             kTreeItemAtom;
    static nsIAtom*             kTreeRowAtom;
    static nsIAtom*             kEditorAtom;

public:
    static nsresult
    Create(nsXULPrototypeElement* aPrototype, nsIContent** aResult);

    static nsresult
    Create(PRInt32 aNameSpaceID, nsIAtom* aTag, nsIContent** aResult);

    // nsISupports
    NS_DECL_ISUPPORTS
       
    // nsIContent (from nsIStyledContent)
    NS_IMETHOD GetDocument(nsIDocument*& aResult) const;
    NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep);
    NS_IMETHOD GetParent(nsIContent*& aResult) const;
    NS_IMETHOD SetParent(nsIContent* aParent);
    NS_IMETHOD CanContainChildren(PRBool& aResult) const;
    NS_IMETHOD ChildCount(PRInt32& aResult) const;
    NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;
    NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const;
    NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
    NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
    NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify);
    NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);
    NS_IMETHOD IsSynthetic(PRBool& aResult);
    NS_IMETHOD GetNameSpaceID(PRInt32& aNameSpeceID) const;
    NS_IMETHOD GetTag(nsIAtom*& aResult) const;
    NS_IMETHOD ParseAttributeString(const nsString& aStr, nsIAtom*& aName, PRInt32& aNameSpaceID);
    NS_IMETHOD GetNameSpacePrefixFromId(PRInt32 aNameSpaceID, nsIAtom*& aPrefix);
    NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, const nsString& aValue, PRBool aNotify);
    NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, nsString& aResult) const;
    NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, PRBool aNotify);
    NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex, PRInt32& aNameSpaceID, 
                                  nsIAtom*& aName) const;
    NS_IMETHOD GetAttributeCount(PRInt32& aResult) const;
    NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
    NS_IMETHOD BeginConvertToXIF(nsXIFConverter& aConverter) const;
    NS_IMETHOD ConvertContentToXIF(nsXIFConverter& aConverter) const;
    NS_IMETHOD FinishConvertToXIF(nsXIFConverter& aConverter) const;
    NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
    NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext,
                              nsEvent* aEvent,
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus& aEventStatus);

    NS_IMETHOD GetContentID(PRUint32* aID);
    NS_IMETHOD SetContentID(PRUint32 aID);

    NS_IMETHOD RangeAdd(nsIDOMRange& aRange);
    NS_IMETHOD RangeRemove(nsIDOMRange& aRange); 
    NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const;

    // nsIStyledContent
    NS_IMETHOD GetID(nsIAtom*& aResult) const;
    NS_IMETHOD GetClasses(nsVoidArray& aArray) const;
    NS_IMETHOD HasClass(nsIAtom* aClass) const;

    NS_IMETHOD GetContentStyleRules(nsISupportsArray* aRules);
    NS_IMETHOD GetInlineStyleRules(nsISupportsArray* aRules);
    NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                        PRInt32& aHint) const;

    // nsIXMLContent
    NS_IMETHOD SetContainingNameSpace(nsINameSpace* aNameSpace);
    NS_IMETHOD GetContainingNameSpace(nsINameSpace*& aNameSpace) const;
    NS_IMETHOD SetNameSpacePrefix(nsIAtom* aNameSpace);
    NS_IMETHOD GetNameSpacePrefix(nsIAtom*& aNameSpace) const;
    NS_IMETHOD SetNameSpaceID(PRInt32 aNameSpaceID);

    // nsIXULContent
    NS_IMETHOD PeekChildCount(PRInt32& aCount) const;
    NS_IMETHOD SetLazyState(PRInt32 aFlags);
    NS_IMETHOD ClearLazyState(PRInt32 aFlags);
    NS_IMETHOD GetLazyState(PRInt32 aFlag, PRBool& aValue);
    NS_IMETHOD ForceElementToOwnResource(PRBool aForce);

    // nsIFocusableContent interface
    NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
    NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);

    // nsIDOMNode (from nsIDOMElement)
    NS_DECL_IDOMNODE
  
    // nsIDOMElement
    NS_DECL_IDOMELEMENT

    // nsIDOMXULElement
    NS_DECL_IDOMXULELEMENT

    // nsIDOMEventTarget interface (from nsIDOMEventReceiver)
    NS_IMETHOD AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                PRBool aUseCapture);
    NS_IMETHOD RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                   PRBool aUseCapture);

    // nsIDOMEventReceiver
    NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
    NS_IMETHOD GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult);

    // nsIScriptObjectOwner
    NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
    NS_IMETHOD SetScriptObject(void *aScriptObject);

    // nsIJSScriptObject
    virtual PRBool AddProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool GetProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool SetProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool EnumerateProperty(JSContext *aContext);
    virtual PRBool Resolve(JSContext *aContext, jsval aID);
    virtual PRBool Convert(JSContext *aContext, jsval aID);
    virtual void   Finalize(JSContext *aContext);

    // nsIStyleRule interface. The node implements this to deal with attributes that
    // need to be mapped into style contexts (e.g., width in treecols).
    NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
    NS_IMETHOD GetStrength(PRInt32& aStrength) const;
    NS_IMETHOD MapFontStyleInto(nsIMutableStyleContext* aContext, nsIPresContext* aPresContext);
    NS_IMETHOD MapStyleInto(nsIMutableStyleContext* aContext, nsIPresContext* aPresContext);
    
protected:
    nsXULElement();
    nsresult Init();
    virtual ~nsXULElement(void);


    // Implementation methods
    nsresult EnsureContentsGenerated(void) const;

    nsresult AddScriptEventListener(nsIAtom* aName, const nsString& aValue, REFNSIID aIID);

    nsresult ExecuteOnBroadcastHandler(nsIDOMElement* anElement, const nsString& attrName);

    PRBool ElementIsInDocument();

    static nsresult
    ExecuteJSCode(nsIDOMElement* anElement, nsEvent* aEvent);

    // Used with treecol width attributes
    static PRBool ParseNumericValue(const nsString& aString,
                                  PRInt32& aIntValue,
                                  float& aFloatValue,
                                  nsHTMLUnit& aValueUnit);

    static nsresult
    GetElementsByTagName(nsIDOMNode* aNode,
                         const nsString& aTagName,
                         nsRDFDOMNodeList* aElements);

    static nsresult
    GetElementsByAttribute(nsIDOMNode* aNode,
                           const nsString& aAttributeName,
                           const nsString& aAttributeValue,
                           nsRDFDOMNodeList* aElements);

    static PRBool IsAncestor(nsIDOMNode* aParentNode, nsIDOMNode* aChildNode);

    // Helper routine that crawls a parent chain looking for a tree element.
    NS_IMETHOD GetParentTree(nsIDOMXULTreeElement** aTreeElement);

    PRBool IsFocusableContent();

protected:
    // Required fields
    nsXULPrototypeElement*              mPrototype;
    nsIDocument*                        mDocument;           // [WEAK]
    nsIContent*                         mParent;             // [WEAK]
    nsCOMPtr<nsISupportsArray>          mChildren;           // [OWNER]

    // The state of our sloth for lazy content model construction via
    // RDF; see nsIXULContent and nsRDFGenericBuilder.
    PRInt32                             mLazyState;

    // Lazily instantiated if/when object is mutated. Instantiating
    // the mSlots makes an nsXULElement 'heavyweight'.
    struct Slots {
        Slots(nsXULElement* mElement);
        ~Slots();

        nsXULElement*                       mElement;            // [WEAK]
        PRInt32                             mNameSpaceID;
        nsCOMPtr<nsINameSpace>              mNameSpace;          // [OWNER]
        nsCOMPtr<nsIAtom>                   mNameSpacePrefix;    // [OWNER]
        nsCOMPtr<nsIAtom>                   mTag;                // [OWNER]
        void*                               mScriptObject;       // [OWNER]
        nsCOMPtr<nsIEventListenerManager>   mListenerManager;    // [OWNER]
        nsVoidArray*                        mBroadcastListeners; // [WEAK]
        nsIDOMXULElement*                   mBroadcaster;        // [WEAK]
        nsCOMPtr<nsIControllers>            mControllers;        // [OWNER]
        nsCOMPtr<nsIRDFCompositeDataSource> mDatabase;           // [OWNER]
        nsCOMPtr<nsIRDFResource>            mOwnedResource;      // [OWNER]
        nsXULAttributes*                    mAttributes;

        // An unreferenced bare pointer to an aggregate that can
        // implement element-specific APIs.
        nsXULAggregateElement*              mInnerXULElement;
    };

    friend struct Slots;
    Slots* mSlots;
    nsresult EnsureSlots();


protected:
    // Internal accessors. These shadow the 'Slots', and return
    // appropriate default values if there are no slots defined in the
    // delegate.
    PRInt32                    NameSpaceID() const     { return mSlots ? mSlots->mNameSpaceID           : mPrototype->mNameSpaceID; }
    nsINameSpace*              NameSpace() const       { return mSlots ? mSlots->mNameSpace.get()       : mPrototype->mNameSpace; }
    nsIAtom*                   NameSpacePrefix() const { return mSlots ? mSlots->mNameSpacePrefix.get() : mPrototype->mNameSpacePrefix; }
    nsIAtom*                   Tag() const             { return mSlots ? mSlots->mTag.get()             : mPrototype->mTag; }
    void*                      ScriptObject() const       { return mSlots ? mSlots->mScriptObject             : nsnull; }
    nsIEventListenerManager*   ListenerManager() const    { return mSlots ? mSlots->mListenerManager.get()    : nsnull; }
    nsVoidArray*               BroadcastListeners() const { return mSlots ? mSlots->mBroadcastListeners       : nsnull; }
    nsIDOMXULElement*          Broadcaster() const        { return mSlots ? mSlots->mBroadcaster              : nsnull; }
    nsIControllers*            Controllers() const        { return mSlots ? mSlots->mControllers.get()        : nsnull; }
    nsIRDFCompositeDataSource* Database() const           { return mSlots ? mSlots->mDatabase.get()           : nsnull; }
    nsIRDFResource*            OwnedResource() const      { return mSlots ? mSlots->mOwnedResource.get()      : nsnull; }
    nsXULAttributes*           Attributes() const         { return mSlots ? mSlots->mAttributes               : nsnull; }
    nsXULAggregateElement*     InnerXULElement() const    { return mSlots ? mSlots->mInnerXULElement          : nsnull; }

protected:

    // XXX Move to nsXULContentSink?
    class ObserverForwardReference : public nsForwardReference
    {
    protected:
        nsCOMPtr<nsIDOMElement> mListener;
        nsString mTargetID;
        nsString mAttributes;

    public:
        ObserverForwardReference(nsIDOMElement* aListener,
                                 const nsString& aTargetID,
                                 const nsString& aAttributes) :
            mListener(aListener),
            mTargetID(aTargetID),
            mAttributes(aAttributes) {}

        virtual ~ObserverForwardReference() {}

        virtual Result Resolve();
    };

    friend class ObserverForwardReference;
};


#endif // nsXULElement_h__
