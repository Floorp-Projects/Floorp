/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
#ifndef NS_TAGHANDLER___
#define NS_TAGHANDLER___


#include "nsDTDUtils.h"
#include "nsString.h"

/**
 * MODULE NOTES:
 * @update  dc 11/20/98
 **/


/**
 *  This class defines an object that can do special tag handling
 *  
 *  @update  DC 11/20/98
 */
class nsTagHandler : public nsITagHandler {

public:
  nsAutoString  mTheTagName;

public:

  /**
   * Constructor
   * @update dc 11/05/98
   */
  nsTagHandler() {}


  /**
   * Destructor
   * @update dc 11/05/98
   */
  virtual ~nsTagHandler() {}

  /**
   * Sets the string (tag) for this nsTagHandler to handle
   * @update dc 11/19/98
   * @param aTheString -- The string (tag) associated with this handler
   * @return VOID
   */
  void SetString(const nsString &aTheString) {mTheTagName = aTheString;}

  /**
   * Returns the string (tag) handled by this nsTagHandler
   * @update dc 11/19/98
   * @return The tagname associated with this class
   */
  nsString* GetString() {return &mTheTagName;}

  /**
   *  Handle this tag prior to the DTD
   *  @update  dc 11/5/98
   *  @return	 A boolean indicating if this token was handled here
   */
  virtual PRBool  HandleToken(CToken* aToken,nsIDTD* aDTD) {return PR_FALSE;};

  /**
   *  Handle this tag prior to the DTD
   *  @update  dc 11/5/98
   *  @return	 A boolean indicating if this token was handled here
   */
  virtual PRBool  HandleCapturedTokens(CToken* aToken,nsIDTD* aDTD) {return PR_FALSE;};

};

#endif 
