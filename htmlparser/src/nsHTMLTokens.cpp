/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#include <ctype.h> 
#include <time.h>
#include <stdio.h>  
#include "nsScanner.h"
#include "nsToken.h" 
#include "nsHTMLTokens.h"
#include "nsIParser.h"
#include "prtypes.h"
#include "nsDebug.h"
#include "nsHTMLTags.h"
#include "nsHTMLEntities.h"
#include "nsCRT.h"



static const char*  gUserdefined = "userdefined";


/**************************************************************
  And now for the token classes...
 **************************************************************/

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CHTMLToken::CHTMLToken(const nsString& aName,eHTMLTags aTag) : CToken(aName) {
  mTypeID=aTag;
}

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

/**
 * Setter method that changes the string value of this token
 * @update	gess5/11/98
 * @param   name is a char* value containing new string value
 */
void CHTMLToken::SetCStringValue(const char* name){
  if(name) {
    mTextValue.AssignWithConversion(name);
    mTypeID = nsHTMLTags::LookupTag(mTextValue);
  }
}


/**
 *  This method retrieves the value of this internal string. 
 *  
 *  @update gess 3/25/98
 *  @return nsString reference to internal string value
 */
nsString& CHTMLToken::GetStringValueXXX(void) {

  if((eHTMLTag_unknown<mTypeID) && (mTypeID<eHTMLTag_text)) {
    if(!mTextValue.Length()) {
      mTextValue.AssignWithConversion(nsHTMLTags::GetStringValue((nsHTMLTag) mTypeID));
    }
  }
  return mTextValue;
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

/*
 *  constructor from tag id
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CStartToken::CStartToken(const nsString& aString) : CHTMLToken(aString) {
  mAttributed=PR_FALSE;
  mEmpty=PR_FALSE;
  mOrigin=-1;
  mContainerInfo=eFormUnknown;
}

CStartToken::CStartToken(const nsString& aName,eHTMLTags aTag) : CHTMLToken(aName,aTag) {
  mAttributed=PR_FALSE;
  mEmpty=PR_FALSE;
  mOrigin=-1;
  mContainerInfo=eFormUnknown;
}

/**
 * 
 * @update	gess8/4/98
 * @param 
 * @return
 */
void CStartToken::Reinitialize(PRInt32 aTag, const nsString& aString){
  CToken::Reinitialize(aTag,aString);
  mAttributed=PR_FALSE;
  mUseCount=1; 
  mEmpty=PR_FALSE;
  mOrigin=-1;
  mTrailingContent.Truncate();
  mContainerInfo=eFormUnknown;
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
 *  @param   aMode -- 1=HTML; 0=text (or other ML)
 *  @return  error result
 */
nsresult CStartToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aMode) {

  //if you're here, we've already Consumed the < char, and are
   //ready to Consume the rest of the open tag identifier.
   //Stop consuming as soon as you see a space or a '>'.
   //NOTE: We don't Consume the tag attributes here, nor do we eat the ">"

  nsresult result=NS_OK;
  if(0==aMode) {
    nsSubsumeStr theSubstr;
    result=aScanner.GetIdentifier(theSubstr,!aMode);
    mTypeID = (PRInt32)nsHTMLTags::LookupTag(theSubstr);
    if(eHTMLTag_userdefined==mTypeID) {
      mTextValue=theSubstr;
    }
  }
  else {
    mTextValue.Assign(aChar);

    //added PR_TRUE to readId() call below to fix bug 46083. The problem was that the tag given
    //was written <title_> but since we didn't respect the '_', we only saw <title>. Then 
    //we searched for end title, which never comes (they give </title_>). 

    result=aScanner.ReadIdentifier(mTextValue,PR_TRUE);  
    mTypeID = nsHTMLTags::LookupTag(mTextValue);
  }

  return result;
}


/*
 *  Dump contents of this token to givne output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CStartToken::DebugDumpSource(nsOutputStream& out) {
  char buffer[1000];
  mTextValue.ToCString(buffer,sizeof(buffer));
  out << "<" << buffer;
  if(!mAttributed)
    out << ">";
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
  SetCStringValue(GetTagName(aTag));
}


/*
 *  default constructor for end token
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- char* containing token name
 *  @return  
 */
CEndToken::CEndToken(const nsString& aName) : CHTMLToken(aName) {
}

/*
 *  Consume the identifier portion of the end tag
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CEndToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aMode) {
  //if you're here, we've already Consumed the <! chars, and are
   //ready to Consume the rest of the open tag identifier.
   //Stop consuming as soon as you see a space or a '>'.
   //NOTE: We don't Consume the tag attributes here, nor do we eat the ">"

  mTextValue.SetLength(0);
  nsresult result=aScanner.ReadUntil(mTextValue,kGreaterThan,PR_FALSE);

  if(NS_OK==result){
    nsAutoString  buffer;
    mTextValue.Left(buffer, mTextValue.FindCharInSet(" \r\n\t\b",0));
    mTypeID= nsHTMLTags::LookupTag(buffer);
    result=aScanner.GetChar(aChar); //eat the closing '>;
  }
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

/*
 *  Dump contents of this token to givne output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CEndToken::DebugDumpSource(nsOutputStream& out) {
  char buffer[1000];
  mTextValue.ToCString(buffer,sizeof(buffer));
  out << "</" << buffer << ">";
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
  anOutputString+=mTextValue;
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
  anOutputString+=mTextValue;
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
CTextToken::CTextToken(const nsString& aName) : CHTMLToken(aName) {
  mTypeID=eHTMLTag_text;
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

/*
 *  Consume as much clear text from scanner as possible.
 *
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CTextToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aMode) {;
  static nsString theTerminals = NS_ConvertToString("\n\r&<",4);  
  nsresult  result=NS_OK;
  PRBool    done=PR_FALSE;

  while((NS_OK==result) && (!done)) {
    result=aScanner.ReadUntil(mTextValue,theTerminals,PR_FALSE);
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
              result=aScanner.GetChar(theNextChar);
            }
            else if(kCR==theNextChar) {
              result=aScanner.GetChar(theNextChar);
              result=aScanner.Peek(theNextChar);    //then see what's next.
              if(kLF==theNextChar) {
                result=aScanner.GetChar(theNextChar);
              }
              mTextValue.AppendWithConversion("\n");
              mNewlineCount++;
            }
            mTextValue.AppendWithConversion("\n");
            mNewlineCount++;
            break;
          case kLF:
            if((kLF==theNextChar) || (kCR==theNextChar)) {
              result=aScanner.GetChar(theNextChar);
              mTextValue.AppendWithConversion("\n");
              mNewlineCount++;
            }
            mTextValue.AppendWithConversion("\n");
            mNewlineCount++;
            break;
          default:
            mTextValue.AppendWithConversion("\n");
            mNewlineCount++;
            break;
        } //switch
      }
      else done=PR_TRUE;
    }
  }
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
                                  nsString& aTerminalString,PRInt32 aMode,PRBool& aFlushTokens){
  nsresult      result=NS_OK;
  PRInt32       theTermStrPos=0;;
  nsString&     theBuffer=aScanner.GetBuffer();
  PRInt32       theStartOffset=aScanner.GetOffset();
  PRInt32       theCurrOffset=theStartOffset;  
  PRInt32       theStartCommentPos=kNotFound;
  PRInt32       theAltTermStrPos=kNotFound;
  PRBool        done=PR_FALSE;
  PRBool        theLastIteration=PR_FALSE;
  PRInt32       termStrLen=aTerminalString.Length();

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


  // When is the disaster enabled?
  // a) when the buffer runs out ot data.
  // b) when the terminal string is not found.
  PRBool disaster=PR_FALSE; 
  while(result==NS_OK && !done) {
    theCurrOffset=theBuffer.FindChar(kLessThan,PR_TRUE,theCurrOffset);
    if(-1<theCurrOffset) {
      PRInt32 tempOffset=theCurrOffset;
      while(1) {
        theTermStrPos=kNotFound;
        tempOffset=theBuffer.FindChar(kGreaterThan,PR_TRUE,tempOffset);
        if(tempOffset>-1) {
          theTermStrPos=theBuffer.RFind(aTerminalString,PR_TRUE,tempOffset,termStrLen+2);
          if(theTermStrPos>-1) break;
          tempOffset++;
        }
        else break;
      }
      //theTermStrPos=theBuffer.Find(aTerminalString,PR_TRUE,theCurrOffset);
      if(theTermStrPos>kNotFound) {
        if((aMode!=eDTDMode_strict) && (aMode!=eDTDMode_transitional) && !theLastIteration) {
          if(!aIgnoreComments) {
            theCurrOffset=theBuffer.Find("<!--",PR_TRUE,theCurrOffset,5);
            if(theStartCommentPos==kNotFound && theCurrOffset>kNotFound) {
              theStartCommentPos=theCurrOffset;
            }
            if(theStartCommentPos>kNotFound) {
              // Search for --> between <!-- and </TERMINALSTRING>.
              theCurrOffset=theBuffer.RFind("-->",PR_TRUE,theTermStrPos,theTermStrPos-theStartCommentPos);
              if(theCurrOffset==kNotFound) {
                // If you're here it means that we have a bogus terminal string.
                theAltTermStrPos=(theAltTermStrPos>-1)? theAltTermStrPos:theTermStrPos; // This could be helpful in case we hit the rock bottom.
                theCurrOffset=theTermStrPos+termStrLen; // We did not find '-->' so keep searching for terminal string.
                continue;
              }        
            }
          }
            
          PRInt32 thePos=theBuffer.FindChar(kGreaterThan,PR_TRUE,theTermStrPos,20);          
          if(thePos>kNotFound && thePos>theTermStrPos+termStrLen) {
              termStrLen +=(thePos-(theTermStrPos+termStrLen));
          }
        }
      
        disaster=PR_FALSE;
        theCurrOffset=theTermStrPos; 
        theBuffer.Mid(aTerminalString,theTermStrPos+2,termStrLen-2);
        PRUnichar ch=theBuffer.CharAt(theTermStrPos+termStrLen);
        theTermStrPos=(ch==kGreaterThan)? theTermStrPos+termStrLen:kNotFound;
        if(theTermStrPos>kNotFound) {
          theBuffer.Mid(mTextValue,theStartOffset,theCurrOffset-theStartOffset);
          aScanner.Mark(theTermStrPos+1);   
          aFlushTokens=PR_TRUE; // We found </SCRIPT>...permit flushing -> Ref: Bug 22485
        }
        done=PR_TRUE;
      }
      else  disaster=PR_TRUE;
    }
    else disaster=PR_TRUE;

    if(disaster) {
      if(!aScanner.IsIncremental()) {
        if(theAltTermStrPos>kNotFound) {
          // If you're here it means..we hit the rock bottom and therefore switch to plan B.
          theCurrOffset=theAltTermStrPos;
          theLastIteration=PR_TRUE;
        }
        else {
          aTerminalString.Cut(0,2); // Do this to fix Bug. 35456
          done=PR_TRUE;
        }
      }
      else
       result=kEOF;
    }
  }
  return result; 
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
CCDATASectionToken::CCDATASectionToken(const nsString& aName) : CHTMLToken(aName) {
  mTypeID=eHTMLTag_unknown;
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
nsresult CCDATASectionToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aMode) {
  static const char* theTerminals="\r]";
  nsresult  result=NS_OK;
  PRBool    done=PR_FALSE;

  while((NS_OK==result) && (!done)) {
    result=aScanner.ReadUntil(mTextValue,theTerminals,PR_FALSE);
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
CCommentToken::CCommentToken(const nsString& aName) : CHTMLToken(aName) {
  mTypeID=eHTMLTag_comment;
}

/*
 *  This method consumes a comment using the (CORRECT) comment parsing
 *  algorithm supplied by W3C. 
 *  
 *  @update  gess 01/04/99
 *  @param   
 *  @param   
 *  @return  
 */
static
nsresult ConsumeStrictComment(PRUnichar aChar, nsScanner& aScanner,nsString& aString) {
  nsresult  result=NS_OK;
 
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
              PRInt32 findpos=-1;
              nsAutoString temp;
              //Read to the first ending sequence '--'
              while((kNotFound==findpos) && (NS_OK==result)) {
                result=aScanner.ReadUntil(temp,kMinus,PR_TRUE);
                findpos=temp.RFind("--");
              } 
              aString+=temp;
              if(NS_OK==result) {
                if(NS_OK==result) {
                  temp.AssignWithConversion("->");
                  result=aScanner.ReadUntil(aString,temp,PR_FALSE);
                }
              } 
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

  PRInt32       theBestAltPos=kNotFound;
  nsString&     theBuffer=aScanner.GetBuffer();
  PRInt32       theStartOffset=aScanner.GetOffset();
  PRInt32       theCurrOffset=theStartOffset;

  result=aScanner.GetChar(aChar);
  if(NS_OK==result) {
    if(kMinus==aChar) {
      result=aScanner.GetChar(aChar);
      if(NS_OK==result) {
        if(kMinus==aChar) {
          //in this case, we're reading a long-form comment <-- xxx -->
          
          while((NS_OK==result)) {
            theCurrOffset=theBuffer.FindChar(kGreaterThan,PR_TRUE,theCurrOffset);
            if(kNotFound!=theCurrOffset) {
              theCurrOffset++;
              aChar=theBuffer[theCurrOffset-3];
              if(kMinus==aChar) {
                aChar=theBuffer[theCurrOffset-2];
                if(kMinus==aChar) {
                  theStartOffset=theStartOffset-2; // Include "<!" also..
                  theBuffer.Mid(aString,theStartOffset,theCurrOffset-theStartOffset);
                  aScanner.Mark(theCurrOffset);
                  return result; // We have found the dflt end comment delimiter ("-->")
                }
              }
              if(kNotFound==theBestAltPos) {
                // If we did not find the dflt then assume that '>' is the end comment
                // until we find '-->'. Nav. Compatibility -- Ref: Bug# 24006
                theBestAltPos=theCurrOffset;
              }
            }
            else {
              result=kEOF;
            }
          } //while
          if((kNotFound==theCurrOffset) && (!aScanner.IsIncremental())) {
            //if you're here, then we're in a special state. 
            //The problem at hand is that we've hit the end of the document without finding the normal endcomment delimiter "-->".
            //In this case, the first thing we try is to see if we found one of the alternate endcomment delimiter ">".
            //If so, rewind just pass than, and use everything up to that point as your comment.
            //If not, the document has no end comment and should be treated as one big comment.
            if(kNotFound<theBestAltPos) {
              theStartOffset=theStartOffset-2;// Include "<!" also..
              theBuffer.Mid(aString,theStartOffset,theBestAltPos-theStartOffset);
              aScanner.Mark(theBestAltPos);
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
nsresult CCommentToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aMode) {
  nsresult result=PR_TRUE;
  
  switch(aMode) {
    case eDTDMode_strict:
      result=ConsumeStrictComment(aChar,aScanner,mTextValue);
      break;
    case eDTDMode_transitional:
    default:
      result=ConsumeComment(aChar,aScanner,mTextValue);
      break;
  } //switch

  return result;
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
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CNewlineToken::CNewlineToken(const nsString& aName) : CHTMLToken(aName) {
  mTypeID=eHTMLTag_newline;
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

/**
 *  This method retrieves the value of this internal string. 
 *  
 *  @update gess 3/25/98
 *  @return nsString reference to internal string value
 */
nsString& CNewlineToken::GetStringValueXXX(void) {
  static nsString* theStr=0;
  if(!theStr) {
    theStr=new nsString;
    theStr->AssignWithConversion("\n");
  }
  return *theStr;
}

/*
 *  Consume as many cr/lf pairs as you can find.
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CNewlineToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aMode) {

#if 1
  mTextValue.Assign(kNewLine);  //This is what I THINK we should be doing.
#else
  mTextValue=aChar;
#endif

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
          mTextValue+=theChar;
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
CAttributeToken::CAttributeToken(const nsString& aName) : CHTMLToken(aName),  
  mTextKey() {
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
CAttributeToken::CAttributeToken(const nsString& aKey, const nsString& aName) : CHTMLToken(aName) {
  mTextKey = aKey;
  mLastAttribute=PR_FALSE;
  mHasEqualWithoutValue=PR_FALSE;
}

/**
 * 
 * @update	gess8/4/98
 * @param 
 * @return
 */
void CAttributeToken::Reinitialize(PRInt32 aTag, const nsString& aString){
  CHTMLToken::Reinitialize(aTag,aString);
  mTextKey.Truncate();
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
    PRUnichar theChar=mTextKey.Last();
    while(!nsCRT::IsAsciiAlpha(theChar) && !nsCRT::IsAsciiDigit(theChar)) {
      mTextKey.Truncate(length-1);
      length = mTextKey.Length();
      if(length <= 0) break;
      theChar = mTextKey.Last();
    }
  }
  return;
}

/*
 *  Dump contents of this token to given output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CAttributeToken::DebugDumpToken(nsOutputStream& out) {
  char buffer[200];
  mTextKey.ToCString(buffer,sizeof(buffer));
  out << "[" << GetClassName() << "] " << buffer << "=";
  mTextValue.ToCString(buffer,sizeof(buffer));
  out << buffer << ": " << mTypeID << nsEndl;
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
  anOutputString+=mTextKey;
  if(mTextValue.Length() || mHasEqualWithoutValue) 
    anOutputString.AppendWithConversion("=");
  anOutputString+=mTextValue;
  // anOutputString.AppendWithConversion(";");
}

/*
 *  This general purpose method is used when you want to
 *  consume a known quoted string. 
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult ConsumeQuotedString(PRUnichar aChar,nsString& aString,nsScanner& aScanner){
  nsresult result=NS_OK;
  PRInt32 theOffset=aScanner.GetOffset();

  switch(aChar) {
    case kQuote:
      result=aScanner.ReadUntil(aString,kQuote,PR_TRUE);
      if(NS_OK==result)
        result=aScanner.SkipOver(kQuote);  //this code is here in case someone mistakenly adds multiple quotes...
      break;
    case kApostrophe:
      result=aScanner.ReadUntil(aString,kApostrophe,PR_TRUE);
      if(NS_OK==result)
        result=aScanner.SkipOver(kApostrophe); //this code is here in case someone mistakenly adds multiple apostrophes...
      break;
    default:
      break;
  }

  // Ref: Bug 35806
  // A back up measure when disaster strikes...
  // Ex <table> <tr d="><td>hello</td></tr></table>
  PRUnichar ch=aString.Last();
  if(ch!=aChar) {
    if(!aScanner.IsIncremental() && result==kEOF) {
      aScanner.Mark(theOffset);
      aString.Assign(aChar);
      result=kBadStringLiteral; 
    }
    else {
        aString+=aChar;
    }
  }
  return result;
}

/*
 *  This general purpose method is used when you want to
 *  consume attributed text value.
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
static
nsresult ConsumeAttributeValueText(PRUnichar,nsString& aString,nsScanner& aScanner){
  static nsString theTerminals = NS_ConvertToString("\b\t\n\r >",6);
  nsresult result=aScanner.ReadUntil(aString,theTerminals,PR_FALSE);
  
  //Let's force quotes if either the first or last char is quoted.
  PRUnichar theLast=aString.Last();
  PRUnichar theFirst=aString.First();
  if(kQuote==theLast) {
    if(kQuote!=theFirst) {
      aString.Insert(kQuote,0);;
    }
  }
  else if(kQuote==theFirst) {
    if(kQuote!=theLast) {
      aString+=kQuote;
    }
  }

  return result;
}

/*
 *  Consume the key and value portions of the attribute.
 *  
 *  @update  rickg 03.23.2000
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @param   aRetainWhitespace -- 0=discard, 1=retain
 *  @return  error result
 */
nsresult CAttributeToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aRetainWhitespace) {

  nsresult result;
 
  //I changed a bit of this method to use aRetainWhitespace so that we do the right
  //thing in viewsource. The ws/cr/lf sequences are now maintained, and viewsource looks good.

  result=(aRetainWhitespace) ? aScanner.ReadWhitespace(mTextKey) : aScanner.SkipWhitespace();

  if(NS_OK==result) {
    result=aScanner.Peek(aChar);
    if(NS_OK==result) {
      
      if(kQuote==aChar) {               //if you're here, handle quoted key...
        result=aScanner.GetChar(aChar); //skip the quote character...
        if(NS_OK==result) {
          mTextKey.Append(aChar);
          result=ConsumeQuotedString(aChar,mTextKey,aScanner);
          if(!aRetainWhitespace)
            mTextKey.StripChars("\r\n"); //per the HTML spec, ignore linefeeds...
        }//if
      }
      else if((kHashsign==aChar) || (nsCRT::IsAsciiDigit(aChar))){
        result=aScanner.ReadNumber(mTextKey);
      }
      else {
          //If you're here, handle an unquoted key.
        static nsString theTerminals = NS_ConvertToString("\b\t\n\r \"=>",8);
        result=aScanner.ReadUntil(mTextKey,theTerminals,PR_FALSE);
      }

        //now it's time to Consume the (optional) value...
      if(NS_OK==result) {

        result=(aRetainWhitespace) ? aScanner.ReadWhitespace(mTextKey) : aScanner.SkipWhitespace();

        if(NS_OK==result) { 
          result=aScanner.Peek(aChar);       //Skip ahead until you find an equal sign or a '>'...
          if(NS_OK==result) {  
            if(kEqual==aChar){
              result=aScanner.GetChar(aChar);  //skip the equal sign...
              if(NS_OK==result) {

                result=(aRetainWhitespace) ? aScanner.ReadWhitespace(mTextValue) : aScanner.SkipWhitespace();

                if(NS_OK==result) {
                  result=aScanner.GetChar(aChar);  //and grab the next char.    
                  if(NS_OK==result) {
                    if((kQuote==aChar) || (kApostrophe==aChar)) {
                      mTextValue.Append(aChar);
                      result=ConsumeQuotedString(aChar,mTextValue,aScanner);
                      if(result==kBadStringLiteral) {
                        result=ConsumeAttributeValueText(aChar,mTextValue,aScanner);
                      }
                      // According to spec. we ( who? ) should ignore linefeeds. But look,
                      // even the carriage return was getting stripped ( wonder why! ) -
                      // Ref. to bug 15204.  Okay, so the spec. told us to ignore linefeeds,
                      // bug then what about bug 47535 ? Should we preserve everything then?
                      // Well, let's make it so! Commenting out the next two lines..
                      /*if(!aRetainWhitespace)
                        mTextValue.StripChars("\r\n"); //per the HTML spec, ignore linefeeds...
                      */
                    }
                    else if(kGreaterThan==aChar){      
                      mHasEqualWithoutValue=PR_TRUE;
                      result=aScanner.PutBack(aChar);
                    }
                    else if(kAmpersand==aChar) {
                      // XXX - Discard script entity for now....except in
                      // view-source
                      PRBool discard=!aRetainWhitespace; 
                      mTextValue.Append(aChar);
                      result=aScanner.GetChar(aChar);
                      if(NS_OK==result) {
                        mTextValue.Append(aChar);
                        result=CEntityToken::ConsumeEntity(aChar,mTextValue,aScanner);
                      }
                      if(discard) mTextValue.Truncate();
                    }
                    else {
                      mTextValue.Append(aChar);       //it's an alphanum attribute...
                      result=ConsumeAttributeValueText(aChar,mTextValue,aScanner);
                    } 
                  }//if
                  if(NS_OK==result) {
                    result=(aRetainWhitespace) ? aScanner.ReadWhitespace(mTextValue) : aScanner.SkipWhitespace();
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

              if(kQuote==aChar){
                result=aScanner.SkipOver(aChar); //strip quote.
              }
            }
          }//if
        } //if
      }//if (consume optional value)

      if(NS_OK==result) {
        result=aScanner.Peek(aChar);
        mLastAttribute= PRBool((kGreaterThan==aChar) || (kEOF==result));
      }
    } //if
  }//if
  return result;
}

/*
 *  Dump contents of this token to givne output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CAttributeToken::DebugDumpSource(nsOutputStream& out) {
  static char buffer[1000];
  mTextKey.ToCString(buffer,sizeof(buffer));
  out << " " << buffer;
  if(mTextValue.Length()){
    mTextValue.ToCString(buffer,sizeof(buffer));
    out << "=" << buffer;
  }
  if(mLastAttribute)
    out<<">";
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
CWhitespaceToken::CWhitespaceToken(const nsString& aName) : CHTMLToken(aName) {
  mTypeID=eHTMLTag_whitespace;
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
nsresult CWhitespaceToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aMode) {
  mTextValue.Assign(aChar);
  nsresult result=aScanner.ReadWhitespace(mTextValue);
  if(NS_OK==result) {
    mTextValue.StripChar(kCR);
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
CEntityToken::CEntityToken() : CHTMLToken(eHTMLTag_entity) {
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CEntityToken::CEntityToken(const nsString& aName) : CHTMLToken(aName) {
  mTypeID=eHTMLTag_entity;
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
nsresult CEntityToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aMode) {
  if(aChar)
    mTextValue.Assign(aChar);
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
PRInt32 CEntityToken::ConsumeEntity(PRUnichar aChar,nsString& aString,nsScanner& aScanner){
  PRUnichar theChar=0;
  PRInt32 result=aScanner.Peek(theChar);
  if(NS_OK==result) {
    if(kLeftBrace==aChar) {
      //you're consuming a script entity...
      PRInt32 rightBraceCount = 0;
      PRInt32 leftBraceCount  = 1;
      while(leftBraceCount!=rightBraceCount) {
        result=aScanner.GetChar(aChar);
        if(NS_OK!=result) return result;
        aString += aChar;
        if(aChar==kRightBrace)
          rightBraceCount++;
        else if(aChar==kLeftBrace)
          leftBraceCount++;
      }
      result=aScanner.ReadUntil(aString,kSemicolon,PR_FALSE);
      if(NS_OK==result) {
        result=aScanner.GetChar(aChar); // This character should be a semicolon
        if(NS_OK==result) aString += aChar;
      }
    } //if
    else {
      if(kHashsign==aChar) {
        if('X'==(toupper((char)theChar))) {
          result=aScanner.GetChar(theChar);
          aString+=theChar;
        }
        if(NS_OK==result){
          result=aScanner.ReadNumber(aString);
        }
      }
      else result=aScanner.ReadIdentifier(aString,PR_TRUE); // Ref. Bug# 23791 - For setting aIgnore to PR_TRUE.
      if(NS_OK==result) {
        result=aScanner.Peek(theChar);
        if(NS_OK==result) {
          if (kSemicolon == theChar) {
            // consume semicolon that stopped the scan
            aString+=theChar;
            result=aScanner.GetChar(theChar);
          }
        }
      }//if
    } //else
  } //if
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
	NOT_USED,
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
	0x017D,  /* CAPITAL Z HACEK */
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
	NOT_USED,
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

/*
 *  Dump contents of this token to givne output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CEntityToken::DebugDumpSource(nsOutputStream& out) {
  char* cp=mTextValue.ToNewCString();
  out << "&" << *cp;
  delete[] cp;
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
CScriptToken::CScriptToken(const nsString& aString) : CHTMLToken(aString,eHTMLTag_script) {
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

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CStyleToken::CStyleToken() : CHTMLToken(eHTMLTag_style) {
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


/*
 *  string based constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CSkippedContentToken::CSkippedContentToken(const nsString& aName) : CAttributeToken(aName) {
  mTextKey.AssignWithConversion("$skipped-content");/* XXX need a better answer! */
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CSkippedContentToken::GetClassName(void) {
  return "skipped";
}

/*
 *  Retrieve the token type as an int.
 *  @update  gess 3/25/98 
 *  @return  
 */
PRInt32 CSkippedContentToken::GetTokenType(void) {
  return eToken_skippedcontent;
}

/* 
 *  Consume content until you find an end sequence that matches 
 *  this objects current mTextValue. Note that this is complicated 
 *  by the fact that you can be parsing content that itself 
 *  contains quoted content of the same type (like <SCRIPT>). 
 *  That means we have to look for quote-pairs, and ignore the 
 *  content inside them. 
 * 
 *  @update  gess 7/25/98 
 *  @param   aScanner -- controller of underlying input source 
 *  @return  error result 
 */ 
nsresult CSkippedContentToken::Consume(PRUnichar aChar,nsScanner& aScanner,PRInt32 aMode) { 
  PRBool      done=PR_FALSE; 
  nsresult    result=NS_OK; 
  nsString    temp; 
  PRUnichar   theChar;

  //We're going to try a new algorithm here. Rather than scan for the matching 
 //end tag like we used to do, we're now going to scan for whitespace and comments. 
 //If we find either, just eat them. If we find text or a tag, then go to the 
 //target endtag, or the start of another comment. 

  while((!done) && (NS_OK==result)) { 
    result=aScanner.GetChar(aChar); 
    if((NS_OK==result) && (kLessThan==aChar)) { 
      //we're reading a tag or a comment... 
      result=aScanner.GetChar(theChar); 
      if((NS_OK==result) && (kExclamation==theChar)) { 
        //read a comment... 
        static CCommentToken theComment; 
        result=theComment.Consume(aChar,aScanner,aMode); 
        if(NS_OK==result) { 
          //result=aScanner.SkipWhitespace();
          temp.Append(theComment.GetStringValueXXX()); 
        } 
      } else { 
        //read a tag... 
        temp+=aChar; 
        temp+=theChar; 
        result=aScanner.ReadUntil(temp,kGreaterThan,PR_TRUE); 
      } 
    } 
    else if(('\b'==theChar) || ('\t'==theChar) || (' '==theChar)) {
      static CWhitespaceToken theWS; 
      result=theWS.Consume(aChar,aScanner,aMode); 
      if(NS_OK==result) { 
        temp.Append(theWS.GetStringValueXXX()); 
      } 
    } 
    else { 
      temp+=aChar; 
      result=aScanner.ReadUntil(temp,kLessThan,PR_FALSE); 
    } 
    nsAutoString theRight;
    temp.Right(theRight,mTextValue.Length());
    done=PRBool(0==theRight.CompareWithConversion(mTextValue,PR_TRUE)); 
  } 
  int len=temp.Length(); 
  temp.Truncate(len-mTextValue.Length()); 
  mTextKey=temp; 
  return result; 
} 

/*
 *  Dump contents of this token to givne output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CSkippedContentToken::DebugDumpSource(nsOutputStream& out) {
  static char buffer[1000];
  mTextKey.ToCString(buffer,sizeof(buffer));
  out << " " << buffer;
  if(mLastAttribute)
    out<<">";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   anOutputString will recieve the result
 *  @return  nada
 */
void CSkippedContentToken::GetSource(nsString& anOutputString){
  anOutputString.AppendWithConversion("$skipped-content");
}

/*
 *  
 *  
 *  @update  harishd 03/23/00
 *  @param   result appended to the output string.
 *  @return  nada
 */
void CSkippedContentToken::AppendSource(nsString& anOutputString){
  anOutputString.AppendWithConversion("$skipped-content");
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
CInstructionToken::CInstructionToken() : CHTMLToken(eHTMLTag_unknown) {
}

/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
CInstructionToken::CInstructionToken(const nsString& aString) : CHTMLToken(aString) {
}

/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
nsresult CInstructionToken::Consume(PRUnichar aChar,nsScanner& aScanner,PRInt32 aMode){
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

// Doctype decl token

CDoctypeDeclToken::CDoctypeDeclToken(eHTMLTags aTag) : CHTMLToken(aTag) {
}

/**
 *  This method consumes a doctype element.
 *  Note: I'm rewriting this method to seek to the first <, since quotes can really screw us up.
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
nsresult CDoctypeDeclToken::Consume(PRUnichar aChar, nsScanner& aScanner,PRInt32 aMode) {
    
  nsresult result =NS_OK;
 
  nsString& theBuffer=aScanner.GetBuffer();
  PRInt32   theCurrOffset=aScanner.GetOffset();
  PRInt32   thePos=(aScanner.GetBuffer()).FindChar(kLessThan,PR_TRUE,theCurrOffset);

  mTextValue.AssignWithConversion("<!");
    
  if(thePos>-1) {
    result=aScanner.ReadUntil(mTextValue,'<',PR_FALSE);
  }
  else {
    result=aScanner.ReadUntil(mTextValue,'>',PR_TRUE);
  }
  return result;
}

const char*  CDoctypeDeclToken::GetClassName(void) {
  return "doctype";
}

PRInt32 CDoctypeDeclToken::GetTokenType(void) {
  return eToken_doctypeDecl;
}
