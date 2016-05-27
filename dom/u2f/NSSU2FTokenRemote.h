/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSU2FTokenRemote_h
#define NSSU2FTokenRemote_h

#include "nsIU2FToken.h"

class NSSU2FTokenRemote : public nsIU2FToken
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIU2FTOKEN

  NSSU2FTokenRemote();

private:
  virtual ~NSSU2FTokenRemote();
};

#endif // NSSU2FTokenRemote_h
