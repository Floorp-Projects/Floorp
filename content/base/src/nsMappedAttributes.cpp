/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A unique per-element set of attributes that is used as an
 * nsIStyleRule; used to implement presentational attributes.
 */

#include "nsMappedAttributes.h"
#include "nsHTMLStyleSheet.h"
#include "nsRuleWalker.h"
#include "prmem.h"
#include "mozilla/HashFunctions.h"

using namespace mozilla;

nsMappedAttributes::nsMappedAttributes(nsHTMLStyleSheet* aSheet,
                                       nsMapRuleToAttributesFunc aMapRuleFunc)
  : mAttrCount(0),
    mSheet(aSheet),
    mRuleMapper(aMapRuleFunc)
{
}

nsMappedAttributes::nsMappedAttributes(const nsMappedAttributes& aCopy)
  : mAttrCount(aCopy.mAttrCount),
    mSheet(aCopy.mSheet),
    mRuleMapper(aCopy.mRuleMapper)
{
  NS_ASSERTION(mBufferSize >= aCopy.mAttrCount, "can't fit attributes");

  PRUint32 i;
  for (i = 0; i < mAttrCount; ++i) {
    new (&Attrs()[i]) InternalAttr(aCopy.Attrs()[i]);
  }
}

nsMappedAttributes::~nsMappedAttributes()
{
  if (mSheet) {
    mSheet->DropMappedAttributes(this);
  }

  PRUint32 i;
  for (i = 0; i < mAttrCount; ++i) {
    Attrs()[i].~InternalAttr();
  }
}


nsMappedAttributes*
nsMappedAttributes::Clone(bool aWillAddAttr)
{
  PRUint32 extra = aWillAddAttr ? 1 : 0;

  // This will call the overridden operator new
  return new (mAttrCount + extra) nsMappedAttributes(*this);
}

void* nsMappedAttributes::operator new(size_t aSize, PRUint32 aAttrCount) CPP_THROW_NEW
{
  NS_ASSERTION(aAttrCount > 0, "zero-attribute nsMappedAttributes requested");

  // aSize will include the mAttrs buffer so subtract that.
  void* newAttrs = ::operator new(aSize - sizeof(void*[1]) +
                                  aAttrCount * sizeof(InternalAttr));

#ifdef DEBUG
  static_cast<nsMappedAttributes*>(newAttrs)->mBufferSize = aAttrCount;
#endif

  return newAttrs;
}

NS_IMPL_ISUPPORTS1(nsMappedAttributes,
                   nsIStyleRule)

nsresult
nsMappedAttributes::SetAndTakeAttr(nsIAtom* aAttrName, nsAttrValue& aValue)
{
  NS_PRECONDITION(aAttrName, "null name");

  PRUint32 i;
  for (i = 0; i < mAttrCount && !Attrs()[i].mName.IsSmaller(aAttrName); ++i) {
    if (Attrs()[i].mName.Equals(aAttrName)) {
      Attrs()[i].mValue.Reset();
      Attrs()[i].mValue.SwapValueWith(aValue);

      return NS_OK;
    }
  }

  NS_ASSERTION(mBufferSize >= mAttrCount + 1, "can't fit attributes");

  if (mAttrCount != i) {
    memmove(&Attrs()[i + 1], &Attrs()[i], (mAttrCount - i) * sizeof(InternalAttr));
  }

  new (&Attrs()[i].mName) nsAttrName(aAttrName);
  new (&Attrs()[i].mValue) nsAttrValue();
  Attrs()[i].mValue.SwapValueWith(aValue);
  ++mAttrCount;

  return NS_OK;
}

const nsAttrValue*
nsMappedAttributes::GetAttr(nsIAtom* aAttrName) const
{
  NS_PRECONDITION(aAttrName, "null name");

  for (PRUint32 i = 0; i < mAttrCount; ++i) {
    if (Attrs()[i].mName.Equals(aAttrName)) {
      return &Attrs()[i].mValue;
    }
  }

  return nullptr;
}

const nsAttrValue*
nsMappedAttributes::GetAttr(const nsAString& aAttrName) const
{
  for (PRUint32 i = 0; i < mAttrCount; ++i) {
    if (Attrs()[i].mName.Atom()->Equals(aAttrName)) {
      return &Attrs()[i].mValue;
    }
  }

  return nullptr;
}

bool
nsMappedAttributes::Equals(const nsMappedAttributes* aOther) const
{
  if (this == aOther) {
    return true;
  }

  if (mRuleMapper != aOther->mRuleMapper || mAttrCount != aOther->mAttrCount) {
    return false;
  }

  PRUint32 i;
  for (i = 0; i < mAttrCount; ++i) {
    if (!Attrs()[i].mName.Equals(aOther->Attrs()[i].mName) ||
        !Attrs()[i].mValue.Equals(aOther->Attrs()[i].mValue)) {
      return false;
    }
  }

  return true;
}

PRUint32
nsMappedAttributes::HashValue() const
{
  PRUint32 hash = HashGeneric(mRuleMapper);

  PRUint32 i;
  for (i = 0; i < mAttrCount; ++i) {
    hash = AddToHash(hash,
                     Attrs()[i].mName.HashValue(),
                     Attrs()[i].mValue.HashValue());
  }

  return hash;
}

void
nsMappedAttributes::SetStyleSheet(nsHTMLStyleSheet* aSheet)
{
  if (mSheet) {
    mSheet->DropMappedAttributes(this);
  }
  mSheet = aSheet;  // not ref counted
}

/* virtual */ void
nsMappedAttributes::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (mRuleMapper) {
    (*mRuleMapper)(this, aRuleData);
  }
}

#ifdef DEBUG
/* virtual */ void
nsMappedAttributes::List(FILE* out, PRInt32 aIndent) const
{
  nsAutoString buffer;
  PRUint32 i;

  for (i = 0; i < mAttrCount; ++i) {
    PRInt32 indent;
    for (indent = aIndent; indent > 0; --indent)
      fputs("  ", out);

    Attrs()[i].mName.GetQualifiedName(buffer);
    fputs(NS_LossyConvertUTF16toASCII(buffer).get(), out);

    Attrs()[i].mValue.ToString(buffer);
    fputs(NS_LossyConvertUTF16toASCII(buffer).get(), out);
    fputs("\n", out);
  }
}
#endif

void
nsMappedAttributes::RemoveAttrAt(PRUint32 aPos, nsAttrValue& aValue)
{
  Attrs()[aPos].mValue.SwapValueWith(aValue);
  Attrs()[aPos].~InternalAttr();
  memmove(&Attrs()[aPos], &Attrs()[aPos + 1],
          (mAttrCount - aPos - 1) * sizeof(InternalAttr));
  mAttrCount--;
}

const nsAttrName*
nsMappedAttributes::GetExistingAttrNameFromQName(const nsAString& aName) const
{
  PRUint32 i;
  for (i = 0; i < mAttrCount; ++i) {
    if (Attrs()[i].mName.IsAtom()) {
      if (Attrs()[i].mName.Atom()->Equals(aName)) {
        return &Attrs()[i].mName;
      }
    }
    else {
      if (Attrs()[i].mName.NodeInfo()->QualifiedNameEquals(aName)) {
        return &Attrs()[i].mName;
      }
    }
  }

  return nullptr;
}

PRInt32
nsMappedAttributes::IndexOfAttr(nsIAtom* aLocalName) const
{
  PRUint32 i;
  for (i = 0; i < mAttrCount; ++i) {
    if (Attrs()[i].mName.Equals(aLocalName)) {
      return i;
    }
  }

  return -1;
}

size_t
nsMappedAttributes::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  NS_ASSERTION(mAttrCount == mBufferSize,
               "mBufferSize and mAttrCount are expected to be the same.");

  size_t n = aMallocSizeOf(this);
  for (PRUint16 i = 0; i < mAttrCount; ++i) {
    n += Attrs()[i].mValue.SizeOfExcludingThis(aMallocSizeOf);
  }
  return n;
}

