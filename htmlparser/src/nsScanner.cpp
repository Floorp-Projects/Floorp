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

//#define __INCREMENTAL 1

#define NS_IMPL_IDS
#include "nsScanner.h"
#include "nsDebug.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsFileSpec.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

const char* kBadHTMLText="<H3>Oops...</H3>You just tried to read a non-existent document: <BR>";
const char* kUnorderedStringError = "String argument must be ordered. Don't you read API's?";

#ifdef __INCREMENTAL
const int   kBufsize=1;
#else
const int   kBufsize=64;
#endif

MOZ_DECL_CTOR_COUNTER(nsScanner);

/**
 *  Use this constructor if you want i/o to be based on 
 *  a single string you hand in during construction.
 *  This short cut was added for Javascript.
 *
 *  @update  gess 5/12/98
 *  @param   aMode represents the parser mode (nav, other)
 *  @return  
 */
nsScanner::nsScanner(nsString& anHTMLString, const nsString& aCharset, nsCharsetSource aSource) : 
  mBuffer(anHTMLString)
{
  MOZ_COUNT_CTOR(nsScanner);

  mTotalRead=mBuffer.Length();
  mIncremental=PR_FALSE;
  mOwnsStream=PR_FALSE;
  mOffset=0;
  mMarkPos=0;
  mInputStream=0;
  mUnicodeDecoder = 0;
  mCharsetSource = kCharsetUninitialized;
  SetDocumentCharset(aCharset, aSource);
  mNewlinesSkipped=0;
}

/**
 *  Use this constructor if you want i/o to be based on strings 
 *  the scanner receives. If you pass a null filename, you
 *  can still provide data to the scanner via append.
 *
 *  @update  gess 5/12/98
 *  @param   aFilename --
 *  @return  
 */
nsScanner::nsScanner(nsString& aFilename,PRBool aCreateStream, const nsString& aCharset, nsCharsetSource aSource) : 
    mFilename(aFilename)
{
  MOZ_COUNT_CTOR(nsScanner);

  mIncremental=PR_TRUE;
  mOffset=0;
  mMarkPos=0;
  mTotalRead=0;
  mOwnsStream=aCreateStream;
  mInputStream=0;
  if(aCreateStream) {
		mInputStream = new nsInputFileStream(nsFileSpec(aFilename));
  } //if
  mUnicodeDecoder = 0;
  mCharsetSource = kCharsetUninitialized;
  SetDocumentCharset(aCharset, aSource);
  mNewlinesSkipped=0;
}

/**
 *  Use this constructor if you want i/o to be stream based.
 *
 *  @update  gess 5/12/98
 *  @param   aStream --
 *  @param   assumeOwnership --
 *  @param   aFilename --
 *  @return  
 */
nsScanner::nsScanner(nsString& aFilename,nsInputStream& aStream,const nsString& aCharset, nsCharsetSource aSource) :
    mFilename(aFilename)
{  
  MOZ_COUNT_CTOR(nsScanner);

  mIncremental=PR_FALSE;
  mOffset=0;
  mMarkPos=0;
  mTotalRead=0;
  mOwnsStream=PR_FALSE;
  mInputStream=&aStream;
  mUnicodeDecoder = 0;
  mCharsetSource = kCharsetUninitialized;
  SetDocumentCharset(aCharset, aSource);
  mNewlinesSkipped=0;
}


nsresult nsScanner::SetDocumentCharset(const nsString& aCharset , nsCharsetSource aSource) {

  nsresult res = NS_OK;

  if( aSource < mCharsetSource) // priority is lower the the current one , just
    return res;

  NS_WITH_SERVICE(nsICharsetAlias, calias, kCharsetAliasCID, &res);
  NS_ASSERTION( nsnull != calias, "cannot find charset alias");
  nsAutoString charsetName; charsetName.Assign(aCharset);
  if( NS_SUCCEEDED(res) && (nsnull != calias))
  {
    PRBool same = PR_FALSE;
    res = calias->Equals(aCharset, mCharset, &same);
    if(NS_SUCCEEDED(res) && same)
    {
      return NS_OK; // no difference, don't change it
    }
    // different, need to change it
    res = calias->GetPreferred(aCharset, charsetName);

    if(NS_FAILED(res) && (kCharsetUninitialized == mCharsetSource) )
    {
       // failed - unknown alias , fallback to ISO-8859-1
      charsetName.AssignWithConversion("ISO-8859-1");
    }
    mCharset = charsetName;
    mCharsetSource = aSource;

    NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &res);
    if(NS_SUCCEEDED(res) && (nsnull != ccm))
    {
      nsIUnicodeDecoder * decoder = nsnull;
      res = ccm->GetUnicodeDecoder(&mCharset, &decoder);
      if(NS_SUCCEEDED(res) && (nsnull != decoder))
      {
         NS_IF_RELEASE(mUnicodeDecoder);

         mUnicodeDecoder = decoder;
      }    
    }
  }
  return res;
}


/**
 *  default destructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsScanner::~nsScanner() {
    
  MOZ_COUNT_DTOR(nsScanner);

  if(mInputStream) {
    mInputStream->close();
    if(mOwnsStream)
      delete mInputStream;
  }
  mInputStream=0;
  NS_IF_RELEASE(mUnicodeDecoder);
}

/**
 *  Resets current offset position of input stream to marked position. 
 *  This allows us to back up to this point if the need should arise, 
 *  such as when tokenization gets interrupted.
 *  NOTE: IT IS REALLY BAD FORM TO CALL RELEASE WITHOUT CALLING MARK FIRST!
 *
 *  @update  gess 5/12/98
 *  @param   
 *  @return  
 */
PRUint32 nsScanner::RewindToMark(void){
  mOffset=mMarkPos;
  return mOffset;
}


/**
 *  Records current offset position in input stream. This allows us
 *  to back up to this point if the need should arise, such as when
 *  tokenization gets interrupted.
 *
 *  @update  gess 7/29/98
 *  @param   
 *  @return  
 */
PRUint32 nsScanner::Mark(PRInt32 anIndex){
  if(kNotFound==anIndex) {
    if((mOffset>0) && (mOffset>eBufferSizeThreshold)) {
      mBuffer.Cut(0,mOffset);   //delete chars up to mark position
      mOffset=0;
    }
    mMarkPos=mOffset;
  }
  else mOffset=(PRUint32)anIndex;
  return 0;
}
 

/** 
 * Insert data to our underlying input buffer as
 * if it were read from an input stream.
 *
 * @update  harishd 01/12/99
 * @return  error code 
 */
PRBool nsScanner::Insert(const nsAReadableString& aBuffer) {

  mBuffer.Insert(aBuffer,mOffset);
  mTotalRead+=aBuffer.Length();
  return PR_TRUE;
}

/** 
 * Append data to our underlying input buffer as
 * if it were read from an input stream.
 *
 * @update  gess4/3/98
 * @return  error code 
 */
PRBool nsScanner::Append(const nsAReadableString& aBuffer) {

  PRUint32 theLen=mBuffer.Length();

  mBuffer.Append(aBuffer);
  mTotalRead+=aBuffer.Length();
  if(theLen<mBuffer.Length()) {

    //Now yank any nulls that were embedded in this given buffer
    mBuffer.StripChar(0,theLen);
  }

  return PR_TRUE;
}

/**
 *  
 *  
 *  @update  gess 5/21/98
 *  @param   
 *  @return  
 */
PRBool nsScanner::Append(const char* aBuffer, PRUint32 aLen){

  PRUint32 theLen=mBuffer.Length();
  
  if(mUnicodeDecoder) {
    PRInt32 unicharBufLen = 0;
    mUnicodeDecoder->GetMaxLength(aBuffer, aLen, &unicharBufLen);
    mUnicodeXferBuf.SetCapacity(unicharBufLen+32);
    mUnicodeXferBuf.Truncate();
    PRUnichar *unichars = (PRUnichar*)mUnicodeXferBuf.GetUnicode();
	  
    nsresult res;
	  do {
	    PRInt32 srcLength = aLen;
		  PRInt32 unicharLength = unicharBufLen;
		  res = mUnicodeDecoder->Convert(aBuffer, &srcLength, unichars, &unicharLength);
      unichars[unicharLength]=0;  //add this since the unicode converters can't be trusted to do so.

		  mBuffer.Append(unichars, unicharLength);
		  mTotalRead += unicharLength;
                  // if we failed, we consume one byte by replace it with U+FFFD
                  // and try conversion again.
		  if(NS_FAILED(res)) {
			  mUnicodeDecoder->Reset();
			  mBuffer.Append( (PRUnichar)0xFFFD);
			  mTotalRead++;
			  if(((PRUint32) (srcLength + 1)) > aLen)
				  srcLength = aLen;
			  else 
				  srcLength++;
			  aBuffer += srcLength;
			  aLen -= srcLength;
		  }
	  } while (NS_FAILED(res) && (aLen > 0));
          // we continue convert the bytes data into Unicode 
          // if we have conversion error and we have more data.

	  // delete[] unichars;
  }
  else {
    mBuffer.AppendWithConversion(aBuffer,aLen);
    mTotalRead+=aLen;

  }

  if(theLen<mBuffer.Length()) {
    //Now yank any nulls that were embedded in this given buffer
    mBuffer.StripChar(0,theLen);
  }

  return PR_TRUE;
}


/** 
 * Append PRUNichar* data to the internal buffer of the scanner
 *
 * @update  gess4/3/98
 * @return  error code (it's always true)
 */
PRBool nsScanner::Append(const PRUnichar* aBuffer, PRUint32 aLen){

  if(-1==(PRInt32)aLen)
    aLen=nsCRT::strlen(aBuffer);

  CBufDescriptor theDesc(aBuffer,PR_TRUE, aLen+1,aLen);
  nsAutoString theBuffer(theDesc);  

  PRUint32 theLen=mBuffer.Length();  
  mBuffer.Append(theBuffer);
  mTotalRead+=aLen;

  if(theLen<mBuffer.Length()) {
    //Now yank any nulls that were embedded in this given buffer
    mBuffer.StripChar(0,theLen);
  }

  return PR_TRUE;
}

/** 
 * Grab data from underlying stream.
 *
 * @update  gess4/3/98
 * @return  error code
 */
nsresult nsScanner::FillBuffer(void) {
  nsresult result=NS_OK;

  if(!mInputStream) {
#if 0
    //This is DEBUG code!!!!!!  XXX DEBUG XXX
    //If you're here, it means someone tried to load a
    //non-existent document. So as a favor, we emit a
    //little bit of HTML explaining the error.
    if(0==mTotalRead) {
      mBuffer.Append((const char*)kBadHTMLText);
      mBuffer.Append(mFilename);
      mTotalRead+=mBuffer.Length();
    }
    else 
#endif
    result=kEOF;
  }
  else {
    PRInt32 numread=0;
    char buf[kBufsize+1];
    buf[kBufsize]=0;

    if(mInputStream) {
    	numread = mInputStream->read(buf, kBufsize);
      if (0 == numread) {
        return kEOF;
      }
    }
    mOffset=mBuffer.Length();
    if((0<numread) && (0==result)) {
      mBuffer.AppendWithConversion((const char*)buf,numread);
      mBuffer.StripChar(0);  //yank the nulls that come in from the net.
    }
    mTotalRead+=mBuffer.Length();
  }

  return result;
}

/**
 *  determine if the scanner has reached EOF
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  0=!eof 1=eof 
 */
nsresult nsScanner::Eof() {
  nsresult theError=NS_OK;

  if(mOffset>=(PRUint32)mBuffer.Length()) {
    theError=FillBuffer();  
  }
  
  if(NS_OK==theError) {
    if (0==(PRUint32)mBuffer.Length()) {
      return kEOF;
    }
  }

  return theError;
}

/**
 *  retrieve next char from scanners internal input stream
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code reflecting read status
 */
nsresult nsScanner::GetChar(PRUnichar& aChar) {
  nsresult result=NS_OK;
  aChar=0;  
  if(mOffset>=(PRUint32)mBuffer.Length()) 
    result=Eof();

  if(NS_OK == result){
    aChar=GetCharAt(mBuffer,mOffset++);
  }
  return result;
}


/**
 *  peek ahead to consume next char from scanner's internal
 *  input buffer
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsresult nsScanner::Peek(PRUnichar& aChar) {
  nsresult result=NS_OK;
  aChar=0;  
  if(mOffset>=(PRUint32)mBuffer.Length()) 
    result=Eof();

  if(NS_OK == result){
    aChar=GetCharAt(mBuffer,mOffset);
  }

  return result;
}


/**
 *  Push the given char back onto the scanner
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
nsresult nsScanner::PutBack(PRUnichar aChar) {
  if(mOffset>0)
    mOffset--;
  else mBuffer.Insert(aChar,0);
  return NS_OK;
}


/**
 *  Skip whitespace on scanner input stream
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error status
 */
nsresult nsScanner::SkipWhitespace(void) {

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  const PRUnichar*  theBuf=mBuffer.GetUnicode();
  PRInt32           theOrigin=mOffset;
  PRBool            found=PR_FALSE;  

  mNewlinesSkipped = 0;

  while(NS_OK==result) {
 
    theChar=theBuf[mOffset++];
    if(theChar) {
      switch(theChar) {
        case '\n': mNewlinesSkipped++;
        case ' ' :
        case '\r':
        case '\b':
        case '\t':
          found=PR_TRUE;
          break;
        default:
          found=PR_FALSE;
          break;
      }
      if(!found) {
        mOffset-=1;
        break;
      }
    }
    else if ((PRUint32)mBuffer.Length()<=mOffset) {
      mOffset-=1;
      result=Peek(theChar);
      theBuf=mBuffer.GetUnicode();
      theOrigin=mOffset;
    }
  }

  //DoErrTest(aString);

  return result;

}

/**
 *  Skip over chars as long as they equal given char
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
nsresult nsScanner::SkipOver(PRUnichar aSkipChar){

  PRUnichar ch=0;
  nsresult   result=NS_OK;

  while(NS_OK==result) {
    result=GetChar(ch);
    if(NS_OK == result) {
      if(ch!=aSkipChar) {
        PutBack(ch);
        break;
      }
    } 
    else break;
  } //while
  return result;

}

/**
 *  Skip over chars as long as they're in aSkipSet
 *  
 *  @update  gess 3/25/98
 *  @param   aSkipSet is an ordered string.
 *  @return  error code
 */
nsresult nsScanner::SkipOver(nsString& aSkipSet){

  PRUnichar theChar=0;
  nsresult  result=NS_OK;

  while(NS_OK==result) {
    result=GetChar(theChar);
    if(NS_OK == result) {
      PRInt32 pos=aSkipSet.FindChar(theChar);
      if(kNotFound==pos) {
        PutBack(theChar);
        break;
      }
    } 
    else break;
  } //while
  return result;

}


/**
 *  Skip over chars until they're in aValidSet
 *  
 *  @update  gess 3/25/98
 *  @param   aValid set is an ordered string that 
 *           contains chars you're looking for
 *  @return  error code
 */
nsresult nsScanner::SkipTo(nsString& aValidSet){
  PRUnichar ch=0;
  nsresult  result=NS_OK;

  while(NS_OK==result) {
    result=GetChar(ch);
    if(NS_OK == result) {
      PRInt32 pos=aValidSet.FindChar(ch);
      if(kNotFound!=pos) {
        PutBack(ch);
        break;
      }
    } 
    else break;
  } //while
  return result;
}

#if 0
void DoErrTest(nsString& aString) {
  PRInt32 pos=aString.FindChar(0);
  if(kNotFound<pos) {
    if(aString.Length()-1!=pos) {
    }
  }
}

void DoErrTest(nsCString& aString) {
  PRInt32 pos=aString.FindChar(0);
  if(kNotFound<pos) {
    if(aString.Length()-1!=pos) {
    }
  }
}
#endif

/**
 *  Skip over chars as long as they're in aValidSet
 *  
 *  @update  gess 3/25/98
 *  @param   aValidSet is an ordered string containing the 
 *           characters you want to skip
 *  @return  error code
 */
nsresult nsScanner::SkipPast(nsString& aValidSet){
  NS_NOTYETIMPLEMENTED("Error: SkipPast not yet implemented.");
  return NS_OK;
}

/**
 *  Consume characters until you did not find the terminal char
 *  
 *  @update  gess 3/25/98
 *  @param   aString - receives new data from stream
 *  @param   aIgnore - If set ignores ':','-','_'
 *  @return  error code
 */
nsresult nsScanner::GetIdentifier(nsSubsumeStr& aString,PRBool allowPunct) {

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  const PRUnichar*  theBuf=mBuffer.GetUnicode();
  PRInt32           theOrigin=mOffset;
  PRBool            found=PR_FALSE;  

  while(NS_OK==result) {
 
    theChar=theBuf[mOffset++];
    if(theChar) {
      found=PR_FALSE;
      switch(theChar) {
        case ':':
        case '_':
        case '-':
          found=allowPunct;
          break;
        default:
          if(('a'<=theChar) && (theChar<='z'))
            found=PR_TRUE;
          else if(('A'<=theChar) && (theChar<='Z'))
            found=PR_TRUE;
          else if(('0'<=theChar) && (theChar<='9'))
            found=PR_TRUE;
          break;
      }

      if(!found) {
        mOffset-=1;
        PRUnichar* thePtr=(PRUnichar*)&theBuf[theOrigin-1];
        aString.Subsume(thePtr,PR_FALSE,mOffset-theOrigin+1);
        break;
      }
    }
  }

  //DoErrTest(aString);

  return result;
}

/**
 *  Consume characters until you did not find the terminal char
 *  
 *  @update  gess 3/25/98
 *  @param   aString - receives new data from stream
 *  @param   allowPunct - If set ignores ':','-','_'
 *  @return  error code
 */
nsresult nsScanner::ReadIdentifier(nsString& aString,PRBool allowPunct) {

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  const PRUnichar*  theBuf=mBuffer.GetUnicode();
  PRInt32           theOrigin=mOffset;
  PRBool            found=PR_FALSE;  

  while(NS_OK==result) {
 
    theChar=theBuf[mOffset++];
    if(theChar) {
      found=PR_FALSE;
      switch(theChar) {
        case ':':
        case '_':
        case '-':
          found=allowPunct;
          break;
        default:
          if(('a'<=theChar) && (theChar<='z'))
            found=PR_TRUE;
          else if(('A'<=theChar) && (theChar<='Z'))
            found=PR_TRUE;
          else if(('0'<=theChar) && (theChar<='9'))
            found=PR_TRUE;
          break;
      }

      if(!found) {
        mOffset-=1;
        aString.Append(&theBuf[theOrigin],mOffset-theOrigin);
        break;
      }
    }
    else if ((PRUint32)mBuffer.Length()<=mOffset) {
      mOffset -= 1;
      aString.Append(&theBuf[theOrigin],mOffset-theOrigin);
      result=Peek(theChar);
      theBuf=mBuffer.GetUnicode();
      theOrigin=mOffset;
    }
  }

  //DoErrTest(aString);

  return result;
}

/**
 *  Consume characters until you find the terminal char
 *  
 *  @update  gess 3/25/98
 *  @param   aString receives new data from stream
 *  @param   addTerminal tells us whether to append terminal to aString
 *  @return  error code
 */
nsresult nsScanner::ReadNumber(nsString& aString) {

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  const PRUnichar*  theBuf=mBuffer.GetUnicode();
  PRInt32           theOrigin=mOffset;
  PRBool            found=PR_FALSE;  

  while(NS_OK==result) {
 
    theChar=theBuf[mOffset++];
    if(theChar) {
      found=PR_FALSE;
      if(('a'<=theChar) && (theChar<='f'))
        found=PR_TRUE;
      else if(('A'<=theChar) && (theChar<='F'))
        found=PR_TRUE;
      else if(('0'<=theChar) && (theChar<='9'))
        found=PR_TRUE;
      else if('#'==theChar)
        found=PR_TRUE;
      if(!found) {
        mOffset-=1;
        aString.Append(&theBuf[theOrigin],mOffset-theOrigin);
        break;
      }
    }
    else if ((PRUint32)mBuffer.Length()<=mOffset) {
      mOffset -= 1;
      aString.Append(&theBuf[theOrigin],mOffset-theOrigin);
      result=Peek(theChar);
      theBuf=mBuffer.GetUnicode();
      theOrigin=mOffset;
    }
  }

  //DoErrTest(aString);

  return result;
}

/**
 *  Consume characters until you find the terminal char
 *  
 *  @update  gess 3/25/98
 *  @param   aString receives new data from stream
 *  @param   addTerminal tells us whether to append terminal to aString
 *  @return  error code
 */
nsresult nsScanner::ReadWhitespace(nsString& aString) {

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  const PRUnichar*  theBuf=mBuffer.GetUnicode();
  PRInt32           theOrigin=mOffset;
  PRBool            found=PR_FALSE;  

  while(NS_OK==result) {
 
    theChar=theBuf[mOffset++];
    if(theChar) {
      switch(theChar) {
        case ' ':
        case '\b':
        case '\t':
        case kLF:
        case kCR:
          found=PR_TRUE;
          break;
        default:
          found=PR_FALSE;
          break;
      }
      if(!found) {
        mOffset-=1;
        aString.Append(&theBuf[theOrigin],mOffset-theOrigin);
        break;
      }
    }
    else if ((PRUint32)mBuffer.Length()<=mOffset) {
      mOffset -= 1;
      aString.Append(&theBuf[theOrigin],mOffset-theOrigin);
      result=Peek(theChar);
      theBuf=mBuffer.GetUnicode();
      theOrigin=mOffset;
    }
  }

  //DoErrTest(aString);

  return result;
}


/**
 *  Consume chars as long as they are <i>in</i> the 
 *  given validSet of input chars.
 *  
 *  @update  gess 3/25/98
 *  @param   aString will contain the result of this method
 *  @param   aValidSet is an ordered string that contains the
 *           valid characters
 *  @return  error code
 */
nsresult nsScanner::ReadWhile(nsString& aString,
                             nsString& aValidSet,
                             PRBool addTerminal){

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  const PRUnichar*  theBuf=mBuffer.GetUnicode();
  PRInt32           theOrigin=mOffset;

  while(NS_OK==result) {
 
    theChar=theBuf[mOffset++];
    if(theChar) {
      PRInt32 pos=aValidSet.FindChar(theChar);
      if(kNotFound==pos) {
        if(!addTerminal)
          mOffset-=1;
        aString.Append(&theBuf[theOrigin],mOffset-theOrigin);
        break;
      }
    }
    else if ((PRUint32)mBuffer.Length()<=mOffset) {
      mOffset -= 1;
      aString.Append(&theBuf[theOrigin],mOffset-theOrigin);
      result=Peek(theChar);
      theBuf=mBuffer.GetUnicode();
      theOrigin=mOffset;
    }
  }

  //DoErrTest(aString);

  return result;

}

/**
 *  Consume characters until you encounter one contained in given
 *  input set.
 *  
 *  @update  gess 3/25/98
 *  @param   aString will contain the result of this method
 *  @param   aTerminalSet is an ordered string that contains
 *           the set of INVALID characters
 *  @return  error code
 */
nsresult nsScanner::ReadUntil(nsString& aString,
                             nsString& aTerminalSet,
                             PRBool addTerminal){
  

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  const PRUnichar*  theBuf=mBuffer.GetUnicode();
  PRInt32           theOrigin=mOffset;

  while(NS_OK==result) {
 
    theChar=theBuf[mOffset++];
    if(theChar) {
      PRInt32 pos=aTerminalSet.FindChar(theChar);
      if(kNotFound!=pos) {
        if(!addTerminal)
          mOffset-=1;
        aString.Append(&theBuf[theOrigin],mOffset-theOrigin);
        break;
      }
    }
    else if ((PRUint32)mBuffer.Length()<=mOffset) {
      mOffset -= 1;
      aString.Append(&theBuf[theOrigin],mOffset-theOrigin);
      result=Peek(theChar);
      theBuf=mBuffer.GetUnicode();
      theOrigin=mOffset;
    }
  }

  //DoErrTest(aString);

  return result;

}

/**
 *  Consume characters until you encounter one contained in given
 *  input set.
 *  
 *  @update  gess 3/25/98
 *  @param   aString will contain the result of this method
 *  @param   aTerminalSet is an ordered string that contains
 *           the set of INVALID characters
 *  @return  error code
 */
nsresult nsScanner::ReadUntil(nsString& aString,
                             nsCString& aTerminalSet,
                             PRBool addTerminal){
  
  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  const PRUnichar*  theBuf=mBuffer.GetUnicode();
  PRInt32           theOrigin=mOffset;

  while(NS_OK==result) {
 
    theChar=theBuf[mOffset++];
    if(theChar) {
      PRInt32 pos=aTerminalSet.FindChar(theChar);
      if(kNotFound!=pos) {
        if(!addTerminal)
          mOffset-=1;
        aString.Append(&theBuf[theOrigin],mOffset-theOrigin);
        break;
      }
    }
    else if ((PRUint32)mBuffer.Length()<=mOffset) {
      mOffset -= 1;
      aString.Append(&theBuf[theOrigin],mOffset-theOrigin);
      result=Peek(theChar);
      theBuf=mBuffer.GetUnicode();
      theOrigin=mOffset;
    }
  }

  //DoErrTest(aString);

  return result;
}


/**
 *  Consume characters until you encounter one contained in given
 *  input set.
 *  
 *  @update  gess 3/25/98
 *  @param   aString will contain the result of this method
 *  @param   aTerminalSet is an ordered string that contains
 *           the set of INVALID characters
 *  @return  error code
 */
nsresult nsScanner::ReadUntil(nsString& aString,
                              const char* aTerminalSet,
                             PRBool addTerminal)
{
  nsresult   result=NS_OK;
  if(aTerminalSet) {
    PRInt32 len=nsCRT::strlen(aTerminalSet);
    if(0<len) {

      CBufDescriptor buf(aTerminalSet,PR_TRUE,len+1,len);
      nsCAutoString theSet(buf);

      result=ReadUntil(aString,theSet,addTerminal);
    } //if
  }//if
  return result;
}

/**
 *  Consumes chars until you see the given terminalChar
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
nsresult nsScanner::ReadUntil(nsString& aString,
                             PRUnichar aTerminalChar,
                             PRBool addTerminal){
  PRUnichar theChar=0;
  nsresult  result=NS_OK;

  const PRUnichar*  theBuf=mBuffer.GetUnicode();
  PRInt32           theOrigin=mOffset;
  result=Peek(theChar);
  PRUint32 theLen=mBuffer.Length();

  while(NS_OK==result) {
    
    theChar=theBuf[mOffset++];
    if(theChar) {
      if(aTerminalChar==theChar) {
        if(!addTerminal)
          mOffset-=1;
        aString.Append(&theBuf[theOrigin],mOffset-theOrigin);
        break;
      }
    }
    else if ((PRUint32)mBuffer.Length()<=mOffset) {
      mOffset -= 1;
      aString.Append(&theBuf[theOrigin],theLen-theOrigin);
      mOffset=theLen;
      result=Peek(theChar);
      theLen=mBuffer.Length();
    }
  }

  //DoErrTest(aString);
  return result;

}


/**
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsString& nsScanner::GetBuffer(void) {
  return mBuffer;
}

/**
 *  Call this to copy bytes out of the scanner that have not yet been consumed
 *  by the tokenization process.
 *  
 *  @update  gess 5/12/98
 *  @param   aCopyBuffer is where the scanner buffer will be copied to
 *  @return  nada
 */
void nsScanner::CopyUnusedData(nsString& aCopyBuffer) {
  PRInt32 theLen=mBuffer.Length();
  if(0<theLen) {
    mBuffer.Right(aCopyBuffer,theLen-mMarkPos);
  }
}

/**
 *  Retrieve the name of the file that the scanner is reading from.
 *  In some cases, it's just a given name, because the scanner isn't
 *  really reading from a file.
 *  
 *  @update  gess 5/12/98
 *  @return  
 */
nsString& nsScanner::GetFilename(void) {
  return mFilename;
}

/**
 *  Conduct self test. Actually, selftesting for this class
 *  occurs in the parser selftest.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */

void nsScanner::SelfTest(void) {
#ifdef _DEBUG
#endif
}



