/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLPrototypeBinding_h__
#define nsXBLPrototypeBinding_h__

#include "nsClassHashtable.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsICSSLoaderObserver.h"
#include "nsInterfaceHashtable.h"
#include "nsWeakReference.h"
#include "nsXBLDocumentInfo.h"
#include "nsXBLProtoImpl.h"
#include "nsXBLProtoImplMethod.h"
#include "nsXBLPrototypeHandler.h"
#include "nsXBLPrototypeResources.h"

class nsIAtom;
class nsIContent;
class nsIDocument;
class nsXBLAttributeEntry;
class nsXBLBinding;
class nsXBLProtoImplField;

namespace mozilla {
class CSSStyleSheet;
} // namespace mozilla

// *********************************************************************/
// The XBLPrototypeBinding class

// Instances of this class are owned by the nsXBLDocumentInfo object returned
// by XBLDocumentInfo().  Consumers who want to refcount things should refcount
// that.
class nsXBLPrototypeBinding MOZ_FINAL
{
public:
  nsIContent* GetBindingElement() const { return mBinding; }
  void SetBindingElement(nsIContent* aElement);

  nsIURI* BindingURI() const { return mBindingURI; }
  nsIURI* AlternateBindingURI() const { return mAlternateBindingURI; }
  nsIURI* DocURI() const { return mXBLDocInfoWeak->DocumentURI(); }
  nsIURI* GetBaseBindingURI() const { return mBaseBindingURI; }

  // Checks if aURI refers to this binding by comparing to both possible
  // binding URIs.
  bool CompareBindingURI(nsIURI* aURI) const;

  bool GetAllowScripts() const;

  nsresult BindingAttached(nsIContent* aBoundElement);
  nsresult BindingDetached(nsIContent* aBoundElement);

  bool LoadResources();
  nsresult AddResource(nsIAtom* aResourceType, const nsAString& aSrc);

  bool InheritsStyle() const { return mInheritStyle; }
  void SetInheritsStyle(bool aInheritStyle) { mInheritStyle = aInheritStyle; }

  nsXBLPrototypeHandler* GetPrototypeHandlers() { return mPrototypeHandler; }
  void SetPrototypeHandlers(nsXBLPrototypeHandler* aHandler) { mPrototypeHandler = aHandler; }

  nsXBLProtoImplAnonymousMethod* GetConstructor();
  nsresult SetConstructor(nsXBLProtoImplAnonymousMethod* aConstructor);
  nsXBLProtoImplAnonymousMethod* GetDestructor();
  nsresult SetDestructor(nsXBLProtoImplAnonymousMethod* aDestructor);

  nsXBLProtoImplField* FindField(const nsString& aFieldName) const
  {
    return mImplementation ? mImplementation->FindField(aFieldName) : nullptr;
  }

  // Resolve all the fields for this binding on the object |obj|.
  // False return means a JS exception was set.
  bool ResolveAllFields(JSContext* cx, JS::Handle<JSObject*> obj) const
  {
    return !mImplementation || mImplementation->ResolveAllFields(cx, obj);
  }

  // Undefine all our fields from object |obj| (which should be a
  // JSObject for a bound element).
  void UndefineFields(JSContext* cx, JS::Handle<JSObject*> obj) const {
    if (mImplementation) {
      mImplementation->UndefineFields(cx, obj);
    }
  }

  const nsCString& ClassName() const {
    return mImplementation ? mImplementation->mClassName : EmptyCString();
  }

  nsresult InitClass(const nsCString& aClassName, JSContext * aContext,
                     JS::Handle<JSObject*> aScriptObject,
                     JS::MutableHandle<JSObject*> aClassObject,
                     bool* aNew);

  nsresult ConstructInterfaceTable(const nsAString& aImpls);

  void SetImplementation(nsXBLProtoImpl* aImpl) { mImplementation = aImpl; }
  nsXBLProtoImpl* GetImplementation() { return mImplementation; }
  nsresult InstallImplementation(nsXBLBinding* aBinding);
  bool HasImplementation() const { return mImplementation != nullptr; }

  void AttributeChanged(nsIAtom* aAttribute, int32_t aNameSpaceID,
                        bool aRemoveFlag, nsIContent* aChangedElement,
                        nsIContent* aAnonymousContent, bool aNotify);

  void SetBasePrototype(nsXBLPrototypeBinding* aBinding);
  nsXBLPrototypeBinding* GetBasePrototype() { return mBaseBinding; }

  nsXBLDocumentInfo* XBLDocumentInfo() const { return mXBLDocInfoWeak; }
  bool IsChrome() { return mXBLDocInfoWeak->IsChrome(); }

  void SetInitialAttributes(nsIContent* aBoundElement, nsIContent* aAnonymousContent);

  void AppendStyleSheet(mozilla::CSSStyleSheet* aSheet);
  void RemoveStyleSheet(mozilla::CSSStyleSheet* aSheet);
  void InsertStyleSheetAt(size_t aIndex, mozilla::CSSStyleSheet* aSheet);
  mozilla::CSSStyleSheet* StyleSheetAt(size_t aIndex) const;
  size_t SheetCount() const;
  bool HasStyleSheets() const;
  void AppendStyleSheetsTo(nsTArray<mozilla::CSSStyleSheet*>& aResult) const;

  nsIStyleRuleProcessor* GetRuleProcessor();

  nsresult FlushSkinSheets();

  nsIAtom* GetBaseTag(int32_t* aNamespaceID);
  void SetBaseTag(int32_t aNamespaceID, nsIAtom* aTag);

  bool ImplementsInterface(REFNSIID aIID) const;

  nsresult AddResourceListener(nsIContent* aBoundElement);

  void Initialize();

  nsresult ResolveBaseBinding();

  const nsCOMArray<nsXBLKeyEventHandler>* GetKeyEventHandlers()
  {
    if (!mKeyHandlersRegistered) {
      CreateKeyHandlers();
      mKeyHandlersRegistered = true;
    }

    return &mKeyHandlers;
  }

private:
  nsresult Read(nsIObjectInputStream* aStream,
                nsXBLDocumentInfo* aDocInfo,
                nsIDocument* aDocument,
                uint8_t aFlags);

  /**
   * Read a new binding from the stream aStream into the xbl document aDocument.
   * aDocInfo should be the xbl document info for the binding document.
   * aFlags can contain XBLBinding_Serialize_InheritStyle to indicate that
   * mInheritStyle flag should be set, and XBLBinding_Serialize_IsFirstBinding
   * to indicate the first binding in a document.
   * XBLBinding_Serialize_ChromeOnlyContent indicates that
   * nsXBLPrototypeBinding::mChromeOnlyContent should be true.
   */
public:
  static nsresult ReadNewBinding(nsIObjectInputStream* aStream,
                                 nsXBLDocumentInfo* aDocInfo,
                                 nsIDocument* aDocument,
                                 uint8_t aFlags);

  /**
   * Write this binding to the stream.
   */
  nsresult Write(nsIObjectOutputStream* aStream);

  /**
   * Read a content node from aStream and return it in aChild.
   * aDocument and aNim are the document and node info manager for the document
   * the child will be inserted into.
   */
  nsresult ReadContentNode(nsIObjectInputStream* aStream,
                           nsIDocument* aDocument,
                           nsNodeInfoManager* aNim,
                           nsIContent** aChild);

  /**
   * Write the content node aNode to aStream.
   *
   * This method is called recursively for each child descendant. For the topmost
   * call, aNode must be an element.
   *
   * Text, CDATA and comment nodes are serialized as:
   *   the constant XBLBinding_Serialize_TextNode, XBLBinding_Serialize_CDATANode
   *     or XBLBinding_Serialize_CommentNode
   *   the text for the node
   * Elements are serialized in the following format:
   *   node's namespace, written with WriteNamespace
   *   node's namespace prefix
   *   node's tag
   *   32-bit attribute count
   *   table of attributes:
   *     attribute's namespace, written with WriteNamespace
   *     attribute's namespace prefix
   *     attribute's tag
   *     attribute's value
   *   attribute forwarding table:
   *     source namespace
   *     source attribute
   *     destination namespace
   *     destination attribute
   *   the constant XBLBinding_Serialize_NoMoreAttributes
   *   32-bit count of the number of child nodes
   *     each child node is serialized in the same manner in sequence
   *   the constant XBLBinding_Serialize_NoContent
   */
  nsresult WriteContentNode(nsIObjectOutputStream* aStream, nsIContent* aNode);

  /**
   * Read or write a namespace id from or to aStream. If the namespace matches
   * one of the built-in ones defined in nsNameSpaceManager.h, it will be written as
   * a single byte with that value. Otherwise, XBLBinding_Serialize_CustomNamespace is
   * written out, followed by a string written with writeWStringZ.
   */
  nsresult ReadNamespace(nsIObjectInputStream* aStream, int32_t& aNameSpaceID);
  nsresult WriteNamespace(nsIObjectOutputStream* aStream, int32_t aNameSpaceID);

public:
  nsXBLPrototypeBinding();
  ~nsXBLPrototypeBinding();

  // Init must be called after construction to initialize the prototype
  // binding.  It may well throw errors (eg on out-of-memory).  Do not confuse
  // this with the Initialize() method, which must be called after the
  // binding's handlers, properties, etc are all set.
  nsresult Init(const nsACString& aRef,
                nsXBLDocumentInfo* aInfo,
                nsIContent* aElement,
                bool aFirstBinding = false);

  void Traverse(nsCycleCollectionTraversalCallback &cb) const;
  void Unlink();
  void Trace(const TraceCallbacks& aCallbacks, void *aClosure) const;

// Internal member functions.
public:
  /**
   * GetImmediateChild locates the immediate child of our binding element which
   * has the localname given by aTag and is in the XBL namespace.
   */
  nsIContent* GetImmediateChild(nsIAtom* aTag);
  nsIContent* LocateInstance(nsIContent* aBoundElt,
                             nsIContent* aTemplRoot,
                             nsIContent* aCopyRoot,
                             nsIContent* aTemplChild);

  bool ChromeOnlyContent() { return mChromeOnlyContent; }

  typedef nsClassHashtable<nsISupportsHashKey, nsXBLAttributeEntry> InnerAttributeTable;

protected:
  // Ensure that mAttributeTable has been created.
  void EnsureAttributeTable();
  // Ad an entry to the attribute table
  void AddToAttributeTable(int32_t aSourceNamespaceID, nsIAtom* aSourceTag,
                           int32_t aDestNamespaceID, nsIAtom* aDestTag,
                           nsIContent* aContent);
  void ConstructAttributeTable(nsIContent* aElement);
  void CreateKeyHandlers();

private:
  void EnsureResources();

// MEMBER VARIABLES
protected:
  nsCOMPtr<nsIURI> mBindingURI;
  nsCOMPtr<nsIURI> mAlternateBindingURI; // Alternate id-less URI that is only non-null on the first binding.
  nsCOMPtr<nsIContent> mBinding; // Strong. We own a ref to our content element in the binding doc.
  nsAutoPtr<nsXBLPrototypeHandler> mPrototypeHandler; // Strong. DocInfo owns us, and we own the handlers.

  // the url of the base binding
  nsCOMPtr<nsIURI> mBaseBindingURI;

  nsXBLProtoImpl* mImplementation; // Our prototype implementation (includes methods, properties, fields,
                                   // the constructor, and the destructor).

  nsXBLPrototypeBinding* mBaseBinding; // Weak.  The docinfo will own our base binding.
  bool mInheritStyle;
  bool mCheckedBaseProto;
  bool mKeyHandlersRegistered;
  bool mChromeOnlyContent;

  nsAutoPtr<nsXBLPrototypeResources> mResources; // If we have any resources, this will be non-null.

  nsXBLDocumentInfo* mXBLDocInfoWeak; // A pointer back to our doc info.  Weak, since it owns us.

  // A table for attribute containers. Namespace IDs are used as
  // keys in the table. Containers are InnerAttributeTables.
  // This table is used to efficiently handle attribute changes.
  nsAutoPtr<nsClassHashtable<nsUint32HashKey, InnerAttributeTable>> mAttributeTable;

  class IIDHashKey : public PLDHashEntryHdr
  {
  public:
    typedef const nsIID& KeyType;
    typedef const nsIID* KeyTypePointer;

    IIDHashKey(const nsIID* aKey)
      : mKey(*aKey)
    {}
    IIDHashKey(const IIDHashKey& aOther)
      : mKey(aOther.GetKey())
    {}
    ~IIDHashKey()
    {}

    KeyType GetKey() const
    {
      return mKey;
    }
    bool KeyEquals(const KeyTypePointer aKey) const
    {
      return mKey.Equals(*aKey);
    }

    static KeyTypePointer KeyToPointer(KeyType aKey)
    {
      return &aKey;
    }
    static PLDHashNumber HashKey(const KeyTypePointer aKey)
    {
      // Just use the 32-bit m0 field.
      return aKey->m0;
    }

    enum { ALLOW_MEMMOVE = true };

  private:
    nsIID mKey;
  };
  nsInterfaceHashtable<IIDHashKey, nsIContent> mInterfaceTable; // A table of cached interfaces that we support.

  int32_t mBaseNameSpaceID;    // If we extend a tagname/namespace, then that information will
  nsCOMPtr<nsIAtom> mBaseTag;  // be stored in here.

  nsCOMArray<nsXBLKeyEventHandler> mKeyHandlers;
};

#endif
