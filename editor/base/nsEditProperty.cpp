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
// inline tags
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
// block tags
nsIAtom * nsIEditProperty::blockquote;
nsIAtom * nsIEditProperty::br;
nsIAtom * nsIEditProperty::h1;
nsIAtom * nsIEditProperty::h2;
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
  nsIEditProperty::a =    NS_NewAtom("a");
  nsIEditProperty::b =    NS_NewAtom("b");
  nsIEditProperty::big =  NS_NewAtom("big");
  nsIEditProperty::font = NS_NewAtom("font");
  nsIEditProperty::i =    NS_NewAtom("i");
  nsIEditProperty::span = NS_NewAtom("span");
  nsIEditProperty::small =NS_NewAtom("small");
  nsIEditProperty::strike=NS_NewAtom("strike");
  nsIEditProperty::sub =  NS_NewAtom("sub");
  nsIEditProperty::sup =  NS_NewAtom("sup");
  nsIEditProperty::tt =   NS_NewAtom("tt");
  nsIEditProperty::u =    NS_NewAtom("u");
  // tags
  nsIEditProperty::blockquote = NS_NewAtom("blockquote");
  nsIEditProperty::br =    NS_NewAtom("br");
  nsIEditProperty::h1 =    NS_NewAtom("h1");
  nsIEditProperty::h2 =    NS_NewAtom("h2");
  // properties
  nsIEditProperty::color= NS_NewAtom("color");
  nsIEditProperty::face = NS_NewAtom("face");
  nsIEditProperty::size = NS_NewAtom("size");
  // special
  nsIEditProperty::allProperties = new nsString("moz_allproperties");
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
  // tags
  NS_IF_RELEASE(nsIEditProperty::blockquote);
  NS_IF_RELEASE(nsIEditProperty::br);
  NS_IF_RELEASE(nsIEditProperty::h1);
  NS_IF_RELEASE(nsIEditProperty::h2);
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
