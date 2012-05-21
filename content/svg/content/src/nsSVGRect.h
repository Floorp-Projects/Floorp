/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGRECT_H__
#define __NS_SVGRECT_H__

#include "gfxRect.h"
#include "nsIDOMSVGRect.h"

nsresult
NS_NewSVGRect(nsIDOMSVGRect** result,
              float x=0.0f, float y=0.0f,
              float width=0.0f, float height=0.0f);

nsresult
NS_NewSVGRect(nsIDOMSVGRect** result, const gfxRect& rect);

////////////////////////////////////////////////////////////////////////
// nsSVGRect class

class nsSVGRect : public nsIDOMSVGRect
{
public:
  nsSVGRect(float x=0.0f, float y=0.0f, float w=0.0f, float h=0.0f);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGRect interface:
  NS_DECL_NSIDOMSVGRECT

protected:
  float mX, mY, mWidth, mHeight;
};

#endif //__NS_SVGRECT_H__
