/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Frank Yung-Fong Tang <ftang@netscape.com>
 */
 
#ifndef CTextInputEventHandler_h__
#define CTextInputEventHandler_h__

#include <CarbonEvents.h>
#include <TextServices.h>

#include "nsIMacTextInputEventSink.h"

#include "CBrowserShell.h"

class CTextInputEventHandler
{
public:
  CTextInputEventHandler() {};
  virtual ~CTextInputEventHandler() {};
  
  virtual OSStatus HandleAll( EventHandlerCallRef inHandlerCallRef, 
                              EventRef inEvent);
protected:  
  virtual OSStatus HandleUnicodeForKeyEvent( CBrowserShell* aBrwoserShell, 
                                             EventHandlerCallRef inHandlerCallRef, 
                                             EventRef inEvent);
  virtual OSStatus HandleUpdateActiveInputArea( CBrowserShell* sink, 
                                                EventHandlerCallRef inHandlerCallRef, 
                                                EventRef inEvent);
  virtual OSStatus HandleOffsetToPos( CBrowserShell* aBrwoserShell, 
                                      EventHandlerCallRef inHandlerCallRef, 
                                      EventRef inEvent);
  virtual OSStatus HandlePosToOffset( CBrowserShell* aBrwoserShell, 
                                      EventHandlerCallRef inHandlerCallRef, 
                                      EventRef inEvent);
  virtual OSStatus HandleGetSelectedText( CBrowserShell* aBrwoserShell, 
                                      EventHandlerCallRef inHandlerCallRef, 
                                      EventRef inEvent);
  
  virtual CBrowserShell* GetGeckoTarget();
 
private:
  OSStatus GetText(EventRef inEvent, nsString& outString);
  OSStatus GetScriptLang(EventRef inEvent, ScriptLanguageRecord& outSlr);
};

void InitializeTextInputEventHandling();

#endif /* CTextInputEventHandler_h__ */

