/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * A unique per-element set of attributes that is used as an
 * nsIStyleRule; used to implement presentational attributes.
 */

#include "nsMappedAttributes.h"
#include "nsHTMLStyleSheet.h"
#include "nsRuleWalker.h"
#include "prmem.h"

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
  if (newAttrs) {
    static_cast<nsMappedAttributes*>(newAttrs)->mBufferSize = aAttrCount;
  }
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

  PRInt32 i = IndexOfAttr(aAttrName, kNameSpaceID_None);
  if (i >= 0) {
    return &Attrs()[i].mValue;
  }

  return nsnull;
}

bool
nsMappedAttributes::Equals(const nsMappedAttributes* aOther) const
{
  if (this == aOther) {
    return PR_TRUE;
  }

  if (mRuleMapper != aOther->mRuleMapper || mAttrCount != aOther->mAttrCount) {
    return PR_FALSE;
  }

  PRUint32 i;
  for (i = 0; i < mAttrCount; ++i) {
    if (!Attrs()[i].mName.Equals(aOther->Attrs()[i].mName) ||
        !Attrs()[i].mValue.Equals(aOther->Attrs()[i].mValue)) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

PRUint32
nsMappedAttributes::HashValue() const
{
  PRUint32 value = NS_PTR_TO_INT32(mRuleMapper);

  PRUint32 i;
  for (i = 0; i < mAttrCount; ++i) {
    value ^= Attrs()[i].mName.HashValue() ^ Attrs()[i].mValue.HashValue();
  }

  return value;
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

  return nsnull;
}

PRInt32
nsMappedAttributes::IndexOfAttr(nsIAtom* aLocalName, PRInt32 aNamespaceID) const
{
  PRUint32 i;
  if (aNamespaceID == kNameSpaceID_None) {
    // This should be the common case so lets make an optimized loop
    for (i = 0; i < mAttrCount; ++i) {
      if (Attrs()[i].mName.Equals(aLocalName)) {
        return i;
      }
    }
  }
  else {
    for (i = 0; i < mAttrCount; ++i) {
      if (Attrs()[i].mName.Equals(aLocalName, aNamespaceID)) {
        return i;
      }
    }
  }

  return -1;
}

PRInt64
nsMappedAttributes::SizeOf() const
{
  NS_ASSERTION(mAttrCount == mBufferSize,
               "mBufferSize and mAttrCount are expected to be the same.");

  PRInt64 size = sizeof(*this) - sizeof(void*) + mAttrCount * sizeof(InternalAttr);

  for (PRUint16 i = 0; i < mAttrCount; ++i) {
    size += Attrs()[i].mValue.SizeOf() - sizeof(Attrs()[i].mValue);
  }

  return size;
}

