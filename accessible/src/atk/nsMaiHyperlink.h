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

/*
 * MaiHyperlink is a auxiliary class for MaiInterfaceHyperText.
 */

class MaiHyperlink
{
public:
  MaiHyperlink(Accessible* aHyperLink);
  ~MaiHyperlink();

public:
  AtkHyperlink *GetAtkHyperlink(void);
  Accessible* GetAccHyperlink()
    { return mHyperlink && mHyperlink->IsLink() ? mHyperlink : nsnull; }

protected:
  Accessible* mHyperlink;
  AtkHyperlink* mMaiAtkHyperlink;
public:
  static nsresult Initialize(AtkHyperlink *aObj, MaiHyperlink *aClass);
};
#endif /* __MAI_HYPERLINK_H__ */
