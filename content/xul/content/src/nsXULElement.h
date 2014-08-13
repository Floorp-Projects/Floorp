/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  The base XUL element class and associates.

*/

#ifndef nsXULElement_h__
#define nsXULElement_h__

#include "js/TracingAPI.h"
#include "mozilla/Attributes.h"
#include "nsIDOMEvent.h"
#include "nsIServiceManager.h"
#include "nsIAtom.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsIControllers.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFResource.h"
#include "nsIURI.h"
#include "nsIXULTemplateBuilder.h"
#include "nsIBoxObject.h"
#include "nsLayoutCID.h"
#include "nsAttrAndChildArray.h"
#include "nsGkAtoms.h"
#include "nsAutoPtr.h"
#include "nsStyledElement.h"
#include "nsIFrameLoader.h"
#include "nsFrameLoader.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/ElementInlines.h"

class nsIDocument;
class nsString;
class nsIDocShell;
class nsXULPrototypeDocument;

class nsIObjectInputStream;
class nsIObjectOutputStream;
class nsIOffThreadScriptReceiver;
class nsXULPrototypeNode;
typedef nsTArray<nsRefPtr<nsXULPrototypeNode> > nsPrototypeArray;

namespace mozilla {
class EventChainPreVisitor;
class EventListenerManager;
namespace css {
class StyleRule;
}
}

namespace JS {
class SourceBufferHolder;
}

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
        : mName(nsGkAtoms::id)  // XXX this is a hack, but names have to have a value
    {
        XUL_PROTOTYPE_ATTRIBUTE_METER(gNumAttributes);
        MOZ_COUNT_CTOR(nsXULPrototypeAttribute);
    }

    ~nsXULPrototypeAttribute();

    nsAttrName mName;
    nsAttrValue mValue;

#ifdef XUL_PROTOTYPE_ATTRIBUTE_METERING
    static uint32_t   gNumElements;
    static uint32_t   gNumAttributes;
    static uint32_t   gNumCacheTests;
    static uint32_t   gNumCacheHits;
    static uint32_t   gNumCacheSets;
    static uint32_t   gNumCacheFills;
#endif /* !XUL_PROTOTYPE_ATTRIBUTE_METERING */
};


/**

  A prototype content model element that holds the "primordial" values
  that have been parsed from the original XUL document.

 */

class nsXULPrototypeNode
{
public:
    enum Type { eType_Element, eType_Script, eType_Text, eType_PI };

    Type                     mType;

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsXULPrototypeDocument* aProtoDoc,
                               const nsTArray<nsRefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) = 0;
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsXULPrototypeDocument* aProtoDoc,
                                 nsIURI* aDocumentURI,
                                 const nsTArray<nsRefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) = 0;

#ifdef NS_BUILD_REFCNT_LOGGING
    virtual const char* ClassName() = 0;
    virtual uint32_t ClassSize() = 0;
#endif

    /**
     * The prototype document must call ReleaseSubtree when it is going
     * away.  This makes the parents through the tree stop owning their
     * children, whether or not the parent's reference count is zero.
     * Individual elements may still own individual prototypes, but
     * those prototypes no longer remember their children to allow them
     * to be constructed.
     */
    virtual void ReleaseSubtree() { }

    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(nsXULPrototypeNode)
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsXULPrototypeNode)

protected:
    nsXULPrototypeNode(Type aType)
        : mType(aType) {}
    virtual ~nsXULPrototypeNode() {}
};

class nsXULPrototypeElement : public nsXULPrototypeNode
{
public:
    nsXULPrototypeElement()
        : nsXULPrototypeNode(eType_Element),
          mNumAttributes(0),
          mHasIdAttribute(false),
          mHasClassAttribute(false),
          mHasStyleAttribute(false),
          mAttributes(nullptr)
    {
    }

    virtual ~nsXULPrototypeElement()
    {
        Unlink();
    }

#ifdef NS_BUILD_REFCNT_LOGGING
    virtual const char* ClassName() MOZ_OVERRIDE { return "nsXULPrototypeElement"; }
    virtual uint32_t ClassSize() MOZ_OVERRIDE { return sizeof(*this); }
#endif

    virtual void ReleaseSubtree() MOZ_OVERRIDE
    {
        for (int32_t i = mChildren.Length() - 1; i >= 0; i--) {
            if (mChildren[i].get())
                mChildren[i]->ReleaseSubtree();
        }
        mChildren.Clear();
        nsXULPrototypeNode::ReleaseSubtree();
    }

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsXULPrototypeDocument* aProtoDoc,
                               const nsTArray<nsRefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) MOZ_OVERRIDE;
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsXULPrototypeDocument* aProtoDoc,
                                 nsIURI* aDocumentURI,
                                 const nsTArray<nsRefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) MOZ_OVERRIDE;

    nsresult SetAttrAt(uint32_t aPos, const nsAString& aValue, nsIURI* aDocumentURI);

    void Unlink();

    // Trace all scripts held by this element and its children.
    void TraceAllScripts(JSTracer* aTrc);

    nsPrototypeArray         mChildren;

    nsRefPtr<mozilla::dom::NodeInfo> mNodeInfo;

    uint32_t                 mNumAttributes:29;
    uint32_t                 mHasIdAttribute:1;
    uint32_t                 mHasClassAttribute:1;
    uint32_t                 mHasStyleAttribute:1;
    nsXULPrototypeAttribute* mAttributes;         // [OWNER]
};

namespace mozilla {
namespace dom {
class XULDocument;
} // namespace dom
} // namespace mozilla

class nsXULPrototypeScript : public nsXULPrototypeNode
{
public:
    nsXULPrototypeScript(uint32_t aLineNo, uint32_t version);
    virtual ~nsXULPrototypeScript();

#ifdef NS_BUILD_REFCNT_LOGGING
    virtual const char* ClassName() MOZ_OVERRIDE { return "nsXULPrototypeScript"; }
    virtual uint32_t ClassSize() MOZ_OVERRIDE { return sizeof(*this); }
#endif

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsXULPrototypeDocument* aProtoDoc,
                               const nsTArray<nsRefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) MOZ_OVERRIDE;
    nsresult SerializeOutOfLine(nsIObjectOutputStream* aStream,
                                nsXULPrototypeDocument* aProtoDoc);
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsXULPrototypeDocument* aProtoDoc,
                                 nsIURI* aDocumentURI,
                                 const nsTArray<nsRefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) MOZ_OVERRIDE;
    nsresult DeserializeOutOfLine(nsIObjectInputStream* aInput,
                                  nsXULPrototypeDocument* aProtoDoc);

    nsresult Compile(JS::SourceBufferHolder& aSrcBuf,
                     nsIURI* aURI, uint32_t aLineNo,
                     nsIDocument* aDocument,
                     nsIOffThreadScriptReceiver *aOffThreadReceiver = nullptr);

    nsresult Compile(const char16_t* aText, int32_t aTextLength,
                     nsIURI* aURI, uint32_t aLineNo,
                     nsIDocument* aDocument,
                     nsIOffThreadScriptReceiver *aOffThreadReceiver = nullptr);

    void UnlinkJSObjects();

    void Set(JSScript* aObject);

    // It's safe to return a handle because we trace mScriptObject, no one ever
    // uses the handle (or the script object) past the point at which the
    // nsXULPrototypeScript dies, and we can't get memmoved so the
    // &mScriptObject pointer can't go stale.
    JS::Handle<JSScript*> GetScriptObject()
    {
        // Calling fromMarkedLocation() is safe because we trace mScriptObject in
        // TraceScriptObject() and because its value is never changed after it has
        // been set.
        return JS::Handle<JSScript*>::fromMarkedLocation(mScriptObject.address());
    }

    void TraceScriptObject(JSTracer* aTrc)
    {
        if (mScriptObject) {
            JS_CallScriptTracer(aTrc, &mScriptObject, "active window XUL prototype script");
        }
    }

    void Trace(const TraceCallbacks& aCallbacks, void* aClosure)
    {
        if (mScriptObject) {
            aCallbacks.Trace(&mScriptObject, "mScriptObject", aClosure);
        }
    }

    nsCOMPtr<nsIURI>         mSrcURI;
    uint32_t                 mLineNo;
    bool                     mSrcLoading;
    bool                     mOutOfLine;
    mozilla::dom::XULDocument* mSrcLoadWaiters;   // [OWNER] but not COMPtr
    uint32_t                 mLangVersion;
private:
    JS::Heap<JSScript*>      mScriptObject;
};

class nsXULPrototypeText : public nsXULPrototypeNode
{
public:
    nsXULPrototypeText()
        : nsXULPrototypeNode(eType_Text)
    {
    }

    virtual ~nsXULPrototypeText()
    {
    }

#ifdef NS_BUILD_REFCNT_LOGGING
    virtual const char* ClassName() MOZ_OVERRIDE { return "nsXULPrototypeText"; }
    virtual uint32_t ClassSize() MOZ_OVERRIDE { return sizeof(*this); }
#endif

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsXULPrototypeDocument* aProtoDoc,
                               const nsTArray<nsRefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) MOZ_OVERRIDE;
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsXULPrototypeDocument* aProtoDoc,
                                 nsIURI* aDocumentURI,
                                 const nsTArray<nsRefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) MOZ_OVERRIDE;

    nsString                 mValue;
};

class nsXULPrototypePI : public nsXULPrototypeNode
{
public:
    nsXULPrototypePI()
        : nsXULPrototypeNode(eType_PI)
    {
    }

    virtual ~nsXULPrototypePI()
    {
    }

#ifdef NS_BUILD_REFCNT_LOGGING
    virtual const char* ClassName() MOZ_OVERRIDE { return "nsXULPrototypePI"; }
    virtual uint32_t ClassSize() MOZ_OVERRIDE { return sizeof(*this); }
#endif

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsXULPrototypeDocument* aProtoDoc,
                               const nsTArray<nsRefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) MOZ_OVERRIDE;
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsXULPrototypeDocument* aProtoDoc,
                                 nsIURI* aDocumentURI,
                                 const nsTArray<nsRefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) MOZ_OVERRIDE;

    nsString                 mTarget;
    nsString                 mData;
};

////////////////////////////////////////////////////////////////////////

/**

  The XUL element.

 */

#define XUL_ELEMENT_FLAG_BIT(n_) NODE_FLAG_BIT(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + (n_))

// XUL element specific bits
enum {
  XUL_ELEMENT_TEMPLATE_GENERATED =        XUL_ELEMENT_FLAG_BIT(0),
  XUL_ELEMENT_HAS_CONTENTMENU_LISTENER =  XUL_ELEMENT_FLAG_BIT(1),
  XUL_ELEMENT_HAS_POPUP_LISTENER =        XUL_ELEMENT_FLAG_BIT(2)
};

ASSERT_NODE_FLAGS_SPACE(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + 3);

#undef XUL_ELEMENT_FLAG_BIT

class nsScriptEventHandlerOwnerTearoff;

class nsXULElement MOZ_FINAL : public nsStyledElement,
                               public nsIDOMXULElement
{
public:
    nsXULElement(already_AddRefed<mozilla::dom::NodeInfo> aNodeInfo);

    static nsresult
    Create(nsXULPrototypeElement* aPrototype, nsIDocument* aDocument,
           bool aIsScriptable, bool aIsRoot, mozilla::dom::Element** aResult);

    NS_IMPL_FROMCONTENT(nsXULElement, kNameSpaceID_XUL)

    // nsISupports
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsXULElement, nsStyledElement)

    // nsINode
    virtual nsresult PreHandleEvent(
                       mozilla::EventChainPreVisitor& aVisitor) MOZ_OVERRIDE;

    // nsIContent
    virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                bool aCompileEventHandlers) MOZ_OVERRIDE;
    virtual void UnbindFromTree(bool aDeep, bool aNullParent) MOZ_OVERRIDE;
    virtual void RemoveChildAt(uint32_t aIndex, bool aNotify) MOZ_OVERRIDE;
    virtual void DestroyContent() MOZ_OVERRIDE;

#ifdef DEBUG
    virtual void List(FILE* out, int32_t aIndent) const MOZ_OVERRIDE;
    virtual void DumpContent(FILE* out, int32_t aIndent,bool aDumpAll) const MOZ_OVERRIDE
    {
    }
#endif

    virtual void PerformAccesskey(bool aKeyCausesActivation,
                                  bool aIsTrustedEvent) MOZ_OVERRIDE;
    nsresult ClickWithInputSource(uint16_t aInputSource);

    virtual nsIContent *GetBindingParent() const MOZ_OVERRIDE;
    virtual bool IsNodeOfType(uint32_t aFlags) const MOZ_OVERRIDE;
    virtual bool IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse) MOZ_OVERRIDE;

    NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker) MOZ_OVERRIDE;
    virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                                int32_t aModType) const MOZ_OVERRIDE;
    NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;

    // XUL element methods
    /**
     * The template-generated flag is used to indicate that a
     * template-generated element has already had its children generated.
     */
    void SetTemplateGenerated() { SetFlags(XUL_ELEMENT_TEMPLATE_GENERATED); }
    void ClearTemplateGenerated() { UnsetFlags(XUL_ELEMENT_TEMPLATE_GENERATED); }
    bool GetTemplateGenerated() { return HasFlag(XUL_ELEMENT_TEMPLATE_GENERATED); }

    // nsIDOMNode
    NS_FORWARD_NSIDOMNODE_TO_NSINODE
    // And since that shadowed GetParentElement with the XPCOM
    // signature, pull in the one we care about.
    using nsStyledElement::GetParentElement;

    // nsIDOMElement
    NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

    // nsIDOMXULElement
    NS_DECL_NSIDOMXULELEMENT

    virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;
    virtual mozilla::EventStates IntrinsicState() const MOZ_OVERRIDE;

    nsresult GetFrameLoader(nsIFrameLoader** aFrameLoader);
    nsresult SwapFrameLoaders(nsIFrameLoaderOwner* aOtherOwner);

    virtual void RecompileScriptEventListeners() MOZ_OVERRIDE;

    // This function should ONLY be used by BindToTree implementations.
    // The function exists solely because XUL elements store the binding
    // parent as a member instead of in the slots, as Element does.
    void SetXULBindingParent(nsIContent* aBindingParent)
    {
      mBindingParent = aBindingParent;
    }

    virtual nsIDOMNode* AsDOMNode() MOZ_OVERRIDE { return this; }

    virtual bool IsEventAttributeName(nsIAtom* aName) MOZ_OVERRIDE;

    void SetXULAttr(nsIAtom* aName, const nsAString& aValue,
                    mozilla::ErrorResult& aError)
    {
        aError = SetAttr(kNameSpaceID_None, aName, aValue, true);
    }
    void SetXULBoolAttr(nsIAtom* aName, bool aValue)
    {
        if (aValue) {
            SetAttr(kNameSpaceID_None, aName, NS_LITERAL_STRING("true"), true);
        } else {
            UnsetAttr(kNameSpaceID_None, aName, true);
        }
    }

    // WebIDL API
    // The XPCOM getter is fine for our string attributes.
    // The XPCOM setter is fine for our bool attributes.
    void SetClassName(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::_class, aValue, rv);
    }
    void SetAlign(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::align, aValue, rv);
    }
    void SetDir(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::dir, aValue, rv);
    }
    void SetFlex(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::flex, aValue, rv);
    }
    void SetFlexGroup(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::flexgroup, aValue, rv);
    }
    void SetOrdinal(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::ordinal, aValue, rv);
    }
    void SetOrient(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::orient, aValue, rv);
    }
    void SetPack(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::pack, aValue, rv);
    }
    bool Hidden() const
    {
        return BoolAttrIsTrue(nsGkAtoms::hidden);
    }
    bool Collapsed() const
    {
        return BoolAttrIsTrue(nsGkAtoms::collapsed);
    }
    void SetObserves(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::observes, aValue, rv);
    }
    void SetMenu(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::menu, aValue, rv);
    }
    void SetContextMenu(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::contextmenu, aValue, rv);
    }
    void SetTooltip(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::tooltip, aValue, rv);
    }
    void SetWidth(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::width, aValue, rv);
    }
    void SetHeight(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::height, aValue, rv);
    }
    void SetMinWidth(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::minwidth, aValue, rv);
    }
    void SetMinHeight(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::minheight, aValue, rv);
    }
    void SetMaxWidth(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::maxwidth, aValue, rv);
    }
    void SetMaxHeight(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::maxheight, aValue, rv);
    }
    void SetPersist(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::persist, aValue, rv);
    }
    void SetLeft(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::left, aValue, rv);
    }
    void SetTop(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::top, aValue, rv);
    }
    void SetDatasources(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::datasources, aValue, rv);
    }
    void SetRef(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::ref, aValue, rv);
    }
    void SetTooltipText(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::tooltiptext, aValue, rv);
    }
    void SetStatusText(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::statustext, aValue, rv);
    }
    bool AllowEvents() const
    {
        return BoolAttrIsTrue(nsGkAtoms::allowevents);
    }
    already_AddRefed<nsIRDFCompositeDataSource> GetDatabase();
    already_AddRefed<nsIXULTemplateBuilder> GetBuilder();
    already_AddRefed<nsIRDFResource> GetResource(mozilla::ErrorResult& rv);
    nsIControllers* GetControllers(mozilla::ErrorResult& rv);
    already_AddRefed<nsIBoxObject> GetBoxObject(mozilla::ErrorResult& rv);
    void Focus(mozilla::ErrorResult& rv);
    void Blur(mozilla::ErrorResult& rv);
    void Click(mozilla::ErrorResult& rv);
    // The XPCOM DoCommand never fails, so it's OK for us.
    already_AddRefed<nsINodeList>
      GetElementsByAttribute(const nsAString& aAttribute,
                             const nsAString& aValue);
    already_AddRefed<nsINodeList>
      GetElementsByAttributeNS(const nsAString& aNamespaceURI,
                               const nsAString& aAttribute,
                               const nsAString& aValue,
                               mozilla::ErrorResult& rv);
    // Style() inherited from nsStyledElement
    already_AddRefed<nsFrameLoader> GetFrameLoader();
    void SwapFrameLoaders(nsXULElement& aOtherOwner, mozilla::ErrorResult& rv);

    // For XUL, the parent is the parent element, if any
    mozilla::dom::ParentObject GetParentObject() const
    {
        Element* parent = GetParentElement();
        if (parent) {
          return GetParentObjectInternal(parent);
        }
        return nsStyledElement::GetParentObject();
    }

protected:
    ~nsXULElement();

    // This can be removed if EnsureContentsGenerated dies.
    friend class nsNSElementTearoff;

    // Implementation methods
    nsresult EnsureContentsGenerated(void) const;

    nsresult ExecuteOnBroadcastHandler(nsIDOMElement* anElement, const nsAString& attrName);

    static nsresult
    ExecuteJSCode(nsIDOMElement* anElement, mozilla::WidgetEvent* aEvent);

    // Helper routine that crawls a parent chain looking for a tree element.
    NS_IMETHOD GetParentTree(nsIDOMXULMultiSelectControlElement** aTreeElement);

    nsresult AddPopupListener(nsIAtom* aName);

    class nsXULSlots : public mozilla::dom::Element::nsDOMSlots
    {
    public:
        nsXULSlots();
        virtual ~nsXULSlots();

        void Traverse(nsCycleCollectionTraversalCallback &cb);

        nsRefPtr<nsFrameLoader> mFrameLoader;
    };

    virtual nsINode::nsSlots* CreateSlots() MOZ_OVERRIDE;

    nsresult LoadSrc();

    /**
     * The nearest enclosing content node with a binding
     * that created us. [Weak]
     */
    nsIContent*                         mBindingParent;

    /**
     * Abandon our prototype linkage, and copy all attributes locally
     */
    nsresult MakeHeavyweight(nsXULPrototypeElement* aPrototype);

    virtual nsresult BeforeSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                   const nsAttrValueOrString* aValue,
                                   bool aNotify) MOZ_OVERRIDE;
    virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                  const nsAttrValue* aValue, bool aNotify) MOZ_OVERRIDE;

    virtual void UpdateEditableState(bool aNotify) MOZ_OVERRIDE;

    virtual bool ParseAttribute(int32_t aNamespaceID,
                                  nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult) MOZ_OVERRIDE;

    virtual mozilla::EventListenerManager*
      GetEventListenerManagerForAttr(nsIAtom* aAttrName,
                                     bool* aDefer) MOZ_OVERRIDE;
  
    /**
     * Add a listener for the specified attribute, if appropriate.
     */
    void AddListenerFor(const nsAttrName& aName,
                        bool aCompileEventHandlers);
    void MaybeAddPopupListener(nsIAtom* aLocalName);

    nsIWidget* GetWindowWidget();

    // attribute setters for widget
    nsresult HideWindowChrome(bool aShouldHide);
    void SetChromeMargins(const nsAttrValue* aValue);
    void ResetChromeMargins();
    void SetTitlebarColor(nscolor aColor, bool aActive);

    void SetDrawsInTitlebar(bool aState);
    void SetDrawsTitle(bool aState);

    void RemoveBroadcaster(const nsAString & broadcasterId);

protected:
    // Internal accessor. This shadows the 'Slots', and returns
    // appropriate value.
    nsIControllers *Controllers() {
      nsDOMSlots* slots = GetExistingDOMSlots();
      return slots ? slots->mControllers : nullptr; 
    }

    void UnregisterAccessKey(const nsAString& aOldValue);
    bool BoolAttrIsTrue(nsIAtom* aName) const;

    friend nsresult
    NS_NewXULElement(mozilla::dom::Element** aResult, mozilla::dom::NodeInfo *aNodeInfo);
    friend void
    NS_TrustedNewXULElement(nsIContent** aResult, mozilla::dom::NodeInfo *aNodeInfo);

    static already_AddRefed<nsXULElement>
    Create(nsXULPrototypeElement* aPrototype, mozilla::dom::NodeInfo *aNodeInfo,
           bool aIsScriptable, bool aIsRoot);

    bool IsReadWriteTextElement() const
    {
        const nsIAtom* tag = Tag();
        return
            GetNameSpaceID() == kNameSpaceID_XUL &&
            (tag == nsGkAtoms::textbox || tag == nsGkAtoms::textarea) &&
            !HasAttr(kNameSpaceID_None, nsGkAtoms::readonly);
    }

    virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

    void MaybeUpdatePrivateLifetime();
};

#endif // nsXULElement_h__
