/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://wwwt.mozilla.org/NPL/
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

#ifndef nsTransactionStack_h__
#define nsTransactionStack_h__

#include "nsTransactionItem.h"
#include "nsDeque.h"

class nsTransactionItem;

class nsTransactionReleaseFunctor : public nsDequeFunctor
{
public:

  nsTransactionReleaseFunctor()          {}
  virtual ~nsTransactionReleaseFunctor() {}
  virtual void *operator()(void *aObject);
};

class nsTransactionStack
{
  nsTransactionReleaseFunctor mRF;
  nsDeque mQue;

public:

  nsTransactionStack();
  virtual ~nsTransactionStack();

  virtual nsresult Push(nsTransactionItem *aTransactionItem);
  virtual nsresult Pop(nsTransactionItem **aTransactionItem);
  virtual nsresult PopBottom(nsTransactionItem **aTransactionItem);
  virtual nsresult Peek(nsTransactionItem **aTransactionItem);
  virtual nsresult Clear(void);
  virtual nsresult GetSize(PRInt32 *aStackSize);
  virtual nsresult Write(nsIOutputStream *aOutputStream);
};

class nsTransactionRedoStack: public nsTransactionStack
{
public:

  virtual ~nsTransactionRedoStack();
  virtual nsresult Clear(void);
};

#endif // nsTransactionStack_h__
