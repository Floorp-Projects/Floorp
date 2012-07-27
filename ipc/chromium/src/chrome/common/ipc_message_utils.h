// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_MESSAGE_UTILS_H_
#define CHROME_COMMON_IPC_MESSAGE_UTILS_H_

#include <string>
#include <vector>
#include <map>

#include "base/file_path.h"
#include "base/string_util.h"
#include "base/string16.h"
#include "base/tuple.h"

#if defined(OS_POSIX)
#include "chrome/common/file_descriptor_set_posix.h"
#endif
#include "chrome/common/ipc_sync_message.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/transport_dib.h"

namespace IPC {

//-----------------------------------------------------------------------------
// An iterator class for reading the fields contained within a Message.

class MessageIterator {
 public:
  explicit MessageIterator(const Message& m) : msg_(m), iter_(NULL) {
  }
  int NextInt() const {
    int val;
    if (!msg_.ReadInt(&iter_, &val))
      NOTREACHED();
    return val;
  }
  intptr_t NextIntPtr() const {
    intptr_t val;
    if (!msg_.ReadIntPtr(&iter_, &val))
      NOTREACHED();
    return val;
  }
  const std::string NextString() const {
    std::string val;
    if (!msg_.ReadString(&iter_, &val))
      NOTREACHED();
    return val;
  }
  const std::wstring NextWString() const {
    std::wstring val;
    if (!msg_.ReadWString(&iter_, &val))
      NOTREACHED();
    return val;
  }
  const void NextData(const char** data, int* length) const {
    if (!msg_.ReadData(&iter_, data, length)) {
      NOTREACHED();
    }
  }
 private:
  const Message& msg_;
  mutable void* iter_;
};

//-----------------------------------------------------------------------------
// ParamTraits specializations, etc.

template <class P> struct ParamTraits {};

template <class P>
static inline void WriteParam(Message* m, const P& p) {
  ParamTraits<P>::Write(m, p);
}

template <class P>
static inline bool WARN_UNUSED_RESULT ReadParam(const Message* m, void** iter,
                                                P* p) {
  return ParamTraits<P>::Read(m, iter, p);
}

template <class P>
static inline void LogParam(const P& p, std::wstring* l) {
  ParamTraits<P>::Log(p, l);
}

template <>
struct ParamTraits<bool> {
  typedef bool param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteBool(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadBool(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(p ? L"true" : L"false");
  }
};

template <>
struct ParamTraits<int16> {
  typedef int16 param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadInt16(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%hd", p));
  }
};

template <>
struct ParamTraits<uint16> {
  typedef uint16 param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadUInt16(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%hu", p));
  }
};

template <>
struct ParamTraits<int> {
  typedef int param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadInt(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%d", p));
  }
};

template <>
struct ParamTraits<long> {
  typedef long param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteLong(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadLong(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%l", p));
  }
};

template <>
struct ParamTraits<unsigned long> {
  typedef unsigned long param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteULong(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadULong(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%ul", p));
  }
};
#if (defined(OS_OPENBSD) && defined(ARCH_CPU_64_BITS))
// On OpenBSD, uint64_t is unsigned long long
// see https://bugzilla.mozilla.org/show_bug.cgi?id=648735#c27
template <>
struct ParamTraits<unsigned long long> {
  typedef unsigned long long param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteData(reinterpret_cast<const char*>(&p), sizeof(param_type));
 }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    bool result = m->ReadData(iter, &data, &data_size);
    if (result && data_size == sizeof(param_type)) {
      memcpy(r, data, sizeof(param_type));
    } else {
      result = false;
      NOTREACHED();
    }
    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%ull", p));
  }
};

template <>
struct ParamTraits<long long> {
  typedef long long param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteData(reinterpret_cast<const char*>(&p), sizeof(param_type));
 }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    bool result = m->ReadData(iter, &data, &data_size);
    if (result && data_size == sizeof(param_type)) {
      memcpy(r, data, sizeof(param_type));
    } else {
      result = false;
      NOTREACHED();
    }
    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%ll", p));
  }
};
#endif

#if !(defined(OS_MACOSX) || defined(OS_OPENBSD) || defined(OS_WIN) || (defined(OS_LINUX) && defined(ARCH_CPU_64_BITS)) || defined(ARCH_CPU_S390))
// There size_t is a synonym for |unsigned long| ...
template <>
struct ParamTraits<size_t> {
  typedef size_t param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteSize(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadSize(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%u", p));
  }
};

#elif !defined(OS_MACOSX)
// ... so we need to define traits for |unsigned int|.
// XXX duplicating OS_MACOSX version below so as not to conflict
template <>
struct ParamTraits<uint32> {
  typedef uint32 param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteUInt32(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadUInt32(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%u", p));
  }
};

#endif // if !(defined(OS_LINUX) && defined(ARCH_CPU_64_BITS))

#if defined(OS_MACOSX)
// On Linux size_t & uint32 can be the same type.
// TODO(playmobil): Fix compilation if this is not the case.
template <>
struct ParamTraits<uint32> {
  typedef uint32 param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteUInt32(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadUInt32(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%u", p));
  }
};
#endif  // defined(OS_MACOSX)

#if !(defined(OS_LINUX) && defined(ARCH_CPU_64_BITS))
// int64 is |long int| on 64-bit systems, uint64 is |unsigned long|
template <>
struct ParamTraits<int64> {
  typedef int64 param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt64(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadInt64(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%" PRId64L, p));
  }
};

template <>
struct ParamTraits<uint64> {
  typedef uint64 param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt64(static_cast<int64>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadInt64(iter, reinterpret_cast<int64*>(r));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%" PRIu64L, p));
  }
};
#endif // if !(defined(OS_LINUX) && defined(ARCH_CPU_64_BITS))

template <>
struct ParamTraits<double> {
  typedef double param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteData(reinterpret_cast<const char*>(&p), sizeof(param_type));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    bool result = m->ReadData(iter, &data, &data_size);
    if (result && data_size == sizeof(param_type)) {
      memcpy(r, data, sizeof(param_type));
    } else {
      result = false;
      NOTREACHED();
    }

    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"e", p));
  }
};

template <>
struct ParamTraits<base::Time> {
  typedef base::Time param_type;
  static void Write(Message* m, const param_type& p) {
    ParamTraits<int64>::Write(m, p.ToInternalValue());
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int64 value;
    if (!ParamTraits<int64>::Read(m, iter, &value))
      return false;
    *r = base::Time::FromInternalValue(value);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    ParamTraits<int64>::Log(p.ToInternalValue(), l);
  }
};

#if defined(OS_WIN)
template <>
struct ParamTraits<LOGFONT> {
  typedef LOGFONT param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteData(reinterpret_cast<const char*>(&p), sizeof(LOGFONT));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    bool result = m->ReadData(iter, &data, &data_size);
    if (result && data_size == sizeof(LOGFONT)) {
      memcpy(r, data, sizeof(LOGFONT));
    } else {
      result = false;
      NOTREACHED();
    }

    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"<LOGFONT>"));
  }
};

template <>
struct ParamTraits<MSG> {
  typedef MSG param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteData(reinterpret_cast<const char*>(&p), sizeof(MSG));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    bool result = m->ReadData(iter, &data, &data_size);
    if (result && data_size == sizeof(MSG)) {
      memcpy(r, data, sizeof(MSG));
    } else {
      result = false;
      NOTREACHED();
    }

    return result;
  }
};
#endif  // defined(OS_WIN)

template <>
struct ParamTraits<std::string> {
  typedef std::string param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteString(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadString(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(UTF8ToWide(p));
  }
};

template <>
struct ParamTraits<std::vector<unsigned char> > {
  typedef std::vector<unsigned char> param_type;
  static void Write(Message* m, const param_type& p) {
    if (p.size() == 0) {
      m->WriteData(NULL, 0);
    } else {
      m->WriteData(reinterpret_cast<const char*>(&p.front()),
                   static_cast<int>(p.size()));
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    if (!m->ReadData(iter, &data, &data_size) || data_size < 0)
      return false;
    r->resize(data_size);
    if (data_size)
      memcpy(&r->front(), data, data_size);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    for (size_t i = 0; i < p.size(); ++i)
      l->push_back(p[i]);
  }
};

template <>
struct ParamTraits<std::vector<char> > {
  typedef std::vector<char> param_type;
  static void Write(Message* m, const param_type& p) {
    if (p.size() == 0) {
      m->WriteData(NULL, 0);
    } else {
      m->WriteData(&p.front(), static_cast<int>(p.size()));
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    if (!m->ReadData(iter, &data, &data_size) || data_size < 0)
      return false;
    r->resize(data_size);
    if (data_size)
      memcpy(&r->front(), data, data_size);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    for (size_t i = 0; i < p.size(); ++i)
      l->push_back(p[i]);
  }
};

template <class P>
struct ParamTraits<std::vector<P> > {
  typedef std::vector<P> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.size()));
    for (size_t i = 0; i < p.size(); i++)
      WriteParam(m, p[i]);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int size;
    if (!m->ReadLength(iter, &size))
      return false;
    // Resizing beforehand is not safe, see BUG 1006367 for details.
    if (m->IteratorHasRoomFor(*iter, size * sizeof(P))) {
      r->resize(size);
      for (int i = 0; i < size; i++) {
        if (!ReadParam(m, iter, &(*r)[i]))
          return false;
      }
    } else {
      for (int i = 0; i < size; i++) {
        P element;
        if (!ReadParam(m, iter, &element))
          return false;
        r->push_back(element);
      }
    }
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    for (size_t i = 0; i < p.size(); ++i) {
      if (i != 0)
        l->append(L" ");

      LogParam((p[i]), l);
    }
  }
};

template <class K, class V>
struct ParamTraits<std::map<K, V> > {
  typedef std::map<K, V> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.size()));
    typename param_type::const_iterator iter;
    for (iter = p.begin(); iter != p.end(); ++iter) {
      WriteParam(m, iter->first);
      WriteParam(m, iter->second);
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int size;
    if (!ReadParam(m, iter, &size) || size < 0)
      return false;
    for (int i = 0; i < size; ++i) {
      K k;
      if (!ReadParam(m, iter, &k))
        return false;
      V& value = (*r)[k];
      if (!ReadParam(m, iter, &value))
        return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<std::map>");
  }
};


template <>
struct ParamTraits<std::wstring> {
  typedef std::wstring param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteWString(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadWString(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(p);
  }
};

// If WCHAR_T_IS_UTF16 is defined, then string16 is a std::wstring so we don't
// need this trait.
#if !defined(WCHAR_T_IS_UTF16)
template <>
struct ParamTraits<string16> {
  typedef string16 param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteString16(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadString16(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(UTF16ToWide(p));
  }
};
#endif

// and, a few more useful types...
#if defined(OS_WIN)
template <>
struct ParamTraits<HANDLE> {
  typedef HANDLE param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteIntPtr(reinterpret_cast<intptr_t>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    DCHECK_EQ(sizeof(param_type), sizeof(intptr_t));
    return m->ReadIntPtr(iter, reinterpret_cast<intptr_t*>(r));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"0x%X", p));
  }
};

template <>
struct ParamTraits<HCURSOR> {
  typedef HCURSOR param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteIntPtr(reinterpret_cast<intptr_t>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    DCHECK_EQ(sizeof(param_type), sizeof(intptr_t));
    return m->ReadIntPtr(iter, reinterpret_cast<intptr_t*>(r));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"0x%X", p));
  }
};

template <>
struct ParamTraits<HWND> {
  typedef HWND param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteIntPtr(reinterpret_cast<intptr_t>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    DCHECK_EQ(sizeof(param_type), sizeof(intptr_t));
    return m->ReadIntPtr(iter, reinterpret_cast<intptr_t*>(r));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"0x%X", p));
  }
};

template <>
struct ParamTraits<HACCEL> {
  typedef HACCEL param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteIntPtr(reinterpret_cast<intptr_t>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    DCHECK_EQ(sizeof(param_type), sizeof(intptr_t));
    return m->ReadIntPtr(iter, reinterpret_cast<intptr_t*>(r));
  }
};

template <>
struct ParamTraits<POINT> {
  typedef POINT param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p.x);
    m->WriteInt(p.y);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int x, y;
    if (!m->ReadInt(iter, &x) || !m->ReadInt(iter, &y))
      return false;
    r->x = x;
    r->y = y;
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"(%d, %d)", p.x, p.y));
  }
};
#endif  // defined(OS_WIN)

template <>
struct ParamTraits<FilePath> {
  typedef FilePath param_type;
  static void Write(Message* m, const param_type& p) {
    ParamTraits<FilePath::StringType>::Write(m, p.value());
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    FilePath::StringType value;
    if (!ParamTraits<FilePath::StringType>::Read(m, iter, &value))
      return false;
    *r = FilePath(value);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    ParamTraits<FilePath::StringType>::Log(p.value(), l);
  }
};

#if defined(OS_POSIX)
// FileDescriptors may be serialised over IPC channels on POSIX. On the
// receiving side, the FileDescriptor is a valid duplicate of the file
// descriptor which was transmitted: *it is not just a copy of the integer like
// HANDLEs on Windows*. The only exception is if the file descriptor is < 0. In
// this case, the receiving end will see a value of -1. *Zero is a valid file
// descriptor*.
//
// The received file descriptor will have the |auto_close| flag set to true. The
// code which handles the message is responsible for taking ownership of it.
// File descriptors are OS resources and must be closed when no longer needed.
//
// When sending a file descriptor, the file descriptor must be valid at the time
// of transmission. Since transmission is not synchronous, one should consider
// dup()ing any file descriptors to be transmitted and setting the |auto_close|
// flag, which causes the file descriptor to be closed after writing.
template<>
struct ParamTraits<base::FileDescriptor> {
  typedef base::FileDescriptor param_type;
  static void Write(Message* m, const param_type& p) {
    const bool valid = p.fd >= 0;
    WriteParam(m, valid);

    if (valid) {
      if (!m->WriteFileDescriptor(p))
        NOTREACHED();
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    bool valid;
    if (!ReadParam(m, iter, &valid))
      return false;

    if (!valid) {
      r->fd = -1;
      r->auto_close = false;
      return true;
    }

    return m->ReadFileDescriptor(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    if (p.auto_close) {
      l->append(StringPrintf(L"FD(%d auto-close)", p.fd));
    } else {
      l->append(StringPrintf(L"FD(%d)", p.fd));
    }
  }
};
#endif // defined(OS_POSIX)

template<>
struct ParamTraits<ThumbnailScore> {
  typedef ThumbnailScore param_type;
  static void Write(Message* m, const param_type& p) {
    IPC::ParamTraits<double>::Write(m, p.boring_score);
    IPC::ParamTraits<bool>::Write(m, p.good_clipping);
    IPC::ParamTraits<bool>::Write(m, p.at_top);
    IPC::ParamTraits<base::Time>::Write(m, p.time_at_snapshot);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    double boring_score;
    bool good_clipping, at_top;
    base::Time time_at_snapshot;
    if (!IPC::ParamTraits<double>::Read(m, iter, &boring_score) ||
        !IPC::ParamTraits<bool>::Read(m, iter, &good_clipping) ||
        !IPC::ParamTraits<bool>::Read(m, iter, &at_top) ||
        !IPC::ParamTraits<base::Time>::Read(m, iter, &time_at_snapshot))
      return false;

    r->boring_score = boring_score;
    r->good_clipping = good_clipping;
    r->at_top = at_top;
    r->time_at_snapshot = time_at_snapshot;
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"(%f, %d, %d)",
                           p.boring_score, p.good_clipping, p.at_top));
  }
};

#if defined(OS_WIN)
template <>
struct ParamTraits<XFORM> {
  typedef XFORM param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteData(reinterpret_cast<const char*>(&p), sizeof(XFORM));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    bool result = m->ReadData(iter, &data, &data_size);
    if (result && data_size == sizeof(XFORM)) {
      memcpy(r, data, sizeof(XFORM));
    } else {
      result = false;
      NOTREACHED();
    }

    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<XFORM>");
  }
};
#endif  // defined(OS_WIN)

struct LogData {
  std::wstring channel;
  int32 routing_id;
  uint16 type;
  std::wstring flags;
  int64 sent;  // Time that the message was sent (i.e. at Send()).
  int64 receive;  // Time before it was dispatched (i.e. before calling
                  // OnMessageReceived).
  int64 dispatch;  // Time after it was dispatched (i.e. after calling
                   // OnMessageReceived).
  std::wstring message_name;
  std::wstring params;
};

template <>
struct ParamTraits<LogData> {
  typedef LogData param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.channel);
    WriteParam(m, p.routing_id);
    WriteParam(m, static_cast<int>(p.type));
    WriteParam(m, p.flags);
    WriteParam(m, p.sent);
    WriteParam(m, p.receive);
    WriteParam(m, p.dispatch);
    WriteParam(m, p.params);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int type = 0;
    bool result =
      ReadParam(m, iter, &r->channel) &&
      ReadParam(m, iter, &r->routing_id) &&
      ReadParam(m, iter, &type) &&
      ReadParam(m, iter, &r->flags) &&
      ReadParam(m, iter, &r->sent) &&
      ReadParam(m, iter, &r->receive) &&
      ReadParam(m, iter, &r->dispatch) &&
      ReadParam(m, iter, &r->params);
    r->type = static_cast<uint16>(type);
    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    // Doesn't make sense to implement this!
  }
};

#if defined(OS_WIN)
template<>
struct ParamTraits<TransportDIB::Id> {
  typedef TransportDIB::Id param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.handle);
    WriteParam(m, p.sequence_num);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return (ReadParam(m, iter, &r->handle) &&
            ReadParam(m, iter, &r->sequence_num));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"TransportDIB(");
    LogParam(p.handle, l);
    l->append(L", ");
    LogParam(p.sequence_num, l);
    l->append(L")");
  }
};
#endif

template <>
struct ParamTraits<Message> {
  static void Write(Message* m, const Message& p) {
    m->WriteInt(p.size());
    m->WriteData(reinterpret_cast<const char*>(p.data()), p.size());
  }
  static bool Read(const Message* m, void** iter, Message* r) {
    int size;
    if (!m->ReadInt(iter, &size))
      return false;
    const char* data;
    if (!m->ReadData(iter, &data, &size))
      return false;
    *r = Message(data, size);
    return true;
  }
  static void Log(const Message& p, std::wstring* l) {
    l->append(L"<IPC::Message>");
  }
};

template <>
struct ParamTraits<Tuple0> {
  typedef Tuple0 param_type;
  static void Write(Message* m, const param_type& p) {
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
  }
};

template <class A>
struct ParamTraits< Tuple1<A> > {
  typedef Tuple1<A> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return ReadParam(m, iter, &r->a);
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.a, l);
  }
};

template <class A, class B>
struct ParamTraits< Tuple2<A, B> > {
  typedef Tuple2<A, B> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
    WriteParam(m, p.b);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return (ReadParam(m, iter, &r->a) &&
            ReadParam(m, iter, &r->b));
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.a, l);
    l->append(L", ");
    LogParam(p.b, l);
  }
};

template <class A, class B, class C>
struct ParamTraits< Tuple3<A, B, C> > {
  typedef Tuple3<A, B, C> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
    WriteParam(m, p.b);
    WriteParam(m, p.c);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return (ReadParam(m, iter, &r->a) &&
            ReadParam(m, iter, &r->b) &&
            ReadParam(m, iter, &r->c));
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.a, l);
    l->append(L", ");
    LogParam(p.b, l);
    l->append(L", ");
    LogParam(p.c, l);
  }
};

template <class A, class B, class C, class D>
struct ParamTraits< Tuple4<A, B, C, D> > {
  typedef Tuple4<A, B, C, D> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
    WriteParam(m, p.b);
    WriteParam(m, p.c);
    WriteParam(m, p.d);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return (ReadParam(m, iter, &r->a) &&
            ReadParam(m, iter, &r->b) &&
            ReadParam(m, iter, &r->c) &&
            ReadParam(m, iter, &r->d));
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.a, l);
    l->append(L", ");
    LogParam(p.b, l);
    l->append(L", ");
    LogParam(p.c, l);
    l->append(L", ");
    LogParam(p.d, l);
  }
};

template <class A, class B, class C, class D, class E>
struct ParamTraits< Tuple5<A, B, C, D, E> > {
  typedef Tuple5<A, B, C, D, E> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
    WriteParam(m, p.b);
    WriteParam(m, p.c);
    WriteParam(m, p.d);
    WriteParam(m, p.e);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return (ReadParam(m, iter, &r->a) &&
            ReadParam(m, iter, &r->b) &&
            ReadParam(m, iter, &r->c) &&
            ReadParam(m, iter, &r->d) &&
            ReadParam(m, iter, &r->e));
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.a, l);
    l->append(L", ");
    LogParam(p.b, l);
    l->append(L", ");
    LogParam(p.c, l);
    l->append(L", ");
    LogParam(p.d, l);
    l->append(L", ");
    LogParam(p.e, l);
  }
};

template <class A, class B, class C, class D, class E, class F>
struct ParamTraits< Tuple6<A, B, C, D, E, F> > {
  typedef Tuple6<A, B, C, D, E, F> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
    WriteParam(m, p.b);
    WriteParam(m, p.c);
    WriteParam(m, p.d);
    WriteParam(m, p.e);
    WriteParam(m, p.f);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return (ReadParam(m, iter, &r->a) &&
            ReadParam(m, iter, &r->b) &&
            ReadParam(m, iter, &r->c) &&
            ReadParam(m, iter, &r->d) &&
            ReadParam(m, iter, &r->e) &&
            ReadParam(m, iter, &r->f));
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.a, l);
    l->append(L", ");
    LogParam(p.b, l);
    l->append(L", ");
    LogParam(p.c, l);
    l->append(L", ");
    LogParam(p.d, l);
    l->append(L", ");
    LogParam(p.e, l);
    l->append(L", ");
    LogParam(p.f, l);
  }
};



//-----------------------------------------------------------------------------
// Generic message subclasses

// Used for asynchronous messages.
template <class ParamType>
class MessageWithTuple : public Message {
 public:
  typedef ParamType Param;

  MessageWithTuple(int32 routing_id, uint16 type, const Param& p)
      : Message(routing_id, type, PRIORITY_NORMAL) {
    WriteParam(this, p);
  }

  static bool Read(const Message* msg, Param* p) {
    void* iter = NULL;
    bool rv = ReadParam(msg, &iter, p);
    DCHECK(rv) << "Error deserializing message " << msg->type();
    return rv;
  }

  // Generic dispatcher.  Should cover most cases.
  template<class T, class Method>
  static bool Dispatch(const Message* msg, T* obj, Method func) {
    Param p;
    if (Read(msg, &p)) {
      DispatchToMethod(obj, func, p);
      return true;
    }
    return false;
  }

  // The following dispatchers exist for the case where the callback function
  // needs the message as well.  They assume that "Param" is a type of Tuple
  // (except the one arg case, as there is no Tuple1).
  template<class T, typename TA>
  static bool Dispatch(const Message* msg, T* obj,
                       void (T::*func)(const Message&, TA)) {
    Param p;
    if (Read(msg, &p)) {
      (obj->*func)(*msg, p);
      return true;
    }
    return false;
  }

  template<class T, typename TA, typename TB>
  static bool Dispatch(const Message* msg, T* obj,
                       void (T::*func)(const Message&, TA, TB)) {
    Param p;
    if (Read(msg, &p)) {
      (obj->*func)(*msg, p.a, p.b);
      return true;
    }
    return false;
  }

  template<class T, typename TA, typename TB, typename TC>
  static bool Dispatch(const Message* msg, T* obj,
                       void (T::*func)(const Message&, TA, TB, TC)) {
    Param p;
    if (Read(msg, &p)) {
      (obj->*func)(*msg, p.a, p.b, p.c);
      return true;
    }
    return false;
  }

  template<class T, typename TA, typename TB, typename TC, typename TD>
  static bool Dispatch(const Message* msg, T* obj,
                       void (T::*func)(const Message&, TA, TB, TC, TD)) {
    Param p;
    if (Read(msg, &p)) {
      (obj->*func)(*msg, p.a, p.b, p.c, p.d);
      return true;
    }
    return false;
  }

  template<class T, typename TA, typename TB, typename TC, typename TD,
           typename TE>
  static bool Dispatch(const Message* msg, T* obj,
                       void (T::*func)(const Message&, TA, TB, TC, TD, TE)) {
    Param p;
    if (Read(msg, &p)) {
      (obj->*func)(*msg, p.a, p.b, p.c, p.d, p.e);
      return true;
    }
    return false;
  }

  static void Log(const Message* msg, std::wstring* l) {
    Param p;
    if (Read(msg, &p))
      LogParam(p, l);
  }

  // Functions used to do manual unpacking.  Only used by the automation code,
  // these should go away once that code uses SyncChannel.
  template<typename TA, typename TB>
  static bool Read(const IPC::Message* msg, TA* a, TB* b) {
    ParamType params;
    if (!Read(msg, &params))
      return false;
    *a = params.a;
    *b = params.b;
    return true;
  }

  template<typename TA, typename TB, typename TC>
  static bool Read(const IPC::Message* msg, TA* a, TB* b, TC* c) {
    ParamType params;
    if (!Read(msg, &params))
      return false;
    *a = params.a;
    *b = params.b;
    *c = params.c;
    return true;
  }

  template<typename TA, typename TB, typename TC, typename TD>
  static bool Read(const IPC::Message* msg, TA* a, TB* b, TC* c, TD* d) {
    ParamType params;
    if (!Read(msg, &params))
      return false;
    *a = params.a;
    *b = params.b;
    *c = params.c;
    *d = params.d;
    return true;
  }

  template<typename TA, typename TB, typename TC, typename TD, typename TE>
  static bool Read(const IPC::Message* msg, TA* a, TB* b, TC* c, TD* d, TE* e) {
    ParamType params;
    if (!Read(msg, &params))
      return false;
    *a = params.a;
    *b = params.b;
    *c = params.c;
    *d = params.d;
    *e = params.e;
    return true;
  }
};

// This class assumes that its template argument is a RefTuple (a Tuple with
// reference elements).
template <class RefTuple>
class ParamDeserializer : public MessageReplyDeserializer {
 public:
  explicit ParamDeserializer(const RefTuple& out) : out_(out) { }

  bool SerializeOutputParameters(const IPC::Message& msg, void* iter) {
    return ReadParam(&msg, &iter, &out_);
  }

  RefTuple out_;
};

// defined in ipc_logging.cc
void GenerateLogData(const std::wstring& channel, const Message& message,
                     LogData* data);

// Used for synchronous messages.
template <class SendParamType, class ReplyParamType>
class MessageWithReply : public SyncMessage {
 public:
  typedef SendParamType SendParam;
  typedef ReplyParamType ReplyParam;

  MessageWithReply(int32 routing_id, uint16 type,
                   const SendParam& send, const ReplyParam& reply)
      : SyncMessage(routing_id, type, PRIORITY_NORMAL,
                    new ParamDeserializer<ReplyParam>(reply)) {
    WriteParam(this, send);
  }

  static void Log(const Message* msg, std::wstring* l) {
    if (msg->is_sync()) {
      SendParam p;
      void* iter = SyncMessage::GetDataIterator(msg);
      if (ReadParam(msg, &iter, &p))
        LogParam(p, l);

#if defined(IPC_MESSAGE_LOG_ENABLED)
      const std::wstring& output_params = msg->output_params();
      if (!l->empty() && !output_params.empty())
        l->append(L", ");

      l->append(output_params);
#endif
    } else {
      // This is an outgoing reply.  Now that we have the output parameters, we
      // can finally log the message.
      typename ReplyParam::ValueTuple p;
      void* iter = SyncMessage::GetDataIterator(msg);
      if (ReadParam(msg, &iter, &p))
        LogParam(p, l);
    }
  }

  template<class T, class Method>
  static bool Dispatch(const Message* msg, T* obj, Method func) {
    SendParam send_params;
    void* iter = GetDataIterator(msg);
    Message* reply = GenerateReply(msg);
    bool error;
    if (ReadParam(msg, &iter, &send_params)) {
      typename ReplyParam::ValueTuple reply_params;
      DispatchToMethod(obj, func, send_params, &reply_params);
      WriteParam(reply, reply_params);
      error = false;
#ifdef IPC_MESSAGE_LOG_ENABLED
      if (msg->received_time() != 0) {
        std::wstring output_params;
        LogParam(reply_params, &output_params);
        msg->set_output_params(output_params);
      }
#endif
    } else {
      NOTREACHED() << "Error deserializing message " << msg->type();
      reply->set_reply_error();
      error = true;
    }

    obj->Send(reply);
    return !error;
  }

  template<class T, class Method>
  static bool DispatchDelayReply(const Message* msg, T* obj, Method func) {
    SendParam send_params;
    void* iter = GetDataIterator(msg);
    Message* reply = GenerateReply(msg);
    bool error;
    if (ReadParam(msg, &iter, &send_params)) {
      Tuple1<Message&> t = MakeRefTuple(*reply);

#ifdef IPC_MESSAGE_LOG_ENABLED
      if (msg->sent_time()) {
        // Don't log the sync message after dispatch, as we don't have the
        // output parameters at that point.  Instead, save its data and log it
        // with the outgoing reply message when it's sent.
        LogData* data = new LogData;
        GenerateLogData(L"", *msg, data);
        msg->set_dont_log();
        reply->set_sync_log_data(data);
      }
#endif
      DispatchToMethod(obj, func, send_params, &t);
      error = false;
    } else {
      NOTREACHED() << "Error deserializing message " << msg->type();
      reply->set_reply_error();
      obj->Send(reply);
      error = true;
    }
    return !error;
  }

  template<typename TA>
  static void WriteReplyParams(Message* reply, TA a) {
    ReplyParam p(a);
    WriteParam(reply, p);
  }

  template<typename TA, typename TB>
  static void WriteReplyParams(Message* reply, TA a, TB b) {
    ReplyParam p(a, b);
    WriteParam(reply, p);
  }

  template<typename TA, typename TB, typename TC>
  static void WriteReplyParams(Message* reply, TA a, TB b, TC c) {
    ReplyParam p(a, b, c);
    WriteParam(reply, p);
  }

  template<typename TA, typename TB, typename TC, typename TD>
  static void WriteReplyParams(Message* reply, TA a, TB b, TC c, TD d) {
    ReplyParam p(a, b, c, d);
    WriteParam(reply, p);
  }

  template<typename TA, typename TB, typename TC, typename TD, typename TE>
  static void WriteReplyParams(Message* reply, TA a, TB b, TC c, TD d, TE e) {
    ReplyParam p(a, b, c, d, e);
    WriteParam(reply, p);
  }
};

//-----------------------------------------------------------------------------

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_MESSAGE_UTILS_H_
