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
#include "nsSelectionPoint.h"

nsSelectionPoint::nsSelectionPoint(nsIContent * aContent,
                                   PRInt32      aOffset,
                                   PRBool       aIsAnchor) 
{
  fContent        = aContent;
  fOffset         = aOffset;
  fIsAnchor       = aIsAnchor; 
  fEntireContent  = PR_FALSE;

  if (fContent != nsnull) 
    NS_ADDREF(fContent);
}

nsSelectionPoint::~nsSelectionPoint() 
{
  NS_IF_RELEASE(fContent);
}


nsIContent * nsSelectionPoint::GetContent() 
{ 
  if (fContent != nsnull) 
    NS_ADDREF(fContent); 
  return fContent; 
}

void nsSelectionPoint::SetContent(nsIContent * aValue) 
{ 
  NS_IF_RELEASE(fContent);
  fContent  = aValue; 
  if (fContent != nsnull) 
    NS_ADDREF(fContent);
}

void nsSelectionPoint::SetPoint(nsIContent * aContent,
                                PRInt32      aOffset,
                                PRBool       aIsAnchor) 
{
  NS_IF_RELEASE(fContent);
  fContent  = aContent;
  fOffset   = aOffset;
  fIsAnchor = aIsAnchor;
  if (fContent != nsnull) 
    NS_ADDREF(fContent);
}

/**
  * For debug only, it leaks memory
 */
char * nsSelectionPoint::ToString() {
  char * str = new char[256];
  sprintf(str, "TextPoint[0x%X, offset=%d, isAnchor=%s]", fContent, fOffset, (fIsAnchor?"PR_TRUE":"PR_FALSE"));
  return str;
}

