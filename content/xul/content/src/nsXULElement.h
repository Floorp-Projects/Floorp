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
 *   Peter Annema <disttsc@bart.nl>
 *   Mike Shaver <shaver@mozilla.org>
 *   Ben Goodger <ben@netscape.com>
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

/*

  The base XUL element class and associates.

*/

#ifndef nsXULElement_h__
#define nsXULElement_h__

// XXX because nsIEventListenerManager has broken includes
#include "nsIDOMEvent.h"
#include "nsIServiceManager.h"
#include "nsHTMLValue.h"
#include "nsIAtom.h"
#include "nsINodeInfo.h"
#include "nsIControllers.h"
#include "nsICSSParser.h"
#include "nsICSSStyleRule.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIEventListenerManager.h"
#include "nsINameSpace.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFResource.h"
#include "nsIScriptObjectOwner.h"
#include "nsIStyledContent.h"
#include "nsIBindingManager.h"
#include "nsIXBLBinding.h"
#include "nsIURI.h"
#include "nsIXULContent.h"
#include "nsIXULPrototypeCache.h"
#include "nsIXULTemplateBuilder.h"
#include "nsIBoxObject.h"
#include "nsIChromeEventHandler.h"
#include "nsIXBLService.h"
#include "nsICSSOMFactory.h"
#include "nsLayoutCID.h"
#include "nsAttrAndChildArray.h"
#include "nsXULAtoms.h"
#include "nsAutoPtr.h"
#include "nsGenericElement.h"

class nsIDocument;
class nsIRDFService;
class nsISupportsArray;
class nsIXULContentUtils;
class nsIXULPrototypeDocument;
class nsString;
class nsVoidArray;
class nsIDocShell;
class nsDOMAttributeMap;

class nsIObjectInputStream;
class nsIObjectOutputStream;

static NS_DEFINE_CID(kCSSParserCID, NS_CSSPARSER_CID);

////////////////////////////////////////////////////////////////////////

#ifdef XUL_PROTOTYPE_ATTRIBUTE_METERING
#define XUL_PROTOTYPE_ATTRIBUTE_METER(counter) (nsXULPrototypeAttribute::counter++)
#else
#define XUL_PROTOTYPE_ATTRIBUTE_METER(counter) ((void) 0)
#endif


/**

  A prototype attribute for an nsXULPrototypeElement.

 */

MOZ_DECL_CTOR_COUNTER(nsXULPrototypeAttribute)

class nsXULPrototypeAttribute
{
public:
    nsXULPrototypeAttribute()
        : mName(nsXULAtoms::id),  // XXX this is a hack, but names have to have a value
          mEventHandler(nsnull)
    {
        XUL_PROTOTYPE_ATTRIBUTE_METER(gNumAttributes);
        MOZ_COUNT_CTOR(nsXULPrototypeAttribute);
    }

    ~nsXULPrototypeAttribute();

    nsAttrName mName;
    nsAttrValue mValue;
    void* mEventHandler;

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

    PRInt32                  mRefCnt;

    virtual ~nsXULPrototypeNode() {}
    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsIScriptContext* aContext,
                               const nsCOMArray<nsINodeInfo> *aNodeInfos) = 0;
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsIScriptContext* aContext,
                                 nsIURI* aDocumentURI,
                                 const nsCOMArray<nsINodeInfo> *aNodeInfos) = 0;

#ifdef NS_BUILD_REFCNT_LOGGING
    virtual const char* ClassName() = 0;
    virtual PRUint32 ClassSize() = 0;
#endif

    void AddRef() {
        ++mRefCnt;
        NS_LOG_ADDREF(this, mRefCnt, ClassName(), ClassSize());
    }
    void Release()
    {
        --mRefCnt;
        NS_LOG_RELEASE(this, mRefCnt, ClassName());
        if (mRefCnt == 0)
            delete this;
    }
    virtual void ReleaseSubtree() { Release(); }

protected:
    nsXULPrototypeNode(Type aType)
        : mType(aType), mRefCnt(1) {}
};

class nsXULPrototypeElement : public nsXULPrototypeNode
{
public:
    nsXULPrototypeElement()
        : nsXULPrototypeNode(eType_Element),
          mNumChildren(0),
          mChildren(nsnull),
          mNumAttributes(0),
          mAttributes(nsnull)
    {
        NS_LOG_ADDREF(this, 1, ClassName(), ClassSize());
    }

    virtual ~nsXULPrototypeElement()
    {
        delete[] mAttributes;
        delete[] mChildren;
    }

#ifdef NS_BUILD_REFCNT_LOGGING
    virtual const char* ClassName() { return "nsXULPrototypeElement"; }
    virtual PRUint32 ClassSize() { return sizeof(*this); }
#endif

    virtual void ReleaseSubtree()
    {
      if (mChildren) {
        for (PRInt32 i = mNumChildren-1; i >= 0; i--) {
          if (! mChildren[i])
            break;
          mChildren[i]->ReleaseSubtree();
        }
      }

      nsXULPrototypeNode::ReleaseSubtree();
    }

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsIScriptContext* aContext,
                               const nsCOMArray<nsINodeInfo> *aNodeInfos);
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsIScriptContext* aContext,
                                 nsIURI* aDocumentURI,
                                 const nsCOMArray<nsINodeInfo> *aNodeInfos);

    nsresult SetAttrAt(PRUint32 aPos, const nsAString& aValue, nsIURI* aDocumentURI);

    PRUint32                 mNumChildren;
    nsXULPrototypeNode**     mChildren;           // [OWNER]

    nsCOMPtr<nsINodeInfo>    mNodeInfo;           // [OWNER]

    PRUint32                 mNumAttributes;
    nsXULPrototypeAttribute* mAttributes;         // [OWNER]

    static void ReleaseGlobals()
    {
        NS_IF_RELEASE(sCSSParser);
    }

protected:
    static nsICSSParser* GetCSSParser()
    {
        if (!sCSSParser) {
            CallCreateInstance(kCSSParserCID, &sCSSParser);
            if (sCSSParser) {
                sCSSParser->SetCaseSensitive(PR_TRUE);
                sCSSParser->SetQuirkMode(PR_FALSE);
            }
        }
        return sCSSParser;
    }
    static nsICSSParser* sCSSParser;
};

struct JSRuntime;
struct JSObject;
class nsXULDocument;

class nsXULPrototypeScript : public nsXULPrototypeNode
{
public:
    nsXULPrototypeScript(PRUint16 aLineNo, const char *aVersion);
    virtual ~nsXULPrototypeScript();

#ifdef NS_BUILD_REFCNT_LOGGING
    virtual const char* ClassName() { return "nsXULPrototypeScript"; }
    virtual PRUint32 ClassSize() { return sizeof(*this); }
#endif

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsIScriptContext* aContext,
                               const nsCOMArray<nsINodeInfo> *aNodeInfos);
    nsresult SerializeOutOfLine(nsIObjectOutputStream* aStream,
                                nsIScriptContext* aContext);
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsIScriptContext* aContext,
                                 nsIURI* aDocumentURI,
                                 const nsCOMArray<nsINodeInfo> *aNodeInfos);
    nsresult DeserializeOutOfLine(nsIObjectInputStream* aInput,
                                  nsIScriptContext* aContext);

    nsresult Compile(const PRUnichar* aText, PRInt32 aTextLength,
                     nsIURI* aURI, PRUint16 aLineNo,
                     nsIDocument* aDocument,
                     nsIXULPrototypeDocument* aPrototypeDocument);

    nsCOMPtr<nsIURI>         mSrcURI;
    PRUint16                 mLineNo;
    PRPackedBool             mSrcLoading;
    PRPackedBool             mOutOfLine;
    nsXULDocument*           mSrcLoadWaiters;   // [OWNER] but not COMPtr
    JSObject*                mJSObject;
    const char*              mLangVersion;

    static void ReleaseGlobals()
    {
        NS_IF_RELEASE(sXULPrototypeCache);
    }

protected:
    static nsIXULPrototypeCache* GetXULCache()
    {
        if (!sXULPrototypeCache)
            CallGetService("@mozilla.org/xul/xul-prototype-cache;1", &sXULPrototypeCache);

        return sXULPrototypeCache;
    }
    static nsIXULPrototypeCache* sXULPrototypeCache;
};

class nsXULPrototypeText : public nsXULPrototypeNode
{
public:
    nsXULPrototypeText()
        : nsXULPrototypeNode(eType_Text)
    {
        NS_LOG_ADDREF(this, 1, ClassName(), ClassSize());
    }

    virtual ~nsXULPrototypeText()
    {
    }

#ifdef NS_BUILD_REFCNT_LOGGING
    virtual const char* ClassName() { return "nsXULPrototypeText"; }
    virtual PRUint32 ClassSize() { return sizeof(*this); }
#endif

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsIScriptContext* aContext,
                               const nsCOMArray<nsINodeInfo> *aNodeInfos);
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsIScriptContext* aContext,
                                 nsIURI* aDocumentURI,
                                 const nsCOMArray<nsINodeInfo> *aNodeInfos);

    nsString                 mValue;
};

////////////////////////////////////////////////////////////////////////

/**

  The XUL element.

 */

class nsXULElement : public nsIXULContent,
                     public nsIDOMXULElement,
                     public nsIScriptEventHandlerOwner,
                     public nsIChromeEventHandler
{
public:
    static nsIXBLService* GetXBLService() {
        if (!gXBLService)
            CallGetService("@mozilla.org/xbl;1", &gXBLService);
        return gXBLService;
    }
    static void ReleaseGlobals() {
        NS_IF_RELEASE(gXBLService);
        NS_IF_RELEASE(gCSSOMFactory);
    }

protected:
    static nsrefcnt             gRefCnt;
    // pseudo-constants
    static nsIRDFService*       gRDFService;
    static nsIXBLService*       gXBLService;
    static nsICSSOMFactory*     gCSSOMFactory;

public:
    static nsresult
    Create(nsXULPrototypeElement* aPrototype, nsIDocument* aDocument,
           PRBool aIsScriptable, nsIContent** aResult);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIContent (from nsIStyledContent)
    virtual void SetDocument(nsIDocument* aDocument, PRBool aDeep,
                             PRBool aCompileEventHandlers);
    virtual void SetParent(nsIContent* aParent);
    virtual PRBool IsNativeAnonymous() const;
    virtual void SetNativeAnonymous(PRBool aAnonymous);
    virtual PRUint32 GetChildCount() const;
    virtual nsIContent *GetChildAt(PRUint32 aIndex) const;
    virtual PRInt32 IndexOf(nsIContent* aPossibleChild) const;
    virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                   PRBool aNotify, PRBool aDeepSetDocument);
    virtual nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                   PRBool aDeepSetDocument);
    virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify);
    virtual void GetNameSpaceID(PRInt32* aNameSpeceID) const;
    virtual nsIAtom *Tag() const;
    virtual nsINodeInfo *GetNodeInfo() const;
    virtual nsIAtom *GetIDAttributeName() const;
    virtual nsIAtom *GetClassAttributeName() const;
    virtual already_AddRefed<nsINodeInfo> GetExistingAttrNameFromQName(const nsAString& aStr) const;
    nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     const nsAString& aValue, PRBool aNotify)
    {
      return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
    }
    virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsIAtom* aPrefix,
                             const nsAString& aValue, PRBool aNotify);
    virtual nsresult GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                             nsAString& aResult) const;
    virtual PRBool HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const;
    virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                               PRBool aNotify);
    virtual nsresult GetAttrNameAt(PRUint32 aIndex, PRInt32* aNameSpaceID,
                                   nsIAtom** aName, nsIAtom** aPrefix) const;
    virtual PRUint32 GetAttrCount() const;
#ifdef DEBUG
    virtual void List(FILE* out, PRInt32 aIndent) const;
    virtual void DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const
    {
    }
#endif
    virtual nsresult HandleDOMEvent(nsIPresContext* aPresContext,
                              nsEvent* aEvent,
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus* aEventStatus);

    virtual PRUint32 ContentID() const;
    virtual void SetContentID(PRUint32 aID);

    virtual nsresult RangeAdd(nsIDOMRange* aRange);
    virtual void RangeRemove(nsIDOMRange* aRange);
    virtual const nsVoidArray *GetRangeList() const;
    virtual void SetFocus(nsIPresContext* aPresContext);
    virtual void RemoveFocus(nsIPresContext* aPresContext);

    virtual nsIContent *GetBindingParent() const;
    virtual nsresult SetBindingParent(nsIContent* aParent);
    virtual PRBool IsContentOfType(PRUint32 aFlags) const;
    virtual already_AddRefed<nsIURI> GetBaseURI() const;
    virtual nsresult GetListenerManager(nsIEventListenerManager** aResult);
    virtual PRBool IsFocusable(PRInt32 *aTabIndex = nsnull);

    // nsIXMLContent
    NS_IMETHOD MaybeTriggerAutoLink(nsIDocShell *aShell);

    // nsIStyledContent
    NS_IMETHOD GetID(nsIAtom** aResult) const;
    virtual const nsAttrValue* GetClasses() const;
    NS_IMETHOD_(PRBool) HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const;

    NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
    NS_IMETHOD GetInlineStyleRule(nsICSSStyleRule** aStyleRule);
    NS_IMETHOD SetInlineStyleRule(nsICSSStyleRule* aStyleRule, PRBool aNotify);
    NS_IMETHOD GetAttributeChangeHint(const nsIAtom* aAttribute,
                                      PRInt32 aModType,
                                      nsChangeHint& aHint) const;
    NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

    // nsIXULContent
    NS_IMETHOD_(PRUint32) PeekChildCount() const;
    NS_IMETHOD SetLazyState(LazyState aFlags);
    NS_IMETHOD ClearLazyState(LazyState aFlags);
    NS_IMETHOD GetLazyState(LazyState aFlag, PRBool& aValue);
    NS_IMETHOD AddScriptEventListener(nsIAtom* aName, const nsAString& aValue);

    // nsIDOMNode (from nsIDOMElement)
    NS_DECL_NSIDOMNODE

    // nsIDOMElement
    NS_DECL_NSIDOMELEMENT

    // nsIDOMXULElement
    NS_DECL_NSIDOMXULELEMENT

    // nsIScriptEventHandlerOwner
    nsresult CompileEventHandler(nsIScriptContext* aContext,
                                 void* aTarget,
                                 nsIAtom *aName,
                                 const nsAString& aBody,
                                 const char* aURL,
                                 PRUint32 aLineNo,
                                 void** aHandler);
    nsresult GetCompiledEventHandler(nsIAtom *aName, void** aHandler);

    // nsIChromeEventHandler
    NS_DECL_NSICHROMEEVENTHANDLER


protected:
    nsXULElement();
    nsresult Init();
    virtual ~nsXULElement(void);


    // Implementation methods
    nsresult EnsureContentsGenerated(void) const;

    nsresult ExecuteOnBroadcastHandler(nsIDOMElement* anElement, const nsAString& attrName);

    static nsresult
    ExecuteJSCode(nsIDOMElement* anElement, nsEvent* aEvent);

    static PRBool IsAncestor(nsIDOMNode* aParentNode, nsIDOMNode* aChildNode);

    // Helper routine that crawls a parent chain looking for a tree element.
    NS_IMETHOD GetParentTree(nsIDOMXULMultiSelectControlElement** aTreeElement);

    nsresult AddPopupListener(nsIAtom* aName);

    nsIContent* GetParent() const {
        // Override nsIContent::GetParent to be more efficient internally,
        // we don't use the low 2 bits of mParentPtrBits for anything.
 
        return NS_REINTERPRET_CAST(nsIContent *, mParentPtrBits);
    }

protected:
    // Required fields
    nsXULPrototypeElement*              mPrototype;
    nsAttrAndChildArray                 mAttrsAndChildren;   // [OWNER]
    nsCOMPtr<nsIEventListenerManager>   mListenerManager;    // [OWNER]

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
        Slots();
        ~Slots();

        nsCOMPtr<nsINodeInfo>               mNodeInfo;           // [OWNER]
        nsCOMPtr<nsIControllers>            mControllers;        // [OWNER]
        nsRefPtr<nsDOMCSSDeclaration>       mDOMStyle;           // [OWNER]
        nsRefPtr<nsDOMAttributeMap>         mAttributeMap;       // [OWNER]
        nsRefPtr<nsChildContentList>        mChildNodes;         // [OWNER]
        PRUint32                            mLazyState;
    };

    friend struct Slots;
    Slots* mSlots;

    /**
     * Ensure that we've got an mSlots object, creating a Slots object
     * if necessary.
     */
    nsresult EnsureSlots();

    /**
     * Abandon our prototype linkage, and copy all attributes locally
     */
    nsresult MakeHeavyweight();

    const nsAttrValue* FindLocalOrProtoAttr(PRInt32 aNameSpaceID,
                                            nsIAtom *aName) const;
  
    /**
     * Return our prototype's attribute, if one exists.
     */
    nsXULPrototypeAttribute *FindPrototypeAttribute(PRInt32 aNameSpaceID,
                                                    nsIAtom *aName) const;
    /**
     * Add a listener for the specified attribute, if appropriate.
     */
    void AddListenerFor(const nsAttrName& aName,
                        PRBool aCompileEventHandlers);
    void MaybeAddPopupListener(nsIAtom* aLocalName);


    nsresult HideWindowChrome(PRBool aShouldHide);

    
    nsresult SetAttrAndNotify(PRInt32 aNamespaceID,
                              nsIAtom* aAttribute,
                              nsIAtom* aPrefix,
                              const nsAString& aOldValue,
                              nsAttrValue& aParsedValue,
                              PRBool aModification,
                              PRBool aFireMutation,
                              PRBool aNotify);

    const nsAttrName* InternalGetExistingAttrNameFromQName(const nsAString& aStr) const;

protected:
    // Internal accessors. These shadow the 'Slots', and return
    // appropriate default values if there are no slots defined in the
    // delegate.
    nsINodeInfo     *NodeInfo() const    { return mSlots ? mSlots->mNodeInfo          : mPrototype->mNodeInfo; }
    nsIControllers  *Controllers() const { return mSlots ? mSlots->mControllers.get() : nsnull; }

    void UnregisterAccessKey(const nsAString& aOldValue);

    friend nsresult
    NS_NewXULElement(nsIContent** aResult, nsINodeInfo *aNodeInfo);
};


#endif // nsXULElement_h__
