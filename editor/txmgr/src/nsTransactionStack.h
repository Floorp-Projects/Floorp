/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsTransactionStack_h__
#define nsTransactionStack_h__

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
  nsDeque mQue;

public:

  nsTransactionStack();
  virtual ~nsTransactionStack();

  virtual nsresult Push(nsTransactionItem *aTransactionItem);
  virtual nsresult Pop(nsTransactionItem **aTransactionItem);
  virtual nsresult PopBottom(nsTransactionItem **aTransactionItem);
  virtual nsresult Peek(nsTransactionItem **aTransactionItem);
  virtual nsresult GetItem(PRInt32 aIndex, nsTransactionItem **aTransactionItem);
  virtual nsresult Clear(void);
  virtual nsresult GetSize(PRInt32 *aStackSize);
};

class nsTransactionRedoStack: public nsTransactionStack
{
public:

  virtual ~nsTransactionRedoStack();
  virtual nsresult Clear(void);
};

#endif // nsTransactionStack_h__
