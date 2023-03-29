/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SyncedContext_h
#define mozilla_dom_SyncedContext_h

#include <array>
#include <type_traits>
#include <utility>
#include "mozilla/Attributes.h"
#include "mozilla/BitSet.h"
#include "mozilla/EnumSet.h"
#include "nsStringFwd.h"
#include "nscore.h"

// Referenced via macro definitions
#include "mozilla/ErrorResult.h"

class PickleIterator;

namespace IPC {
class Message;
class MessageReader;
class MessageWriter;
}  // namespace IPC

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
template <typename T>
class MaybeDiscarded;

namespace syncedcontext {

template <size_t I>
using Index = typename std::integral_constant<size_t, I>;

// We're going to use the empty base optimization for synced fields of different
// sizes, so we define an empty class for that purpose.
template <size_t I, size_t S>
struct Empty {};

// A templated container for a synced field. I is the index and T is the type.
template <size_t I, typename T>
struct Field {
  T mField{};
};

// SizedField is a Field with a helper to define either an "empty" field, or a
// field of a given type.
template <size_t I, typename T, size_t S>
using SizedField = std::conditional_t<((sizeof(T) > 8) ? 8 : sizeof(T)) == S,
                                      Field<I, T>, Empty<I, S>>;

template <typename Context>
class Transaction {
 public:
  using IndexSet = EnumSet<size_t, BitSet<Context::FieldValues::count>>;

  // Set a field at the given index in this `Transaction`. Creating a
  // `Transaction` object and setting multiple fields on it allows for
  // multiple mutations to be performed atomically.
  template <size_t I, typename U>
  void Set(U&& aValue) {
    mValues.Get(Index<I>{}) = std::forward<U>(aValue);
    mModified += I;
  }

  // Apply the changes from this transaction to the specified Context in all
  // processes. This method will call the correct `CanSet` and `DidSet` methods,
  // as well as move the value.
  //
  // If the target has been discarded, changes will be ignored.
  //
  // NOTE: This method mutates `this`, clearing the modified field set.
  [[nodiscard]] nsresult Commit(Context* aOwner);

  // Called from `ContentParent` in response to a transaction from content.
  mozilla::ipc::IPCResult CommitFromIPC(const MaybeDiscarded<Context>& aOwner,
                                        ContentParent* aSource);

  // Called from `ContentChild` in response to a transaction from the parent.
  mozilla::ipc::IPCResult CommitFromIPC(const MaybeDiscarded<Context>& aOwner,
                                        uint64_t aEpoch, ContentChild* aSource);

  // Apply the changes from this transaction to the specified Context WITHOUT
  // syncing the changes to other processes.
  //
  // Unlike `Commit`, this method will NOT call the corresponding `CanSet` or
  // `DidSet` methods, and can be performed when the target context is
  // unattached or discarded.
  //
  // NOTE: YOU PROBABLY DO NOT WANT TO USE THIS METHOD
  void CommitWithoutSyncing(Context* aOwner);

 private:
  friend struct mozilla::ipc::IPDLParamTraits<Transaction<Context>>;

  void Write(IPC::MessageWriter* aWriter,
             mozilla::ipc::IProtocol* aActor) const;
  bool Read(IPC::MessageReader* aReader, mozilla::ipc::IProtocol* aActor);

  // You probably don't want to directly call this method - instead call
  // `Commit`, which will perform the necessary synchronization.
  //
  // `Validate` must be called before calling this method.
  void Apply(Context* aOwner, bool aFromIPC);

  // Returns the set of fields which failed to validate, or an empty set if
  // there were no validation errors.
  //
  // NOTE: This method mutates `this` if any changes were reverted.
  IndexSet Validate(Context* aOwner, ContentParent* aSource);

  template <typename F>
  static void EachIndex(F&& aCallback) {
    Context::FieldValues::EachIndex(aCallback);
  }

  template <size_t I>
  static uint64_t& FieldEpoch(Index<I>, Context* aContext) {
    return std::get<I>(aContext->mFields.mEpochs);
  }

  typename Context::FieldValues mValues;
  IndexSet mModified;
};

template <typename Base, size_t Count>
class FieldValues : public Base {
 public:
  // The number of fields stored by this type.
  static constexpr size_t count = Count;

  // The base type will define a series of `Get` methods for looking up a field
  // by its field index.
  using Base::Get;

  // Calls a generic lambda with an `Index<I>` for each index less than the
  // field count.
  template <typename F>
  static void EachIndex(F&& aCallback) {
    EachIndexInner(std::make_index_sequence<count>(),
                   std::forward<F>(aCallback));
  }

 private:
  friend struct mozilla::ipc::IPDLParamTraits<FieldValues<Base, Count>>;

  void Write(IPC::MessageWriter* aWriter,
             mozilla::ipc::IProtocol* aActor) const;
  bool Read(IPC::MessageReader* aReader, mozilla::ipc::IProtocol* aActor);

  template <typename F, size_t... Indexes>
  static void EachIndexInner(std::index_sequence<Indexes...> aIndexes,
                             F&& aCallback) {
    (aCallback(Index<Indexes>()), ...);
  }
};

// Storage related to synchronized context fields. Contains both a tuple of
// individual field values, and epoch information for field synchronization.
template <typename Values>
class FieldStorage {
 public:
  // Unsafely grab a reference directly to the internal values structure which
  // can be modified without telling other processes about the change.
  //
  // This is only sound in specific code which is already messaging other
  // processes, and doesn't need to worry about epochs or other properties of
  // field synchronization.
  Values& RawValues() { return mValues; }
  const Values& RawValues() const { return mValues; }

  // Get an individual field by index.
  template <size_t I>
  const auto& Get() const {
    return RawValues().Get(Index<I>{});
  }

  // Set the value of a field without telling other processes about the change.
  //
  // This is only sound in specific code which is already messaging other
  // processes, and doesn't need to worry about epochs or other properties of
  // field synchronization.
  template <size_t I, typename U>
  void SetWithoutSyncing(U&& aValue) {
    GetNonSyncingReference<I>() = std::move(aValue);
  }

  // Get a reference to a field that can modify without telling other
  // processes about the change.
  //
  // This is only sound in specific code which is already messaging other
  // processes, and doesn't need to worry about epochs or other properties of
  // field synchronization.
  template <size_t I>
  auto& GetNonSyncingReference() {
    return RawValues().Get(Index<I>{});
  }

  FieldStorage() = default;
  explicit FieldStorage(Values&& aInit) : mValues(std::move(aInit)) {}

 private:
  template <typename Context>
  friend class Transaction;

  // Data Members
  std::array<uint64_t, Values::count> mEpochs{};
  Values mValues;
};

// Alternative return type enum for `CanSet` validators which allows specifying
// more behaviour.
enum class CanSetResult : uint8_t {
  // The set attempt is denied. This is equivalent to returning `false`.
  Deny,
  // The set attempt is allowed. This is equivalent to returning `true`.
  Allow,
  // The set attempt is reverted non-fatally.
  Revert,
};

// Helper type traits to use concrete types rather than generic forwarding
// references for the `SetXXX` methods defined on the synced context type.
//
// This helps avoid potential issues where someone accidentally declares an
// overload of these methods with slightly different types and different
// behaviours. See bug 1659520.
template <typename T>
struct GetFieldSetterType {
  using SetterArg = T;
};
template <>
struct GetFieldSetterType<nsString> {
  using SetterArg = const nsAString&;
};
template <>
struct GetFieldSetterType<nsCString> {
  using SetterArg = const nsACString&;
};
template <typename T>
using FieldSetterType = typename GetFieldSetterType<T>::SetterArg;

#define MOZ_DECL_SYNCED_CONTEXT_FIELD_INDEX(name, type) IDX_##name,

#define MOZ_DECL_SYNCED_CONTEXT_FIELD_GETSET(name, type)                       \
  const type& Get##name() const { return mFields.template Get<IDX_##name>(); } \
                                                                               \
  [[nodiscard]] nsresult Set##name(                                            \
      ::mozilla::dom::syncedcontext::FieldSetterType<type> aValue) {           \
    Transaction txn;                                                           \
    txn.template Set<IDX_##name>(std::move(aValue));                           \
    return txn.Commit(this);                                                   \
  }                                                                            \
  void Set##name(::mozilla::dom::syncedcontext::FieldSetterType<type> aValue,  \
                 ErrorResult& aRv) {                                           \
    nsresult rv = this->Set##name(std::move(aValue));                          \
    if (NS_FAILED(rv)) {                                                       \
      aRv.ThrowInvalidStateError("cannot set synced field '" #name             \
                                 "': context is discarded");                   \
    }                                                                          \
  }

#define MOZ_DECL_SYNCED_CONTEXT_TRANSACTION_SET(name, type)  \
  template <typename U>                                      \
  void Set##name(U&& aValue) {                               \
    this->template Set<IDX_##name>(std::forward<U>(aValue)); \
  }
#define MOZ_DECL_SYNCED_CONTEXT_INDEX_TO_NAME(name, type) \
  case IDX_##name:                                        \
    return #name;

#define MOZ_DECL_SYNCED_FIELD_INHERIT(name, type) \
 public                                           \
  syncedcontext::SizedField<IDX_##name, type, Size>,

#define MOZ_DECL_SYNCED_CONTEXT_BASE_FIELD_GETTER(name, type) \
  type& Get(FieldIndex<IDX_##name>) {                         \
    return Field<IDX_##name, type>::mField;                   \
  }                                                           \
  const type& Get(FieldIndex<IDX_##name>) const {             \
    return Field<IDX_##name, type>::mField;                   \
  }

// Declare a type as a synced context type.
//
// clazz is the name of the type being declared, and `eachfield` is a macro
// which, when called with the name of the macro, will call that macro once for
// each field in the synced context.
#define MOZ_DECL_SYNCED_CONTEXT(clazz, eachfield)                              \
 public:                                                                       \
  /* Index constants for referring to each field in generic code */            \
  enum FieldIndexes {                                                          \
    eachfield(MOZ_DECL_SYNCED_CONTEXT_FIELD_INDEX) SYNCED_FIELD_COUNT          \
  };                                                                           \
                                                                               \
  /* Helper for overloading methods like `CanSet` and `DidSet` */              \
  template <size_t I>                                                          \
  using FieldIndex = typename ::mozilla::dom::syncedcontext::Index<I>;         \
                                                                               \
  /* Fields contain all synced fields defined by                               \
   * `eachfield(MOZ_DECL_SYNCED_FIELD_INHERIT)`, but only those where the size \
   * of the field is equal to size Size will be present. We use SizedField to  \
   * remove fields of the wrong size. */                                       \
  template <size_t Size>                                                       \
  struct Fields : eachfield(MOZ_DECL_SYNCED_FIELD_INHERIT)                     \
                      syncedcontext::Empty<SYNCED_FIELD_COUNT, Size> {};       \
                                                                               \
  /* Struct containing the data for all synced fields as members. We filter    \
   * sizes to lay out fields of size 1, then 2, then 4 and last 8 or greater.  \
   * This way we don't need to consider packing when defining fields, but      \
   * we'll just reorder them here.                                             \
   */                                                                          \
  struct BaseFieldValues : public Fields<1>,                                   \
                           public Fields<2>,                                   \
                           public Fields<4>,                                   \
                           public Fields<8> {                                  \
    template <size_t I>                                                        \
    auto& Get() {                                                              \
      return Get(FieldIndex<I>{});                                             \
    }                                                                          \
    template <size_t I>                                                        \
    const auto& Get() const {                                                  \
      return Get(FieldIndex<I>{});                                             \
    }                                                                          \
    eachfield(MOZ_DECL_SYNCED_CONTEXT_BASE_FIELD_GETTER)                       \
  };                                                                           \
  using FieldValues =                                                          \
      typename ::mozilla::dom::syncedcontext::FieldValues<BaseFieldValues,     \
                                                          SYNCED_FIELD_COUNT>; \
                                                                               \
 protected:                                                                    \
  friend class ::mozilla::dom::syncedcontext::Transaction<clazz>;              \
  ::mozilla::dom::syncedcontext::FieldStorage<FieldValues> mFields;            \
                                                                               \
 public:                                                                       \
  /* Transaction types for bulk mutations */                                   \
  using BaseTransaction = ::mozilla::dom::syncedcontext::Transaction<clazz>;   \
  class Transaction final : public BaseTransaction {                           \
   public:                                                                     \
    eachfield(MOZ_DECL_SYNCED_CONTEXT_TRANSACTION_SET)                         \
  };                                                                           \
                                                                               \
  /* Field name getter by field index */                                       \
  static const char* FieldIndexToName(size_t aIndex) {                         \
    switch (aIndex) { eachfield(MOZ_DECL_SYNCED_CONTEXT_INDEX_TO_NAME) }       \
    return "<unknown>";                                                        \
  }                                                                            \
  eachfield(MOZ_DECL_SYNCED_CONTEXT_FIELD_GETSET)

}  // namespace syncedcontext
}  // namespace dom

namespace ipc {

template <typename Context>
struct IPDLParamTraits<dom::syncedcontext::Transaction<Context>> {
  typedef dom::syncedcontext::Transaction<Context> paramType;

  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const paramType& aParam) {
    aParam.Write(aWriter, aActor);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    return aResult->Read(aReader, aActor);
  }
};

template <typename Base, size_t Count>
struct IPDLParamTraits<dom::syncedcontext::FieldValues<Base, Count>> {
  typedef dom::syncedcontext::FieldValues<Base, Count> paramType;

  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const paramType& aParam) {
    aParam.Write(aWriter, aActor);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    return aResult->Read(aReader, aActor);
  }
};

}  // namespace ipc
}  // namespace mozilla

#endif  // !defined(mozilla_dom_SyncedContext_h)
