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
#ifndef nsITimer_h___
#define nsITimer_h___

#include "nscore.h"
#include "nsISupports.h"

class nsITimer;
class nsITimerCallback;

/// Signature of timer callback function
typedef void
(*nsTimerCallbackFunc) (nsITimer *aTimer, void *aClosure);

/// Interface IID for nsITimer
#define NS_ITIMER_IID         \
{ 0x497eed20, 0xb740, 0x11d1,  \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

/**
 * Timer class, used to invoke a function or method after a fixed
 * millisecond interval. <B>Note that this interface is subject to
 * change!</B>
 */
class nsITimer : public nsISupports {
public:  
  /**
   * Initialize a timer to fire after the given millisecond interval.
   * This version takes a function to call and a closure to pass to
   * that function.
   *
   * @param aFunc - The function to invoke
   * @param aClosure - an opaque pointer to pass to that function
   * @param aRepeat - (Not yet implemented) One-shot or repeating
   * @param aDelay - The millisecond interval
   * @result - NS_OK if this operation was successful
   */
  virtual nsresult Init(nsTimerCallbackFunc aFunc,
                        void *aClosure,
//                      PRBool aRepeat, 
                        PRUint32 aDelay)=0;

  /**
   * Initialize a timer to fire after the given millisecond interval.
   * This version takes an interface of type <code>nsITimerCallback</code>. 
   * The <code>Notify</code> method of this method is invoked.
   *
   * @param aCallback - The interface to notify
   * @param aRepeat - (Not yet implemented) One-shot or repeating
   * @param aDelay - The millisecond interval
   * @result - NS_OK if this operation was successful
   */
  virtual nsresult Init(nsITimerCallback *aCallback,
//                      PRBool aRepeat, 
                        PRUint32 aDelay)=0;

  /// Cancels the timeout
  virtual void Cancel()=0;

  /// @return the millisecond delay of the timeout
  virtual PRUint32 GetDelay()=0;

  /// Change the millisecond interval for the timeout
  virtual void SetDelay(PRUint32 aDelay)=0;
};

/** Factory method for creating an nsITimer */
extern NS_BASE nsresult NS_NewTimer(nsITimer** aInstancePtrResult);

#endif
