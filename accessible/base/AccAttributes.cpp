/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccAttributes.h"
#include "StyleInfo.h"

using namespace mozilla::a11y;

bool AccAttributes::GetAttribute(nsAtom* aAttrName, nsAString& aAttrValue) {
  if (auto value = mData.Lookup(aAttrName)) {
    StringFromValueAndName(aAttrName, *value, aAttrValue);
    return true;
  }

  return false;
}

void AccAttributes::StringFromValueAndName(nsAtom* aAttrName,
                                           const AttrValueType& aValue,
                                           nsAString& aValueString) {
  aValueString.Truncate();

  aValue.match(
      [&aValueString](const bool& val) {
        aValueString.Assign(val ? u"true" : u"false");
      },
      [&aValueString](const float& val) {
        aValueString.AppendFloat(val * 100);
        aValueString.Append(u"%");
      },
      [&aValueString](const double& val) { aValueString.AppendFloat(val); },
      [&aValueString](const int32_t& val) { aValueString.AppendInt(val); },
      [&aValueString](const RefPtr<nsAtom>& val) {
        val->ToString(aValueString);
      },
      [&aValueString](const CopyableTArray<int32_t>& val) {
        for (size_t i = 0; i < val.Length() - 1; i++) {
          aValueString.AppendInt(val[i]);
          aValueString.Append(u", ");
        }
        aValueString.AppendInt(val[val.Length() - 1]);
      },
      [&aValueString](const CSSCoord& val) {
        aValueString.AppendFloat(val);
        aValueString.Append(u"px");
      },
      [&aValueString](const FontSize& val) {
        aValueString.AppendInt(val.mValue);
        aValueString.Append(u"pt");
      },
      [&aValueString](const Color& val) {
        StyleInfo::FormatColor(val.mValue, aValueString);
      },
      [&aValueString](const DeleteEntry& val) {
        aValueString.Append(u"-delete-entry-");
      });
}

void AccAttributes::Update(AccAttributes* aOther) {
  for (auto entry : *aOther) {
    if (entry.mValue->is<DeleteEntry>()) {
      mData.Remove(entry.mName);
      continue;
    }

    mData.InsertOrUpdate(entry.mName, *entry.mValue);
  }
}
