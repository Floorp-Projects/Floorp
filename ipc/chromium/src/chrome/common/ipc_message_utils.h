/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_MESSAGE_UTILS_H_
#define CHROME_COMMON_IPC_MESSAGE_UTILS_H_

#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include "ErrorList.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/common/ipc_message.h"

#if defined(OS_WIN)
#  include <windows.h>
#endif

template <typename T>
class RefPtr;
template <typename T>
class nsCOMPtr;

namespace IPC {

//-----------------------------------------------------------------------------
// An iterator class for reading the fields contained within a Message.

class MessageIterator {
 public:
  explicit MessageIterator(const Message& m) : msg_(m), iter_(m) {}
  int NextInt() const {
    int val;
    if (!msg_.ReadInt(&iter_, &val)) NOTREACHED();
    return val;
  }
  intptr_t NextIntPtr() const {
    intptr_t val;
    if (!msg_.ReadIntPtr(&iter_, &val)) NOTREACHED();
    return val;
  }
  const std::string NextString() const {
    std::string val;
    if (!msg_.ReadString(&iter_, &val)) NOTREACHED();
    return val;
  }
  const std::wstring NextWString() const {
    std::wstring val;
    if (!msg_.ReadWString(&iter_, &val)) NOTREACHED();
    return val;
  }

 private:
  const Message& msg_;
  mutable PickleIterator iter_;
};

//-----------------------------------------------------------------------------
// ParamTraits specializations, etc.
//
// The full set of types ParamTraits is specialized upon contains *possibly*
// repeated types: unsigned long may be uint32_t or size_t, unsigned long long
// may be uint64_t or size_t, nsresult may be uint32_t, and so on.  You can't
// have ParamTraits<unsigned int> *and* ParamTraits<uint32_t> if unsigned int
// is uint32_t -- that's multiple definitions, and you can only have one.
//
// You could use #ifs and macro conditions to avoid duplicates, but they'd be
// hairy: heavily dependent upon OS and compiler author choices, forced to
// address all conflicts by hand.  Happily there's a better way.  The basic
// idea looks like this, where T -> U represents T inheriting from U:
//
// class ParamTraits<P>
// |
// --> class ParamTraits1<P>
//     |
//     --> class ParamTraits2<P>
//         |
//         --> class ParamTraitsN<P> // or however many levels
//
// The default specialization of ParamTraits{M}<P> is an empty class that
// inherits from ParamTraits{M + 1}<P> (or nothing in the base case).
//
// Now partition the set of parameter types into sets without duplicates.
// Assign each set of types to a level M.  Then specialize ParamTraitsM for
// each of those types.  A reference to ParamTraits<P> will consist of some
// number of empty classes inheriting in sequence, ending in a non-empty
// ParamTraits{N}<P>.  It's okay for the parameter types to be duplicative:
// either name of a type will resolve to the same ParamTraits{N}<P>.
//
// The nice thing is that because templates are instantiated lazily, if we
// indeed have uint32_t == unsigned int, say, with the former in level N and
// the latter in M > N, ParamTraitsM<unsigned int> won't be created (as long as
// nobody uses ParamTraitsM<unsigned int>, but why would you), and no duplicate
// code will be compiled or extra symbols generated.  It's as efficient at
// runtime as manually figuring out and avoiding conflicts by #ifs.
//
// The scheme we follow below names the various classes according to the types
// in them, and the number of ParamTraits levels is larger, but otherwise it's
// exactly the above idea.
//

template <class P>
struct ParamTraits;

template <typename P>
static inline void WriteParam(Message* m, P&& p) {
  ParamTraits<std::decay_t<P>>::Write(m, std::forward<P>(p));
}

template <typename P>
static inline bool WARN_UNUSED_RESULT ReadParam(const Message* m,
                                                PickleIterator* iter, P* p) {
  return ParamTraits<P>::Read(m, iter, p);
}

template <typename P>
static inline void LogParam(const P& p, std::wstring* l) {
  ParamTraits<P>::Log(p, l);
}

// Fundamental types.

template <class P>
struct ParamTraitsFundamental {};

template <>
struct ParamTraitsFundamental<bool> {
  typedef bool param_type;
  static void Write(Message* m, const param_type& p) { m->WriteBool(p); }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadBool(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(p ? L"true" : L"false");
  }
};

template <>
struct ParamTraitsFundamental<int> {
  typedef int param_type;
  static void Write(Message* m, const param_type& p) { m->WriteInt(p); }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadInt(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%d", p));
  }
};

template <>
struct ParamTraitsFundamental<long> {
  typedef long param_type;
  static void Write(Message* m, const param_type& p) { m->WriteLong(p); }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadLong(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%l", p));
  }
};

template <>
struct ParamTraitsFundamental<unsigned long> {
  typedef unsigned long param_type;
  static void Write(Message* m, const param_type& p) { m->WriteULong(p); }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadULong(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%ul", p));
  }
};

template <>
struct ParamTraitsFundamental<long long> {
  typedef long long param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteBytes(&p, sizeof(param_type));
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadBytesInto(iter, r, sizeof(*r));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%ll", p));
  }
};

template <>
struct ParamTraitsFundamental<unsigned long long> {
  typedef unsigned long long param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteBytes(&p, sizeof(param_type));
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadBytesInto(iter, r, sizeof(*r));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%ull", p));
  }
};

template <>
struct ParamTraitsFundamental<double> {
  typedef double param_type;
  static void Write(Message* m, const param_type& p) { m->WriteDouble(p); }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadDouble(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"e", p));
  }
};

// Fixed-size <stdint.h> types.

template <class P>
struct ParamTraitsFixed : ParamTraitsFundamental<P> {};

template <>
struct ParamTraitsFixed<int16_t> {
  typedef int16_t param_type;
  static void Write(Message* m, const param_type& p) { m->WriteInt16(p); }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadInt16(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%hd", p));
  }
};

template <>
struct ParamTraitsFixed<uint16_t> {
  typedef uint16_t param_type;
  static void Write(Message* m, const param_type& p) { m->WriteUInt16(p); }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadUInt16(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%hu", p));
  }
};

template <>
struct ParamTraitsFixed<uint32_t> {
  typedef uint32_t param_type;
  static void Write(Message* m, const param_type& p) { m->WriteUInt32(p); }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadUInt32(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%u", p));
  }
};

template <>
struct ParamTraitsFixed<int64_t> {
  typedef int64_t param_type;
  static void Write(Message* m, const param_type& p) { m->WriteInt64(p); }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadInt64(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%" PRId64L, p));
  }
};

template <>
struct ParamTraitsFixed<uint64_t> {
  typedef uint64_t param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt64(static_cast<int64_t>(p));
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadInt64(iter, reinterpret_cast<int64_t*>(r));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%" PRIu64L, p));
  }
};

// std::* types.

template <class P>
struct ParamTraitsStd : ParamTraitsFixed<P> {};

template <>
struct ParamTraitsStd<std::string> {
  typedef std::string param_type;
  static void Write(Message* m, const param_type& p) { m->WriteString(p); }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadString(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(UTF8ToWide(p));
  }
};

template <>
struct ParamTraitsStd<std::wstring> {
  typedef std::wstring param_type;
  static void Write(Message* m, const param_type& p) { m->WriteWString(p); }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadWString(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) { l->append(p); }
};

template <class K, class V>
struct ParamTraitsStd<std::map<K, V>> {
  typedef std::map<K, V> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.size()));
    typename param_type::const_iterator iter;
    for (iter = p.begin(); iter != p.end(); ++iter) {
      WriteParam(m, iter->first);
      WriteParam(m, iter->second);
    }
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    int size;
    if (!ReadParam(m, iter, &size) || size < 0) return false;
    for (int i = 0; i < size; ++i) {
      K k;
      if (!ReadParam(m, iter, &k)) return false;
      V& value = (*r)[k];
      if (!ReadParam(m, iter, &value)) return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<std::map>");
  }
};

// Windows-specific types.

template <class P>
struct ParamTraitsWindows : ParamTraitsStd<P> {};

#if defined(OS_WIN)
template <>
struct ParamTraitsWindows<HANDLE> {
  static_assert(sizeof(HANDLE) == sizeof(intptr_t), "Wrong size for HANDLE?");

  static void Write(Message* m, HANDLE p) {
    m->WriteIntPtr(reinterpret_cast<intptr_t>(p));
  }
  static bool Read(const Message* m, PickleIterator* iter, HANDLE* r) {
    return m->ReadIntPtr(iter, reinterpret_cast<intptr_t*>(r));
  }
  static void Log(const HANDLE& p, std::wstring* l) {
    l->append(StringPrintf(L"0x%X", p));
  }
};

template <>
struct ParamTraitsWindows<HWND> {
  static_assert(sizeof(HWND) == sizeof(intptr_t), "Wrong size for HWND?");

  static void Write(Message* m, HWND p) {
    m->WriteIntPtr(reinterpret_cast<intptr_t>(p));
  }
  static bool Read(const Message* m, PickleIterator* iter, HWND* r) {
    return m->ReadIntPtr(iter, reinterpret_cast<intptr_t*>(r));
  }
  static void Log(const HWND& p, std::wstring* l) {
    l->append(StringPrintf(L"0x%X", p));
  }
};
#endif  // defined(OS_WIN)

// Various ipc/chromium types.

template <class P>
struct ParamTraitsIPC : ParamTraitsWindows<P> {};

// `UniqueFileHandle` may be serialized over IPC channels. On the receiving
// side, the UniqueFileHandle is a valid duplicate of the handle which was
// transmitted.
//
// When sending a UniqueFileHandle, the handle must be valid at the time of
// transmission. As transmission is asynchronous, this requires passing
// ownership of the handle to IPC.
//
// A UniqueFileHandle may only be read once. After it has been read once, it
// will be consumed, and future reads will return an invalid handle.
template <>
struct ParamTraitsIPC<mozilla::UniqueFileHandle> {
  typedef mozilla::UniqueFileHandle param_type;
  static void Write(Message* m, param_type&& p) {
    const bool valid = p != nullptr;
    WriteParam(m, valid);
    if (valid) {
      if (!m->WriteFileHandle(std::move(p))) {
        NOTREACHED() << "Too many file handles for one message!";
      }
    }
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    bool valid;
    if (!ReadParam(m, iter, &valid)) {
      return false;
    }

    if (!valid) {
      *r = nullptr;
      return true;
    }

    return m->ConsumeFileHandle(iter, r);
  }
};

#if defined(OS_MACOSX)
// `UniqueMachSendRight` may be serialized over IPC channels. On the receiving
// side, the UniqueMachSendRight is the local name of the right which was
// transmitted.
//
// When sending a UniqueMachSendRight, the right must be valid at the time of
// transmission. As transmission is asynchronous, this requires passing
// ownership of the handle to IPC.
//
// A UniqueMachSendRight may only be read once. After it has been read once, it
// will be consumed, and future reads will return an invalid right.
template <>
struct ParamTraitsIPC<mozilla::UniqueMachSendRight> {
  typedef mozilla::UniqueMachSendRight param_type;
  static void Write(Message* m, param_type&& p) {
    const bool valid = p != nullptr;
    WriteParam(m, valid);
    if (valid) {
      if (!m->WriteMachSendRight(std::move(p))) {
        NOTREACHED() << "Too many mach send rights for one message!";
      }
    }
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    bool valid;
    if (!ReadParam(m, iter, &valid)) {
      return false;
    }

    if (!valid) {
      *r = nullptr;
      return true;
    }

    return m->ConsumeMachSendRight(iter, r);
  }
};
#endif

// Mozilla-specific types.

template <class P>
struct ParamTraitsMozilla : ParamTraitsIPC<P> {};

template <>
struct ParamTraitsMozilla<nsresult> {
  typedef nsresult param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteUInt32(static_cast<uint32_t>(p));
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadUInt32(iter, reinterpret_cast<uint32_t*>(r));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%u", static_cast<uint32_t>(p)));
  }
};

// See comments for the IPDLParamTraits specializations for RefPtr<T> and
// nsCOMPtr<T> for more details.
template <class T>
struct ParamTraitsMozilla<RefPtr<T>> {
  static void Write(Message* m, const RefPtr<T>& p) {
    ParamTraits<T*>::Write(m, p.get());
  }

  static bool Read(const Message* m, PickleIterator* iter, RefPtr<T>* r) {
    return ParamTraits<T*>::Read(m, iter, r);
  }
};

template <class T>
struct ParamTraitsMozilla<nsCOMPtr<T>> {
  static void Write(Message* m, const nsCOMPtr<T>& p) {
    ParamTraits<T*>::Write(m, p.get());
  }

  static bool Read(const Message* m, PickleIterator* iter, nsCOMPtr<T>* r) {
    RefPtr<T> refptr;
    if (!ParamTraits<T*>::Read(m, iter, &refptr)) {
      return false;
    }
    *r = std::move(refptr);
    return true;
  }
};

// Finally, ParamTraits itself.

template <class P>
struct ParamTraits : ParamTraitsMozilla<P> {};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_MESSAGE_UTILS_H_
