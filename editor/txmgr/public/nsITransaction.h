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

#ifndef nsITransaction_h__
#define nsITransaction_h__
#include "nsISupports.h"
#include "nsIOutputStream.h"
/*
Transaction interface to outside world
*/

#define NS_TRANSACTION_IID \
{ /* 58E330C1-7B48-11d2-98B9-00805F297D89 */ \
0x58e330c1, 0x7b48, 0x11d2, \
{ 0x98, 0xb9, 0x0, 0x80, 0x5f, 0x29, 0x7d, 0x89 } }


/**
 * A transaction specific interface. 
 * <P>
 * It's implemented by an object that executes some behavior that must be tracked
 * by the transaction manager.
 */
class nsITransaction  : public nsISupports{
public:

  /**
   * Execute() tells the implementation of nsITransaction to execute itself.
   */
  virtual nsresult Execute(void) = 0;

  /**
   * Undo() tells the implementation of nsITransaction to undo it's execution.
   */
  virtual nsresult Undo(void) = 0;

  /**
   * Redo() tells the implementation of nsITransaction to redo it's execution.
   */
  virtual nsresult Redo(void) = 0;

  /**
   * Write() tells the implementation of nsITransaction to write a representation
   * of itself.
   * @param nsIOutputStream *
   */
  virtual nsresult Write(nsIOutputStream *) = 0;
};

#endif // nsITransaction

