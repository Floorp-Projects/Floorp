/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef dom_base_ipdl_bindings_IPDLProtocol_h
#define dom_base_ipdl_bindings_IPDLProtocol_h

#include "jsapi.h"
#include "nsDataHashtable.h"
#include "nsStringFwd.h"
#include "nsCycleCollectionParticipant.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {
class IPDL;
} // dom

namespace ipdl {
class IPDLProtocolInstance;

namespace ipc {
class IPCInterface;
} // ipc

namespace ffi {
struct AST;
struct Union;
struct Struct;
struct NamedProtocol;
struct MessageDecl;
struct TranslationUnit;
struct Param;
struct TypeSpec;
struct QualifiedId;
struct Namespace;
} // ffi

// Represents the side of the protocol.
// This is un-nested to be forward-declarable.
enum class IPDLSide : bool
{
  Parent,
  Child
};

/**
 * Represents an IPDLProtocol to be used under the cover from JS.
 * This is owned by the IPDL class, and represents an abstract IPDL protocol
 * class which can be extended from JS.
 *
 * Example usage from JS:
 *
 *     IPDL.registerTopLevelClass(TestParent);
 *
 *     class TestParent extends IPDL.PTestParent {
 *       // We add a listener for the message `SomeMessageFromChild` defined in
 *       // the IPDL protocol
 *       recvSomeMessageFromChild(arg1, arg2) {
 *           // In IPDL, out params are named. This is how we return them.
 *           return {out1: "test", out2: "test2" };
 *       };
 *
 *       // We define the allocation function
 *       allocPSubTest() {
 *         return new SubTestParent();
 *       }
 *
 *       // We need to pass the content parent ID to the super constructor
 *       constructor(id) {
 *         super(id);
 *         // We send the sync message `SomeSyncMessageToChild` to the child
 *         var syncResult = this.sendSomeSyncMessageToChild(123, [4, 5, 6]);
 *         console.log(syncResult);
 *
 *         // We send the async message `SomeAsyncMessageToChild` to the child
 *         // Async calls return a promise
 *         protocol.sendSomeAsyncMessageToChild("hi").then(function(asyncResult)
 * { console.log(asyncResult);
 *         });
 *       }
 *     }
 */
class IPDLProtocol : public nsISupports
{

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IPDLProtocol)

  // Custom deleter for our AST smart pointer.
  struct ASTDeletePolicy
  {
    void operator()(const mozilla::ipdl::ffi::AST* aASTPtr);
  };

  IPDLProtocol(dom::IPDL* aIPDL,
               IPDLSide aSide,
               const nsACString& aIPDLFile,
               nsIGlobalObject* aGlobal,
               JS::HandleObject aParent,
               JSContext* aCx);

  // Get the JSObject corresponding to the constructor of the JS class
  // of our protocol.
  JSObject* GetProtocolClassConstructor();

  // Returns a new channel ID and updates the internal channel ID counter.
  uint32_t RegisterExternalInstance();

  // Returns the protocol name.
  nsCString GetProtocolName();

  // Returns the protocol side.
  IPDLSide GetSide() { return mSide; }

  // Returns a message decl for a given name.
  const ffi::MessageDecl* GetMessageDecl(const nsACString& aName)
  {
    return mMessageTable.Get(aName);
  }

  // Type checks a parameter against a JS Value argument.
  // This is just a slim wrapper around CheckTypeSpec.
  bool CheckParamTypeSpec(JSContext* aCx,
                          JS::HandleValue aJSVal,
                          ffi::Param aParam);

  // Returns the global object associated to that protocol.
  nsIGlobalObject* GetGlobal() { return mGlobal; }

  void RemoveInstance(IPDLProtocolInstance* instance);

  // Returns the protocol name suffixed with the protocol side.
  static nsCString GetSidedProtocolName(const nsCString& aProtocolName,
                                        IPDLSide aSide)
  {
    return aProtocolName + (aSide == IPDLSide::Parent
                             ? NS_LITERAL_CSTRING("Parent")
                             : NS_LITERAL_CSTRING("Child"));
  }

  // Small helper function to join a qualified id into a string.
  static nsCString JoinQualifiedId(const ffi::QualifiedId qid);
  // Small helper function to join a namespace into a string.
  static nsCString JoinNamespace(const ffi::Namespace ns);

protected:
  // The IPC interface object that we use for the actual IPC stuff.
  // Which side of the protocol we're on.
  IPDLSide mSide;
  // Our Rust-owned AST.
  UniquePtr<const ffi::AST, ASTDeletePolicy> mAST;
  // The global JS object interface needed for promises.
  nsCOMPtr<nsIGlobalObject> mGlobal;
  // The JS constructor we expose to the API user.
  JS::Heap<JSObject*> mConstructorObj;
  // The JS prototype we expose to the API user.
  JS::Heap<JSObject*> mProtoObj;
  // The C++ JSClass corresponding to the protocol.
  JSClass mProtocolClass;
  // The unsided protocol name.
  nsCString mProtocolName;
  // The sided protocol name.
  nsCString mSidedProtocolName;
  // The IPDL global object managing this protocol.
  dom::IPDL* MOZ_NON_OWNING_REF mIPDL;

  // Typedefs for the member variables defined below.
  typedef nsDataHashtable<nsCStringHashKey, const ffi::MessageDecl*>
    MessageTable;

  typedef nsDataHashtable<nsCStringHashKey, const ffi::Struct*> StructTable;

  typedef nsDataHashtable<nsCStringHashKey, const ffi::Union*> UnionTable;

  typedef nsDataHashtable<nsCStringHashKey, const ffi::NamedProtocol*>
    ProtocolTable;

  typedef nsTHashtable<nsRefPtrHashKey<IPDLProtocolInstance>> InstanceList;

  // List of current IPDL protocol instances living in JS.
  InstanceList mInstances;
  uint32_t mNextProtocolInstanceChannelId;

  // Maps message names to the AST struct describing them.
  MessageTable mMessageTable;
  // Maps struct names to the AST struct describing them.
  StructTable mStructTable;
  // Maps union names to the AST struct describing them.
  UnionTable mUnionTable;
  // Maps protocol names to the AST struct describing them.
  ProtocolTable mProtocolTable;

  virtual ~IPDLProtocol();

  // Builds the struct, union and protocol name->AST lookup tables so that we
  // can easily access name from the TypeSpecs given by message parameters.
  void BuildNameLookupTables();

  // Used internally to get the protocol JSClass.
  JSClass& GetProtocolClass();

  // Returns the main translation unit.
  const ffi::TranslationUnit* GetMainTU();

  // Type checks a TypeSpec against a JS Value.
  // This takes care of checking the nullable and array extras, and then calls
  // CheckType, possibly on all elements if dealing with an array.
  bool CheckTypeSpec(JSContext* cx,
                     JS::HandleValue jsVal,
                     ffi::TypeSpec typeSpec);

  // Type checks a type name against a JS Value.
  bool CheckType(JSContext* cx, JS::HandleValue jsVal, ffi::QualifiedId type);

  // Type checks a protocol type name against a JS Value.
  bool CheckProtocolType(JSContext* cx,
                         JS::HandleValue jsVal,
                         ffi::QualifiedId type);

  // Type checks a struct type name against a JS Value.
  bool CheckStructType(JSContext* cx,
                       JS::HandleValue jsVal,
                       ffi::QualifiedId type);

  // Type checks a union type name against a JS Value.
  bool CheckUnionType(JSContext* cx,
                      JS::HandleValue jsVal,
                      ffi::QualifiedId type);

  // Type checks a builtin type name against a JS Value.
  // Note that some usual IPDL C++-inspired types are not available to use
  // with this JS API (for instance, int64_t).
  static bool CheckBuiltinType(JSContext* cx,
                               JS::HandleValue jsVal,
                               ffi::QualifiedId type);

  // These are wrappers unpacking the IPDLProtocolInstance stored in the private
  // data field of the callee, then dispatching the call to that instance.
  static bool SendMessageDispatch(JSContext* cx, unsigned argc, JS::Value* vp);

  static bool SendConstructorDispatch(JSContext* cx,
                                      unsigned argc,
                                      JS::Value* vp);
  static bool SendDeleteDispatch(JSContext* cx, unsigned argc, JS::Value* vp);

  // Constructor for the protocol class, which is called from the inherited
  // classes.
  static bool Constructor(JSContext* cx, unsigned argc, JS::Value* vp);

  // Default implementations for various IPDL stuff. Abstract methods throw on
  // call because they MUST be reimplemented.
  static bool RecvDelete(JSContext* cx, unsigned argc, JS::Value* vp);
  static bool RecvConstructor(JSContext* cx, unsigned argc, JS::Value* vp);
  static bool AbstractRecvMessage(JSContext* cx, unsigned argc, JS::Value* vp);
  static bool AbstractAlloc(JSContext* cx, unsigned argc, JS::Value* vp);

  // Our IPDLProtocol JS class custom operations.
  static constexpr JSClassOps sIPDLJSClassOps = {};
};

} // ipdl
} // mozilla

#endif // dom_base_ipdl_bindings_IPDLProtocol_h
