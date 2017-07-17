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
#include "nsLayoutCID.h"
#include "nsAttrAndChildArray.h"
#include "nsGkAtoms.h"
#include "nsStyledElement.h"
#include "nsIFrameLoader.h"
#include "nsFrameLoader.h" // Needed because we return an
                           // already_AddRefed<nsFrameLoader> where bindings
                           // want an already_AddRefed<nsIFrameLoader> and hence
                           // bindings need to know that the former can cast to
                           // the latter.
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/DOMString.h"

class nsIDocument;
class nsString;
class nsXULPrototypeDocument;

class nsIObjectInputStream;
class nsIObjectOutputStream;
class nsIOffThreadScriptReceiver;
class nsXULPrototypeNode;
typedef nsTArray<RefPtr<nsXULPrototypeNode> > nsPrototypeArray;

namespace mozilla {
class EventChainPreVisitor;
class EventListenerManager;
namespace css {
class StyleRule;
} // namespace css
namespace dom {
class BoxObject;
class HTMLIFrameElement;
enum class CallerType : uint32_t;
} // namespace dom
} // namespace mozilla

namespace JS {
class SourceBufferHolder;
} // namespace JS

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
                               const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) = 0;
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsXULPrototypeDocument* aProtoDoc,
                                 nsIURI* aDocumentURI,
                                 const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) = 0;

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
    explicit nsXULPrototypeNode(Type aType)
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

    virtual void ReleaseSubtree() override
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
                               const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) override;
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsXULPrototypeDocument* aProtoDoc,
                                 nsIURI* aDocumentURI,
                                 const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) override;

    nsresult SetAttrAt(uint32_t aPos, const nsAString& aValue, nsIURI* aDocumentURI);

    void Unlink();

    // Trace all scripts held by this element and its children.
    void TraceAllScripts(JSTracer* aTrc);

    nsPrototypeArray         mChildren;

    RefPtr<mozilla::dom::NodeInfo> mNodeInfo;

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

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsXULPrototypeDocument* aProtoDoc,
                               const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) override;
    nsresult SerializeOutOfLine(nsIObjectOutputStream* aStream,
                                nsXULPrototypeDocument* aProtoDoc);
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsXULPrototypeDocument* aProtoDoc,
                                 nsIURI* aDocumentURI,
                                 const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) override;
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

    bool HasScriptObject()
    {
        // Conversion to bool doesn't trigger mScriptObject's read barrier.
        return mScriptObject;
    }

    JSScript* GetScriptObject()
    {
        return mScriptObject;
    }

    void TraceScriptObject(JSTracer* aTrc)
    {
        JS::TraceEdge(aTrc, &mScriptObject, "active window XUL prototype script");
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

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsXULPrototypeDocument* aProtoDoc,
                               const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) override;
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsXULPrototypeDocument* aProtoDoc,
                                 nsIURI* aDocumentURI,
                                 const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) override;

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

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsXULPrototypeDocument* aProtoDoc,
                               const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) override;
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsXULPrototypeDocument* aProtoDoc,
                                 nsIURI* aDocumentURI,
                                 const nsTArray<RefPtr<mozilla::dom::NodeInfo>> *aNodeInfos) override;

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

class nsXULElement final : public nsStyledElement,
                           public nsIDOMXULElement
{
public:
    using Element::Blur;
    using Element::Focus;
    explicit nsXULElement(already_AddRefed<mozilla::dom::NodeInfo> aNodeInfo);

    static nsresult
    Create(nsXULPrototypeElement* aPrototype, nsIDocument* aDocument,
           bool aIsScriptable, bool aIsRoot, mozilla::dom::Element** aResult);

    NS_IMPL_FROMCONTENT(nsXULElement, kNameSpaceID_XUL)

    // nsISupports
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsXULElement, nsStyledElement)

    // nsINode
    virtual nsresult GetEventTargetParent(
                       mozilla::EventChainPreVisitor& aVisitor) override;
    virtual nsresult PreHandleEvent(
                       mozilla::EventChainVisitor& aVisitor) override;
    // nsIContent
    virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                bool aCompileEventHandlers) override;
    virtual void UnbindFromTree(bool aDeep, bool aNullParent) override;
    virtual void RemoveChildAt(uint32_t aIndex, bool aNotify) override;
    virtual void DestroyContent() override;

#ifdef DEBUG
    virtual void List(FILE* out, int32_t aIndent) const override;
    virtual void DumpContent(FILE* out, int32_t aIndent,bool aDumpAll) const override
    {
    }
#endif

    virtual bool PerformAccesskey(bool aKeyCausesActivation,
                                  bool aIsTrustedEvent) override;
    void ClickWithInputSource(uint16_t aInputSource, bool aIsTrustedEvent);

    virtual nsIContent *GetBindingParent() const override;
    virtual bool IsNodeOfType(uint32_t aFlags) const override;
    virtual bool IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse) override;

    NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker) override;
    virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                                int32_t aModType) const override;
    NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;

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

    virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                           bool aPreallocateChildren) const override;
    virtual mozilla::EventStates IntrinsicState() const override;

    nsresult GetFrameLoaderXPCOM(nsIFrameLoader** aFrameLoader);
    void PresetOpenerWindow(mozIDOMWindowProxy* aWindow, ErrorResult& aRv);
    nsresult SetIsPrerendered();

    virtual void RecompileScriptEventListeners() override;

    // This function should ONLY be used by BindToTree implementations.
    // The function exists solely because XUL elements store the binding
    // parent as a member instead of in the slots, as Element does.
    void SetXULBindingParent(nsIContent* aBindingParent)
    {
      mBindingParent = aBindingParent;
    }

    virtual nsIDOMNode* AsDOMNode() override { return this; }

    virtual bool IsEventAttributeNameInternal(nsIAtom* aName) override;

    typedef mozilla::dom::DOMString DOMString;
    void GetXULAttr(nsIAtom* aName, DOMString& aResult) const
    {
        GetAttr(kNameSpaceID_None, aName, aResult);
    }
    void SetXULAttr(nsIAtom* aName, const nsAString& aValue,
                    mozilla::ErrorResult& aError)
    {
        SetAttr(aName, aValue, aError);
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
    void GetAlign(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::align, aValue);
    }
    void SetAlign(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::align, aValue, rv);
    }
    void GetDir(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::dir, aValue);
    }
    void SetDir(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::dir, aValue, rv);
    }
    void GetFlex(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::flex, aValue);
    }
    void SetFlex(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::flex, aValue, rv);
    }
    void GetFlexGroup(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::flexgroup, aValue);
    }
    void SetFlexGroup(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::flexgroup, aValue, rv);
    }
    void GetOrdinal(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::ordinal, aValue);
    }
    void SetOrdinal(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::ordinal, aValue, rv);
    }
    void GetOrient(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::orient, aValue);
    }
    void SetOrient(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::orient, aValue, rv);
    }
    void GetPack(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::pack, aValue);
    }
    void SetPack(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::pack, aValue, rv);
    }
    bool Hidden() const
    {
        return BoolAttrIsTrue(nsGkAtoms::hidden);
    }
    void SetHidden(bool aHidden)
    {
        SetXULBoolAttr(nsGkAtoms::hidden, aHidden);
    }
    bool Collapsed() const
    {
        return BoolAttrIsTrue(nsGkAtoms::collapsed);
    }
    void SetCollapsed(bool aCollapsed)
    {
        SetXULBoolAttr(nsGkAtoms::collapsed, aCollapsed);
    }
    void GetObserves(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::observes, aValue);
    }
    void SetObserves(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::observes, aValue, rv);
    }
    void GetMenu(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::menu, aValue);
    }
    void SetMenu(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::menu, aValue, rv);
    }
    void GetContextMenu(DOMString& aValue)
    {
        GetXULAttr(nsGkAtoms::contextmenu, aValue);
    }
    void SetContextMenu(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::contextmenu, aValue, rv);
    }
    void GetTooltip(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::tooltip, aValue);
    }
    void SetTooltip(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::tooltip, aValue, rv);
    }
    void GetWidth(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::width, aValue);
    }
    void SetWidth(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::width, aValue, rv);
    }
    void GetHeight(DOMString& aValue)
    {
        GetXULAttr(nsGkAtoms::height, aValue);
    }
    void SetHeight(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::height, aValue, rv);
    }
    void GetMinWidth(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::minwidth, aValue);
    }
    void SetMinWidth(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::minwidth, aValue, rv);
    }
    void GetMinHeight(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::minheight, aValue);
    }
    void SetMinHeight(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::minheight, aValue, rv);
    }
    void GetMaxWidth(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::maxwidth, aValue);
    }
    void SetMaxWidth(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::maxwidth, aValue, rv);
    }
    void GetMaxHeight(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::maxheight, aValue);
    }
    void SetMaxHeight(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::maxheight, aValue, rv);
    }
    void GetPersist(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::persist, aValue);
    }
    void SetPersist(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::persist, aValue, rv);
    }
    void GetLeft(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::left, aValue);
    }
    void SetLeft(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::left, aValue, rv);
    }
    void GetTop(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::top, aValue);
    }
    void SetTop(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::top, aValue, rv);
    }
    void GetDatasources(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::datasources, aValue);
    }
    void SetDatasources(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::datasources, aValue, rv);
    }
    void GetRef(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::ref, aValue);
    }
    void SetRef(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::ref, aValue, rv);
    }
    void GetTooltipText(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::tooltiptext, aValue);
    }
    void SetTooltipText(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::tooltiptext, aValue, rv);
    }
    void GetStatusText(DOMString& aValue) const
    {
        GetXULAttr(nsGkAtoms::statustext, aValue);
    }
    void SetStatusText(const nsAString& aValue, mozilla::ErrorResult& rv)
    {
        SetXULAttr(nsGkAtoms::statustext, aValue, rv);
    }
    bool AllowEvents() const
    {
        return BoolAttrIsTrue(nsGkAtoms::allowevents);
    }
    void SetAllowEvents(bool aAllowEvents)
    {
        SetXULBoolAttr(nsGkAtoms::allowevents, aAllowEvents);
    }
    already_AddRefed<nsIRDFCompositeDataSource> GetDatabase();
    already_AddRefed<nsIXULTemplateBuilder> GetBuilder();
    already_AddRefed<nsIRDFResource> GetResource(mozilla::ErrorResult& rv);
    nsIControllers* GetControllers(mozilla::ErrorResult& rv);
    // Note: this can only fail if the do_CreateInstance for the boxobject
    // contact fails for some reason.
    already_AddRefed<mozilla::dom::BoxObject> GetBoxObject(mozilla::ErrorResult& rv);
    void Click(mozilla::dom::CallerType aCallerType);
    void DoCommand();
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
    void InternalSetFrameLoader(nsIFrameLoader* aNewFrameLoader);
    void SwapFrameLoaders(mozilla::dom::HTMLIFrameElement& aOtherLoaderOwner,
                          mozilla::ErrorResult& rv);
    void SwapFrameLoaders(nsXULElement& aOtherLoaderOwner,
                          mozilla::ErrorResult& rv);
    void SwapFrameLoaders(nsIFrameLoaderOwner* aOtherLoaderOwner,
                          mozilla::ErrorResult& rv);

    nsINode* GetScopeChainParent() const override
    {
        // For XUL, the parent is the parent element, if any
        Element* parent = GetParentElement();
        return parent ? parent : nsStyledElement::GetScopeChainParent();
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
                                   bool aNotify) override;
    virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                  const nsAttrValue* aValue,
                                  const nsAttrValue* aOldValue,
                                  bool aNotify) override;

    virtual void UpdateEditableState(bool aNotify) override;

    virtual bool ParseAttribute(int32_t aNamespaceID,
                                  nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult) override;

    virtual mozilla::EventListenerManager*
      GetEventListenerManagerForAttr(nsIAtom* aAttrName,
                                     bool* aDefer) override;

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
    void UpdateBrightTitlebarForeground(nsIDocument* aDocument);

    void RemoveBroadcaster(const nsAString & broadcasterId);

protected:
    // Internal accessor. This shadows the 'Slots', and returns
    // appropriate value.
    nsIControllers *Controllers() {
      nsExtendedDOMSlots* slots = GetExistingExtendedDOMSlots();
      return slots ? slots->mControllers.get() : nullptr;
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
        return IsAnyOfXULElements(nsGkAtoms::textbox, nsGkAtoms::textarea) &&
               !HasAttr(kNameSpaceID_None, nsGkAtoms::readonly);
    }

    virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

    void MaybeUpdatePrivateLifetime();

    bool IsEventStoppedFromAnonymousScrollbar(mozilla::EventMessage aMessage);

    nsresult DispatchXULCommand(const mozilla::EventChainVisitor& aVisitor,
                                nsAutoString& aCommand);
};

#endif // nsXULElement_h__
