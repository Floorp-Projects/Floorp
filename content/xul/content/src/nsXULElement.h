/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Peter Annema <disttsc@bart.nl>
 *   Mike Shaver <shaver@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*

  The base XUL element class and associates.

*/

#ifndef nsXULElement_h__
#define nsXULElement_h__

// XXX because nsIEventListenerManager has broken includes
#include "nslayout.h" 
#include "nsIDOMEvent.h"
#include "nsIServiceManager.h"
#include "nsHTMLValue.h"
#include "nsIAtom.h"
#include "nsINodeInfo.h"
#include "nsIControllers.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULTreeElement.h"
#include "nsIEventListenerManager.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFResource.h"
#include "nsIScriptObjectOwner.h"
#include "nsIStyledContent.h"
#include "nsIBindingManager.h"
#include "nsIXBLBinding.h"
#include "nsIURI.h"
#include "nsIXULContent.h"
#include "nsIXULTemplateBuilder.h"
#include "nsIBoxObject.h"
#include "nsXULAttributes.h"
#include "nsIChromeEventHandler.h"
#include "nsXULAttributeValue.h"
#include "nsIXBLService.h"

#include "nsGenericElement.h" // for nsCheapVoidArray

class nsISizeOfHandler;

class nsIDocument;
class nsIRDFService;
class nsISupportsArray;
class nsIXULContentUtils;
class nsIXULPrototypeDocument;
class nsRDFDOMNodeList;
class nsString;
class nsXULAttributes;
class nsVoidArray;
class nsIWebShell;

class nsIObjectInputStream;
class nsIObjectOutputStream;

////////////////////////////////////////////////////////////////////////

#ifdef XUL_PROTOTYPE_ATTRIBUTE_METERING
#define XUL_PROTOTYPE_ATTRIBUTE_METER(counter) (nsXULPrototypeAttribute::counter++)
#else
#define XUL_PROTOTYPE_ATTRIBUTE_METER(counter) ((void) 0)
#endif


/**

  A prototype attribute for an nsXULPrototypeElement.

 */

class nsXULPrototypeAttribute
{
public:
    nsXULPrototypeAttribute()
        : mEventHandler(nsnull)
    {
        XUL_PROTOTYPE_ATTRIBUTE_METER(gNumAttributes);
    }

    ~nsXULPrototypeAttribute();

    nsCOMPtr<nsINodeInfo> mNodeInfo;
    nsXULAttributeValue   mValue;
    void*                 mEventHandler;

#ifdef XUL_PROTOTYPE_ATTRIBUTE_METERING
    /**
      If enough attributes, on average, are event handlers, it pays to keep
      mEventHandler here, instead of maintaining a separate mapping in each
      nsXULElement associating those mName values with their mEventHandlers.
      Assume we don't need to keep mNameSpaceID along with mName in such an
      event-handler-only name-to-function-pointer mapping.

      Let
        minAttrSize  = sizeof(mNodeInof) + sizeof(mValue)
        mappingSize  = sizeof(mNodeInfo) + sizeof(mEventHandler)
        elemOverhead = nElems * sizeof(MappingPtr)

      Then
        nAttrs * minAttrSize + nEventHandlers * mappingSize + elemOverhead
        > nAttrs * (minAttrSize + mappingSize - sizeof(mNodeInfo))
      which simplifies to
        nEventHandlers * mappingSize + elemOverhead
        > nAttrs * (mappingSize - sizeof(mNodeInfo))
      or
        nEventHandlers + (nElems * sizeof(MappingPtr)) / mappingSize
        > nAttrs * (1 - sizeof(mNodeInfo) / mappingSize)

      If nsCOMPtr and all other pointers are the same size, this reduces to
        nEventHandlers + nElems / 2 > nAttrs / 2

      To measure how many attributes are event handlers, compile XUL source
      with XUL_PROTOTYPE_ATTRIBUTE_METERING and watch the counters below.
      Plug into the above relation -- if true, it pays to put mEventHandler
      in nsXULPrototypeAttribute rather than to keep a separate mapping.

      Recent numbers after opening four browser windows:
        nElems 3537, nAttrs 2528, nEventHandlers 1042
      giving 1042 + 3537/2 > 2528/2 or 2810 > 1264.

      As it happens, mEventHandler also makes this struct power-of-2 sized,
      8 words on most architectures, which makes for strength-reduced array
      index-to-pointer calculations.
     */
    static PRUint32   gNumElements;
    static PRUint32   gNumAttributes;
    static PRUint32   gNumEventHandlers;
    static PRUint32   gNumCacheTests;
    static PRUint32   gNumCacheHits;
    static PRUint32   gNumCacheSets;
    static PRUint32   gNumCacheFills;
#endif /* !XUL_PROTOTYPE_ATTRIBUTE_METERING */
};


/**

  A prototype content model element that holds the "primordial" values
  that have been parsed from the original XUL document. A
  'lightweight' nsXULElement may delegate its representation to this
  structure, which is shared.

 */

class nsXULPrototypeNode
{
public:
    enum Type { eType_Element, eType_Script, eType_Text };

    Type                     mType;
    PRInt32                  mLineNo;

    virtual ~nsXULPrototypeNode() {}
    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsIScriptContext* aContext);
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsIScriptContext* aContext);

protected:
    nsXULPrototypeNode(Type aType, PRInt32 aLineNo)
        : mType(aType), mLineNo(aLineNo) {}
};

class nsXULPrototypeElement : public nsXULPrototypeNode
{
public:
    nsXULPrototypeElement(PRInt32 aLineNo)
        : nsXULPrototypeNode(eType_Element, aLineNo),
          mDocument(nsnull),
          mNumChildren(0),
          mChildren(nsnull),
          mNumAttributes(0),
          mAttributes(nsnull),
          mClassList(nsnull)
    {
        MOZ_COUNT_CTOR(nsXULPrototypeElement);
    }

    virtual ~nsXULPrototypeElement()
    {
        MOZ_COUNT_DTOR(nsXULPrototypeElement);

        delete[] mAttributes;
        delete mClassList;

        for (PRInt32 i = mNumChildren - 1; i >= 0; --i)
            delete mChildren[i];

        delete[] mChildren;
    }

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsIScriptContext* aContext);
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsIScriptContext* aContext);

    nsIXULPrototypeDocument* mDocument;           // [WEAK] because doc is refcounted
    PRInt32                  mNumChildren;
    nsXULPrototypeNode**     mChildren;           // [OWNER]

    nsCOMPtr<nsINameSpace>   mNameSpace;          // [OWNER]
    nsCOMPtr<nsINodeInfo>    mNodeInfo;           // [OWNER]

    PRInt32                  mNumAttributes;
    nsXULPrototypeAttribute* mAttributes;         // [OWNER]

    nsCOMPtr<nsIStyleRule>   mInlineStyleRule;    // [OWNER]
    nsClassList*             mClassList;

    nsresult GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsAWritableString& aValue);
};

struct JSRuntime;
struct JSObject;
class nsXULDocument;

class nsXULPrototypeScript : public nsXULPrototypeNode
{
public:
    nsXULPrototypeScript(PRInt32 aLineNo, const char *aVersion);
    virtual ~nsXULPrototypeScript();

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsIScriptContext* aContext);
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsIScriptContext* aContext);

    nsresult Compile(const PRUnichar* aText, PRInt32 aTextLength,
                     nsIURI* aURI, PRInt32 aLineNo,
                     nsIDocument* aDocument,
                     nsIXULPrototypeDocument* aPrototypeDocument);

    nsCOMPtr<nsIURI>         mSrcURI;
    PRBool                   mSrcLoading;
    nsXULDocument*           mSrcLoadWaiters;   // [OWNER] but not COMPtr
    JSObject*                mJSObject;
    const char*              mLangVersion;
};

class nsXULPrototypeText : public nsXULPrototypeNode
{
public:
    nsXULPrototypeText(PRInt32 aLineNo)
        : nsXULPrototypeNode(eType_Text, aLineNo)
    {
        MOZ_COUNT_CTOR(nsXULPrototypeText);
    }

    virtual ~nsXULPrototypeText()
    {
        MOZ_COUNT_DTOR(nsXULPrototypeText);
    }

    nsString                 mValue;
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

class nsXULElement : public nsIXULContent,
                     public nsIDOMXULElement,
                     public nsIDOMEventReceiver,
                     public nsIScriptEventHandlerOwner,
                     public nsIChromeEventHandler
{
public:
    static nsIXBLService* GetXBLService() {
        if (!gXBLService)
            CallGetService("@mozilla.org/xbl;1", &gXBLService);
        return gXBLService;
    }
    static void ReleaseGlobals() { NS_IF_RELEASE(gXBLService); }

protected:
    static nsrefcnt             gRefCnt;
    // pseudo-constants
    static nsIRDFService*       gRDFService;
    static nsINameSpaceManager* gNameSpaceManager;
    static PRInt32              kNameSpaceID_RDF;
    static PRInt32              kNameSpaceID_XUL;

public:
    static nsresult
    Create(nsXULPrototypeElement* aPrototype, nsIDocument* aDocument,
           PRBool aIsScriptable, nsIContent** aResult);

    static nsresult
    Create(nsINodeInfo* aNodeInfo, nsIContent** aResult);

    // nsISupports
    NS_DECL_ISUPPORTS
       
    // nsIContent (from nsIStyledContent)
    NS_IMETHOD GetDocument(nsIDocument*& aResult) const;
    NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers);
    NS_IMETHOD GetParent(nsIContent*& aResult) const;
    NS_IMETHOD SetParent(nsIContent* aParent);
    NS_IMETHOD CanContainChildren(PRBool& aResult) const;
    NS_IMETHOD ChildCount(PRInt32& aResult) const;
    NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;
    NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const;
    NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                             PRBool aDeepSetDocument);
    NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                              PRBool aNotify, PRBool aDeepSetDocument);
    NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify,
                             PRBool aDeepSetDocument);
    NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);
    NS_IMETHOD GetNameSpaceID(PRInt32& aNameSpeceID) const;
    NS_IMETHOD GetTag(nsIAtom*& aResult) const;
    NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const;
    NS_IMETHOD NormalizeAttrString(const nsAReadableString& aStr, nsINodeInfo*& aNodeInfo);
    NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, const nsAReadableString& aValue, PRBool aNotify);
    NS_IMETHOD SetAttr(nsINodeInfo *aNodeInfo, const nsAReadableString& aValue, PRBool aNotify);
    NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsAWritableString& aResult) const;
    NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsIAtom*& aPrefix, nsAWritableString& aResult) const;
    NS_IMETHOD UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, PRBool aNotify);
    NS_IMETHOD GetAttrNameAt(PRInt32 aIndex, PRInt32& aNameSpaceID, 
                             nsIAtom*& aName, nsIAtom*& aPrefix) const;
    NS_IMETHOD GetAttrCount(PRInt32& aResult) const;
#ifdef DEBUG
    NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
    NS_IMETHOD DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const { return NS_OK; }
    NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif
    NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                              nsEvent* aEvent,
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus* aEventStatus);

    NS_IMETHOD GetContentID(PRUint32* aID);
    NS_IMETHOD SetContentID(PRUint32 aID);

    NS_IMETHOD RangeAdd(nsIDOMRange& aRange);
    NS_IMETHOD RangeRemove(nsIDOMRange& aRange); 
    NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const;
    NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
    NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);

    NS_IMETHOD GetBindingParent(nsIContent** aContent);
    NS_IMETHOD SetBindingParent(nsIContent* aParent);
    NS_IMETHOD_(PRBool) IsContentOfType(PRUint32 aFlags);

    // nsIXMLContent
    NS_IMETHOD SetContainingNameSpace(nsINameSpace* aNameSpace);
    NS_IMETHOD GetContainingNameSpace(nsINameSpace*& aNameSpace) const;
    NS_IMETHOD MaybeTriggerAutoLink(nsIWebShell *aShell);
    NS_IMETHOD GetXMLBaseURI(nsIURI **aURI);

    // nsIStyledContent
    NS_IMETHOD GetID(nsIAtom*& aResult) const;
    NS_IMETHOD GetClasses(nsVoidArray& aArray) const;
    NS_IMETHOD HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const;

    NS_IMETHOD WalkContentStyleRules(nsIRuleWalker* aRuleWalker);
    NS_IMETHOD WalkInlineStyleRules(nsIRuleWalker* aRuleWalker);
    NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                        PRInt32& aHint) const;


    // nsIXULContent
    NS_IMETHOD PeekChildCount(PRInt32& aCount) const;
    NS_IMETHOD SetLazyState(PRInt32 aFlags);
    NS_IMETHOD ClearLazyState(PRInt32 aFlags);
    NS_IMETHOD GetLazyState(PRInt32 aFlag, PRBool& aValue);
    NS_IMETHOD AddScriptEventListener(nsIAtom* aName, const nsAReadableString& aValue);
    
    // nsIDOMNode (from nsIDOMElement)
    NS_DECL_NSIDOMNODE
  
    // nsIDOMElement
    NS_DECL_NSIDOMELEMENT

    // nsIDOMXULElement
    NS_DECL_NSIDOMXULELEMENT

    // nsIDOMEventTarget interface (from nsIDOMEventReceiver)
    NS_IMETHOD AddEventListener(const nsAReadableString& aType, nsIDOMEventListener* aListener, 
                                PRBool aUseCapture);
    NS_IMETHOD RemoveEventListener(const nsAReadableString& aType, nsIDOMEventListener* aListener, 
                                   PRBool aUseCapture);
    NS_IMETHOD DispatchEvent(nsIDOMEvent* aEvent, PRBool* _retval);

    // nsIDOMEventReceiver
    NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
    NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);

    // nsIScriptEventHandlerOwner
    NS_IMETHOD CompileEventHandler(nsIScriptContext* aContext,
                                   void* aTarget,
                                   nsIAtom *aName,
                                   const nsAReadableString& aBody,
                                   void** aHandler);
    NS_IMETHOD GetCompiledEventHandler(nsIAtom *aName, void** aHandler);

#ifdef DEBUG
    virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);
#endif
    
    // nsIChromeEventHandler
    NS_DECL_NSICHROMEEVENTHANDLER


protected:
    nsXULElement();
    nsresult Init();
    virtual ~nsXULElement(void);


    // Implementation methods
    nsresult EnsureContentsGenerated(void) const;

    nsresult ExecuteOnBroadcastHandler(nsIDOMElement* anElement, const nsAReadableString& attrName);

    static nsresult
    ExecuteJSCode(nsIDOMElement* anElement, nsEvent* aEvent);

    // Static helpers
    static nsresult
    GetElementsByTagName(nsIDOMNode* aNode,
                         const nsAReadableString& aTagName,
                         nsRDFDOMNodeList* aElements);

    static nsresult
    GetElementsByAttribute(nsIDOMNode* aNode,
                           const nsAReadableString& aAttributeName,
                           const nsAReadableString& aAttributeValue,
                           nsRDFDOMNodeList* aElements);

    static PRBool IsAncestor(nsIDOMNode* aParentNode, nsIDOMNode* aChildNode);

    // Helper routine that crawls a parent chain looking for a tree element.
    NS_IMETHOD GetParentTree(nsIDOMXULTreeElement** aTreeElement);

    nsresult AddPopupListener(nsIAtom* aName);

protected:
    // Required fields
    nsXULPrototypeElement*              mPrototype;
    nsIDocument*                        mDocument;           // [WEAK]
    nsIContent*                         mParent;             // [WEAK]
    nsCheapVoidArray                    mChildren;           // [OWNER]
    nsCOMPtr<nsIEventListenerManager>   mListenerManager;    // [OWNER]

    /**
     * The state of our sloth for lazy content model construction via
     * RDF; see nsIXULContent and nsRDFGenericBuilder.
     */
    PRInt32                             mLazyState;

    /**
     * The nearest enclosing content node with a binding
     * that created us. [Weak]
     */
    nsIContent*                         mBindingParent;

    /**
     * Lazily instantiated if/when object is mutated. mAttributes are
     * lazily copied from the prototype when changed.
     */
    struct Slots {
        Slots(nsXULElement* mElement);
        ~Slots();

        nsXULElement*                       mElement;            // [WEAK]
        nsCOMPtr<nsINameSpace>              mNameSpace;          // [OWNER]
        nsCOMPtr<nsINodeInfo>               mNodeInfo;           // [OWNER]
        nsVoidArray*                        mBroadcastListeners; // [WEAK]
        nsCOMPtr<nsIControllers>            mControllers;        // [OWNER]

        /**
         * Attribute values that override the values from the prototype.
         */
        nsXULAttributes*                    mAttributes;

        /**
         * The state of our sloth for lazy content model construction via
         * RDF; see nsIXULContent and nsRDFGenericBuilder.
         */
        PRInt32                             mLazyState;

        /**
         * An unreferenced bare pointer to an aggregate that can
         * implement element-specific APIs.
         */
        nsXULAggregateElement*              mInnerXULElement;
    };

    friend struct Slots;
    Slots* mSlots;

    /**
     * Ensure that we've got an mSlots object, creating a Slots object
     * if necessary.
     */
    nsresult EnsureSlots();

    /**
     * Ensure that our mSlots has an mAttributes, creating an
     * nsXULAttributes object if necessary.
     */
    nsresult EnsureAttributes();

    /**
     * Abandon our prototype linkage, and copy all attributes locally
     */
    nsresult MakeHeavyweight();

    /**
     * Return our private copy of the attribute, if one exists.
     */
    nsXULAttribute *FindLocalAttribute(nsINodeInfo *info) const;

    /**
     * Return our private copy of the attribute, if one exists.
     */
    nsXULAttribute *FindLocalAttribute(PRInt32 aNameSpaceID,
                                       nsIAtom *aName,
                                       PRInt32 *aIndex = nsnull) const;

    /**
     * Return our prototype's attribute, if one exists.
     */
    nsXULPrototypeAttribute *FindPrototypeAttribute(nsINodeInfo *info) const;

    /**
     * Return our prototype's attribute, if one exists.
     */
    nsXULPrototypeAttribute *FindPrototypeAttribute(PRInt32 aNameSpaceID,
                                                    nsIAtom *aName) const;
    /**
     * Add a listener for the specified attribute, if appropriate.
     */
    nsresult AddListenerFor(nsINodeInfo *aNodeInfo,
                            PRBool aCompileEventHandlers);

protected:
    // Internal accessors. These shadow the 'Slots', and return
    // appropriate default values if there are no slots defined in the
    // delegate.
    nsINameSpace*              NameSpace() const       { return mSlots ? mSlots->mNameSpace.get()       : mPrototype->mNameSpace.get(); }
    nsINodeInfo*               NodeInfo() const        { return mSlots ? mSlots->mNodeInfo              : mPrototype->mNodeInfo; }
    nsVoidArray*               BroadcastListeners() const { return mSlots ? mSlots->mBroadcastListeners       : nsnull; }
    nsIControllers*            Controllers() const        { return mSlots ? mSlots->mControllers.get()        : nsnull; }
    nsXULAttributes*           Attributes() const         { return mSlots ? mSlots->mAttributes               : nsnull; }
    nsXULAggregateElement*     InnerXULElement() const    { return mSlots ? mSlots->mInnerXULElement          : nsnull; }

    static nsIXBLService *gXBLService;
};


#endif // nsXULElement_h__
