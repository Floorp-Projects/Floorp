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
#ifndef globalvariables_h___
#define globalvariables_h___

#include "nslayout.h"

// forward declarations
class nsIPresContext;
class nsGlobalVariables;

/**
 * The objective is to emulate the Dogbert page setup control over printing
 * behavior. The initial implementation used the style system. However, Kipp
 * pointed out that the net result is that exactly one class used it and the
 * weight to the system was probably around >2kb for what's really a simple
 * PRBool property that only the rendering code uses  - can you say bloatware!
 *
 * Hence, this class.
 */
class nsGlobalVariables
{
private:
  /** The PRBools below are only valid if the rendering presentation context
    * is the printing presentation context.
    */
  nsIPresContext * mPresentationContext;

  /** mBeveledLines == true means print all HRs as though they have set the
    * NOSHADE attribute.
    */
  PRBool mBeveledLines;

  /** mBlackLines == true means print all HR and table ruling lines black,
    * ignoring whatever color attribute has been set.
    */
  PRBool mBlackLines;

  /** mBlackText == true means print all text black, ignoring whatever color
    * attribute has been set.
    */
  PRBool mBlackText;

  /** mBackground == true means print the background.
    */
  PRBool mBackground;

  /** mInstance is the one-and-only instantiation of this class.
    * It is accessable only through Instance
    */
  static nsGlobalVariables *gInstance;

  /** constructor
    */
  nsGlobalVariables();

public:

  /** destructor
    */
  virtual ~nsGlobalVariables();

  /** public way to release global instance variables.  
    * WARNING: This should only be called when the program exits
    * No attemp to release this prior to termination will have
    * disastrous consequences.
  */
  static NS_HTML void Release();

  /** public accessor.  
    * This is the ONLY way to get the one-and-only GlobalVariable object.
    * I own the storage for the returned pointer, so do not delete it when you're done!
    */
  static nsGlobalVariables * Instance();

  /**
    */
  PRBool GetPrinting(nsIPresContext * aPresentationContext);  

  /**
    */
  void   SetPrinting(nsIPresContext * aPresentationContext);

  /**
    */
  PRBool GetBeveledLines();

  /**
    */
  void   SetBeveledLines(PRBool aBeveledLines);

  /**
    */
  PRBool GetBlackLines();

  /**
    */
  void   SetBlackLines(PRBool aBlackLines);

  /**
    */
  PRBool GetBlackText();

  /**
    */
  void   SetBlackText(PRBool aBlackText);

  /**
    */
  PRBool GetBackground();

  /**
    */
  void   SetBackground(PRBool aBackground);

};

#endif
