/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <ctype.h> 
#include <time.h>
#include <stdio.h>  
#include "nsScanner.h"
#include "nsToken.h" 
#include "nsHTMLTokens.h"
#include "prtypes.h"
#include "nsDebug.h"
#include "nsHTMLTags.h"
#include "nsHTMLEntities.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsScanner.h"


static const char*  gUserdefined = "userdefined";

static const PRUnichar kAttributeTerminalChars[] = {
  PRUnichar('&'), PRUnichar('\b'), PRUnichar('\t'), 
  PRUnichar('\n'), PRUnichar('\r'), PRUnichar(' '),  
  PRUnichar('>'),  
  PRUnichar(0) 
};
                   

/**************************************************************
  And now for the token classes...
 **************************************************************/

/*
 *  constructor from tag id
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CHTMLToken::CHTMLToken(eHTMLTags aTag) : CToken(aTag) {
}


CHTMLToken::~CHTMLToken() {

}

/*
 *  constructor from tag id
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CStartToken::CStartToken(eHTMLTags aTag) : CHTMLToken(aTag) {
  mAttributed=PR_FALSE;
  mEmpty=PR_FALSE;
  mOrigin=-1;
  mContainerInfo=eFormUnknown;
}

CStartToken::CStartToken(const nsAReadableString& aName) : CHTMLToken(eHTMLTag_unknown) {
  mAttributed=PR_FALSE;
  mEmpty=PR_FALSE;
  mOrigin=-1;
  mContainerInfo=eFormUnknown;
  mTextValue.Assign(aName);
}

CStartToken::CStartToken(const nsAReadableString& aName,eHTMLTags aTag) : CHTMLToken(aTag) {
  mAttributed=PR_FALSE;
  mEmpty=PR_FALSE;
  mOrigin=-1;
  mContainerInfo=eFormUnknown;
  mTextValue.Assign(aName);
}

nsresult CStartToken::GetIDAttributeAtom(nsIAtom** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mIDAttributeAtom;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}


nsresult CStartToken::SetIDAttributeAtom(nsIAtom* aID)
{
  NS_ENSURE_ARG(aID);
  mIDAttributeAtom = aID;

  return NS_OK;
}


/*
 *  This method returns the typeid (the tag type) for this token.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CStartToken::GetTypeID(){
  if(eHTMLTag_unknown==mTypeID) {
    mTypeID = nsHTMLTags::LookupTag(mTextValue);
  }
  return mTypeID;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CStartToken::GetClassName(void) {
  return "start";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CStartToken::GetTokenType(void) {
  return eToken_start;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void CStartToken::SetAttributed(PRBool aValue) {
  mAttributed=aValue;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRBool CStartToken::IsAttributed(void) {
  return mAttributed;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void CStartToken::SetEmpty(PRBool aValue) {
  mEmpty=aValue;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRBool CStartToken::IsEmpty(void) {
  return mEmpty;
}


/*
 *  Consume the identifier portion of the start tag
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @param   aFlag - contains information such as |dtd mode|view mode|doctype|etc...
 *  @return  error result
 */
nsresult CStartToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {

  //if you're here, we've already Consumed the < char, and are
   //ready to Consume the rest of the open tag identifier.
   //Stop consuming as soon as you see a space or a '>'.
   //NOTE: We don't Consume the tag attributes here, nor do we eat the ">"

  nsresult result=NS_OK;
  if (aFlag & NS_IPARSER_FLAG_HTML) {
    nsAutoString theSubstr;
    result=aScanner.GetIdentifier(theSubstr,PR_TRUE);
    mTypeID = (PRInt32)nsHTMLTags::LookupTag(theSubstr);
    if(eHTMLTag_userdefined==mTypeID) {
      mTextValue=theSubstr;
    }
  }
  else {
    //added PR_TRUE to readId() call below to fix bug 46083. The problem was that the tag given
    //was written <title_> but since we didn't respect the '_', we only saw <title>. Then 
    //we searched for end title, which never comes (they give </title_>). 

    result=aScanner.ReadIdentifier(mTextValue,PR_TRUE);  
    mTypeID = nsHTMLTags::LookupTag(mTextValue);
  }

  return result;
}


#ifdef DEBUG
/*
 *  Dump contents of this token to givne output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CStartToken::DebugDumpSource(nsOutputStream& out) {
  out << "<" << NS_LossyConvertUCS2toASCII(mTextValue).get();
  if(!mAttributed)
    out << ">";
}
#endif

const nsAReadableString& CStartToken::GetStringValue()
{
  if((eHTMLTag_unknown<mTypeID) && (mTypeID<eHTMLTag_text)) {
    if(!mTextValue.Length()) {
      mTextValue.AssignWithConversion(nsHTMLTags::GetStringValue((nsHTMLTag) mTypeID));
    }
  }
  return mTextValue;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   anOutputString will recieve the result
 *  @return  nada
 */
void CStartToken::GetSource(nsString& anOutputString){
  anOutputString.AppendWithConversion("<");
  /*
   * Watch out for Bug 15204 
   */
  if(mTrailingContent.Length()>0)
    anOutputString=mTrailingContent;
  else {
    if(mTextValue.Length()>0)
      anOutputString.Append(mTextValue);
    else
     anOutputString.AssignWithConversion(GetTagName(mTypeID));
    anOutputString.AppendWithConversion('>');
  }
}

/*
 *  
 *  
 *  @update  harishd 03/23/00
 *  @param   result appended to the output string.
 *  @return  nada
 */
void CStartToken::AppendSource(nsString& anOutputString){
  anOutputString.AppendWithConversion("<");
  /*
   * Watch out for Bug 15204 
   */
  if(mTrailingContent.Length()>0)
    anOutputString+=mTrailingContent;
  else {
    if(mTextValue.Length()>0)
      anOutputString+=mTextValue;
    else
     anOutputString.AppendWithConversion(GetTagName(mTypeID));
    anOutputString.AppendWithConversion('>');
  }
}

/*
 *  constructor from tag id
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CEndToken::CEndToken(eHTMLTags aTag) : CHTMLToken(aTag) {
}

CEndToken::CEndToken(const nsAReadableString& aName) : CHTMLToken(eHTMLTag_unknown) {
  mTextValue.Assign(aName);
}

CEndToken::CEndToken(const nsAReadableString& aName,eHTMLTags aTag) : CHTMLToken(aTag) {
  mTextValue.Assign(aName);
}

/*
 *  Consume the identifier portion of the end tag
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @param   aFlag - contains information such as |dtd mode|view mode|doctype|etc...
 *  @return  error result
 */
nsresult CEndToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {
  //if you're here, we've already Consumed the <! chars, and are
   //ready to Consume the rest of the open tag identifier.
   //Stop consuming as soon as you see a space or a '>'.
   //NOTE: We don't Consume the tag attributes here, nor do we eat the ">"

  nsresult result=NS_OK;
  nsAutoString buffer;
  PRInt32 offset;
  if (aFlag & NS_IPARSER_FLAG_HTML) {
    nsAutoString theSubstr;
    result=aScanner.ReadUntil(theSubstr,kGreaterThan,PR_FALSE);
    if (NS_FAILED(result)) {
      return result;
    }
    
    offset = theSubstr.FindCharInSet(" \r\n\t\b",0);
    if (offset != kNotFound) {
      theSubstr.Left(buffer, offset);
      mTypeID = nsHTMLTags::LookupTag(buffer);
    }
    else {
      mTypeID = nsHTMLTags::LookupTag(theSubstr);
    }

    if(eHTMLTag_userdefined==mTypeID) {
      mTextValue=theSubstr;
    }

  }
  else {
    mTextValue.SetLength(0);
    result=aScanner.ReadUntil(mTextValue,kGreaterThan,PR_FALSE);
    if (NS_FAILED(result)) {
      return result;
    }
    mTypeID = eHTMLTag_userdefined;
  }

  result=aScanner.GetChar(aChar); //eat the closing '>;

  return result;
}


/*
 *  Asks the token to determine the <i>HTMLTag type</i> of
 *  the token. This turns around and looks up the tag name
 *  in the tag dictionary.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  eHTMLTag id of this endtag
 */
PRInt32 CEndToken::GetTypeID(){
  if(eHTMLTag_unknown==mTypeID) {
    mTypeID = nsHTMLTags::LookupTag(mTextValue);
    switch(mTypeID) {
      case eHTMLTag_dir:
      case eHTMLTag_menu:
        mTypeID=eHTMLTag_ul;
        break;
      default:
        break;
    }
  }
  return mTypeID;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CEndToken::GetClassName(void) {
  return "/end";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CEndToken::GetTokenType(void) {
  return eToken_end;
}

#ifdef DEBUG
/*
 *  Dump contents of this token to givne output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CEndToken::DebugDumpSource(nsOutputStream& out) {
  out << "</" << NS_LossyConvertUCS2toASCII(mTextValue).get() << ">";
}
#endif

const nsAReadableString& CEndToken::GetStringValue()
{
  if((eHTMLTag_unknown<mTypeID) && (mTypeID<eHTMLTag_text)) {
    if(!mTextValue.Length()) {
      mTextValue.AssignWithConversion(nsHTMLTags::GetStringValue((nsHTMLTag) mTypeID));
    }
  }
  return mTextValue;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   anOutputString will recieve the result
 *  @return  nada
 */
void CEndToken::GetSource(nsString& anOutputString){
  anOutputString.AppendWithConversion("</");
  if(mTextValue.Length()>0)
    anOutputString.Append(mTextValue);
  else
    anOutputString.AppendWithConversion(GetTagName(mTypeID));
  anOutputString.AppendWithConversion(">");
}

/*
 *  
 *  
 *  @update  harishd 03/23/00
 *  @param   result appended to the output string.
 *  @return  nada
 */
void CEndToken::AppendSource(nsString& anOutputString){
  anOutputString.AppendWithConversion("</");
  if(mTextValue.Length()>0)
    anOutputString.Append(mTextValue);
  else
    anOutputString.AppendWithConversion(GetTagName(mTypeID));
  anOutputString.AppendWithConversion(">");
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CTextToken::CTextToken() : CHTMLToken(eHTMLTag_text) {
}


/*
 *  string based constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CTextToken::CTextToken(const nsAReadableString& aName) : CHTMLToken(eHTMLTag_text) {
  mTextValue.Rebind(aName);
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CTextToken::GetClassName(void) {
  return "text";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CTextToken::GetTokenType(void) {
  return eToken_text;
}

PRInt32 CTextToken::GetTextLength(void) {
  return mTextValue.Length();
}

/*
 *  Consume as much clear text from scanner as possible.
 *
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CTextToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {
  static const PRUnichar theTerminalsChars[] = 
    { PRUnichar('\n'), PRUnichar('\r'), PRUnichar('&'), PRUnichar('<'),
      PRUnichar(0) };
  static const nsReadEndCondition theEndCondition(theTerminalsChars);
  nsresult  result=NS_OK;
  PRBool    done=PR_FALSE;
  nsReadingIterator<PRUnichar> origin, start, end;
  
  // Start scanning after the first character, because we know it to
  // be part of this text token (we wouldn't have come here if it weren't)
  aScanner.CurrentPosition(origin);
  start = origin;
  ++start;
  aScanner.SetPosition(start);
  aScanner.EndReading(end);

  while((NS_OK==result) && (!done)) {
    result=aScanner.ReadUntil(start, end, theEndCondition, PR_FALSE);
    if(NS_OK==result) {
      result=aScanner.Peek(aChar);

      if(((kCR==aChar) || (kNewLine==aChar)) && (NS_OK==result)) {
        result=aScanner.GetChar(aChar); //strip off the char
        PRUnichar theNextChar;
        result=aScanner.Peek(theNextChar);    //then see what's next.
        switch(aChar) {
          case kCR:
            // result=aScanner.GetChar(aChar);       
            if(kLF==theNextChar) {
              // If the "\r" is followed by a "\n", don't replace it and 
              // let it be ignored by the layout system
              end.advance(2);
              result=aScanner.GetChar(theNextChar);
            }
            else {
              // If it standalone, replace the "\r" with a "\n" so that 
              // it will be considered by the layout system
              aScanner.ReplaceCharacter(end, kLF);
              ++end;
            }
            ++mNewlineCount;
            break;
          case kLF:
            ++end;
            ++mNewlineCount;
            break;
        } //switch
      }
      else done=PR_TRUE;
    }
  }
  
  aScanner.BindSubstring(mTextValue, origin, end);

  return result;
}

/*
 *  Consume as much clear text from scanner as possible.
 *
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CTextToken::ConsumeUntil(PRUnichar aChar,PRBool aIgnoreComments,nsScanner& aScanner,
                                  nsString& aEndTagName,PRInt32 aFlag,PRBool& aFlushTokens){
  nsresult      result=NS_OK;
  nsReadingIterator<PRUnichar> theStartOffset, theCurrOffset, theTermStrPos, theStartCommentPos, theAltTermStrPos, endPos;
  PRBool        done=PR_FALSE;
  PRBool        theLastIteration=PR_FALSE;

  aScanner.CurrentPosition(theStartOffset);
  theCurrOffset = theStartOffset;
  aScanner.EndReading(endPos);
  theTermStrPos = theStartCommentPos = theAltTermStrPos = endPos;

  // ALGORITHM: *** The performance is based on correctness of the document ***
  // 1. Look for a '<' character.  This could be
  //    a) Start of a comment (<!--), b) Start of the terminal string, or c) a start of a tag.
  //    We are interested in a) and b). c) is ignored because in CDATA we don't care for tags.
  //    NOTE: Technically speaking in CDATA we should ignore the comments too!! But for compatibility
  //          we don't.
  // 2. Having the offset, for '<', search for the terminal string from there on and record its offset.
  // 3. From the same '<' offset also search for start of a comment '<!--'. If found search for
  //    end comment '-->' between the terminal string and '<!--'.  If you did not find the end
  //    comment, then we have a malformed document, i.e., this section has a prematured terminal string
  //    Ex. <SCRIPT><!-- document.write('</SCRIPT>') //--> </SCRIPT>. But anyway record terminal string's
  //    offset and update the current offset to the terminal string (prematured) offset and goto step 1.
  // 4. Amen...If you found a terminal string and '-->'. Otherwise goto step 1.
  // 5. If the end of the document is reached and if we still don't have the condition in step 4. then
  //    assume that the prematured terminal string is the actual terminal string and goto step 1. This
  //    will be our last iteration.
  nsAutoString theTerminalString(aEndTagName);
  theTerminalString.InsertWithConversion("</",0,2);

  PRUint32 termStrLen=theTerminalString.Length();
  while((result == NS_OK) && !done) {
    PRBool found = PR_FALSE;
    nsReadingIterator<PRUnichar> gtOffset,ltOffset = theCurrOffset;
    while (FindCharInReadable(PRUnichar(kLessThan), ltOffset, endPos) &&
           Distance(ltOffset, endPos) >= termStrLen) {
      // Make a copy of the (presumed) end tag and
      // do a case-insensitive comparision

      nsReadingIterator<PRUnichar> start(ltOffset), end(ltOffset);
      end.advance(termStrLen);

      if (CaseInsensitiveFindInReadable(theTerminalString,start,end) && 
          end != endPos && (*end == '>'  || *end == ' '  || 
                            *end == '\t' || *end == '\n' || 
                            *end == '\r' || *end == '\b')) {
        gtOffset = end;
        if (FindCharInReadable(PRUnichar(kGreaterThan), gtOffset, endPos)) {
          found = PR_TRUE;
          theTermStrPos = start;
        }
        break;
      }
      ltOffset.advance(1);
    }
     
    if (found && theTermStrPos != endPos) {
      if(!(aFlag & NS_IPARSER_FLAG_STRICT_MODE) &&
         !theLastIteration && !aIgnoreComments) {
        nsReadingIterator<PRUnichar> endComment(ltOffset);
        endComment.advance(5);
         
        if ((theStartCommentPos == endPos) &&
            FindInReadable(NS_LITERAL_STRING("<!--"), theCurrOffset, endComment)) {
          theStartCommentPos = theCurrOffset;
        }
         
        if (theStartCommentPos != endPos) {
          // Search for --> between <!-- and </TERMINALSTRING>.
          theCurrOffset = theStartCommentPos;
          nsReadingIterator<PRUnichar> terminal(theTermStrPos);
          if (!RFindInReadable(NS_LITERAL_STRING("-->"),
                               theCurrOffset, terminal)) {
            // If you're here it means that we have a bogus terminal string.
            // Even though it is bogus, the position of the terminal string
            // could be helpful in case we hit the rock bottom.
            theAltTermStrPos = theTermStrPos;
    
            // We did not find '-->' so keep searching for terminal string.
            theCurrOffset = theTermStrPos;
            theCurrOffset.advance(termStrLen);
            continue;
          }
        }
      }

      // Make sure to preserve the end tag's representation in viewsource
      if(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
        CopyUnicodeTo(ltOffset.advance(2),gtOffset,aEndTagName);
      }

      aScanner.BindSubstring(mTextValue, theStartOffset, theTermStrPos);
      aScanner.SetPosition(gtOffset.advance(1));
      
      // We found </SCRIPT>...permit flushing -> Ref: Bug 22485
      aFlushTokens=PR_TRUE;
      done = PR_TRUE;
    }
    else {
      // We end up here if:
      // a) when the buffer runs out ot data.
      // b) when the terminal string is not found.
      if(!aScanner.IsIncremental()) {
        if(theAltTermStrPos != endPos) {
          // If you're here it means..we hit the rock bottom and therefore switch to plan B.
          theCurrOffset = theAltTermStrPos;
          theLastIteration = PR_TRUE;
        }
        else {
          done = PR_TRUE; // Do this to fix Bug. 35456
        }
      }
      else {
       result=kEOF;
      }
    }
  }
  return result;
}

void CTextToken::CopyTo(nsAWritableString& aStr)
{
  aStr.Assign(mTextValue);
}

const nsAReadableString& CTextToken::GetStringValue(void)
{
  return mTextValue;
}

void CTextToken::Bind(nsScanner* aScanner, nsReadingIterator<PRUnichar>& aStart, nsReadingIterator<PRUnichar>& aEnd)
{
  aScanner->BindSubstring(mTextValue, aStart, aEnd);
}

void CTextToken::Bind(const nsAReadableString& aStr)
{
  mTextValue.Rebind(aStr);
}

/*
 *  default constructor
 *  
 *  @update  vidur 11/12/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CCDATASectionToken::CCDATASectionToken() : CHTMLToken(eHTMLTag_unknown) {
}


/*
 *  string based constructor
 *  
 *  @update  vidur 11/12/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CCDATASectionToken::CCDATASectionToken(const nsAReadableString& aName) : CHTMLToken(eHTMLTag_unknown) {
  mTextValue.Assign(aName);
}

/*
 *  
 *  
 *  @update  vidur 11/12/98
 *  @param   
 *  @return  
 */
const char*  CCDATASectionToken::GetClassName(void) {
  return "cdatasection";
}

/*
 *  
 *  @update  vidur 11/12/98
 *  @param   
 *  @return  
 */
PRInt32 CCDATASectionToken::GetTokenType(void) {
  return eToken_cdatasection;
}

/*
 *  Consume as much marked test from scanner as possible.
 *
 *  @update  rgess 12/15/99: had to handle case: "<![ ! IE 5]>", in addition to "<![..[..]]>".
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CCDATASectionToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {
  static const PRUnichar theTerminalsChars[] = 
  { PRUnichar('\r'), PRUnichar(']'), PRUnichar(0) };
  static const nsReadEndCondition theEndCondition(theTerminalsChars);
  nsresult  result=NS_OK;
  PRBool    done=PR_FALSE;

  while((NS_OK==result) && (!done)) {
    result=aScanner.ReadUntil(mTextValue,theEndCondition,PR_FALSE);
    if(NS_OK==result) {
      result=aScanner.Peek(aChar);
      if((kCR==aChar) && (NS_OK==result)) {
        result=aScanner.GetChar(aChar); //strip off the \r
        result=aScanner.Peek(aChar);    //then see what's next.
        if(NS_OK==result) {
          switch(aChar) {
            case kCR:
              result=aScanner.GetChar(aChar); //strip off the \r
              mTextValue.AppendWithConversion("\n\n");
              break;
            case kNewLine:
               //which means we saw \r\n, which becomes \n
              result=aScanner.GetChar(aChar); //strip off the \n
                  //now fall through on purpose...
            default:
              mTextValue.AppendWithConversion("\n");
              break;
          } //switch
        } //if
      }
      else if (']'==aChar) {        
        result=aScanner.GetChar(aChar); //strip off the ]
        mTextValue.Append(aChar);
        result=aScanner.Peek(aChar);    //then see what's next.
        if((NS_OK==result) && (kRightSquareBracket==aChar)) {
          result=aScanner.GetChar(aChar);    //strip off the second ]
          mTextValue.Append(aChar);
          result=aScanner.Peek(aChar);    //then see what's next.
        }
        if((NS_OK==result) && (kGreaterThan==aChar)) {
          result=aScanner.GetChar(aChar); //strip off the >
          done=PR_TRUE;
        }
      }
      else done=PR_TRUE;
    }
  }
  return result;
}

const nsAReadableString& CCDATASectionToken::GetStringValue(void)
{
  return mTextValue;
}


/*
 *  default constructor
 *  
 *  @param   aName -- string to init token name with
 *  @return  
 */
CMarkupDeclToken::CMarkupDeclToken() : CHTMLToken(eHTMLTag_markupDecl) {
}


/*
 *  string based constructor
 *  
 *  @param   aName -- string to init token name with
 *  @return  
 */
CMarkupDeclToken::CMarkupDeclToken(const nsAReadableString& aName) : CHTMLToken(eHTMLTag_markupDecl) {
  mTextValue.Rebind(aName);
}

/*
 *  
 *  
 *  @param   
 *  @return  
 */
const char*  CMarkupDeclToken::GetClassName(void) {
  return "markupdeclaration";
}

/*
 *  
 *  @param   
 *  @return  
 */
PRInt32 CMarkupDeclToken::GetTokenType(void) {
  return eToken_markupDecl;
}

/*
 *  Consume as much declaration from scanner as possible.
 *  Declaration is a markup declaration of ELEMENT, ATTLIST, ENTITY or
 *  NOTATION, which can span multiple lines and ends in >.
 *
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CMarkupDeclToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {
  static const PRUnichar theTerminalsChars[] = 
    { PRUnichar('\n'), PRUnichar('\r'), PRUnichar('\''), PRUnichar('"'),
      PRUnichar('>'),
      PRUnichar(0) };
  static const nsReadEndCondition theEndCondition(theTerminalsChars);
  nsresult  result=NS_OK;
  PRBool    done=PR_FALSE;
  PRUnichar quote=0;

  nsReadingIterator<PRUnichar> origin, start, end;
  aScanner.CurrentPosition(origin);
  start = origin;

  while((NS_OK==result) && (!done)) {
    aScanner.SetPosition(start);
    result=aScanner.ReadUntil(start, end, theEndCondition, PR_FALSE);
    if(NS_OK==result) {
      result=aScanner.Peek(aChar);

      if(NS_OK==result) {
        PRUnichar theNextChar=0;
        if ((kCR==aChar) || (kNewLine==aChar)) {
          result=aScanner.GetChar(aChar); //strip off the char
          result=aScanner.Peek(theNextChar);    //then see what's next.
        }
        switch(aChar) {
          case kCR:
            // result=aScanner.GetChar(aChar);       
            if(kLF==theNextChar) {
              // If the "\r" is followed by a "\n", don't replace it and 
              // let it be ignored by the layout system
              end.advance(2);
              result=aScanner.GetChar(theNextChar);
            }
            else {
              // If it standalone, replace the "\r" with a "\n" so that 
              // it will be considered by the layout system
              aScanner.ReplaceCharacter(end, kLF);
              ++end;
            }
            ++mNewlineCount;
            break;
          case kLF:
            ++end;
            ++mNewlineCount;
            break;
          case '\'':
          case '"':
            ++end;
            if (quote) {
              if (quote == aChar) {
                quote = 0;
              }
            } else {
              quote = aChar;
            }
            break;
          case kGreaterThan:
            if (quote) {
              ++end;
            } else {
              start = end;
              ++start;  // Note that start is wrong after this, we just avoid temp var
              aScanner.SetPosition(start); // Skip the >
              done=PR_TRUE;
            }
            break;
          default:
            NS_ABORT_IF_FALSE(0,"should not happen, switch is missing cases?");
            break;
        } //switch
        start = end;
      }
      else done=PR_TRUE;
    } // if read until !ok
  } // while
  
  aScanner.BindSubstring(mTextValue, origin, end);

  return result;
}

const nsAReadableString& CMarkupDeclToken::GetStringValue(void)
{
  return mTextValue;
}


/*
 *  Default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CCommentToken::CCommentToken() : CHTMLToken(eHTMLTag_comment) {
}


/*
 *  Copy constructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CCommentToken::CCommentToken(const nsAReadableString& aName) : CHTMLToken(eHTMLTag_comment) {
  mTextValue.Assign(aName);
}

/*
 *  This method consumes a comment using the (CORRECT) comment parsing
 *  algorithm according to ISO 8879 (SGML).
 *  
 *  @update  gess 01/04/99
 *  @param   
 *  @param   
 *  @return  
 */
static
nsresult ConsumeStrictComment(PRUnichar aChar, nsScanner& aScanner,nsString& aString) {
  nsresult  result=NS_OK;
  static const PRUnichar theTerminalsChars[] = 
    { PRUnichar('-'), PRUnichar('>'), PRUnichar(0) };
  static const nsReadEndCondition endCommentCondition(theTerminalsChars);
 
  /*********************************************************
    NOTE: This algorithm does a fine job of handling comments
          when they're formatted per spec, but if they're not
          we don't handle them well. For example, we gack
          on the following:

          <!-- xx -- xx --> 
   *********************************************************/

  aString.AssignWithConversion("<!");
  while(NS_OK==result) {
    result=aScanner.GetChar(aChar);
    if(NS_OK==result) {
      aString+=aChar;
      if(kMinus==aChar) {
        result=aScanner.GetChar(aChar);
        if(NS_OK==result) {
          if(kMinus==aChar) {
               //in this case, we're reading a long-form comment <-- xxx -->
            aString+=aChar;
            if(NS_OK==result) {
              //Find the first ending sequence '--'
              PRBool found = PR_FALSE;
              nsAutoString temp;
              do {
                result = aScanner.ReadUntil(temp,kMinus,PR_TRUE);
                if (NS_FAILED(result)) return result;
                  
                result = aScanner.Peek(aChar);
                if (NS_FAILED(result)) return result;
                  
                if (aChar == kMinus) {
                  // found matching '--'
                  found = PR_TRUE;
                  aScanner.GetChar(aChar);
                  aString += temp;
                  aString += aChar;
                  // Read until "-" or ">"
                  result = aScanner.ReadUntil(aString,endCommentCondition,PR_FALSE);
                }
              } while (!found);
            }
          } //
          else break; //go find '>'
        }
      }//if
      else if(kGreaterThan==aChar) {
        return result;
      }
      else break; //go find '>'
    }//if
  }//while
  if(NS_OK==result) {
     //Read up to the closing '>', unless you already did!  (such as <!>).
    if(kGreaterThan!=aChar) {
      result=aScanner.ReadUntil(aString,kGreaterThan,PR_TRUE);
    }
  }
  return result;
}

/*
 *  This method consumes a comment using common (actually non-standard)
 *  algorithm that seems to work against the content on the web.
 *  
 *  @update  gess 01/04/99
 *  @param   
 *  @param   
 *  @return  
 */
static
nsresult ConsumeComment(PRUnichar aChar, nsScanner& aScanner,nsString& aString) {
  

  nsresult  result=NS_OK;
 
  /*********************************************************
    NOTE: This algorithm does a fine job of handling comments
          commonly used, but it doesn't really consume them
          per spec (But then, neither does IE or Nav).
   *********************************************************/

  nsReadingIterator<PRUnichar> theStartOffset, theCurrOffset, theBestAltPos, endPos;
  aScanner.EndReading(endPos);
  theBestAltPos = endPos;

  aScanner.CurrentPosition(theStartOffset);
  result=aScanner.GetChar(aChar);
  if(NS_OK==result) {
    if(kMinus==aChar) {
      result=aScanner.GetChar(aChar);
      if(NS_OK==result) {
        if(kMinus==aChar) {
          //in this case, we're reading a long-form comment <-- xxx -->

          theCurrOffset = theStartOffset;
        
          while((NS_OK==result)) {
            if (FindCharInReadable(PRUnichar(kGreaterThan), theCurrOffset, endPos)) {
              ++theCurrOffset;
              nsReadingIterator<PRUnichar> temp = theCurrOffset;
              temp.advance(-3);
              aChar=*temp;
              if(kMinus==aChar) {
                ++temp;
                aChar=*temp;
                if(kMinus==aChar) {
                  theStartOffset.advance(-2); // Include "<!" also..
                  CopyUnicodeTo(theStartOffset, theCurrOffset, aString);
                  aScanner.SetPosition(theCurrOffset);
                  return result; // We have found the dflt end comment delimiter ("-->")
                }
              }
              if(theBestAltPos == endPos) {
                // If we did not find the dflt then assume that '>' is the end comment
                // until we find '-->'. Nav. Compatibility -- Ref: Bug# 24006
                theBestAltPos=theCurrOffset;
              }
            }
            else {
              result=kEOF;
            }
          } //while
          if((endPos==theCurrOffset) && (!aScanner.IsIncremental())) {
            //if you're here, then we're in a special state. 
            //The problem at hand is that we've hit the end of the document without finding the normal endcomment delimiter "-->".
            //In this case, the first thing we try is to see if we found one of the alternate endcomment delimiter ">".
            //If so, rewind just pass than, and use everything up to that point as your comment.
            //If not, the document has no end comment and should be treated as one big comment.
            if(endPos != theBestAltPos) {
              theStartOffset.advance(-2);// Include "<!" also..
              CopyUnicodeTo(theStartOffset, theBestAltPos, aString);
              aScanner.SetPosition(theBestAltPos);
              result=NS_OK;
            }
          }
          return result;

        } //if
      }//if
    }//if
  }//if
  if(NS_OK==result) {
     //Read up to the closing '>', unless you already did!  (such as <!>).
    if(kGreaterThan!=aChar) {
      aString.AppendWithConversion("<!- ");
      result=aScanner.ReadUntil(aString,kGreaterThan,PR_TRUE);
    }
  }
  return result;
}

/*
 *  Consume the identifier portion of the comment. 
 *  Note that we've already eaten the "<!" portion.
 *  
 *  @update  gess 16June2000
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CCommentToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {
  nsresult result=PR_TRUE;
  
  if (aFlag & NS_IPARSER_FLAG_STRICT_MODE) {
    //Enabling strict comment parsing for Bug 53011 and  2749 contradicts!!!!
    result=ConsumeStrictComment(aChar,aScanner,mTextValue);
  }
  else {
    result=ConsumeComment(aChar,aScanner,mTextValue);
  }
  return result;
}

const nsAReadableString& CCommentToken::GetStringValue(void)
{
  return mTextValue;
}

/* 
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char* CCommentToken::GetClassName(void){
  return "/**/";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CCommentToken::GetTokenType(void) {
  return eToken_comment;
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CNewlineToken::CNewlineToken() : CHTMLToken(eHTMLTag_newline) {
}


/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CNewlineToken::GetClassName(void) {
  return "crlf";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CNewlineToken::GetTokenType(void) {
  return eToken_newline;
}


static nsSlidingSubstring* gNewlineStr;
void CNewlineToken::AllocNewline()
{
  gNewlineStr = new nsSlidingSubstring(NS_LITERAL_STRING("\n"));
}

void CNewlineToken::FreeNewline()
{
  if (gNewlineStr) {
    delete gNewlineStr;
    gNewlineStr = nsnull;
  }
}

/**
 *  This method retrieves the value of this internal string. 
 *  
 *  @update gess 3/25/98
 *  @return nsString reference to internal string value
 */
const nsAReadableString& CNewlineToken::GetStringValue(void) {
  return *gNewlineStr;
}

/*
 *  Consume as many cr/lf pairs as you can find.
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CNewlineToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {

/*******************************************************************

  Here's what the HTML spec says about newlines:

  "A line break is defined to be a carriage return (&#x000D;), 
   a line feed (&#x000A;), or a carriage return/line feed pair. 
   All line breaks constitute white space."

 *******************************************************************/

  PRUnichar theChar;
  nsresult result=aScanner.Peek(theChar);

  if(NS_OK==result) {
    switch(aChar) {
      case kNewLine:
        if(kCR==theChar) {
          result=aScanner.GetChar(theChar);
        }
        break;
      case kCR: 
          //convert CRLF into just CR
        if(kNewLine==theChar) {
          result=aScanner.GetChar(theChar);
        }
        break;
      default:
        break;
    }
  }  
  return result;
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CAttributeToken::CAttributeToken() : CHTMLToken(eHTMLTag_unknown) {
  mLastAttribute=PR_FALSE;
  mHasEqualWithoutValue=PR_FALSE;
}

/*
 *  string based constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CAttributeToken::CAttributeToken(const nsAReadableString& aName) : CHTMLToken(eHTMLTag_unknown) {
  mTextValue.Assign(aName);
  mLastAttribute=PR_FALSE;
  mHasEqualWithoutValue=PR_FALSE;
}

/*
 *  construct initializing data to 
 *  key value pair
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CAttributeToken::CAttributeToken(const nsAReadableString& aKey, const nsAReadableString& aName) : CHTMLToken(eHTMLTag_unknown) {
  mTextValue.Assign(aName);
  mTextKey.Rebind(aKey);
  mLastAttribute=PR_FALSE;
  mHasEqualWithoutValue=PR_FALSE;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CAttributeToken::GetClassName(void) {
  return "attr";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CAttributeToken::GetTokenType(void) {
  return eToken_attribute;
}

/*
 *  Removes non-alpha-non-digit characters from the end of a KEY
 *  
 *  @update harishd 07/15/99
 *  @param  
 *  @return  
 */
void CAttributeToken::SanitizeKey() {
  PRInt32   length=mTextKey.Length();
  if(length > 0) {
    nsReadingIterator<PRUnichar> iter, begin, end;
    mTextKey.BeginReading(begin);
    mTextKey.EndReading(end);
    iter = end;

    // Look for the first legal character starting from
    // the end of the string
    do {
      --iter;
    } while (!nsCRT::IsAsciiAlpha(*iter) && 
             !nsCRT::IsAsciiDigit(*iter) && 
             (iter != begin));
    
    // If there were any illegal characters, just copy out the
    // legal part
    if (iter != --end) {
      nsAutoString str;
      CopyUnicodeTo(begin, iter, str);
      mTextKey.Rebind(str);
    }
  }

  return;
}

#ifdef DEBUG
/*
 *  Dump contents of this token to given output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CAttributeToken::DebugDumpToken(nsOutputStream& out) {
  out << "[" << GetClassName() << "] "
      << NS_LossyConvertUCS2toASCII(mTextKey).get() << "="
      << NS_LossyConvertUCS2toASCII(mTextValue).get() << ": " << mTypeID
      << nsEndl;
}
#endif

const nsAReadableString& CAttributeToken::GetStringValue(void)
{
  return mTextValue;
}
 
/*
 *  
 *  
 *  @update  rickg  6June2000
 *  @param   anOutputString will recieve the result
 *  @return  nada
 */
void CAttributeToken::GetSource(nsString& anOutputString){
  anOutputString.Truncate();
  AppendSource(anOutputString);
}

/*
 *  
 *  
 *  @update  rickg  6June2000
 *  @param   result appended to the output string.
 *  @return  nada
 */
void CAttributeToken::AppendSource(nsString& anOutputString){
  anOutputString.Append(mTextKey);
  if(mTextValue.Length() || mHasEqualWithoutValue) 
    anOutputString.AppendWithConversion("=");
  anOutputString.Append(mTextValue);
  // anOutputString.AppendWithConversion(";");
}

/*
 *  @param   aScanner -- controller of underlying input source
 *  @param   aFlag -- If NS_IPARSER_FLAG_VIEW_SOURCE do not reduce entities...
 *  @return  error result
 *
 */
static
nsresult ConsumeAttributeEntity(nsString& aString,
                                nsScanner& aScanner,
                                PRInt32 aFlag) 
{
 
  nsresult result=NS_OK;

  PRUnichar ch;
  result=aScanner.Peek(ch, 1);

  if (NS_SUCCEEDED(result)) {
    PRUnichar amp=0;
    PRInt32 theNCRValue=0;
    nsAutoString entity;

    if (nsCRT::IsAsciiAlpha(ch) && !(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
      result=CEntityToken::ConsumeEntity(ch,entity,aScanner);
      if (NS_SUCCEEDED(result)) {
        theNCRValue = nsHTMLEntities::EntityToUnicode(entity);
        PRUnichar theTermChar=entity.Last();
        // If an entity value is greater than 255 then:
        // Nav 4.x does not treat it as an entity,
        // IE treats it as an entity if terminated with a semicolon.
        // Resembling IE!!
        if(theNCRValue < 0 || (theNCRValue > 255 && theTermChar != ';')) {
          // Looks like we're not dealing with an entity
          aString.Append(kAmpersand);
          aString.Append(entity);
        }
        else {
          // A valid entity so reduce it.
          aString.Append(PRUnichar(theNCRValue));
        }
      }
    }
    else if (ch==kHashsign && !(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
      result=CEntityToken::ConsumeEntity(ch,entity,aScanner);
      if (NS_SUCCEEDED(result)) {
        if (result == NS_HTMLTOKENS_NOT_AN_ENTITY) {
          // Looked like an entity but it's not
          aScanner.GetChar(amp);
          aString.Append(amp);
          result = NS_OK; // just being safe..
        }
        else {
          PRInt32 err;
          theNCRValue=entity.ToInteger(&err,kAutoDetect);
          aString.Append(PRUnichar(theNCRValue));
        }
      }
    }
    else {
      // What we thought as entity is not really an entity...
      aScanner.GetChar(amp);
      aString.Append(amp);
    }//if
  }

  return result;
}

/*
 *  This general purpose method is used when you want to
 *  consume attributed text value. 
 *  Note: It also reduces entities within attributes.
 *  
 *  @param   aScanner -- controller of underlying input source
 *  @param   aTerminalChars -- characters that stop consuming attribute.
 *  @param   aFlag - contains information such as |dtd mode|view mode|doctype|etc...
 *  @return  error result
 */
static
nsresult ConsumeAttributeValueText(nsString& aString,
                                   nsScanner& aScanner,
                                   const nsReadEndCondition& aEndCondition,
                                   PRInt32 aFlag)
{
  nsresult result = NS_OK;
  PRBool   done = PR_FALSE;
  
  do {
    result = aScanner.ReadUntil(aString,aEndCondition,PR_FALSE);
    if(NS_SUCCEEDED(result)) {
      PRUnichar ch;
      aScanner.Peek(ch);
      if(ch == kAmpersand) {
        result = ConsumeAttributeEntity(aString,aScanner,aFlag);
      }
      else {
        done = PR_TRUE;
      }
    }
  } while (NS_SUCCEEDED(result) && !done);

  return result;
}

/*
 *  This general purpose method is used when you want to
 *  consume a known quoted string. 
 *  
 *  @param   aScanner -- controller of underlying input source
 *  @param   aTerminalChars -- characters that stop consuming attribute.
 *  @param   aFlag - contains information such as |dtd mode|view mode|doctype|etc...
 *  @return  error result
 */
static
nsresult ConsumeQuotedString(PRUnichar aChar,
                              nsString& aString,
                              nsScanner& aScanner,
                              PRInt32 aFlag)
{
  NS_ASSERTION(aChar==kQuote || aChar==kApostrophe,"char is neither quote nor apostrophe");

  static const PRUnichar theTerminalCharsQuote[] = { 
    PRUnichar(kQuote), PRUnichar('&'),  PRUnichar(0) };
  static const PRUnichar theTerminalCharsApostrophe[] = { 
    PRUnichar(kApostrophe), PRUnichar('&'), PRUnichar(0) };
  static const nsReadEndCondition
    theTerminateConditionQuote(theTerminalCharsQuote);
  static const nsReadEndCondition
    theTerminateConditionApostrophe(theTerminalCharsApostrophe);

  // Assume Quote to init to something
  const nsReadEndCondition *terminateCondition = &theTerminateConditionQuote;
  if (aChar==kApostrophe)
    terminateCondition = &theTerminateConditionApostrophe;
  
  nsresult result=NS_OK;
  nsReadingIterator<PRUnichar> theOffset;
  aScanner.CurrentPosition(theOffset);

  result=ConsumeAttributeValueText(aString,aScanner,*terminateCondition,aFlag);

  if(NS_SUCCEEDED(result)) {
    result = aScanner.SkipOver(aChar); // aChar should be " or '
  }

  // Ref: Bug 35806
  // A back up measure when disaster strikes...
  // Ex <table> <tr d="><td>hello</td></tr></table>
  if(!aString.IsEmpty() && aString.Last()!=aChar &&
     !aScanner.IsIncremental() && result==kEOF) {
    static const nsReadEndCondition
      theAttributeTerminator(kAttributeTerminalChars);
    aString.Truncate();
    aScanner.SetPosition(theOffset, PR_FALSE, PR_TRUE);
    result=ConsumeAttributeValueText(aString,aScanner,theAttributeTerminator,aFlag);
  }
  return result;
}

/*
 *  Consume the key and value portions of the attribute.
 *  
 *  @update  rickg 03.23.2000
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @param   aFlag - contains information such as |dtd mode|view mode|doctype|etc...
 *  @return  error result
 */
nsresult CAttributeToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {

  nsresult result;
 
  //I changed a bit of this method to use aRetain so that we do the right
  //thing in viewsource. The ws/cr/lf sequences are now maintained, and viewsource looks good.

  nsReadingIterator<PRUnichar> wsstart, wsend;
  
  if (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
    result = aScanner.ReadWhitespace(wsstart, wsend);
  }
  else {
    result = aScanner.SkipWhitespace();
  }

  if (NS_OK==result) {
    static const PRUnichar theTerminalsChars[] = 
    { PRUnichar(' '), PRUnichar('"'), 
      PRUnichar('='), PRUnichar('\n'), 
      PRUnichar('\r'), PRUnichar('\t'), 
      PRUnichar('>'), PRUnichar('\b'),
      PRUnichar(0) };
    static const nsReadEndCondition theEndCondition(theTerminalsChars);

    nsReadingIterator<PRUnichar> start, end;
    result=aScanner.ReadUntil(start,end,theEndCondition,PR_FALSE);

    if (!(aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
      aScanner.BindSubstring(mTextKey, start, end);
    }

    //now it's time to Consume the (optional) value...
    if (NS_OK==result) {
      if (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
        result = aScanner.ReadWhitespace(start, wsend);
        aScanner.BindSubstring(mTextKey, wsstart, wsend);
      }
      else {
        result = aScanner.SkipWhitespace();
      }

      if (NS_OK==result) { 
        result=aScanner.Peek(aChar);       //Skip ahead until you find an equal sign or a '>'...
        if (NS_OK==result) {  
          if (kEqual==aChar){
            result=aScanner.GetChar(aChar);  //skip the equal sign...
            if (NS_OK==result) {
              if (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
                result = aScanner.ReadWhitespace(mTextValue);
              }
              else {
                result = aScanner.SkipWhitespace();
              }

              if (NS_OK==result) {
                result=aScanner.Peek(aChar);  //and grab the next char.    
                if (NS_OK==result) {
                  if ((kQuote==aChar) || (kApostrophe==aChar)) {
                    aScanner.GetChar(aChar);
                    result=ConsumeQuotedString(aChar,mTextValue,aScanner,aFlag);
                    if (NS_SUCCEEDED(result) && (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE)) {
                      mTextValue.Insert(aChar,0);
                      mTextValue.Append(aChar);
                    }
                    // According to spec. we ( who? ) should ignore linefeeds. But look,
                    // even the carriage return was getting stripped ( wonder why! ) -
                    // Ref. to bug 15204.  Okay, so the spec. told us to ignore linefeeds,
                    // bug then what about bug 47535 ? Should we preserve everything then?
                    // Well, let's make it so! Commenting out the next two lines..
                    /*if(!aRetain)
                      mTextValue.StripChars("\r\n"); //per the HTML spec, ignore linefeeds...
                    */
                  }
                  else if (kGreaterThan==aChar){      
                    mHasEqualWithoutValue=PR_TRUE;
                  }
                  else {
                    static const nsReadEndCondition
                      theAttributeTerminator(kAttributeTerminalChars);
                    result=ConsumeAttributeValueText(mTextValue,
                                                     aScanner,
                                                     theAttributeTerminator,
                                                     aFlag);
                  } 
                }//if
                if (NS_OK==result) {
                  if (aFlag & NS_IPARSER_FLAG_VIEW_SOURCE) {
                    result = aScanner.ReadWhitespace(mTextValue);
                  }
                  else {
                    result = aScanner.SkipWhitespace();
                  }
                }
              }//if
            }//if
          }//if
          else {
            //This is where we have to handle fairly busted content.
            //If you're here, it means we saw an attribute name, but couldn't find 
            //the following equal sign.  <tag NAME=....
        
            //Doing this right in all cases is <i>REALLY</i> ugly. 
            //My best guess is to grab the next non-ws char. We know it's not '=',
            //so let's see what it is. If it's a '"', then assume we're reading
            //from the middle of the value. Try stripping the quote and continuing...
            if (kQuote==aChar){
              result=aScanner.SkipOver(aChar); //strip quote.
            }
          }
        }//if
      } //if
    }//if (consume optional value)

    if (NS_OK==result) {
      result=aScanner.Peek(aChar);
      mLastAttribute= PRBool((kGreaterThan==aChar) || (kEOF==result));
    }
  }//if
  return result;
}

#ifdef DEBUG
/*
 *  Dump contents of this token to givne output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CAttributeToken::DebugDumpSource(nsOutputStream& out) {
  out << " " << NS_LossyConvertUCS2toASCII(mTextKey).get();
  if (!mTextValue.IsEmpty()) {
    out << "=" << NS_LossyConvertUCS2toASCII(mTextValue).get();
  }
  if(mLastAttribute)
    out << ">";
}
#endif

void CAttributeToken::SetKey(const nsAReadableString& aKey)
{
  mTextKey.Rebind(aKey);
}

void CAttributeToken::BindKey(nsScanner* aScanner, 
                              nsReadingIterator<PRUnichar>& aStart, 
                              nsReadingIterator<PRUnichar>& aEnd)
{
  aScanner->BindSubstring(mTextKey, aStart, aEnd);
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CWhitespaceToken::CWhitespaceToken() : CHTMLToken(eHTMLTag_whitespace) {
}


/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CWhitespaceToken::CWhitespaceToken(const nsAReadableString& aName) : CHTMLToken(eHTMLTag_whitespace) {
  mTextValue.Assign(aName);
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CWhitespaceToken::GetClassName(void) {
  return "ws";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CWhitespaceToken::GetTokenType(void) {
  return eToken_whitespace;
}

/*
 *  This general purpose method is used when you want to
 *  consume an aribrary sequence of whitespace. 
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CWhitespaceToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {
  mTextValue.Assign(aChar);
  nsresult result=aScanner.ReadWhitespace(mTextValue);
  if(NS_OK==result) {
    mTextValue.StripChar(kCR);
  }
  return result;
}

const nsAReadableString& CWhitespaceToken::GetStringValue(void)
{
  return mTextValue;
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CEntityToken::CEntityToken() : CHTMLToken(eHTMLTag_entity) {
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CEntityToken::CEntityToken(const nsAReadableString& aName) : CHTMLToken(eHTMLTag_entity) {
  mTextValue.Assign(aName);
#ifdef VERBOSE_DEBUG
  if(!VerifyEntityTable())  {
    cout<<"Entity table is invalid!" << endl;
  }
#endif
}


/*
 *  Consume the rest of the entity. We've already eaten the "&".
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CEntityToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {
  nsresult result=ConsumeEntity(aChar,mTextValue,aScanner);
  return result;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CEntityToken::GetClassName(void) {
  return "&entity";
}


/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CEntityToken::GetTokenType(void) {
  return eToken_entity;
}

/*
 *  This general purpose method is used when you want to
 *  consume an entity &xxxx;. Keep in mind that entities
 *  are <i>not</i> reduced inline.
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult
CEntityToken::ConsumeEntity(PRUnichar aChar,
                            nsString& aString,
                            nsScanner& aScanner) {
  nsresult result=NS_OK;
  if(kLeftBrace==aChar) {
    //you're consuming a script entity...
    aScanner.GetChar(aChar); // Consume &

    PRInt32 rightBraceCount = 0;
    PRInt32 leftBraceCount  = 0;

    do {
      result=aScanner.GetChar(aChar);
      
      if (NS_FAILED(result)) {
        return result;
      }

      aString.Append(aChar);
      if(aChar==kRightBrace)
        rightBraceCount++;
      else if(aChar==kLeftBrace)
        leftBraceCount++;
    } while(leftBraceCount!=rightBraceCount);
  } //if
  else {
    PRUnichar theChar=0;
    if (kHashsign==aChar) {
      result = aScanner.Peek(theChar,2);
       
      if (NS_FAILED(result)) {
        return result;
      }

      if (nsCRT::IsAsciiDigit(theChar)) {
        aScanner.GetChar(aChar); // Consume &
        aScanner.GetChar(aChar); // Consume #
        aString.Assign(aChar);
        result=aScanner.ReadNumber(aString,10);
      }
      else if (theChar == 'x' || theChar == 'X') {
        aScanner.GetChar(aChar);   // Consume &
        aScanner.GetChar(aChar);   // Consume #
        aScanner.GetChar(theChar); // Consume x
        aString.Assign(aChar);
        aString.Append(theChar); 
        result=aScanner.ReadNumber(aString,16);
      }
      else {
        return NS_HTMLTOKENS_NOT_AN_ENTITY; 
      }
    }
    else {
      result = aScanner.Peek(theChar,1);
       
      if (NS_FAILED(result)) {
        return result;
      }

      if(nsCRT::IsAsciiAlpha(theChar) || 
        theChar == '_' ||
        theChar == ':') {
        aScanner.GetChar(aChar); // Consume &
        result=aScanner.ReadIdentifier(aString,PR_TRUE); // Ref. Bug# 23791 - For setting aIgnore to PR_TRUE.
      }
      else {
        return NS_HTMLTOKENS_NOT_AN_ENTITY;
      }
    }
  }
    
  if (NS_FAILED(result)) {
    return result;
  }
    
  result=aScanner.Peek(aChar);
  
  if (NS_FAILED(result)) {
    return result;
  }

  if (aChar == kSemicolon) {
    // consume semicolon that stopped the scan
    aString.Append(aChar);
    result=aScanner.GetChar(aChar);
  }
  
  return result;
}

#define PA_REMAP_128_TO_160_ILLEGAL_NCR 1

#ifdef PA_REMAP_128_TO_160_ILLEGAL_NCR
/**
 * Map some illegal but commonly used numeric entities into their
 * appropriate unicode value.
 */
#define NOT_USED 0xfffd

static PRUint16 PA_HackTable[] = {
	0x20ac,  /* EURO SIGN */
	NOT_USED,
	0x201a,  /* SINGLE LOW-9 QUOTATION MARK */
	0x0192,  /* LATIN SMALL LETTER F WITH HOOK */
	0x201e,  /* DOUBLE LOW-9 QUOTATION MARK */
	0x2026,  /* HORIZONTAL ELLIPSIS */
	0x2020,  /* DAGGER */
	0x2021,  /* DOUBLE DAGGER */
	0x02c6,  /* MODIFIER LETTER CIRCUMFLEX ACCENT */
	0x2030,  /* PER MILLE SIGN */
	0x0160,  /* LATIN CAPITAL LETTER S WITH CARON */
	0x2039,  /* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
	0x0152,  /* LATIN CAPITAL LIGATURE OE */
	NOT_USED,
	0x017D,  /* LATIN CAPITAL LETTER Z WITH CARON */
	NOT_USED,
	NOT_USED,
	0x2018,  /* LEFT SINGLE QUOTATION MARK */
	0x2019,  /* RIGHT SINGLE QUOTATION MARK */
	0x201c,  /* LEFT DOUBLE QUOTATION MARK */
	0x201d,  /* RIGHT DOUBLE QUOTATION MARK */
	0x2022,  /* BULLET */
	0x2013,  /* EN DASH */
	0x2014,  /* EM DASH */
	0x02dc,  /* SMALL TILDE */
	0x2122,  /* TRADE MARK SIGN */
	0x0161,  /* LATIN SMALL LETTER S WITH CARON */
	0x203a,  /* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
	0x0153,  /* LATIN SMALL LIGATURE OE */
	NOT_USED,
	0x017E,  /* LATIN SMALL LETTER Z WITH CARON */
	0x0178   /* LATIN CAPITAL LETTER Y WITH DIAERESIS */
};
#endif /* PA_REMAP_128_TO_160_ILLEGAL_NCR */


/*
 *  This method converts this entity into its underlying
 *  unicode equivalent.
 *  
 *  @update  gess 3/25/98
 *  @param   aString will hold the resulting string value
 *  @return  numeric (unichar) value
 */
PRInt32 CEntityToken::TranslateToUnicodeStr(nsString& aString) {
  PRInt32 value=0;

  if(mTextValue.Length()>1) {
    PRUnichar theChar0=mTextValue.CharAt(0);

    if(kHashsign==theChar0) {
      PRInt32 err=0;
      
      value=mTextValue.ToInteger(&err,kAutoDetect);

      if(0==err) {
  #ifdef PA_REMAP_128_TO_160_ILLEGAL_NCR
        /* for some illegal, but popular usage */
        if ((value >= 0x0080) && (value <= 0x009f)) {
          value = PA_HackTable[value - 0x0080];
        }
  #endif
        aString.Append(PRUnichar(value));
      }//if
    }
    else{
      value = nsHTMLEntities::EntityToUnicode(mTextValue);
      if(-1<value) {
        //we found a named entity...
        aString.Assign(PRUnichar(value));
      }
    }//else
  }//if

  return value;
}

#ifdef DEBUG
/*
 *  Dump contents of this token to givne output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CEntityToken::DebugDumpSource(nsOutputStream& out) {
  char* cp = ToNewCString(mTextValue);
  out << "&" << *cp;
  delete[] cp;
}
#endif

const nsAReadableString& CEntityToken::GetStringValue(void)
{
  return mTextValue;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   anOutputString will recieve the result
 *  @return  nada
 */
void CEntityToken::GetSource(nsString& anOutputString){
  anOutputString.AppendWithConversion("&");
  anOutputString+=mTextValue;
  //anOutputString+=";";
}

/*
 *  
 *  
 *  @update  harishd 03/23/00
 *  @param   result appended to the output string.
 *  @return  nada
 */
void CEntityToken::AppendSource(nsString& anOutputString){
  anOutputString.AppendWithConversion("&");
  anOutputString+=mTextValue;
  //anOutputString+=";";
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CScriptToken::CScriptToken() : CHTMLToken(eHTMLTag_script) {
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CScriptToken::CScriptToken(const nsAReadableString& aString) : CHTMLToken(eHTMLTag_script) {
  mTextValue.Assign(aString);
}
                        

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CScriptToken::GetClassName(void) {
  return "script";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CScriptToken::GetTokenType(void) {
  return eToken_script;
}

const nsAReadableString& CScriptToken::GetStringValue(void)
{
  return mTextValue;
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CStyleToken::CStyleToken() : CHTMLToken(eHTMLTag_style) {
}

CStyleToken::CStyleToken(const nsAReadableString& aString) : CHTMLToken(eHTMLTag_style) {
  mTextValue.Assign(aString);
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CStyleToken::GetClassName(void) {
  return "style";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CStyleToken::GetTokenType(void) {
  return eToken_style;
}

const nsAReadableString& CStyleToken::GetStringValue(void)
{
  return mTextValue;
}


/**
 * 
 * @update	gess4/25/98
 * @param 
 * @return
 */
const char* GetTagName(PRInt32 aTag) {
  const nsCString& result = nsHTMLTags::GetStringValue((nsHTMLTag) aTag);
  if (0 == result.Length()) {
    if(aTag>=eHTMLTag_userdefined)
      return gUserdefined;
    else return 0;
  }
  return result;
}


/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
CInstructionToken::CInstructionToken() : CHTMLToken(eHTMLTag_instruction) {
}

/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
CInstructionToken::CInstructionToken(const nsAReadableString& aString) : CHTMLToken(eHTMLTag_unknown) {
  mTextValue.Assign(aString);
}

/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
nsresult CInstructionToken::Consume(PRUnichar aChar,nsScanner& aScanner,PRInt32 aFlag){
  mTextValue.AssignWithConversion("<?");
  nsresult result=aScanner.ReadUntil(mTextValue,kGreaterThan,PR_TRUE);
  return result;
}

/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
const char* CInstructionToken::GetClassName(void){
  return "instruction";
}

/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
PRInt32 CInstructionToken::GetTokenType(void){
  return eToken_instruction;
}

const nsAReadableString& CInstructionToken::GetStringValue(void)
{
  return mTextValue;
}


CErrorToken::CErrorToken(nsParserError *aError) : CHTMLToken(eHTMLTag_unknown)
{
  mError = aError;
}

CErrorToken::~CErrorToken() 
{
  delete mError;
}

PRInt32 CErrorToken::GetTokenType(void){
  return eToken_error;
}

const char* CErrorToken::GetClassName(void){
  return "error";
}

void CErrorToken::SetError(nsParserError *aError) {
  mError = aError;
}

const nsParserError * CErrorToken::GetError(void) 
{ 
  return mError; 
}

const nsAReadableString& CErrorToken::GetStringValue(void)
{
  return mTextValue;
}

// Doctype decl token

CDoctypeDeclToken::CDoctypeDeclToken(eHTMLTags aTag)
  : CHTMLToken(aTag) {
}

CDoctypeDeclToken::CDoctypeDeclToken(const nsAReadableString& aString,eHTMLTags aTag)
  : CHTMLToken(aTag), mTextValue(aString) {
}

/**
 *  This method consumes a doctype element.
 *  Note: I'm rewriting this method to seek to the first <, since quotes can really screw us up.
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
nsresult CDoctypeDeclToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aFlag) {
    
  static const PRUnichar terminalChars[] = 
  { PRUnichar('>'), PRUnichar('<'),
    PRUnichar(0) 
  };
  static const nsReadEndCondition theEndCondition(terminalChars);

  nsReadingIterator<PRUnichar> start, end;
  
  aScanner.CurrentPosition(start);
  aScanner.EndReading(end);

  nsresult result=aScanner.ReadUntil(start, end, theEndCondition, PR_FALSE);

  if (NS_SUCCEEDED(result)) {
    PRUnichar ch;
    aScanner.Peek(ch);
    if (ch == kGreaterThan) {
      // Include '>' but not '<' since '<' 
      // could belong to another tag.
      aScanner.GetChar(ch);
      end.advance(1); 
    }
  }
  else if (!aScanner.IsIncremental()) {
    // We have reached the document end but haven't
    // found either a '<' or a '>'. Therefore use
    // whatever we have.
    result = NS_OK; 
  }
  
  if (NS_SUCCEEDED(result)) {
    start.advance(-2); // Make sure to consume <!
    CopyUnicodeTo(start,end,mTextValue);
  }
  
  return result;
}

const char*  CDoctypeDeclToken::GetClassName(void) {
  return "doctype";
}

PRInt32 CDoctypeDeclToken::GetTokenType(void) {
  return eToken_doctypeDecl;
}

const nsAReadableString& CDoctypeDeclToken::GetStringValue(void)
{
  return mTextValue;
}

void CDoctypeDeclToken::SetStringValue(const nsAReadableString& aStr)
{
  mTextValue.Assign(aStr);
}
