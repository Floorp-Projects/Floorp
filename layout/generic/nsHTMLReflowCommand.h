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
#ifndef nsHTMLReflowCommand_h___
#define nsHTMLReflowCommand_h___

#include "nsIReflowCommand.h"
#include "nsVoidArray.h"

/**
 * An HTML reflow command
 */
class nsHTMLReflowCommand : public nsIReflowCommand {
public:
  /**
   * Construct an HTML reflow command of type aReflowType and with target
   * frame aTargetFrame. You can also specify an optional child frame, e.g.
   * to indicate the inserted child frame
   */
  nsHTMLReflowCommand(nsIFrame*  aTargetFrame,
                      ReflowType aReflowType,
                      nsIFrame*  aChildFrame = nsnull);

  virtual ~nsHTMLReflowCommand();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIReflowCommand
  NS_IMETHOD Dispatch(nsIPresContext&  aPresContext,
                      nsReflowMetrics& aDesiredSize,
                      const nsSize&    aMaxSize);
  NS_IMETHOD GetNext(nsIFrame*& aNextFrame);
  NS_IMETHOD GetTarget(nsIFrame*& aTargetFrame) const;
  NS_IMETHOD GetType(ReflowType& aReflowType) const;
  NS_IMETHOD GetChildFrame(nsIFrame*& aChildFrame) const;

private:
  ReflowType      mType;
  nsIFrame*       mTargetFrame;
  nsIFrame*       mChildFrame;
  nsVoidArray     mPath;
};

#endif /* nsHTMLReflowCommand_h___ */
