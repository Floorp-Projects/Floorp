/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  The base XUL element class and associates.

*/

#ifndef nsXULElement_h__
#define nsXULElement_h__

#include <stdint.h>
#include <stdio.h>
#include "ErrorList.h"
#include "js/RootingAPI.h"
#include "js/SourceText.h"
#include "js/TracingAPI.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/DOMString.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FragmentOrElement.h"
#include "mozilla/dom/FromParser.h"
#include "mozilla/dom/NameSpaceConstants.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsAtom.h"
#include "nsAttrName.h"
#include "nsAttrValue.h"
#include "nsCOMPtr.h"
#include "nsCaseTreatment.h"
#include "nsChangeHint.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsISupports.h"
#include "nsLiteralString.h"
#include "nsString.h"
#include "nsStyledElement.h"
#include "nsTArray.h"
#include "nsTLiteralString.h"
#include "nscore.h"

class JSObject;
class JSScript;
class nsAttrValueOrString;
class nsIControllers;
class nsIObjectInputStream;
class nsIObjectOutputStream;
class nsIOffThreadScriptReceiver;
class nsIPrincipal;
class nsIURI;
class nsXULPrototypeDocument;
class nsXULPrototypeNode;
struct JSContext;

using nsPrototypeArray = nsTArray<RefPtr<nsXULPrototypeNode>>;

namespace JS {
class CompileOptions;
}

namespace mozilla {
class ErrorResult;
class EventChainPreVisitor;
class EventListenerManager;
namespace css {
class StyleRule;
}  // namespace css
namespace dom {
class Document;
class HTMLIFrameElement;
class PrototypeDocumentContentSink;
enum class CallerType : uint32_t;
}  // namespace dom
}  // namespace mozilla

////////////////////////////////////////////////////////////////////////

#ifdef XUL_PROTOTYPE_ATTRIBUTE_METERING
#  define XUL_PROTOTYPE_ATTRIBUTE_METER(counter) \
    (nsXULPrototypeAttribute::counter++)
#else
#  define XUL_PROTOTYPE_ATTRIBUTE_METER(counter) ((void)0)
#endif

/**

  A prototype attribute for an nsXULPrototypeElement.

 */

class nsXULPrototypeAttribute {
 public:
  nsXULPrototypeAttribute()
      : mName(nsGkAtoms::id)  // XXX this is a hack, but names have to have a
                              // value
  {
    XUL_PROTOTYPE_ATTRIBUTE_METER(gNumAttributes);
    MOZ_COUNT_CTOR(nsXULPrototypeAttribute);
  }

  ~nsXULPrototypeAttribute();

  nsAttrName mName;
  nsAttrValue mValue;

#ifdef XUL_PROTOTYPE_ATTRIBUTE_METERING
  static uint32_t gNumElements;
  static uint32_t gNumAttributes;
  static uint32_t gNumCacheTests;
  static uint32_t gNumCacheHits;
  static uint32_t gNumCacheSets;
  static uint32_t gNumCacheFills;
#endif /* !XUL_PROTOTYPE_ATTRIBUTE_METERING */
};

/**

  A prototype content model element that holds the "primordial" values
  that have been parsed from the original XUL document.

 */

class nsXULPrototypeNode {
 public:
  enum Type { eType_Element, eType_Script, eType_Text, eType_PI };

  Type mType;

  virtual nsresult Serialize(
      nsIObjectOutputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
      const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) = 0;
  virtual nsresult Deserialize(
      nsIObjectInputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
      nsIURI* aDocumentURI,
      const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) = 0;

  /**
   * The prototype document must call ReleaseSubtree when it is going
   * away.  This makes the parents through the tree stop owning their
   * children, whether or not the parent's reference count is zero.
   * Individual elements may still own individual prototypes, but
   * those prototypes no longer remember their children to allow them
   * to be constructed.
   */
  virtual void ReleaseSubtree() {}

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(nsXULPrototypeNode)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsXULPrototypeNode)

 protected:
  explicit nsXULPrototypeNode(Type aType) : mType(aType) {}
  virtual ~nsXULPrototypeNode() = default;
};

class nsXULPrototypeElement : public nsXULPrototypeNode {
 public:
  explicit nsXULPrototypeElement(mozilla::dom::NodeInfo* aNodeInfo = nullptr)
      : nsXULPrototypeNode(eType_Element),
        mNodeInfo(aNodeInfo),
        mHasIdAttribute(false),
        mHasClassAttribute(false),
        mHasStyleAttribute(false),
        mIsAtom(nullptr) {}

 private:
  virtual ~nsXULPrototypeElement() { Unlink(); }

 public:
  virtual void ReleaseSubtree() override {
    for (int32_t i = mChildren.Length() - 1; i >= 0; i--) {
      if (mChildren[i].get()) mChildren[i]->ReleaseSubtree();
    }
    mChildren.Clear();
    nsXULPrototypeNode::ReleaseSubtree();
  }

  virtual nsresult Serialize(
      nsIObjectOutputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
      const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) override;
  virtual nsresult Deserialize(
      nsIObjectInputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
      nsIURI* aDocumentURI,
      const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) override;

  nsresult SetAttrAt(uint32_t aPos, const nsAString& aValue,
                     nsIURI* aDocumentURI);

  void Unlink();

  // Trace all scripts held by this element and its children.
  void TraceAllScripts(JSTracer* aTrc);

  nsPrototypeArray mChildren;

  RefPtr<mozilla::dom::NodeInfo> mNodeInfo;

  uint32_t mHasIdAttribute : 1;
  uint32_t mHasClassAttribute : 1;
  uint32_t mHasStyleAttribute : 1;
  nsTArray<nsXULPrototypeAttribute> mAttributes;  // [OWNER]
  RefPtr<nsAtom> mIsAtom;
};

class nsXULPrototypeScript : public nsXULPrototypeNode {
 public:
  explicit nsXULPrototypeScript(uint32_t aLineNo);

 private:
  virtual ~nsXULPrototypeScript();

  void FillCompileOptions(JS::CompileOptions& options);

 public:
  virtual nsresult Serialize(
      nsIObjectOutputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
      const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) override;
  nsresult SerializeOutOfLine(nsIObjectOutputStream* aStream,
                              nsXULPrototypeDocument* aProtoDoc);
  virtual nsresult Deserialize(
      nsIObjectInputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
      nsIURI* aDocumentURI,
      const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) override;
  nsresult DeserializeOutOfLine(nsIObjectInputStream* aInput,
                                nsXULPrototypeDocument* aProtoDoc);

  nsresult Compile(const char16_t* aText, size_t aTextLength,
                   JS::SourceOwnership aOwnership, nsIURI* aURI,
                   uint32_t aLineNo, mozilla::dom::Document* aDocument,
                   nsIOffThreadScriptReceiver* aOffThreadReceiver = nullptr);

  void UnlinkJSObjects();

  void Set(JSScript* aObject);

  bool HasScriptObject() {
    // Conversion to bool doesn't trigger mScriptObject's read barrier.
    return mScriptObject;
  }

  JSScript* GetScriptObject() { return mScriptObject; }

  void TraceScriptObject(JSTracer* aTrc) {
    JS::TraceEdge(aTrc, &mScriptObject, "active window XUL prototype script");
  }

  void Trace(const TraceCallbacks& aCallbacks, void* aClosure) {
    if (mScriptObject) {
      aCallbacks.Trace(&mScriptObject, "mScriptObject", aClosure);
    }
  }

  nsCOMPtr<nsIURI> mSrcURI;
  uint32_t mLineNo;
  bool mSrcLoading;
  bool mOutOfLine;
  mozilla::dom::PrototypeDocumentContentSink*
      mSrcLoadWaiters;  // [OWNER] but not COMPtr
 private:
  JS::Heap<JSScript*> mScriptObject;
};

class nsXULPrototypeText : public nsXULPrototypeNode {
 public:
  nsXULPrototypeText() : nsXULPrototypeNode(eType_Text) {}

 private:
  virtual ~nsXULPrototypeText() = default;

 public:
  virtual nsresult Serialize(
      nsIObjectOutputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
      const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) override;
  virtual nsresult Deserialize(
      nsIObjectInputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
      nsIURI* aDocumentURI,
      const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) override;

  nsString mValue;
};

class nsXULPrototypePI : public nsXULPrototypeNode {
 public:
  nsXULPrototypePI() : nsXULPrototypeNode(eType_PI) {}

 private:
  virtual ~nsXULPrototypePI() = default;

 public:
  virtual nsresult Serialize(
      nsIObjectOutputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
      const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) override;
  virtual nsresult Deserialize(
      nsIObjectInputStream* aStream, nsXULPrototypeDocument* aProtoDoc,
      nsIURI* aDocumentURI,
      const nsTArray<RefPtr<mozilla::dom::NodeInfo>>* aNodeInfos) override;

  nsString mTarget;
  nsString mData;
};

////////////////////////////////////////////////////////////////////////

/**

  The XUL element.

 */

#define XUL_ELEMENT_FLAG_BIT(n_) \
  NODE_FLAG_BIT(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + (n_))

// XUL element specific bits
enum {
  XUL_ELEMENT_HAS_CONTENTMENU_LISTENER = XUL_ELEMENT_FLAG_BIT(0),
  XUL_ELEMENT_HAS_POPUP_LISTENER = XUL_ELEMENT_FLAG_BIT(1)
};

ASSERT_NODE_FLAGS_SPACE(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + 2);

#undef XUL_ELEMENT_FLAG_BIT

class nsXULElement : public nsStyledElement {
 protected:
  using Document = mozilla::dom::Document;

  // Use Construct to construct elements instead of this constructor.
  explicit nsXULElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

 public:
  using Element::Blur;
  using Element::Focus;

  static nsresult CreateFromPrototype(nsXULPrototypeElement* aPrototype,
                                      Document* aDocument, bool aIsScriptable,
                                      bool aIsRoot,
                                      mozilla::dom::Element** aResult);

  // This is the constructor for nsXULElements.
  static nsXULElement* Construct(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  NS_IMPL_FROMNODE(nsXULElement, kNameSpaceID_XUL)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsXULElement, nsStyledElement)

  // nsINode
  void GetEventTargetParent(mozilla::EventChainPreVisitor& aVisitor) override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual nsresult PreHandleEvent(
      mozilla::EventChainVisitor& aVisitor) override;
  // nsIContent
  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual void UnbindFromTree(bool aNullParent) override;
  virtual void DestroyContent() override;
  virtual void DoneAddingChildren(bool aHaveNotified) override;

#ifdef MOZ_DOM_LIST
  virtual void List(FILE* out, int32_t aIndent) const override;
  virtual void DumpContent(FILE* out, int32_t aIndent,
                           bool aDumpAll) const override {}
#endif

  MOZ_CAN_RUN_SCRIPT int32_t ScreenX();
  MOZ_CAN_RUN_SCRIPT int32_t ScreenY();

  MOZ_CAN_RUN_SCRIPT bool HasMenu();
  MOZ_CAN_RUN_SCRIPT void OpenMenu(bool aOpenFlag);

  MOZ_CAN_RUN_SCRIPT virtual bool PerformAccesskey(
      bool aKeyCausesActivation, bool aIsTrustedEvent) override;
  void ClickWithInputSource(uint16_t aInputSource, bool aIsTrustedEvent);

  virtual bool IsNodeOfType(uint32_t aFlags) const override;
  virtual bool IsFocusableInternal(int32_t* aTabIndex,
                                   bool aWithMouse) override;

  virtual nsChangeHint GetAttributeChangeHint(const nsAtom* aAttribute,
                                              int32_t aModType) const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo*,
                         nsINode** aResult) const override;

  virtual void RecompileScriptEventListeners() override;

  virtual bool IsEventAttributeNameInternal(nsAtom* aName) override;

  using DOMString = mozilla::dom::DOMString;
  void GetXULAttr(nsAtom* aName, DOMString& aResult) const {
    GetAttr(kNameSpaceID_None, aName, aResult);
  }
  void SetXULAttr(nsAtom* aName, const nsAString& aValue,
                  mozilla::ErrorResult& aError) {
    SetAttr(aName, aValue, aError);
  }
  bool GetXULBoolAttr(nsAtom* aName) const {
    return AttrValueIs(kNameSpaceID_None, aName, u"true"_ns, eCaseMatters);
  }
  void SetXULBoolAttr(nsAtom* aName, bool aValue) {
    if (aValue) {
      SetAttr(kNameSpaceID_None, aName, u"true"_ns, true);
    } else {
      UnsetAttr(kNameSpaceID_None, aName, true);
    }
  }

  // WebIDL API
  void GetFlex(DOMString& aValue) const { GetXULAttr(nsGkAtoms::flex, aValue); }
  void SetFlex(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetXULAttr(nsGkAtoms::flex, aValue, rv);
  }
  bool Hidden() const { return BoolAttrIsTrue(nsGkAtoms::hidden); }
  void SetHidden(bool aHidden) { SetXULBoolAttr(nsGkAtoms::hidden, aHidden); }
  bool Collapsed() const { return BoolAttrIsTrue(nsGkAtoms::collapsed); }
  void SetCollapsed(bool aCollapsed) {
    SetXULBoolAttr(nsGkAtoms::collapsed, aCollapsed);
  }
  void GetObserves(DOMString& aValue) const {
    GetXULAttr(nsGkAtoms::observes, aValue);
  }
  void SetObserves(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetXULAttr(nsGkAtoms::observes, aValue, rv);
  }
  void GetMenu(DOMString& aValue) const { GetXULAttr(nsGkAtoms::menu, aValue); }
  void SetMenu(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetXULAttr(nsGkAtoms::menu, aValue, rv);
  }
  void GetContextMenu(DOMString& aValue) {
    GetXULAttr(nsGkAtoms::contextmenu, aValue);
  }
  void SetContextMenu(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetXULAttr(nsGkAtoms::contextmenu, aValue, rv);
  }
  void GetTooltip(DOMString& aValue) const {
    GetXULAttr(nsGkAtoms::tooltip, aValue);
  }
  void SetTooltip(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetXULAttr(nsGkAtoms::tooltip, aValue, rv);
  }
  void GetWidth(DOMString& aValue) const {
    GetXULAttr(nsGkAtoms::width, aValue);
  }
  void SetWidth(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetXULAttr(nsGkAtoms::width, aValue, rv);
  }
  void GetHeight(DOMString& aValue) { GetXULAttr(nsGkAtoms::height, aValue); }
  void SetHeight(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetXULAttr(nsGkAtoms::height, aValue, rv);
  }
  void GetMinWidth(DOMString& aValue) const {
    GetXULAttr(nsGkAtoms::minwidth, aValue);
  }
  void SetMinWidth(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetXULAttr(nsGkAtoms::minwidth, aValue, rv);
  }
  void GetMinHeight(DOMString& aValue) const {
    GetXULAttr(nsGkAtoms::minheight, aValue);
  }
  void SetMinHeight(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetXULAttr(nsGkAtoms::minheight, aValue, rv);
  }
  void GetMaxWidth(DOMString& aValue) const {
    GetXULAttr(nsGkAtoms::maxwidth, aValue);
  }
  void SetMaxWidth(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetXULAttr(nsGkAtoms::maxwidth, aValue, rv);
  }
  void GetMaxHeight(DOMString& aValue) const {
    GetXULAttr(nsGkAtoms::maxheight, aValue);
  }
  void SetMaxHeight(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetXULAttr(nsGkAtoms::maxheight, aValue, rv);
  }
  void GetTooltipText(DOMString& aValue) const {
    GetXULAttr(nsGkAtoms::tooltiptext, aValue);
  }
  void SetTooltipText(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetXULAttr(nsGkAtoms::tooltiptext, aValue, rv);
  }
  void GetSrc(DOMString& aValue) const { GetXULAttr(nsGkAtoms::src, aValue); }
  void SetSrc(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetXULAttr(nsGkAtoms::src, aValue, rv);
  }
  nsIControllers* GetControllers(mozilla::ErrorResult& rv);
  void Click(mozilla::dom::CallerType aCallerType);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void DoCommand();
  // Style() inherited from nsStyledElement

  nsINode* GetScopeChainParent() const override {
    // For XUL, the parent is the parent element, if any
    Element* parent = GetParentElement();
    return parent ? parent : nsStyledElement::GetScopeChainParent();
  }

  bool IsInteractiveHTMLContent() const override;

 protected:
  ~nsXULElement();

  // This can be removed if EnsureContentsGenerated dies.
  friend class nsNSElementTearoff;

  // Implementation methods
  nsresult EnsureContentsGenerated(void) const;

  nsresult AddPopupListener(nsAtom* aName);

  /**
   * Abandon our prototype linkage, and copy all attributes locally
   */
  nsresult MakeHeavyweight(nsXULPrototypeElement* aPrototype);

  virtual nsresult BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) override;
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

  virtual void UpdateEditableState(bool aNotify) override;

  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;

  virtual mozilla::EventListenerManager* GetEventListenerManagerForAttr(
      nsAtom* aAttrName, bool* aDefer) override;

  /**
   * Add a listener for the specified attribute, if appropriate.
   */
  void AddListenerForAttributeIfNeeded(const nsAttrName& aName);
  void AddListenerForAttributeIfNeeded(nsAtom* aLocalName);

 protected:
  void AddTooltipSupport();
  void RemoveTooltipSupport();

  // Internal accessor. This shadows the 'Slots', and returns
  // appropriate value.
  nsIControllers* Controllers() {
    nsExtendedDOMSlots* slots = GetExistingExtendedDOMSlots();
    return slots ? slots->mControllers.get() : nullptr;
  }

  bool SupportsAccessKey() const;
  void RegUnRegAccessKey(bool aDoReg) override;
  bool BoolAttrIsTrue(nsAtom* aName) const;

  friend nsXULElement* NS_NewBasicXULElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  friend nsresult NS_NewXULElement(mozilla::dom::Element** aResult,
                                   mozilla::dom::NodeInfo* aNodeInfo,
                                   mozilla::dom::FromParser aFromParser,
                                   const nsAString* aIs);
  friend void NS_TrustedNewXULElement(mozilla::dom::Element** aResult,
                                      mozilla::dom::NodeInfo* aNodeInfo);

  static already_AddRefed<nsXULElement> CreateFromPrototype(
      nsXULPrototypeElement* aPrototype, mozilla::dom::NodeInfo* aNodeInfo,
      bool aIsScriptable, bool aIsRoot);

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

  bool IsEventStoppedFromAnonymousScrollbar(mozilla::EventMessage aMessage);

  MOZ_CAN_RUN_SCRIPT
  nsresult DispatchXULCommand(const mozilla::EventChainVisitor& aVisitor,
                              nsAutoString& aCommand);
};

#endif  // nsXULElement_h__
