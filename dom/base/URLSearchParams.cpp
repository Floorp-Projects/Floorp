/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLSearchParams.h"
#include "mozilla/dom/URLSearchParamsBinding.h"

namespace mozilla {
namespace dom {

URLSearchParams::URLSearchParams()
{
}

URLSearchParams::~URLSearchParams()
{
  DeleteAll();
}

JSObject*
URLSearchParams::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return URLSearchParamsBinding::Wrap(aCx, aScope, this);
}

/* static */ already_AddRefed<URLSearchParams>
URLSearchParams::Constructor(const GlobalObject& aGlobal,
                             const nsAString& aInit,
                             ErrorResult& aRv)
{
  nsRefPtr<URLSearchParams> sp = new URLSearchParams();
  sp->ParseInput(aInit);
  return sp.forget();
}

/* static */ already_AddRefed<URLSearchParams>
URLSearchParams::Constructor(const GlobalObject& aGlobal,
                             URLSearchParams& aInit,
                             ErrorResult& aRv)
{
  nsRefPtr<URLSearchParams> sp = new URLSearchParams();
  aInit.mSearchParams.EnumerateRead(CopyEnumerator, sp);
  return sp.forget();
}

void
URLSearchParams::ParseInput(const nsAString& aInput)
{
  nsAString::const_iterator start, end;
  aInput.BeginReading(start);
  aInput.EndReading(end);
  nsAString::const_iterator iter(start);

  while (start != end) {
    nsAutoString string;

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

    nsAString::const_iterator eqStart, eqEnd;
    string.BeginReading(eqStart);
    string.EndReading(eqEnd);
    nsAString::const_iterator eqIter(eqStart);

    nsAutoString name;
    nsAutoString value;

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

void
URLSearchParams::DecodeString(const nsAString& aInput, nsAString& aOutput)
{
  nsAString::const_iterator start, end;
  aInput.BeginReading(start);
  aInput.EndReading(end);

  while (start != end) {
    // replace '+' with U+0020
    if (*start == '+') {
      aOutput.Append(' ');
      ++start;
      continue;
    }

    // Percent decode algorithm
    if (*start == '%') {
      nsAString::const_iterator first(start);
      ++first;

      nsAString::const_iterator second(first);
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
        aOutput.Append(HEX_DIGIT(first) * 16 + HEX_DIGIT(second));
        start = ++second;
        continue;

      } else {
        aOutput.Append('%');
        ++start;
        continue;
      }
    }

    aOutput.Append(*start);
    ++start;
  }
}

/* static */ PLDHashOperator
URLSearchParams::CopyEnumerator(const nsAString& aName,
                                nsTArray<nsString>* aArray,
                                void *userData)
{
  URLSearchParams* aSearchParams = static_cast<URLSearchParams*>(userData);

  nsTArray<nsString>* newArray = new nsTArray<nsString>();
  newArray->AppendElements(*aArray);

  aSearchParams->mSearchParams.Put(aName, newArray);
  return PL_DHASH_NEXT;
}

void
URLSearchParams::Get(const nsAString& aName, nsString& aRetval)
{
  nsTArray<nsString>* array;
  if (!mSearchParams.Get(aName, &array)) {
    aRetval.Truncate();
    return;
  }

  aRetval.Assign(array->ElementAt(0));
}

void
URLSearchParams::GetAll(const nsAString& aName, nsTArray<nsString>& aRetval)
{
  nsTArray<nsString>* array;
  if (!mSearchParams.Get(aName, &array)) {
    return;
  }

  aRetval.AppendElements(*array);
}

void
URLSearchParams::Set(const nsAString& aName, const nsAString& aValue)
{
  nsTArray<nsString>* array;
  if (!mSearchParams.Get(aName, &array)) {
    array = new nsTArray<nsString>();
    array->AppendElement(aValue);
    mSearchParams.Put(aName, array);
  } else {
    array->ElementAt(0) = aValue;
  }
}

void
URLSearchParams::Append(const nsAString& aName, const nsAString& aValue)
{
  nsTArray<nsString>* array;
  if (!mSearchParams.Get(aName, &array)) {
    array = new nsTArray<nsString>();
    mSearchParams.Put(aName, array);
  }

  array->AppendElement(aValue);
}

bool
URLSearchParams::Has(const nsAString& aName)
{
  return mSearchParams.Get(aName, nullptr);
}

void
URLSearchParams::Delete(const nsAString& aName)
{
  nsTArray<nsString>* array;
  if (!mSearchParams.Get(aName, &array)) {
    return;
  }

  mSearchParams.Remove(aName);
}

void
URLSearchParams::DeleteAll()
{
  mSearchParams.Clear();
}

} // namespace dom
} // namespace mozilla
