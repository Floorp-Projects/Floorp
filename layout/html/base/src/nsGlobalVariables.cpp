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
#include "nsGlobalVariables.h"

nsGlobalVariables* nsGlobalVariables::gInstance;

nsGlobalVariables::nsGlobalVariables() 
{
	mPresentationContext = nsnull;
	mBeveledLines=PR_TRUE;
	mBlackLines=PR_FALSE;
	mBlackText=PR_FALSE;
}

nsGlobalVariables::~nsGlobalVariables() 
{}

/**
*
* WARNING: This should only be called when the program exits
* No attemp to release this prior to termination will have
* disastrous consequences.
*
**/
void nsGlobalVariables::Release()
{
  if (gInstance != nsnull)
    delete gInstance;
  gInstance = nsnull;
}

nsGlobalVariables * nsGlobalVariables::Instance() 
{
  if (nsnull==gInstance)
    gInstance = new nsGlobalVariables();
  return gInstance;
}

PRBool nsGlobalVariables::GetPrinting(nsIPresContext * aPresentationContext)
{
	return (PRBool) (aPresentationContext == mPresentationContext);
}

void nsGlobalVariables::SetPrinting(nsIPresContext * aPresentationContext)
{
	mPresentationContext = aPresentationContext;
}

PRBool nsGlobalVariables::GetBeveledLines()
{
	return mBeveledLines;
}

void nsGlobalVariables::SetBeveledLines(PRBool aBeveledLines)
{
	mBeveledLines = aBeveledLines;
}

PRBool nsGlobalVariables::GetBlackLines()
{
	return mBlackLines;
}

void nsGlobalVariables::SetBlackLines(PRBool aBlackLines)
{
	mBlackLines = aBlackLines;
}

PRBool nsGlobalVariables::GetBlackText()
{
	return mBlackText;
}

void nsGlobalVariables::SetBlackText(PRBool aBlackText)
{
	mBlackText = aBlackText;
}

PRBool nsGlobalVariables::GetBackground()
{
	return mBackground;
}

void nsGlobalVariables::SetBackground(PRBool aBackground)
{
	mBackground = aBackground;
}
