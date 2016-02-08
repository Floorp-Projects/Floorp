/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MAI_HYPERLINK_H__
#define __MAI_HYPERLINK_H__

#include "nsMai.h"
#include "Accessible.h"

struct _AtkHyperlink;
typedef struct _AtkHyperlink                      AtkHyperlink;

namespace mozilla {
namespace a11y {

/*
 * MaiHyperlink is a auxiliary class for MaiInterfaceHyperText.
 */

class MaiHyperlink
{
public:
  explicit MaiHyperlink(AccessibleOrProxy aHyperLink);
  ~MaiHyperlink();

public:
  AtkHyperlink* GetAtkHyperlink() const { return mMaiAtkHyperlink; }
  Accessible* GetAccHyperlink()
    {
      if (!mHyperlink.IsAccessible())
        return nullptr;

      Accessible* link = mHyperlink.AsAccessible();
      if (!link) {
        return nullptr;
      }

      NS_ASSERTION(link->IsLink(), "Why isn't it a link!");
      return link;
    }

  ProxyAccessible* Proxy() const { return mHyperlink.AsProxy(); }

protected:
  AccessibleOrProxy mHyperlink;
  AtkHyperlink* mMaiAtkHyperlink;
};

} // namespace a11y
} // namespace mozilla

#endif /* __MAI_HYPERLINK_H__ */
