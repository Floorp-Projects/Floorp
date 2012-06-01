/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

#ifndef _AccessibleWrap_H_
#define _AccessibleWrap_H_

#include "nsCOMPtr.h"
#include "Accessible.h"

class AccessibleWrap : public Accessible
{
public: // construction, destruction
  AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~AccessibleWrap();

  protected:
    virtual nsresult FirePlatformEvent(AccEvent* aEvent)
    {
      return NS_OK;
    }
};

#endif
