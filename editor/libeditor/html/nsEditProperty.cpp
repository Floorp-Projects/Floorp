/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsEditProperty.h"
#include "nsString.h"

static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIEditPropertyIID, NS_IEDITPROPERTY_IID);


NS_IMPL_ADDREF(nsEditProperty)

NS_IMPL_RELEASE(nsEditProperty)

// XXX: remove when html atoms are exported from layout
// tags
nsIAtom * nsIEditProperty::a;
nsIAtom * nsIEditProperty::b;
nsIAtom * nsIEditProperty::big;
nsIAtom * nsIEditProperty::font;
nsIAtom * nsIEditProperty::i;
nsIAtom * nsIEditProperty::span;
nsIAtom * nsIEditProperty::small;
nsIAtom * nsIEditProperty::strike;
nsIAtom * nsIEditProperty::sub;
nsIAtom * nsIEditProperty::sup;
nsIAtom * nsIEditProperty::tt;
nsIAtom * nsIEditProperty::u;
// properties
nsIAtom * nsIEditProperty::color;
nsIAtom * nsIEditProperty::face;
nsIAtom * nsIEditProperty::size;
// special
nsString * nsIEditProperty::allProperties;

void
nsEditProperty::InstanceInit()
{
  // tags
  nsIEditProperty::a =    NS_NewAtom("A");
  nsIEditProperty::b =    NS_NewAtom("B");
  nsIEditProperty::big =  NS_NewAtom("BIG");
  nsIEditProperty::font = NS_NewAtom("FONT");
  nsIEditProperty::i =    NS_NewAtom("I");
  nsIEditProperty::span = NS_NewAtom("SPAN");
  nsIEditProperty::small =NS_NewAtom("SMALL");
  nsIEditProperty::strike=NS_NewAtom("STRIKE");
  nsIEditProperty::sub =  NS_NewAtom("SUB");
  nsIEditProperty::sup =  NS_NewAtom("SUP");
  nsIEditProperty::tt =   NS_NewAtom("TT");
  nsIEditProperty::u =    NS_NewAtom("U");
  // properties
  nsIEditProperty::color= NS_NewAtom("COLOR");
  nsIEditProperty::face = NS_NewAtom("FACE");
  nsIEditProperty::size = NS_NewAtom("SIZE");
  // special
  nsIEditProperty::allProperties = new nsString("moz_AllProperties");
}

void
nsEditProperty::InstanceShutdown()
{
  // tags
  NS_IF_RELEASE(nsIEditProperty::a);
  NS_IF_RELEASE(nsIEditProperty::b);
  NS_IF_RELEASE(nsIEditProperty::big);
  NS_IF_RELEASE(nsIEditProperty::font);
  NS_IF_RELEASE(nsIEditProperty::i);
  NS_IF_RELEASE(nsIEditProperty::span);
  NS_IF_RELEASE(nsIEditProperty::small);
  NS_IF_RELEASE(nsIEditProperty::strike);
  NS_IF_RELEASE(nsIEditProperty::sub);
  NS_IF_RELEASE(nsIEditProperty::sup);
  NS_IF_RELEASE(nsIEditProperty::tt);
  NS_IF_RELEASE(nsIEditProperty::u);
  // properties
  NS_IF_RELEASE(nsIEditProperty::color);
  NS_IF_RELEASE(nsIEditProperty::face);
  NS_IF_RELEASE(nsIEditProperty::size);
  // special
  if (nsIEditProperty::allProperties) {
    delete (nsIEditProperty::allProperties);
  }
}

NS_IMETHODIMP
nsEditProperty::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIEditPropertyIID)) {
    *aInstancePtr = (void*)(nsIEditProperty*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult NS_NewEditProperty(nsIEditProperty **aResult)
{
  if (aResult)
  {
    *aResult = new nsEditProperty();
    if (!*aResult) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(*aResult);
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}