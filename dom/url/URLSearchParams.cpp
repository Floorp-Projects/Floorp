/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLSearchParams.h"
#include "mozilla/dom/URLSearchParamsBinding.h"
#include "mozilla/Encoding.h"
#include "nsDOMString.h"
#include "nsIInputStream.h"
#include "nsStringStream.h"

namespace mozilla {
namespace dom {

bool
URLParams::Has(const nsAString& aName)
{
  for (uint32_t i = 0, len = mParams.Length(); i < len; ++i) {
    if (mParams[i].mKey.Equals(aName)) {
      return true;
    }
  }

  return false;
}

void
URLParams::Get(const nsAString& aName, nsString& aRetval)
{
  SetDOMStringToNull(aRetval);

  for (uint32_t i = 0, len = mParams.Length(); i < len; ++i) {
    if (mParams[i].mKey.Equals(aName)) {
      aRetval.Assign(mParams[i].mValue);
      break;
    }
  }
}

void
URLParams::GetAll(const nsAString& aName, nsTArray<nsString>& aRetval)
{
  aRetval.Clear();

  for (uint32_t i = 0, len = mParams.Length(); i < len; ++i) {
    if (mParams[i].mKey.Equals(aName)) {
      aRetval.AppendElement(mParams[i].mValue);
    }
  }
}

void
URLParams::Append(const nsAString& aName, const nsAString& aValue)
{
  Param* param = mParams.AppendElement();
  param->mKey = aName;
  param->mValue = aValue;
}

void
URLParams::Set(const nsAString& aName, const nsAString& aValue)
{
  Param* param = nullptr;
  for (uint32_t i = 0, len = mParams.Length(); i < len;) {
    if (!mParams[i].mKey.Equals(aName)) {
      ++i;
      continue;
    }
    if (!param) {
      param = &mParams[i];
      ++i;
      continue;
    }
    // Remove duplicates.
    mParams.RemoveElementAt(i);
    --len;
  }

  if (!param) {
    param = mParams.AppendElement();
    param->mKey = aName;
  }

  param->mValue = aValue;
}

void
URLParams::Delete(const nsAString& aName)
{
  for (uint32_t i = 0; i < mParams.Length();) {
    if (mParams[i].mKey.Equals(aName)) {
      mParams.RemoveElementAt(i);
    } else {
      ++i;
    }
  }
}

/* static */ void
URLParams::ConvertString(const nsACString& aInput, nsAString& aOutput)
{
  if (NS_FAILED(UTF_8_ENCODING->DecodeWithoutBOMHandling(aInput, aOutput))) {
    MOZ_CRASH("Out of memory when converting URL params.");
  }
}

/* static */ void
URLParams::DecodeString(const nsACString& aInput, nsAString& aOutput)
{
  nsACString::const_iterator start, end;
  aInput.BeginReading(start);
  aInput.EndReading(end);

  nsCString unescaped;

  while (start != end) {
    // replace '+' with U+0020
    if (*start == '+') {
      unescaped.Append(' ');
      ++start;
      continue;
    }

    // Percent decode algorithm
    if (*start == '%') {
      nsACString::const_iterator first(start);
      ++first;

      nsACString::const_iterator second(first);
      ++second;

#define ASCII_HEX_DIGIT( x )    \
  ((x >= 0x41 && x <= 0x46) ||  \
   (x >= 0x61 && x <= 0x66) ||  \
   (x >= 0x30 && x <= 0x39))

#define HEX_DIGIT( x )              \
   (*x >= 0x30 && *x <= 0x39        \
     ? *x - 0x30                    \
     : (*x >= 0x41 && *x <= 0x46    \
        ? *x - 0x37                 \
        : *x - 0x57))

      if (first != end && second != end &&
          ASCII_HEX_DIGIT(*first) && ASCII_HEX_DIGIT(*second)) {
        unescaped.Append(HEX_DIGIT(first) * 16 + HEX_DIGIT(second));
        start = ++second;
        continue;

      } else {
        unescaped.Append('%');
        ++start;
        continue;
      }
    }

    unescaped.Append(*start);
    ++start;
  }

  ConvertString(unescaped, aOutput);
}

/* static */ bool
URLParams::Parse(const nsACString& aInput, ForEachIterator& aIterator)
{
  nsACString::const_iterator start, end;
  aInput.BeginReading(start);
  aInput.EndReading(end);
  nsACString::const_iterator iter(start);

  while (start != end) {
    nsAutoCString string;

    if (FindCharInReadable('&', iter, end)) {
      string.Assign(Substring(start, iter));
      start = ++iter;
    } else {
      string.Assign(Substring(start, end));
      start = end;
    }

    if (string.IsEmpty()) {
      continue;
    }

    nsACString::const_iterator eqStart, eqEnd;
    string.BeginReading(eqStart);
    string.EndReading(eqEnd);
    nsACString::const_iterator eqIter(eqStart);

    nsAutoCString name;
    nsAutoCString value;

    if (FindCharInReadable('=', eqIter, eqEnd)) {
      name.Assign(Substring(eqStart, eqIter));

      ++eqIter;
      value.Assign(Substring(eqIter, eqEnd));
    } else {
      name.Assign(string);
    }

    nsAutoString decodedName;
    DecodeString(name, decodedName);

    nsAutoString decodedValue;
    DecodeString(value, decodedValue);

    if (!aIterator.URLParamsIterator(decodedName, decodedValue)) {
      return false;
    }
  }
  return true;
}

class MOZ_STACK_CLASS PopulateIterator final
  : public URLParams::ForEachIterator
{
public:
  explicit PopulateIterator(URLParams* aParams)
    : mParams(aParams)
  {
    MOZ_ASSERT(aParams);
  }

  bool URLParamsIterator(const nsAString& aName,
                         const nsAString& aValue) override
  {
    mParams->Append(aName, aValue);
    return true;
  }

private:
  URLParams* mParams;
};

void
URLParams::ParseInput(const nsACString& aInput)
{
  // Remove all the existing data before parsing a new input.
  DeleteAll();

  PopulateIterator iter(this);
  URLParams::Parse(aInput, iter);
}

namespace {

void SerializeString(const nsCString& aInput, nsAString& aValue)
{
  const unsigned char* p = (const unsigned char*) aInput.get();
  const unsigned char* end = p + aInput.Length();

  while (p != end) {
    // ' ' to '+'
    if (*p == 0x20) {
      aValue.Append(0x2B);
    // Percent Encode algorithm
    } else if (*p == 0x2A || *p == 0x2D || *p == 0x2E ||
               (*p >= 0x30 && *p <= 0x39) ||
               (*p >= 0x41 && *p <= 0x5A) || *p == 0x5F ||
               (*p >= 0x61 && *p <= 0x7A)) {
      aValue.Append(*p);
    } else {
      aValue.AppendPrintf("%%%.2X", *p);
    }

    ++p;
  }
}

} // namespace

void
URLParams::Serialize(nsAString& aValue) const
{
  aValue.Truncate();
  bool first = true;

  for (uint32_t i = 0, len = mParams.Length(); i < len; ++i) {
    if (first) {
      first = false;
    } else {
      aValue.Append('&');
    }

    SerializeString(NS_ConvertUTF16toUTF8(mParams[i].mKey), aValue);
    aValue.Append('=');
    SerializeString(NS_ConvertUTF16toUTF8(mParams[i].mValue), aValue);
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(URLSearchParams, mParent, mObserver)
NS_IMPL_CYCLE_COLLECTING_ADDREF(URLSearchParams)
NS_IMPL_CYCLE_COLLECTING_RELEASE(URLSearchParams)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(URLSearchParams)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIXHRSendable)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

URLSearchParams::URLSearchParams(nsISupports* aParent,
                                 URLSearchParamsObserver* aObserver)
  : mParams(new URLParams())
  , mParent(aParent)
  , mObserver(aObserver)
{
}

URLSearchParams::~URLSearchParams()
{
  DeleteAll();
}

JSObject*
URLSearchParams::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return URLSearchParamsBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<URLSearchParams>
URLSearchParams::Constructor(const GlobalObject& aGlobal,
                             const USVStringSequenceSequenceOrUSVStringUSVStringRecordOrUSVString& aInit,
                             ErrorResult& aRv)
{
  RefPtr<URLSearchParams> sp =
    new URLSearchParams(aGlobal.GetAsSupports(), nullptr);

  if (aInit.IsUSVString()) {
    NS_ConvertUTF16toUTF8 input(aInit.GetAsUSVString());
    if (StringBeginsWith(input, NS_LITERAL_CSTRING("?"))) {
      sp->ParseInput(Substring(input, 1, input.Length() - 1));
    } else {
      sp->ParseInput(input);
    }
  } else if (aInit.IsUSVStringSequenceSequence()) {
    const Sequence<Sequence<nsString>>& list =
      aInit.GetAsUSVStringSequenceSequence();
    for (uint32_t i = 0; i < list.Length(); ++i) {
      const Sequence<nsString>& item = list[i];
      if (item.Length() != 2) {
        aRv.Throw(NS_ERROR_DOM_TYPE_ERR);
        return nullptr;
      }
      sp->Append(item[0], item[1]);
    }
  } else if (aInit.IsUSVStringUSVStringRecord()) {
    const Record<nsString, nsString>& record =
      aInit.GetAsUSVStringUSVStringRecord();
    for (auto& entry : record.Entries()) {
      sp->Append(entry.mKey, entry.mValue);
    }
  } else {
    MOZ_CRASH("This should not happen.");
  }

  return sp.forget();
}

void
URLSearchParams::ParseInput(const nsACString& aInput)
{
  mParams->ParseInput(aInput);
}

void
URLSearchParams::Get(const nsAString& aName, nsString& aRetval)
{
  return mParams->Get(aName, aRetval);
}

void
URLSearchParams::GetAll(const nsAString& aName, nsTArray<nsString>& aRetval)
{
  return mParams->GetAll(aName, aRetval);
}

void
URLSearchParams::Set(const nsAString& aName, const nsAString& aValue)
{
  mParams->Set(aName, aValue);
  NotifyObserver();
}

void
URLSearchParams::Append(const nsAString& aName, const nsAString& aValue)
{
  mParams->Append(aName, aValue);
  NotifyObserver();
}

bool
URLSearchParams::Has(const nsAString& aName)
{
  return mParams->Has(aName);
}

void
URLSearchParams::Delete(const nsAString& aName)
{
  mParams->Delete(aName);
  NotifyObserver();
}

void
URLSearchParams::DeleteAll()
{
  mParams->DeleteAll();
}

void
URLSearchParams::Serialize(nsAString& aValue) const
{
  mParams->Serialize(aValue);
}

void
URLSearchParams::NotifyObserver()
{
  if (mObserver) {
    mObserver->URLSearchParamsUpdated(this);
  }
}

uint32_t
URLSearchParams::GetIterableLength() const
{
  return mParams->Length();
}

const nsAString&
URLSearchParams::GetKeyAtIndex(uint32_t aIndex) const
{
  return mParams->GetKeyAtIndex(aIndex);
}

const nsAString&
URLSearchParams::GetValueAtIndex(uint32_t aIndex) const
{
  return mParams->GetValueAtIndex(aIndex);
}

void
URLSearchParams::Sort(ErrorResult& aRv)
{
  aRv = mParams->Sort();
  if (!aRv.Failed()) {
    NotifyObserver();
  }
}

// Helper functions for structured cloning
inline bool
ReadString(JSStructuredCloneReader* aReader, nsString& aString)
{
  MOZ_ASSERT(aReader);

  bool read;
  uint32_t nameLength, zero;
  read = JS_ReadUint32Pair(aReader, &nameLength, &zero);
  if (!read) {
    return false;
  }
  MOZ_ASSERT(zero == 0);
  aString.SetLength(nameLength);
  size_t charSize = sizeof(nsString::char_type);
  read = JS_ReadBytes(aReader, (void*) aString.BeginWriting(),
                      nameLength * charSize);
  if (!read) {
    return false;
  }

  return true;
}

nsresult
URLParams::Sort()
{
  // Unfortunately we cannot use nsTArray<>.Sort() because it doesn't keep the
  // correct order of the values for equal keys.

  // Let's sort the keys, without duplicates.
  FallibleTArray<nsString> keys;
  for (const Param& param : mParams) {
    if (!keys.Contains(param.mKey) &&
        !keys.InsertElementSorted(param.mKey, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  FallibleTArray<Param> params;

  // Here we recreate the array starting from the sorted keys.
  for (uint32_t keyId = 0, keysLength = keys.Length(); keyId < keysLength;
       ++keyId) {
    const nsString& key = keys[keyId];
    for (const Param& param : mParams) {
      if (param.mKey.Equals(key) &&
          !params.AppendElement(param, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  mParams.SwapElements(params);
  return NS_OK;
}

inline bool
WriteString(JSStructuredCloneWriter* aWriter, const nsString& aString)
{
  MOZ_ASSERT(aWriter);

  size_t charSize = sizeof(nsString::char_type);
  return JS_WriteUint32Pair(aWriter, aString.Length(), 0) &&
         JS_WriteBytes(aWriter, aString.get(), aString.Length() * charSize);
}

bool
URLParams::WriteStructuredClone(JSStructuredCloneWriter* aWriter) const
{
  const uint32_t& nParams = mParams.Length();
  if (!JS_WriteUint32Pair(aWriter, nParams, 0)) {
    return false;
  }
  for (uint32_t i = 0; i < nParams; ++i) {
    if (!WriteString(aWriter, mParams[i].mKey) ||
        !WriteString(aWriter, mParams[i].mValue)) {
      return false;
    }
  }
  return true;
}

bool
URLParams::ReadStructuredClone(JSStructuredCloneReader* aReader)
{
  MOZ_ASSERT(aReader);

  DeleteAll();

  uint32_t nParams, zero;
  nsAutoString key, value;
  if (!JS_ReadUint32Pair(aReader, &nParams, &zero)) {
    return false;
  }
  MOZ_ASSERT(zero == 0);
  for (uint32_t i = 0; i < nParams; ++i) {
    if (!ReadString(aReader, key) || !ReadString(aReader, value)) {
      return false;
    }
    Append(key, value);
  }
  return true;
}

bool
URLSearchParams::WriteStructuredClone(JSStructuredCloneWriter* aWriter) const
{
  return mParams->WriteStructuredClone(aWriter);
}

bool
URLSearchParams::ReadStructuredClone(JSStructuredCloneReader* aReader)
{
 return mParams->ReadStructuredClone(aReader);
}

NS_IMETHODIMP
URLSearchParams::GetSendInfo(nsIInputStream** aBody, uint64_t* aContentLength,
                             nsACString& aContentTypeWithCharset,
                             nsACString& aCharset)
{
  aContentTypeWithCharset.AssignLiteral("application/x-www-form-urlencoded;charset=UTF-8");
  aCharset.AssignLiteral("UTF-8");

  nsAutoString serialized;
  Serialize(serialized);
  NS_ConvertUTF16toUTF8 converted(serialized);
  *aContentLength = converted.Length();
  return NS_NewCStringInputStream(aBody, converted);
}

} // namespace dom
} // namespace mozilla
