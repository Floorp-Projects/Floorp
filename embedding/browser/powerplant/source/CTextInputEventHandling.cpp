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
/*
 * The embedding application could choose to install either Carbon Event Handler 
 * or Apple Event to implement Asian Input Method and/or Unicode keyboard mapping.
 *
 * This file is a sample implementation of how ot use Carbon Text Input event handler 
 * that. Example of Apple event handler could be found at nsMacTSMMessagePump.{h,cpp}
 */ 
 
#include "CTextInputEventHandler.h"
#include "nsCRT.h"

#pragma mark -

class nsSpillableStackBuffer
{
protected:

  enum {
      kStackBufferSize		= 256

  };

public:

  nsSpillableStackBuffer()
  :	mBufferPtr(mBuffer)
  , mCurCapacity(kStackBufferSize)
  {
  }

  ~nsSpillableStackBuffer()
  {
    DeleteBuffer();
  }


  PRBool EnsureCapacity(PRInt32 inCharsCapacity)
  {
    if (inCharsCapacity < mCurCapacity)
      return PR_TRUE;
    
    if (inCharsCapacity > kStackBufferSize)
    {
      DeleteBuffer();
      mBufferPtr = (PRUnichar*)nsMemory::Alloc(inCharsCapacity * sizeof(PRUnichar));
      mCurCapacity = inCharsCapacity;
      return (mBufferPtr != NULL);
    }
    
    mCurCapacity = kStackBufferSize;
    return PR_TRUE;
  }
                
  PRUnichar*  GetBuffer()     { return mBufferPtr;    }
  
  PRInt32     GetCapacity()   { return mCurCapacity;  }

protected:

  void DeleteBuffer()
  {
    if (mBufferPtr != mBuffer)
    {
      nsMemory::Free(mBufferPtr);
      mBufferPtr = mBuffer;
    }                
  }
  
protected:

  PRUnichar	    *mBufferPtr;
  PRUnichar	    mBuffer[kStackBufferSize];
  PRInt32       mCurCapacity;

};
 
#pragma mark -


//*************************************************************************************
//  GetGeckoTarget
//*************************************************************************************
CBrowserShell* CTextInputEventHandler::GetGeckoTarget()
{
  return dynamic_cast<CBrowserShell*> (LCommander::GetTarget());  
}

#pragma mark -

//*************************************************************************************
//  GetScriptLang
//*************************************************************************************
OSStatus CTextInputEventHandler::GetScriptLang(EventRef inEvent, ScriptLanguageRecord& outSlr )
{
  OSStatus err = ::GetEventParameter(inEvent, kEventParamTextInputSendSLRec, typeIntlWritingCode, NULL, 
                          sizeof(outSlr), NULL, &outSlr);
  return err;
}

//*************************************************************************************
//  GetText
//*************************************************************************************
OSStatus CTextInputEventHandler::GetText(EventRef inEvent, nsString& outString)
{
  UInt32 neededSize;
  outString.Truncate(0);
  OSStatus err = ::GetEventParameter(inEvent, kEventParamTextInputSendText, typeUnicodeText, NULL, 0, &neededSize, NULL);
  if (noErr != err)
    return err;
    
  if (neededSize > 0) 
  {
    nsSpillableStackBuffer buf;
    if (! buf.EnsureCapacity(neededSize/sizeof(PRUnichar)))
      return eventParameterNotFoundErr;

    err = ::GetEventParameter(inEvent, kEventParamTextInputSendText, typeUnicodeText, NULL, 
                            neededSize, &neededSize, buf.GetBuffer());
                            
    if (noErr == err) 
       outString.Assign(buf.GetBuffer(), neededSize/sizeof(PRUnichar));
  }
  return err;
}

#pragma mark -


//*************************************************************************************
//  HandleUnicodeForKeyEvent
//*************************************************************************************
OSStatus CTextInputEventHandler::HandleUnicodeForKeyEvent(
  CBrowserShell* aBrowserShell, 
  EventHandlerCallRef inHandlerCallRef, 
  EventRef inEvent)
{
  NS_ENSURE_TRUE(aBrowserShell, eventNotHandledErr);
  EventRef keyboardEvent;
  OSStatus err = ::GetEventParameter(inEvent, kEventParamTextInputSendKeyboardEvent, typeEventRef, NULL, 
                          sizeof(keyboardEvent), NULL, &keyboardEvent);
  
  if (noErr != err)
    return eventParameterNotFoundErr;
  
  // Check first to see if the key event is a cmd-key or control-key.
  // If so, return eventNotHandledErr, and the keydown event will end
  // up being handled by LEventDispatcher::EventKeyDown() which is the
  // standard PowerPlant handling.
  UInt32 keyModifiers;  
  err = ::GetEventParameter(keyboardEvent, kEventParamKeyModifiers, typeUInt32,
        NULL, sizeof(UInt32), NULL, &keyModifiers);
  if ((noErr == err) && (keyModifiers & (cmdKey | controlKey)))
    return eventNotHandledErr;
    
  EventRecord eventRecord;                        
                       
  if (! ::ConvertEventRefToEventRecord(keyboardEvent, &eventRecord))
    return eventParameterNotFoundErr;
  // printf("uk1- %d %d %d %d %d\n", eventRecord.what, eventRecord.message, eventRecord.when, eventRecord.where, eventRecord.modifiers);

  ScriptLanguageRecord slr;
  err = GetScriptLang(inEvent, slr);
  if (noErr != err)
    return eventParameterNotFoundErr;
  
  nsAutoString text;
  err = GetText(inEvent, text);
  if (noErr != err)
    return eventParameterNotFoundErr;
  
  // printf("call HandleUnicodeForKeyEvent textlength = %d script=%d language=%d\n",  text.Length(), slr.fScript, slr.fLanguage);
  err = aBrowserShell->HandleUnicodeForKeyEvent(text, slr.fScript, slr.fLanguage, &eventRecord);
  // printf("err = %d\n", err);
  return err;
}


//*************************************************************************************
//  HandleUpdateActiveInputArea
//*************************************************************************************
OSStatus CTextInputEventHandler::HandleUpdateActiveInputArea(
  CBrowserShell* aBrowserShell, 
  EventHandlerCallRef inHandlerCallRef, 
  EventRef inEvent)
{
  NS_ENSURE_TRUE(aBrowserShell, eventNotHandledErr);
  PRUint32 fixLength;
  OSStatus err = ::GetEventParameter(inEvent, kEventParamTextInputSendFixLen, typeLongInteger, NULL, 
                          sizeof(fixLength), NULL, &fixLength);
  if (noErr != err)
    return eventParameterNotFoundErr;

  ScriptLanguageRecord slr;
  err = GetScriptLang(inEvent, slr);
  if (noErr != err)
    return eventParameterNotFoundErr;
  
  nsAutoString text;
  err = GetText(inEvent, text);
   if (noErr != err)
    return eventParameterNotFoundErr;

  // kEventParamTextInputSendHiliteRng is optional parameter, don't return if we cannot get it.
  
  TextRangeArray* hiliteRng = nsnull;                        
  UInt32 rngSize=0;   
  err = ::GetEventParameter(inEvent, kEventParamTextInputSendHiliteRng, typeTextRangeArray, NULL, 
                          0, NULL, &rngSize);
  if (noErr == err)  
  {
    TextRangeArray* pt = (TextRangeArray*)::malloc(rngSize);
    NS_WARN_IF_FALSE( (pt), "Cannot malloc for hiliteRng") ; 
    if (pt)
    { 
      hiliteRng = pt;
      err = ::GetEventParameter(inEvent, kEventParamTextInputSendHiliteRng, typeTextRangeArray, NULL, 
                              rngSize, &rngSize, hiliteRng);
      NS_WARN_IF_FALSE( (noErr == err), "Cannot get hiliteRng") ; 
    }                          
  }                     
  // printf("call HandleUpdateActiveInputArea textlength = %d ",text.Length());
  // printf("script=%d language=%d fixlen=%d\n", slr.fScript, slr.fLanguage, fixLength / 2);
  err = aBrowserShell->HandleUpdateActiveInputArea(text, slr.fScript, slr.fLanguage,
                                          fixLength / sizeof(PRUnichar), hiliteRng);
  if (hiliteRng)
     ::free(hiliteRng);
  return err;
}
//*************************************************************************************
//  HandleUpdateActiveInputArea
//*************************************************************************************
OSStatus CTextInputEventHandler::HandleGetSelectedText(
  CBrowserShell* aBrowserShell, 
  EventHandlerCallRef inHandlerCallRef, 
  EventRef inEvent)
{
  NS_ENSURE_TRUE(aBrowserShell, eventNotHandledErr);

  nsAutoString outString;
  OSStatus err = aBrowserShell->HandleGetSelectedText(outString);   
  if (noErr != err)
    return eventParameterNotFoundErr;
  
  err = ::SetEventParameter(inEvent, kEventParamTextInputReplyText, typeUnicodeText,
                          outString.Length()*sizeof(PRUnichar), outString.get());
  return err; 
}


//*************************************************************************************
//  HandleOffsetToPos
//*************************************************************************************
OSStatus CTextInputEventHandler::HandleOffsetToPos(
  CBrowserShell* aBrowserShell, 
  EventHandlerCallRef inHandlerCallRef, 
  EventRef inEvent)
{
  NS_ENSURE_TRUE(aBrowserShell, eventNotHandledErr);
  PRUint32 offset;
  OSStatus err = ::GetEventParameter(inEvent, kEventParamTextInputSendTextOffset, typeLongInteger, NULL, 
                          sizeof(offset), NULL, &offset);
  if (noErr != err)
    return eventParameterNotFoundErr;

  Point thePoint;
  // printf("call HandleOffsetToPos offset = %d\n", offset);
  err = aBrowserShell->HandleOffsetToPos(offset, &thePoint.h, &thePoint.v);   
  // printf("return %d, %d\n", thePoint.v, thePoint.h);                          
  if (noErr != err)
    return eventParameterNotFoundErr;
  
  err = ::SetEventParameter(inEvent, kEventParamTextInputReplyPoint, typeQDPoint,
                          sizeof(thePoint), &thePoint);
  return err; 
}


//*************************************************************************************
//  HandlePosToOffset
//*************************************************************************************
OSStatus CTextInputEventHandler::HandlePosToOffset(
  CBrowserShell* aBrowserShell, 
  EventHandlerCallRef inHandlerCallRef, 
  EventRef inEvent)
{
  NS_ENSURE_TRUE(aBrowserShell, eventNotHandledErr);
  PRInt32 offset;
  Point thePoint;
  short regionClass;

  OSStatus err = ::GetEventParameter(inEvent, kEventParamTextInputSendCurrentPoint, typeQDPoint, NULL, 
                                   sizeof(thePoint), NULL, &thePoint);
  if (noErr != err)
    return eventParameterNotFoundErr;

  // printf("call HandlePosToOffset Point = %d, %d\n", thePoint.h, thePoint.v);
  err = aBrowserShell->HandlePosToOffset(thePoint.h, thePoint.v, &offset, &regionClass);
  // printf("return offset = %d, region = %d\n", offset, regionClass);                          
  if (noErr != err)
    return eventParameterNotFoundErr;

  err = ::SetEventParameter(inEvent, kEventParamTextInputReplyRegionClass, typeShortInteger,
                          sizeof(regionClass), &regionClass);
  if (noErr != err)
    return eventParameterNotFoundErr;
  
  err = ::SetEventParameter(inEvent, kEventParamTextInputReplyTextOffset, typeLongInteger,
                          sizeof(offset), &offset);
  return err;   
}


//*************************************************************************************
//  HandleAll
//*************************************************************************************
OSStatus CTextInputEventHandler::HandleAll(EventHandlerCallRef inHandlerCallRef, EventRef inEvent)
{
  CBrowserShell* aBrowserShell = GetGeckoTarget();
  // if this is not an event for Gecko, just return eventNotHandledErr and let 
  // OS to fallback to class event handling 
  if (!aBrowserShell)
    return eventNotHandledErr;
  
  UInt32 eventClass = ::GetEventClass(inEvent);
  if (eventClass != kEventClassTextInput)
    return eventNotHandledErr;

  UInt32 eventKind = ::GetEventKind(inEvent);
  if ((kEventTextInputUpdateActiveInputArea != eventKind) &&
                  (kEventTextInputUnicodeForKeyEvent!= eventKind) &&
                  (kEventTextInputOffsetToPos != eventKind) &&
                  (kEventTextInputPosToOffset != eventKind) &&
                  (kEventTextInputGetSelectedText != eventKind))
    return eventNotHandledErr;
    
  switch(eventKind)
  {
    case kEventTextInputUpdateActiveInputArea:
      return HandleUpdateActiveInputArea(aBrowserShell, inHandlerCallRef, inEvent);
    case kEventTextInputUnicodeForKeyEvent:
      return HandleUnicodeForKeyEvent(aBrowserShell, inHandlerCallRef, inEvent);
    case kEventTextInputOffsetToPos:
      return HandleOffsetToPos(aBrowserShell, inHandlerCallRef, inEvent);
    case kEventTextInputPosToOffset:
      return HandlePosToOffset(aBrowserShell, inHandlerCallRef, inEvent);
    case kEventTextInputGetSelectedText:
      return HandleGetSelectedText(aBrowserShell, inHandlerCallRef, inEvent);
  }
  return eventNotHandledErr;
}


#pragma mark -

//*************************************************************************************
//  TextInputHandler
//*************************************************************************************
static pascal OSStatus TextInputHandler(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
{
   CTextInputEventHandler* realHandler = (CTextInputEventHandler*)inUserData;
   return realHandler->HandleAll(inHandlerCallRef, inEvent);
}

//*************************************************************************************
//  InitializeTextInputEventHandling
//*************************************************************************************
void InitializeTextInputEventHandling()
{
  static CTextInputEventHandler Singleton;
  EventTypeSpec eventTypes[5] = {
    {kEventClassTextInput, kEventTextInputUpdateActiveInputArea },
    {kEventClassTextInput, kEventTextInputUnicodeForKeyEvent },
    {kEventClassTextInput, kEventTextInputOffsetToPos },
    {kEventClassTextInput, kEventTextInputPosToOffset },
    {kEventClassTextInput, kEventTextInputGetSelectedText }
  };  
  
  EventHandlerUPP textInputUPP = NewEventHandlerUPP(TextInputHandler); 
  OSStatus err = InstallApplicationEventHandler( textInputUPP, 5, eventTypes, &Singleton, NULL);
  NS_ASSERTION(err==noErr, "Cannot install carbon event");
}
