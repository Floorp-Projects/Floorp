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

#include "nsAFMObject.h"


static struct keyname_st
{
  char *name;
  AFMKey key;
} keynames[] =
{
  {"Ascender", 			kAscender},
  {"Axes", 			kAxes},
  {"AxisLabel", 		kAxisLabel},
  {"AxisType", 			kAxisType},
  {"B",				kB},
  {"BlendAxisTypes", 		kBlendAxisTypes},
  {"BlendDesignMap", 		kBlendDesignMap},
  {"BlendDesignPositions", 	kBlendDesignPositions},
  {"C",				kC},
  {"CC", 			kCC},
  {"CH",			kCH},
  {"CapHeight", 		kCapHeight},
  {"CharWidth", 		kCharWidth},
  {"CharacterSet", 		kCharacterSet},
  {"Characters", 		kCharacters},
  {"Comment", 			kComment},
  {"Descendents", 		kDescendents},
  {"Descender", 		kDescender},
  {"EncodingScheme", 		kEncodingScheme},
  {"EndAxis", 			kEndAxis},
  {"EndCharMetrics", 		kEndCharMetrics},
  {"EndCompFontMetrics", 	kEndCompFontMetrics},
  {"EndComposites", 		kEndComposites},
  {"EndDescendent", 		kEndDescendent},
  {"EndDirection", 		kEndDirection},
  {"EndFontMetrics", 		kEndFontMetrics},
  {"EndKernData", 		kEndKernData},
  {"EndKernPairs",		kEndKernPairs},
  {"EndMaster", 		kEndMaster},
  {"EndMasterFontMetrics", 	kEndMasterFontMetrics},
  {"EndTrackKern", 		kEndTrackKern},
  {"EscChar", 			kEscChar},
  {"FamilyName", 		kFamilyName},
  {"FontBBox", 			kFontBBox},
  {"FontName", 			kFontName},
  {"FullName", 			kFullName},
  {"IsBaseFont", 		kIsBaseFont},
  {"IsFixedPitch", 		kIsFixedPitch},
  {"IsFixedV", 			kIsFixedV},
  {"ItalicAngle", 		kItalicAngle},
  {"KP", 			kKP},
  {"KPH", 			kKPH},
  {"KPX", 			kKPX},
  {"KPY", 			kKPY},
  {"L",				kL},
  {"MappingScheme", 		kMappingScheme},
  {"Masters", 			kMasters},
  {"MetricsSets", 		kMetricsSets},
  {"N",				kN},
  {"Notice", 			kNotice},
  {"PCC", 			kPCC},
  {"StartAxis", 		kStartAxis},
  {"StartCharMetrics", 		kStartCharMetrics},
  {"StartCompFontMetrics", 	kStartCompFontMetrics},
  {"StartComposites", 		kStartComposites},
  {"StartDescendent", 		kStartDescendent},
  {"StartDirection", 		kStartDirection},
  {"StartFontMetrics", 		kStartFontMetrics},
  {"StartKernData", 		kStartKernData},
  {"StartKernPairs",		kStartKernPairs},
  {"StartMaster", 		kStartMaster},
  {"StartMasterFontMetrics", 	kStartMasterFontMetrics},
  {"StartTrackKern", 		kStartTrackKern},
  {"TrackKern", 		kTrackKern},
  {"UnderlinePosition", 	kUnderlinePosition},
  {"UnderlineThickness", 	kUnderlineThickness},
  {"VV",			kVV},
  {"VVector", 			kVVector},
  {"Version", 			kVersion},
  {"W",				kW},
  {"W0",			kW0},
  {"W0X",			kW0X},
  {"W0Y",			kW0Y},
  {"W1",			kW1},
  {"W1X",			kW1X},
  {"W1Y",			kW1Y},
  {"WX",			kWX},
  {"WY",			kWY},
  {"Weight", 			kWeight},
  {"WeightVector", 		kWeightVector},
  {"XHeight", 			kXHeight},
  {"", (AFMKey)0},
};

#define ISSPACE(ch) ((ch)==' '||(ch)=='\n'||(ch)=='\r'||(ch)=='\t'||(ch)==';')

/*
 * The AFM keys.  This array must be kept sorted because keys are
 * searched by using binary search.
 */

// ==============================================================================

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/25/99 dwc
 */
nsAFMObject :: nsAFMObject()
{
}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/25/99 dwc
 */ 
nsAFMObject :: ~nsAFMObject()
{

}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/25/99 dwc
 */
void
nsAFMObject :: Init(char *aFontName,PRInt32 aFontHeight)
{

  //mFontHeight = mFont->size/20;  // convert the font size to twips
  // read in the Helvetica font
  AFM_ReadFile();
  mFontHeight = aFontHeight;

}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/25/99 dwc
 */
void
nsAFMObject::AFM_ReadFile()
{
PRBool              done=PR_FALSE;
PRBool              bvalue;
AFMKey              key;
double              value;
PRInt32             ivalue;
AFMFontInformation  *fontinfo;

  mPSFontInfo = new AFMFontInformation;
  fontinfo = mPSFontInfo;

  // Open the file
  mAFMFile = fopen("hv.afm","r");
  
  if(nsnull != mAFMFile) {
    // Check for valid AFM file
    GetKey(&key);
    if(key == kStartFontMetrics){
      GetAFMNumber(&fontinfo->mFontVersion);

      while(!done){
        GetKey(&key);
        switch (key){
          case kComment:
            GetLine();
            break;
          case kStartFontMetrics:
            GetAFMNumber(&fontinfo->mFontVersion);
            break;
          case kEndFontMetrics:
            done = PR_TRUE;
            break;
          case kStartCompFontMetrics:
          case kEndCompFontMetrics:
          case kStartMasterFontMetrics:
          case kEndMasterFontMetrics:
            break;
          case kFontName:
            fontinfo->mFontName = GetAFMString();
            break;
          case kFullName:
            fontinfo->mFullName = GetAFMString();
            break;
          case kFamilyName:
            fontinfo->mFamilyName = GetAFMString();
            break;
          case kWeight:
            fontinfo->mWeight = GetAFMString();
            break;
          case kFontBBox:
            GetAFMNumber(&fontinfo->mFontBBox_llx);
            GetAFMNumber(&fontinfo->mFontBBox_lly);
            GetAFMNumber(&fontinfo->mFontBBox_urx);
            GetAFMNumber(&fontinfo->mFontBBox_ury);
            break;
          case kVersion:
            fontinfo->mVersion = GetAFMString();
            break;
	        case kNotice:
	          fontinfo->mNotice = GetAFMString();
            // we really dont want to keep this around...
            delete [] fontinfo->mNotice;
            fontinfo->mNotice = 0;
	          break;
	        case kEncodingScheme:
	          fontinfo->mEncodingScheme = GetAFMString();
	          break;
	        case kMappingScheme:
	          GetAFMInt(&fontinfo->mMappingScheme);
	          break;
	        case kEscChar:
	          GetAFMInt(&fontinfo->mEscChar);
	          break;
	        case kCharacterSet:
	          fontinfo->mCharacterSet = GetAFMString();
	          break;
	        case kCharacters:
	          GetAFMInt(&fontinfo->mCharacters);
	          break;
	        case kIsBaseFont:
	          GetAFMBool (&fontinfo->mIsBaseFont);
	          break;
	        case kVVector:
	          GetAFMNumber(&fontinfo->mVVector_0);
	          GetAFMNumber(&fontinfo->mVVector_1);
	          break;
	        case kIsFixedV:
	          GetAFMBool (&fontinfo->mIsFixedV);
	          break;
	        case kCapHeight:
	          GetAFMNumber(&fontinfo->mCapHeight);
	          break;
	        case kXHeight:
	          GetAFMNumber(&fontinfo->mXHeight);
	          break;
	        case kAscender:
	          GetAFMNumber(&fontinfo->mAscender);
	          break;
	        case kDescender:
	          GetAFMNumber(&fontinfo->mDescender);
	          break;
	        case kStartDirection:
	          GetAFMInt(&ivalue);
	          break;
	        case kUnderlinePosition:
	          GetAFMNumber(&fontinfo->mUnderlinePosition);
	          break;
	        case kUnderlineThickness:
	          GetAFMNumber(&fontinfo->mUnderlineThickness);
	          break;
	        case kItalicAngle:
	          GetAFMNumber(&value);
	          break;
	        case kCharWidth:
	          GetAFMNumber(&value);   // x
	          GetAFMNumber(&value);   // y
	          break;
	        case kIsFixedPitch:
	          GetAFMBool (&bvalue);
	          break;
	        case kEndDirection:
	          break;
	        case kStartCharMetrics:
	          GetAFMInt(&ivalue);     // number of charaters that follow
            fontinfo->AFMCharMetrics = new AFMscm[ivalue];
	          ReadCharMetrics (fontinfo,ivalue);
	          break;
	        case kStartKernData:
	          break;
	        case kStartKernPairs:
            break;
        }
      }
    }
  fclose(mAFMFile);
  }
}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/26/99 dwc
 */
void

nsAFMObject::GetKey(AFMKey *aKey)
{
PRInt32   key,len;

  while(1){
    len = GetToken(); 
    if(len>0) {
      key = MatchKey(mToken);
      if(key >=0){
        *aKey = (AFMKey)key;
        return;
      }

    GetLine(); // skip the entire line, and key
    }
  }
}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/26/99 dwc
 */
PRInt32
nsAFMObject::MatchKey(char *aKey)
{
PRInt32 lower = 0;
PRInt32 upper = NUM_KEYS;
PRInt32 midpoint,cmpvalue;
PRBool  found = PR_FALSE;

  while((upper >=lower) && !found) {
    midpoint = (lower+upper)/2;
    if(keynames[midpoint].name == NULL){
      break;
    }
    cmpvalue = strcmp(aKey,keynames[midpoint].name);
    if(cmpvalue == 0){
      found = PR_TRUE;
    }else{
     if (cmpvalue <0){
        upper = midpoint-1;
      }else{
        lower = midpoint+1;
      }
    }
  }

  if(found)
    return keynames[midpoint].key;
  else
    return -1;
}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/26/99 dwc
 */
PRInt32
nsAFMObject::GetToken()
{
PRInt32 ch;
PRInt32 i;
PRInt32  len;

  // skip leading whitespace
  while((ch=getc(mAFMFile)) != EOF) {
    if(!ISSPACE(ch))
      break;
  }

  if(ch == EOF)
    return 0;

  ungetc(ch,mAFMFile);

  // get name
  for(i=0,ch=getc(mAFMFile);i<sizeof(mToken) && ch!=EOF && !ISSPACE(ch);i++,ch=getc(mAFMFile)){
    len = (PRInt32)sizeof(mToken);
    mToken[i] = ch;
  }

  // is line longer than the AFM specifications
  if(i>=sizeof(mToken))
    return 0;

  mToken[i] = '\0';
  return i;
}


/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/26/99 dwc
 */
PRInt32 
nsAFMObject::GetLine()
{
PRInt32 i, ch;

  // Skip the leading whitespace. 
  while ((ch = getc (mAFMFile)) != EOF){
      if (!ISSPACE (ch))
        break;
  }

  if (ch == EOF)
    return 0;

  ungetc (ch, mAFMFile);

  // Read to the end of the line. 
  for (i = 0, ch = getc (mAFMFile);i < sizeof (mToken) && ch != EOF && ch != '\n';i++, ch = getc (mAFMFile)){
    mToken[i] = ch;
  }

  if (i >= sizeof (mToken)){
    //parse_error (handle, AFM_ERROR_SYNTAX);
  }

  // Skip all trailing whitespace. 
  for (i--; i >= 0 && ISSPACE (mToken[i]); i--)
    ;
  i++;

  mToken[i] = '\0';

  return i;
}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/26/99 dwc
 */
void
nsAFMObject::GetAFMBool (PRBool *aBool)
{

  GetToken();
  if (strcmp (mToken, "true") == 0) {
	  *aBool = PR_TRUE;
  }else if(strcmp (mToken, "false")){
    *aBool = PR_FALSE;
  }else {
    *aBool = PR_FALSE;
  }
}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/26/99 dwc
 */
void
nsAFMObject::ReadCharMetrics (AFMFontInformation *aFontInfo,PRInt32 aNumCharacters)
{
PRInt32 i = 0,ivalue,first=1;
AFMscm  *cm = NULL;
AFMKey  key;
PRBool  done = PR_FALSE;

  while (done!=PR_TRUE && i<aNumCharacters){
    GetKey (&key);
    switch (key){
      case kC:
        if (first){
          first = 0;
        }else{
          i++;
        }
        if (i >= aNumCharacters){
          done = PR_TRUE;
          //parse_error (handle, AFM_ERROR_SYNTAX);
        }

        cm = &(aFontInfo->AFMCharMetrics[i]);
        // character code
        GetAFMInt(&ivalue);          // character code
        cm->mCharacter_Code = ivalue;
        //if (cm->mCharacter_Code >= 0 && cm->mCharacter_Code <= 255)
          //font->encoding[cm->character_code] = cm;
        break;
      case kCH:
        break;
      case kWX:
      case kW0X:
        GetAFMNumber(&(cm->mW0x));
        cm->mW0y = 0.0;
        break;
      case kW1X:
        GetAFMNumber(&(cm->mW1x));
        cm->mW1y = 0.0;
        break;
      case kWY:
      case kW0Y:
        GetAFMNumber(&(cm->mW0y));
        cm->mW0x = 0.0;
        break;
      case kW1Y:
        GetAFMNumber(&(cm->mW1y));
        cm->mW1x = 0.0;
        break;
      case kW:
      case kW0:
        GetAFMNumber(&(cm->mW0x));
        GetAFMNumber(&(cm->mW0y));
        break;

      case kW1:
        GetAFMNumber(&(cm->mW1x));
        GetAFMNumber(&(cm->mW1y));
        break;
      case kVV:
        GetAFMNumber(&(cm->mVv_x));
        GetAFMNumber(&(cm->mVv_y));
        break;
      case kN:
        cm->mName = GetAFMName();
        break;

      case kB:
        GetAFMNumber(&(cm->mLlx));
        GetAFMNumber(&(cm->mLly));
        GetAFMNumber(&(cm->mUrx));
        GetAFMNumber(&(cm->mUry));
        break;
      case kL:
        // XXX Skip ligatures.
        GetLine ();
      break;

      case kEndCharMetrics:
        //  SYNTAX ERROR???
        done = PR_TRUE;
        break;
      default:
        break;
    }
  }
}


/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/26/99 dwc
 */
char*
nsAFMObject::GetAFMString (void) 
{
PRInt32 len;
char    *thestring;

  GetLine();
  len = strlen(mToken);
  thestring = new char[len+1];
  strcpy(thestring,mToken);
  return(thestring);
}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/26/99 dwc
 */
char*
nsAFMObject::GetAFMName (void) 
{
PRInt32 len;
char    *thestring;

  GetToken();
  len = strlen(mToken);
  thestring = new char[len+1];
  strcpy(thestring,mToken);
  return(thestring);
}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/1/99 dwc
 */
void
nsAFMObject :: GetStringWidth(const char *aString,nscoord& aWidth,nscoord aLength)
{
char    *cptr;
PRInt32 i,fwidth,index;
PRInt32 totallen=0;

  aWidth = 0;
  cptr = (char*) aString;

  for(i=0;i<aLength;i++,cptr++){
    index = *cptr-32;
    fwidth = mPSFontInfo->AFMCharMetrics[index].mW0x;
    totallen += (fwidth*mFontHeight)/1000;
  }

  aWidth = totallen;
}


/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/1/99 dwc
 */
void
nsAFMObject :: GetStringWidth(const PRUnichar *aString,nscoord& aWidth,nscoord aLength)
{
PRUint8       asciichar;
PRUnichar     *cptr;
PRInt32       i ,fwidth,index;
PRInt32       totallen=0;

 //XXX This is not handle correctly yet!!  DWC
  aWidth = 0;
 cptr = (PRUnichar*)aString;

  for(i=0;i<aLength;i++,cptr++){
    asciichar = (*cptr)&0x00ff;
    index = asciichar-32;
    fwidth = mPSFontInfo->AFMCharMetrics[index].mW0x;
    totallen += (fwidth*mFontHeight)/1000;
  }

  aWidth = totallen;
}
