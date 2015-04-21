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
  explicit MaiHyperlink(Accessible* aHyperLink);
  ~MaiHyperlink();

public:
  AtkHyperlink* GetAtkHyperlink() const { return mMaiAtkHyperlink; }
  Accessible* GetAccHyperlink()
    { return mHyperlink && mHyperlink->IsLink() ? mHyperlink : nullptr; }

protected:
  Accessible* mHyperlink;
  AtkHyperlink* mMaiAtkHyperlink;
};

} // namespace a11y
} // namespace mozilla

#endif /* __MAI_HYPERLINK_H__ */
