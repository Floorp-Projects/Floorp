/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  This sort service is used to sort content by attribute.
 */

#ifndef nsXULSortService_h
#define nsXULSortService_h

#include "nsISupportsImpl.h"
#include "nsIXULSortService.h"

////////////////////////////////////////////////////////////////////////
// ServiceImpl
//
//   This is the sort service.
//
class XULSortServiceImpl : public nsIXULSortService
{
public:
  XULSortServiceImpl(void) {}

protected:
  virtual ~XULSortServiceImpl(void) {}

public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsISortService
  NS_DECL_NSIXULSORTSERVICE

};

#endif // nsXULSortService_h
