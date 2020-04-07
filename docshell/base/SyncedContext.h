/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SyncedContext_h
#define mozilla_dom_SyncedContext_h

#include "mozilla/dom/MaybeDiscarded.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Tuple.h"
#include <utility>

namespace mozilla {
namespace ipc {
class IProtocol;
class IPCResult;
template <typename T>
struct IPDLParamTraits;
}  // namespace ipc

namespace dom {
class ContentParent;
class ContentChild;

namespace syncedcontext {

template <size_t I>
using Index = typename std::integral_constant<size_t, I>;

using IndexSet = EnumSet<size_t, uint64_t>;

template <typename Context>
class Transaction {
 public:
  // Set a field at the given index in this `Transaction`. Creating a
  // `Transaction` object and setting multiple fields on it allows for
  // multiple mutations to be performed atomically.
  template <size_t I, typename U>
  void Set(U&& aValue) {
    auto& field = mozilla::Get<I>(mMaybeFields);
    field.emplace(std::forward<U>(aValue));
  }

  // Apply the changes from this transaction to the specified Context in all
  // processes. This method will call the correct `CanSet` and `DidSet` methods,
  // as well as move the value.
  //
  // If the target has been discarded, changes will be ignored.
  //
  // NOTE: This method mutates `this`, resetting all members to `Nothing()`
  nsresult Commit(Context* aOwner);

  // Called from `ContentParent` in response to a transaction from content.
  mozilla::ipc::IPCResult CommitFromIPC(const MaybeDiscarded<Context>& aOwner,
                                        ContentParent* aSource);

  // Called from `ContentChild` in response to a transaction from the parent.
  mozilla::ipc::IPCResult CommitFromIPC(const MaybeDiscarded<Context>& aOwner,
                                        uint64_t aEpoch, ContentChild* aSource);

  void Write(IPC::Message* aMsg, mozilla::ipc::IProtocol* aActor) const;
  bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
            mozilla::ipc::IProtocol* aActor);

 private:
  friend struct mozilla::ipc::IPDLParamTraits<Transaction<Context>>;

  // You probably don't want to directly call this method - instead call
  // `Commit`, which will perform the necessary synchronization.
  //
  // `Validate` must be called before calling this method.
  void Apply(Context* aOwner);

  // Returns the set of fields which failed to validate, or an empty set if
  // there were no validation errors.
  IndexSet Validate(Context* aOwner, ContentParent* aSource);

  using FieldStorage = typename Context::FieldStorage;
  static FieldStorage& GetFieldStorage(Context* aContext) {
    return aContext->mFields;
  }

  // Call a generic lambda with a `Index<I>` for each index less than the number
  // of elements in `FieldStorage`.
  template <typename F>
  static void EachIndexInner(Index<0>, F&&) {}
  template <size_t I, typename F>
  static void EachIndexInner(Index<I>, F&& aCallback) {
    aCallback(Index<I - 1>());
    EachIndexInner(Index<I - 1>(), aCallback);
  }
  template <typename F>
  static void EachIndex(F&& aCallback) {
    EachIndexInner(Index<FieldStorage::fieldCount>(), aCallback);
  }

  // Helper for invoking `std::get` or `mozilla::Get` using a
  // `SyncedFieldIndex<I>` value.
  template <size_t I, typename... Ts>
  static auto& GetAt(Index<I>, mozilla::Tuple<Ts...>& aTarget) {
    return mozilla::Get<I>(aTarget);
  }
  template <size_t I, typename T, size_t L>
  static auto& GetAt(Index<I>, std::array<T, L>& aTarget) {
    return std::get<I>(aTarget);
  }

  typename FieldStorage::MaybeFieldTuple mMaybeFields;
};

// Storage related to synchronized context fields. Contains both a tuple of
// individual field values, and epoch information for field synchronization.
template <typename, typename... Ts>
class FieldStorage {
 public:
  using FieldTuple = mozilla::Tuple<Ts...>;

  static constexpr size_t fieldCount = sizeof...(Ts);
  static_assert(fieldCount < 64,
                "At most 64 synced fields are supported. Please file a bug if "
                "you need to additional fields.");

  const FieldTuple& Fields() const { return mFields; }

  // Get an individual field by index.
  template <size_t I>
  const auto& Get() const {
    return mozilla::Get<I>(mFields);
  }

  // Set the value of a field without telling other processes about the change.
  //
  // This is only sound in specific code which is already messaging other
  // processes, and doesn't need to worry about epochs or other properties of
  // field synchronization.
  template <size_t I, typename U>
  void SetWithoutSyncing(U&& aValue) {
    mozilla::Get<I>(mFields) = std::move(aValue);
  }

  // Get a reference to a field that can modify without telling other
  // processes about the change.
  //
  // This is only sound in specific code which is already messaging other
  // processes, and doesn't need to worry about epochs or other properties of
  // field synchronization.
  template <size_t I>
  auto& GetNonSyncingReference() {
    return mozilla::Get<I>(mFields);
  }

  FieldStorage() = default;
  explicit FieldStorage(FieldTuple&& aInit) : mFields(std::move(aInit)) {}

 private:
  template <typename Context>
  friend class Transaction;

  // Helper type for `Transaction`.
  using MaybeFieldTuple = mozilla::Tuple<mozilla::Maybe<Ts>...>;

  // Data Members
  std::array<uint64_t, sizeof...(Ts)> mEpochs{};
  FieldTuple mFields;
};

#define MOZ_DECL_SYNCED_CONTEXT_FIELD_TYPE(name, type) , type
#define MOZ_DECL_SYNCED_CONTEXT_FIELD_INDEX(name, type) IDX_##name,
#define MOZ_DECL_SYNCED_CONTEXT_FIELD_GETSET(name, type)                       \
  const type& Get##name() const { return mFields.template Get<IDX_##name>(); } \
                                                                               \
  template <typename U>                                                        \
  void Set##name(U&& aValue) {                                                 \
    Transaction txn;                                                           \
    txn.template Set<IDX_##name>(std::forward<U>(aValue));                     \
    txn.Commit(this);                                                          \
  }
#define MOZ_DECL_SYNCED_CONTEXT_TRANSACTION_SET(name, type)  \
  template <typename U>                                      \
  void Set##name(U&& aValue) {                               \
    this->template Set<IDX_##name>(std::forward<U>(aValue)); \
  }
#define MOZ_DECL_SYNCED_CONTEXT_INDEX_TO_NAME(name, type) \
  case IDX_##name:                                        \
    return #name;

// Declare a type as a synced context type.
//
// clazz is the name of the type being declared, and `eachfield` is a macro
// which, when called with the name of the macro, will call that macro once for
// each field in the synced context.
#define MOZ_DECL_SYNCED_CONTEXT(clazz, eachfield)                            \
 protected:                                                                  \
  friend class ::mozilla::dom::syncedcontext::Transaction<clazz>;            \
  enum FieldIndexes { eachfield(MOZ_DECL_SYNCED_CONTEXT_FIELD_INDEX) };      \
  using FieldStorage =                                                       \
      typename ::mozilla::dom::syncedcontext::FieldStorage<void eachfield(   \
          MOZ_DECL_SYNCED_CONTEXT_FIELD_TYPE)>;                              \
  FieldStorage mFields;                                                      \
                                                                             \
 public:                                                                     \
  /* Helper for overloading methods like `CanSet` and `DidSet` */            \
  template <size_t I>                                                        \
  using FieldIndex = typename ::mozilla::dom::syncedcontext::Index<I>;       \
                                                                             \
  /* Field tuple type for use by initializers */                             \
  using FieldTuple = typename FieldStorage::FieldTuple;                      \
                                                                             \
  /* Transaction types for bulk mutations */                                 \
  using BaseTransaction = ::mozilla::dom::syncedcontext::Transaction<clazz>; \
  class Transaction final : public BaseTransaction {                         \
   public:                                                                   \
    eachfield(MOZ_DECL_SYNCED_CONTEXT_TRANSACTION_SET)                       \
  };                                                                         \
                                                                             \
  /* Field name getter by field index */                                     \
  static const char* FieldIndexToName(size_t aIndex) {                       \
    switch (aIndex) { eachfield(MOZ_DECL_SYNCED_CONTEXT_INDEX_TO_NAME) }     \
    return "<unknown>";                                                      \
  }                                                                          \
  eachfield(MOZ_DECL_SYNCED_CONTEXT_FIELD_GETSET)

}  // namespace syncedcontext
}  // namespace dom

namespace ipc {

template <typename Context>
struct IPDLParamTraits<dom::syncedcontext::Transaction<Context>> {
  typedef dom::syncedcontext::Transaction<Context> paramType;

  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    aParam.Write(aMsg, aActor);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    return aResult->Read(aMsg, aIter, aActor);
  }
};

}  // namespace ipc
}  // namespace mozilla

#endif  // !defined(mozilla_dom_SyncedContext_h)
