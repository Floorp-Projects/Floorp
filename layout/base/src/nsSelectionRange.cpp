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
#include "nsSelectionRange.h"

nsSelectionRange::nsSelectionRange() {
  mStart = new nsSelectionPoint(nsnull, -1, PR_TRUE);
  mEnd   = new nsSelectionPoint(nsnull, -1, PR_TRUE);
} 

nsSelectionRange::nsSelectionRange(nsIContent * aStartContent,
                                   PRInt32     aStartOffset,
                                   PRBool       aStartIsAnchor,
                                   nsIContent * aEndContent,
                                   PRInt32     aEndOffset,
                                   PRBool       aEndIsAnchor) {
  mStart = new nsSelectionPoint(aStartContent, aStartOffset, aStartIsAnchor);
  mEnd   = new nsSelectionPoint(aEndContent,   aEndOffset,   aEndIsAnchor);
} 

nsSelectionRange::~nsSelectionRange() {
  delete mStart;
  delete mEnd;
}

void nsSelectionRange::SetRange(nsIContent * aStartContent,
                                PRInt32     aStartOffset,
                                PRBool       aStartIsAnchor,
                                nsIContent * aEndContent,
                                PRInt32     aEndOffset,
                                PRBool       aEndIsAnchor) {
  mStart->SetPoint(aStartContent, aStartOffset, aStartIsAnchor);
  mEnd->SetPoint(aEndContent,   aEndOffset,   aEndIsAnchor);
}

void nsSelectionRange::SetRange(nsSelectionPoint * aStartPoint,
                                nsSelectionPoint * aEndPoint) {
  SetStartPoint(aStartPoint);
  SetEndPoint(aEndPoint);
}

void nsSelectionRange::SetRange(nsSelectionRange * aRange) {
  SetStartPoint(aRange->GetStartPoint());
  SetEndPoint(aRange->GetEndPoint());
}

void nsSelectionRange::SetStartPoint(nsSelectionPoint * aPoint) {
  nsIContent * content = aPoint->GetContent();
  mStart->SetPoint(content, aPoint->GetOffset(), aPoint->IsAnchor());
  NS_IF_RELEASE(content);
}

void nsSelectionRange::SetEndPoint(nsSelectionPoint * aPoint) {
  nsIContent * content = aPoint->GetContent();
  mEnd->SetPoint(content, aPoint->GetOffset(), aPoint->IsAnchor());
  NS_IF_RELEASE(content);
}

void nsSelectionRange::GetStartPoint(nsSelectionPoint * aPoint) {
  nsIContent * content = mStart->GetContent();
  aPoint->SetPoint(content, mStart->GetOffset(), mStart->IsAnchor());
  NS_IF_RELEASE(content);
}

void nsSelectionRange::GetEndPoint(nsSelectionPoint * aPoint) {
  nsIContent * content = mEnd->GetContent();
  aPoint->SetPoint(content, mEnd->GetOffset(), mEnd->IsAnchor());
  NS_IF_RELEASE(content);
}

void nsSelectionRange::GetRange(nsSelectionRange * aRange) {
  aRange->SetStartPoint(mStart);
  aRange->SetEndPoint(mEnd);
}

/**
  * For debug only, it potentially leaks memory
 */
char * nsSelectionRange::ToString() {
  static char str[1024];
  char * startStr = mStart->ToString();
  char * endStr   = mEnd->ToString();
  sprintf(str, "nsSelectionRange[\nStart=%s,\nEnd  =%s\n]", startStr, endStr);
  delete startStr;
  delete endStr;
  return str;
}

