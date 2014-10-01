/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLSearchParams.h"
#include "mozilla/dom/URLSearchParamsBinding.h"
#include "mozilla/dom/EncodingUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(URLSearchParams, mObservers)
NS_IMPL_CYCLE_COLLECTING_ADDREF(URLSearchParams)
NS_IMPL_CYCLE_COLLECTING_RELEASE(URLSearchParams)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(URLSearchParams)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

URLSearchParams::URLSearchParams()
{
  SetIsDOMBinding();
}

URLSearchParams::~URLSearchParams()
{
  DeleteAll();
}

JSObject*
URLSearchParams::WrapObject(JSContext* aCx)
{
  return URLSearchParamsBinding::Wrap(aCx, this);
}

/* static */ already_AddRefed<URLSearchParams>
URLSearchParams::Constructor(const GlobalObject& aGlobal,
                             const nsAString& aInit,
                             ErrorResult& aRv)
{
  nsRefPtr<URLSearchParams> sp = new URLSearchParams();
  sp->ParseInput(NS_ConvertUTF16toUTF8(aInit), nullptr);
  return sp.forget();
}

/* static */ already_AddRefed<URLSearchParams>
URLSearchParams::Constructor(const GlobalObject& aGlobal,
                             URLSearchParams& aInit,
                             ErrorResult& aRv)
{
  nsRefPtr<URLSearchParams> sp = new URLSearchParams();
  sp->mSearchParams = aInit.mSearchParams;
  return sp.forget();
}

void
URLSearchParams::ParseInput(const nsACString& aInput,
                            URLSearchParamsObserver* aObserver)
{
  // Remove all the existing data before parsing a new input.
  DeleteAll();

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

    AppendInternal(decodedName, decodedValue);
  }

  NotifyObservers(aObserver);
}

void
URLSearchParams::DecodeString(const nsACString& aInput, nsAString& aOutput)
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

void
URLSearchParams::ConvertString(const nsACString& aInput, nsAString& aOutput)
{
  aOutput.Truncate();

  if (!mDecoder) {
    mDecoder = EncodingUtils::DecoderForEncoding("UTF-8");
    if (!mDecoder) {
      MOZ_ASSERT(mDecoder, "Failed to create a decoder.");
      return;
    }
  }

  int32_t inputLength = aInput.Length();
  int32_t outputLength = 0;

  nsresult rv = mDecoder->GetMaxLength(aInput.BeginReading(), inputLength,
                                       &outputLength);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (!aOutput.SetLength(outputLength, fallible_t())) {
    return;
  }

  int32_t newOutputLength = outputLength;
  rv = mDecoder->Convert(aInput.BeginReading(), &inputLength,
                         aOutput.BeginWriting(), &newOutputLength);
  if (NS_FAILED(rv)) {
    aOutput.Truncate();
    return;
  }

  if (newOutputLength < outputLength) {
    aOutput.Truncate(newOutputLength);
  }
}

void
URLSearchParams::AddObserver(URLSearchParamsObserver* aObserver)
{
  MOZ_ASSERT(aObserver);
  MOZ_ASSERT(!mObservers.Contains(aObserver));
  mObservers.AppendElement(aObserver);
}

void
URLSearchParams::RemoveObserver(URLSearchParamsObserver* aObserver)
{
  MOZ_ASSERT(aObserver);
  mObservers.RemoveElement(aObserver);
}

void
URLSearchParams::RemoveObservers()
{
  mObservers.Clear();
}

void
URLSearchParams::Get(const nsAString& aName, nsString& aRetval)
{
  aRetval.Truncate();

  for (uint32_t i = 0, len = mSearchParams.Length(); i < len; ++i) {
    if (mSearchParams[i].mKey.Equals(aName)) {
      aRetval.Assign(mSearchParams[i].mValue);
      break;
    }
  }
}

void
URLSearchParams::GetAll(const nsAString& aName, nsTArray<nsString>& aRetval)
{
  aRetval.Clear();

  for (uint32_t i = 0, len = mSearchParams.Length(); i < len; ++i) {
    if (mSearchParams[i].mKey.Equals(aName)) {
      aRetval.AppendElement(mSearchParams[i].mValue);
    }
  }
}

void
URLSearchParams::Set(const nsAString& aName, const nsAString& aValue)
{
  Param* param = nullptr;
  for (uint32_t i = 0, len = mSearchParams.Length(); i < len; ++i) {
    if (mSearchParams[i].mKey.Equals(aName)) {
      param = &mSearchParams[i];
      break;
    }
  }

  if (!param) {
    param = mSearchParams.AppendElement();
    param->mKey = aName;
  }

  param->mValue = aValue;

  NotifyObservers(nullptr);
}

void
URLSearchParams::Append(const nsAString& aName, const nsAString& aValue)
{
  AppendInternal(aName, aValue);
  NotifyObservers(nullptr);
}

void
URLSearchParams::AppendInternal(const nsAString& aName, const nsAString& aValue)
{
  Param* param = mSearchParams.AppendElement();
  param->mKey = aName;
  param->mValue = aValue;
}

bool
URLSearchParams::Has(const nsAString& aName)
{
  for (uint32_t i = 0, len = mSearchParams.Length(); i < len; ++i) {
    if (mSearchParams[i].mKey.Equals(aName)) {
      return true;
    }
  }

  return false;
}

void
URLSearchParams::Delete(const nsAString& aName)
{
  bool found = false;
  for (uint32_t i = 0; i < mSearchParams.Length();) {
    if (mSearchParams[i].mKey.Equals(aName)) {
      mSearchParams.RemoveElementAt(i);
      found = true;
    } else {
      ++i;
    }
  }

  if (found) {
    NotifyObservers(nullptr);
  }
}

void
URLSearchParams::DeleteAll()
{
  mSearchParams.Clear();
}

namespace {

void SerializeString(const nsCString& aInput, nsAString& aValue)
{
  const unsigned char* p = (const unsigned char*) aInput.get();

  while (p && *p) {
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

} // anonymous namespace

void
URLSearchParams::Serialize(nsAString& aValue) const
{
  aValue.Truncate();
  bool first = true;

  for (uint32_t i = 0, len = mSearchParams.Length(); i < len; ++i) {
    if (first) {
      first = false;
    } else {
      aValue.Append('&');
    }

    SerializeString(NS_ConvertUTF16toUTF8(mSearchParams[i].mKey), aValue);
    aValue.Append('=');
    SerializeString(NS_ConvertUTF16toUTF8(mSearchParams[i].mValue), aValue);
  }
}

void
URLSearchParams::NotifyObservers(URLSearchParamsObserver* aExceptObserver)
{
  for (uint32_t i = 0; i < mObservers.Length(); ++i) {
    if (mObservers[i] != aExceptObserver) {
      mObservers[i]->URLSearchParamsUpdated(this);
    }
  }
}

} // namespace dom
} // namespace mozilla
