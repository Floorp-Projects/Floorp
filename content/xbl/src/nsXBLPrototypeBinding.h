/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLPrototypeBinding_h__
#define nsXBLPrototypeBinding_h__

#include "nsCOMPtr.h"
#include "nsXBLPrototypeResources.h"
#include "nsXBLPrototypeHandler.h"
#include "nsXBLProtoImplMethod.h"
#include "nsICSSLoaderObserver.h"
#include "nsWeakReference.h"
#include "nsIContent.h"
#include "nsHashtable.h"
#include "nsClassHashtable.h"
#include "nsXBLDocumentInfo.h"
#include "nsCOMArray.h"
#include "nsXBLProtoImpl.h"

class nsIAtom;
class nsIDocument;
class nsIScriptContext;
class nsSupportsHashtable;
class nsIXBLService;
class nsFixedSizeAllocator;
class nsXBLProtoImplField;
class nsXBLBinding;
class nsCSSStyleSheet;

// This structure represents an insertion point, and is used when writing out
// insertion points. It contains comparison operators so that it can be stored
// in an array sorted by index.
struct InsertionItem {
  PRUint32 insertionIndex;
  nsIAtom* tag;
  nsIContent* defaultContent;

  InsertionItem(PRUint32 aInsertionIndex, nsIAtom* aTag, nsIContent* aDefaultContent)
    : insertionIndex(aInsertionIndex), tag(aTag), defaultContent(aDefaultContent) { }

  bool operator<(const InsertionItem& item) const
  {
    NS_ASSERTION(insertionIndex != item.insertionIndex || defaultContent == item.defaultContent,
                 "default content is different for same index");
    return insertionIndex < item.insertionIndex;
  }

  bool operator==(const InsertionItem& item) const
  {
    NS_ASSERTION(insertionIndex != item.insertionIndex || defaultContent == item.defaultContent,
                 "default content is different for same index");
    return insertionIndex == item.insertionIndex;
  }
};

typedef nsClassHashtable<nsISupportsHashKey, nsAutoTArray<InsertionItem, 1> > ArrayOfInsertionPointsByContent;

// *********************************************************************/
// The XBLPrototypeBinding class

// Instances of this class are owned by the nsXBLDocumentInfo object returned
// by XBLDocumentInfo().  Consumers who want to refcount things should refcount
// that.
class nsXBLPrototypeBinding
{
public:
  already_AddRefed<nsIContent> GetBindingElement();
  void SetBindingElement(nsIContent* aElement);

  nsIURI* BindingURI() const { return mBindingURI; }
  nsIURI* AlternateBindingURI() const { return mAlternateBindingURI; }
  nsIURI* DocURI() const { return mXBLDocInfoWeak->DocumentURI(); }
  nsIURI* GetBaseBindingURI() const { return mBaseBindingURI; }

  // Checks if aURI refers to this binding by comparing to both possible
  // binding URIs.
  bool CompareBindingURI(nsIURI* aURI) const;

  bool GetAllowScripts();

  nsresult BindingAttached(nsIContent* aBoundElement);
  nsresult BindingDetached(nsIContent* aBoundElement);

  bool LoadResources();
  nsresult AddResource(nsIAtom* aResourceType, const nsAString& aSrc);

  bool InheritsStyle() const { return mInheritStyle; }

  nsXBLPrototypeHandler* GetPrototypeHandlers() { return mPrototypeHandler; }
  void SetPrototypeHandlers(nsXBLPrototypeHandler* aHandler) { mPrototypeHandler = aHandler; }

  nsXBLProtoImplAnonymousMethod* GetConstructor();
  nsresult SetConstructor(nsXBLProtoImplAnonymousMethod* aConstructor);
  nsXBLProtoImplAnonymousMethod* GetDestructor();
  nsresult SetDestructor(nsXBLProtoImplAnonymousMethod* aDestructor);

  nsXBLProtoImplField* FindField(const nsString& aFieldName) const
  {
    return mImplementation ? mImplementation->FindField(aFieldName) : nsnull;
  }

  // Resolve all the fields for this binding on the object |obj|.
  // False return means a JS exception was set.
  bool ResolveAllFields(JSContext* cx, JSObject* obj) const
  {
    return !mImplementation || mImplementation->ResolveAllFields(cx, obj);
  }

  // Undefine all our fields from object |obj| (which should be a
  // JSObject for a bound element).
  void UndefineFields(JSContext* cx, JSObject* obj) const {
    if (mImplementation) {
      mImplementation->UndefineFields(cx, obj);
    }
  }

  const nsCString& ClassName() const {
    return mImplementation ? mImplementation->mClassName : EmptyCString();
  }

  nsresult InitClass(const nsCString& aClassName, JSContext * aContext,
                     JSObject * aGlobal, JSObject * aScriptObject,
                     JSObject** aClassObject);

  nsresult ConstructInterfaceTable(const nsAString& aImpls);
  
  void SetImplementation(nsXBLProtoImpl* aImpl) { mImplementation = aImpl; }
  nsresult InstallImplementation(nsIContent* aBoundElement);
  bool HasImplementation() const { return mImplementation != nsnull; }

  void AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID,
                        bool aRemoveFlag, nsIContent* aChangedElement,
                        nsIContent* aAnonymousContent, bool aNotify);

  void SetBasePrototype(nsXBLPrototypeBinding* aBinding);
  nsXBLPrototypeBinding* GetBasePrototype() { return mBaseBinding; }

  nsXBLDocumentInfo* XBLDocumentInfo() const { return mXBLDocInfoWeak; }
  bool IsChrome() { return mXBLDocInfoWeak->IsChrome(); }
  
  void SetInitialAttributes(nsIContent* aBoundElement, nsIContent* aAnonymousContent);

  nsIStyleRuleProcessor* GetRuleProcessor();
  nsXBLPrototypeResources::sheet_array_type* GetStyleSheets();

  bool HasInsertionPoints() { return mInsertionPointTable != nsnull; }
  
  bool HasStyleSheets() {
    return mResources && mResources->mStyleSheetList.Length() > 0;
  }

  nsresult FlushSkinSheets();

  void InstantiateInsertionPoints(nsXBLBinding* aBinding);

  // XXXbz this aIndex has nothing to do with an index into the child
  // list of the insertion parent or anything.
  nsIContent* GetInsertionPoint(nsIContent* aBoundElement,
                                nsIContent* aCopyRoot,
                                const nsIContent *aChild,
                                PRUint32* aIndex);

  nsIContent* GetSingleInsertionPoint(nsIContent* aBoundElement,
                                      nsIContent* aCopyRoot,
                                      PRUint32* aIndex, bool* aMultiple);

  nsIAtom* GetBaseTag(PRInt32* aNamespaceID);
  void SetBaseTag(PRInt32 aNamespaceID, nsIAtom* aTag);

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

  /**
   * Read this binding from the stream aStream into the xbl document aDocument.
   * aDocInfo should be the xbl document info for the binding document.
   * aFlags can contain XBLBinding_Serialize_InheritStyle to indicate that
   * mInheritStyle flag should be set, and XBLBinding_Serialize_IsFirstBinding
   * to indicate the first binding in a document.
   */
  nsresult Read(nsIObjectInputStream* aStream,
                nsXBLDocumentInfo* aDocInfo,
                nsIDocument* aDocument,
                PRUint8 aFlags);

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
   * aInsertionPointsByContent is a hash of the insertion points in the binding,
   * keyed by where there are in the content hierarchy.
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
   *   insertion points within this node:
   *     child index to insert within node
   *     default content serialized in the same manner or XBLBinding_Serialize_NoContent
   *     count of insertion points at that index
   *       that number of string tags (those in the <children>'s includes attribute)
   *   the constant XBLBinding_Serialize_NoMoreInsertionPoints
   *   32-bit count of the number of child nodes
   *     each child node is serialized in the same manner in sequence
   *   the constant XBLBinding_Serialize_NoContent
   */
  nsresult WriteContentNode(nsIObjectOutputStream* aStream,
                            nsIContent* aNode,
                            ArrayOfInsertionPointsByContent& aInsertionPointsByContent);

  /**
   * Read or write a namespace id from or to aStream. If the namespace matches
   * one of the built-in ones defined in nsINameSpaceManager.h, it will be written as
   * a single byte with that value. Otherwise, XBLBinding_Serialize_CustomNamespace is
   * written out, followed by a string written with writeWStringZ.
   */
  nsresult ReadNamespace(nsIObjectInputStream* aStream, PRInt32& aNameSpaceID);
  nsresult WriteNamespace(nsIObjectOutputStream* aStream, PRInt32 aNameSpaceID);

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
  void UnlinkJSObjects();
  void Trace(TraceCallback aCallback, void *aClosure) const;

// Static members
  static PRUint32 gRefCnt;
 
  static nsFixedSizeAllocator* kAttrPool;

// Internal member functions.
// XXXbz GetImmediateChild needs to be public to be called by SetAttrs,
// InstantiateInsertionPoints, etc; those should probably be a class static
// method instead of a global (non-static!) ones.
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

protected:
  // Ensure that mAttributeTable has been created.
  void EnsureAttributeTable();
  // Ad an entry to the attribute table
  void AddToAttributeTable(PRInt32 aSourceNamespaceID, nsIAtom* aSourceTag,
                           PRInt32 aDestNamespaceID, nsIAtom* aDestTag,
                           nsIContent* aContent);
  void ConstructAttributeTable(nsIContent* aElement);
  void ConstructInsertionTable(nsIContent* aElement);
  void GetNestedChildren(nsIAtom* aTag, PRInt32 aNamespace,
                         nsIContent* aContent,
                         nsCOMArray<nsIContent> & aList);
  void CreateKeyHandlers();

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
 
  nsXBLPrototypeResources* mResources; // If we have any resources, this will be non-null.
                                      
  nsXBLDocumentInfo* mXBLDocInfoWeak; // A pointer back to our doc info.  Weak, since it owns us.

  nsObjectHashtable* mAttributeTable; // A table for attribute containers. Namespace IDs are used as
                                      // keys in the table. Containers are nsObjectHashtables.
                                      // This table is used to efficiently handle attribute changes.

  nsObjectHashtable* mInsertionPointTable; // A table of insertion points for placing explicit content
                                           // underneath anonymous content.

  nsSupportsHashtable* mInterfaceTable; // A table of cached interfaces that we support.

  PRInt32 mBaseNameSpaceID;    // If we extend a tagname/namespace, then that information will
  nsCOMPtr<nsIAtom> mBaseTag;  // be stored in here.

  nsCOMArray<nsXBLKeyEventHandler> mKeyHandlers;
};

#endif
