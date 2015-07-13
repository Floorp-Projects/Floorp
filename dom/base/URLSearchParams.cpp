/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLSearchParams.h"
#include "mozilla/dom/URLSearchParamsBinding.h"
#include "mozilla/dom/EncodingUtils.h"
#include "nsDOMString.h"

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

bool
URLParams::Delete(const nsAString& aName)
{
  bool found = false;
  for (uint32_t i = 0; i < mParams.Length();) {
    if (mParams[i].mKey.Equals(aName)) {
      mParams.RemoveElementAt(i);
      found = true;
    } else {
      ++i;
    }
  }

  return found;
}

void
URLParams::ConvertString(const nsACString& aInput, nsAString& aOutput)
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

  if (!aOutput.SetLength(outputLength, fallible)) {
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

void
URLParams::ParseInput(const nsACString& aInput)
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

    Append(decodedName, decodedValue);
  }
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
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

URLSearchParams::URLSearchParams(nsISupports* aParent,
                                 URLSearchParamsObserver* aObserver)
  : mParams(new URLParams())
  , mParent(aParent)
  , mObserver(aObserver)
{
}

URLSearchParams::URLSearchParams(nsISupports* aParent,
                                 const URLSearchParams& aOther)
  : mParams(new URLParams(*aOther.mParams.get()))
  , mParent(aParent)
  , mObserver(aOther.mObserver)
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
                             const nsAString& aInit,
                             ErrorResult& aRv)
{
  nsRefPtr<URLSearchParams> sp =
    new URLSearchParams(aGlobal.GetAsSupports(), nullptr);
  sp->ParseInput(NS_ConvertUTF16toUTF8(aInit));

  return sp.forget();
}

/* static */ already_AddRefed<URLSearchParams>
URLSearchParams::Constructor(const GlobalObject& aGlobal,
                             URLSearchParams& aInit,
                             ErrorResult& aRv)
{
  nsRefPtr<URLSearchParams> sp =
    new URLSearchParams(aGlobal.GetAsSupports(), aInit);

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
  if (mParams->Delete(aName)) {
    NotifyObserver();
  }
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

} // namespace dom
} // namespace mozilla
