/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLCOMMANDQUEUE_H_
#define WEBGLCOMMANDQUEUE_H_

#include <type_traits>
#include "mozilla/FunctionTypeTraits.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "QueueParamTraits.h"
#include "WebGLTypes.h"

namespace mozilla {

using webgl::QueueStatus;

namespace webgl {

class RangeConsumerView final : public webgl::ConsumerView<RangeConsumerView> {
  RangedPtr<const uint8_t> mSrcItr;
  const RangedPtr<const uint8_t> mSrcEnd;

 public:
  auto Remaining() const { return *MaybeAs<size_t>(mSrcEnd - mSrcItr); }

  explicit RangeConsumerView(const Range<const uint8_t> range)
      : ConsumerView(this, nullptr, 0),
        mSrcItr(range.begin()),
        mSrcEnd(range.end()) {
    (void)Remaining();  // assert size non-negative
  }

  void AlignTo(const size_t alignment) {
    const auto offset = AlignmentOffset(alignment, mSrcItr.get());
    if (offset > Remaining()) {
      mSrcItr = mSrcEnd;
      return;
    }
    mSrcItr += offset;
  }

  template <typename T>
  Maybe<Range<const T>> ReadRange(const size_t elemCount) {
    AlignTo(alignof(T));

    constexpr auto elemSize = sizeof(T);
    const auto byteSizeChecked = CheckedInt<size_t>(elemCount) * elemSize;
    MOZ_RELEASE_ASSERT(byteSizeChecked.isValid());
    const auto& byteSize = byteSizeChecked.value();

    const auto remaining = Remaining();
    if (byteSize > remaining) return {};

    const auto begin = reinterpret_cast<const T*>(mSrcItr.get());
    mSrcItr += byteSize;
    return Some(Range<const T>{begin, elemCount});
  }
};

// -

namespace details {

class SizeOnlyProducerView final
    : public webgl::ProducerView<SizeOnlyProducerView> {
  size_t mRequiredSize = 0;

 public:
  SizeOnlyProducerView() : ProducerView(this, 0, nullptr) {}

  template <typename T>
  void WriteFromRange(const Range<const T>& src) {
    constexpr auto alignment = alignof(T);
    const size_t byteSize = ByteSize(src);
    // printf_stderr("SizeOnlyProducerView: @%zu +%zu\n", alignment, byteSize);

    const auto offset = AlignmentOffset(alignment, mRequiredSize);
    mRequiredSize += offset;

    mRequiredSize += byteSize;
  }

  const auto& RequiredSize() const { return mRequiredSize; }
};

// -

class RangeProducerView final : public webgl::ProducerView<RangeProducerView> {
  const RangedPtr<uint8_t> mDestBegin;
  const RangedPtr<uint8_t> mDestEnd;
  RangedPtr<uint8_t> mDestItr;

 public:
  auto Remaining() const { return *MaybeAs<size_t>(mDestEnd - mDestItr); }

  explicit RangeProducerView(const Range<uint8_t> range)
      : ProducerView(this, 0, nullptr),
        mDestBegin(range.begin()),
        mDestEnd(range.end()),
        mDestItr(mDestBegin) {
    (void)Remaining();  // assert size non-negative
  }

  template <typename T>
  void WriteFromRange(const Range<const T>& src) {
    constexpr auto alignment = alignof(T);
    const size_t byteSize = ByteSize(src);
    // printf_stderr("RangeProducerView: @%zu +%zu\n", alignment, byteSize);

    const auto offset = AlignmentOffset(alignment, mDestItr.get());
    mDestItr += offset;

    MOZ_ASSERT(byteSize <= Remaining());
    if (byteSize) {
      memcpy(mDestItr.get(), src.begin().get(), byteSize);
    }
    mDestItr += byteSize;
  }
};

// -

template <typename ProducerViewT>
inline void Serialize(ProducerViewT&) {}

template <typename ProducerViewT, typename Arg, typename... Args>
inline void Serialize(ProducerViewT& view, const Arg& arg,
                      const Args&... args) {
  MOZ_ALWAYS_TRUE(view.WriteParam(arg) == QueueStatus::kSuccess);
  Serialize(view, args...);
}

}  // namespace details

// -

template <typename... Args>
size_t SerializedSize(const Args&... args) {
  webgl::details::SizeOnlyProducerView sizeView;
  webgl::details::Serialize(sizeView, args...);
  return sizeView.RequiredSize();
}

template <typename... Args>
void Serialize(Range<uint8_t> dest, const Args&... args) {
  webgl::details::RangeProducerView view(dest);
  webgl::details::Serialize(view, args...);
}

// -

inline Maybe<uint16_t> Deserialize(RangeConsumerView&, size_t) { return {}; }

template <typename Arg, typename... Args>
inline Maybe<uint16_t> Deserialize(RangeConsumerView& view,
                                   const uint16_t argId, Arg& arg,
                                   Args&... args) {
  if (!webgl::QueueParamTraits<Arg>::Read(view, &arg)) {
    return Some(argId);
  }
  return Deserialize(view, argId + 1, args...);
}

}  // namespace webgl

// The MethodDispatcher setup uses a CommandSink to read parameters, call the
// given method using the given synchronization protocol, and provide
// compile-time lookup of the ID by class method.
// To use this system, first define a dispatcher subclass of
// EmptyMethodDispatcher.  This class must be parameterized by command ID.
//
// Example:
// template <size_t id=0> class MyDispatcher
//    : public EmptyMethodDispatcher<MyDispatcher> {};
//
// Then, for each command handled, specialize this to subclass MethodDispatcher.
// The subclass must define the Method.  It may optionally define isSync for
// synchronous methods.
//
// Example:
// template <>
// class MyDispatcher<0>
//    : public MethodDispatcher<MyDispatcher, 0,
//        decltype(&MyClass::MyMethod), MyClass::MyMethod> {};
//
// The method may then be called from the source and run on the sink.
//
// Example:
// int result = Run<MyClass::MyMethod>(param1, std::move(param2));

template <template <size_t> typename Derived>
class EmptyMethodDispatcher {
 public:
  template <typename ObjectT>
  static MOZ_ALWAYS_INLINE bool DispatchCommand(ObjectT&, const size_t id,
                                                webgl::RangeConsumerView&) {
    const nsPrintfCString cstr(
        "MethodDispatcher<%i> not found. Please file a bug!", int(id));
    const auto str = ToString(cstr);
    gfxCriticalError() << str;
    return false;
  }
};

// -

template <typename ReturnT, typename ObjectT, typename... Args>
std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> ArgsTuple(
    ReturnT (ObjectT::*)(Args... args)) {
  return std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...>{};
}

template <typename ReturnT, typename ObjectT, typename... Args>
std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> ArgsTuple(
    ReturnT (ObjectT::*)(Args... args) const) {
  return std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...>{};
}

// Derived type must be parameterized by the ID.
template <template <size_t> typename Derived, size_t ID, typename MethodType,
          MethodType method>
class MethodDispatcher {
  static constexpr size_t kId = ID;
  using DerivedType = Derived<ID>;
  using NextDispatcher = Derived<ID + 1>;

 public:
  template <typename ObjectT>
  static MOZ_ALWAYS_INLINE bool DispatchCommand(
      ObjectT& obj, const size_t id, webgl::RangeConsumerView& view) {
    if (id == kId) {
      auto argsTuple = ArgsTuple(method);

      const auto viewWas = view;
      (void)viewWas;  // For debugging.
      return std::apply(
          [&](auto&... args) {
            const auto badArgId = webgl::Deserialize(view, 1, args...);
            if (badArgId) {
              const auto& name = DerivedType::Name();
              gfxCriticalError() << "webgl::Deserialize failed for " << name
                                 << " arg " << *badArgId;
              return false;
            }
            (obj.*method)(args...);
            return true;
          },
          argsTuple);
    }
    return Derived<kId + 1>::DispatchCommand(obj, id, view);
  }

  static constexpr size_t Id() { return kId; }
  static constexpr MethodType Method() { return method; }
};

}  // namespace mozilla

#endif  // WEBGLCOMMANDQUEUE_H_
