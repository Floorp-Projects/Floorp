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

#include "nsAFMObject.h"
#include "nsFileSpec.h" // for nsAutoCString
#include "Helvetica.h"
#include "Helvetica-Bold.h"
#include "Helvetica-BoldOblique.h"
#include "Helvetica-Oblique.h"
#include "Times-Roman.h"
#include "Times-Bold.h"
#include "Times-BoldItalic.h"
#include "Times-Italic.h"
#include "Courier.h"
#include "Courier-Bold.h"
#include "Courier-BoldOblique.h"
#include "Courier-Oblique.h"
#include "Symbol.h"



// this is the basic font set supported currently
DefFonts gSubstituteFonts[] = 
{
  {"Times-Roman","Times",400,0,&Times_RomanAFM,AFMTimes_RomanChars,-1},
  {"Times-Bold","Times",700,0,&Times_BoldAFM,AFMTimes_RomanChars,-1},
  {"Times-BoldItalic","Times",700,1,&Times_BoldItalicAFM,AFMTimes_RomanChars,-1},
  {"Times-Italic","Times",400,1,&Times_ItalicAFM,AFMTimes_RomanChars,-1},
  {"Helvetica","Helvetica",400,0,&HelveticaAFM,AFMHelveticaChars,-1},
  {"Helvetica-Bold","Helvetica",700,0,&Helvetica_BoldAFM,AFMHelveticaChars,-1},
  {"Helvetica-BoldOblique","Helvetica",700,2,&Helvetica_BoldObliqueAFM,AFMHelveticaChars,-1},
  {"Helvetica_Oblique","Helvetica",400,2,&Helvetica_ObliqueAFM,AFMHelveticaChars,-1},
  {"Courier","Courier",400,0,&CourierAFM,AFMCourierChars,-1},
  {"Courier-Bold","Courier",700,0,&Courier_BoldAFM,AFMCourierChars,-1},
  {"Courier-BoldOblique","Courier",700,2,&Courier_BoldObliqueAFM,AFMCourierChars,-1},
  {"Courier-Oblique","Courier",400,2,&Courier_ObliqueAFM,AFMCourierChars,-1},
  {"Symbol","Symbol",400,0,&SymbolAFM,AFMSymbolChars,-1}
};


/** ---------------------------------------------------
 *  A static structure initialized AFM font keyword definitions
 *	@update 3/12/99 dwc
 */
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
  mPSFontInfo = nsnull;
}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/25/99 dwc
 */ 
nsAFMObject :: ~nsAFMObject()
{

  if(mPSFontInfo->mAFMCharMetrics){
    delete [] mPSFontInfo->mAFMCharMetrics;
  }

  if(mPSFontInfo){
    delete mPSFontInfo;
  }
}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/25/99 dwc
 */
void
nsAFMObject :: Init(PRInt32 aFontHeight)
{
  // read the file asked for
  mFontHeight = aFontHeight;
}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/25/99 dwc
 */
PRInt16
nsAFMObject::CheckBasicFonts(const nsFont &aFont,PRBool aPrimaryOnly)
{
PRInt16     ourfont = -1;
PRInt32     i,curIndex,score;
nsAutoString    psfontname;

  // have to find the correct fontfamily, weight and style
  psfontname = aFont.name;
  
  // look in the font table for one of the fonts in the passed in list
  for(i=0,curIndex=-1;i<NUM_AFM_FONTS;i++){
    gSubstituteFonts[i].mIndex = psfontname.RFind((const char*)gSubstituteFonts[i].mFamily,PR_TRUE);

    // if a font was found matching this criteria
    if((gSubstituteFonts[i].mIndex==0) || (!aPrimaryOnly && gSubstituteFonts[i].mIndex>=0)){
      // give it a score
      score = abs(aFont.weight-gSubstituteFonts[i].mWeight);
      score+= abs(aFont.style-gSubstituteFonts[i].mStyle);
      if(score == 0){
        curIndex = i;
        break;
      }
      gSubstituteFonts[i].mIndex = score;
    }
  }
  
  // if its ok to look for the second best, and we did not find a perfect match
  score = 32000;
  if((PR_FALSE == aPrimaryOnly)&&(curIndex !=0)) {
    for(i=0;i<NUM_AFM_FONTS;i++){
      if((gSubstituteFonts[i].mIndex>0) && (gSubstituteFonts[i].mIndex<score)){
        score = gSubstituteFonts[i].mIndex;
        curIndex = i;
      }   
    }
  }


  if(curIndex>=0){
    mPSFontInfo = new AFMFontInformation;
    memset(mPSFontInfo,0,sizeof(AFMFontInformation));
    
    memcpy(mPSFontInfo,(gSubstituteFonts[curIndex].mFontInfo),sizeof(AFMFontInformation));
    mPSFontInfo->mAFMCharMetrics = new AFMscm[mPSFontInfo->mNumCharacters];
    memset(mPSFontInfo->mAFMCharMetrics,0,sizeof(AFMscm)*mPSFontInfo->mNumCharacters);
    memcpy(mPSFontInfo->mAFMCharMetrics,gSubstituteFonts[curIndex].mCharInfo,gSubstituteFonts[curIndex].mFontInfo->mNumCharacters*sizeof(AFMscm));
    ourfont = curIndex;
  }
  
  return ourfont;
}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/25/99 dwc
 */
PRInt16
nsAFMObject::CreateSubstituteFont(const nsFont &aFontName)
{
PRInt16     ourfont = 0;

  mPSFontInfo = new AFMFontInformation;
  memset(mPSFontInfo,0,sizeof(AFMFontInformation));

  // put in default AFM data, can't find the correct AFM file
  memcpy(mPSFontInfo,&Times_RomanAFM,sizeof(AFMFontInformation));
  mPSFontInfo->mAFMCharMetrics = new AFMscm[mPSFontInfo->mNumCharacters];
  memset(mPSFontInfo->mAFMCharMetrics,0,sizeof(AFMscm)*mPSFontInfo->mNumCharacters);
  memcpy(mPSFontInfo->mAFMCharMetrics,AFMTimes_RomanChars,Times_RomanAFM.mNumCharacters*sizeof(AFMscm));
  return ourfont;
}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/25/99 dwc
 */
PRBool
nsAFMObject::AFM_ReadFile(const nsFont &aFontName)
{
PRBool  done=PR_FALSE;
PRBool  success = PR_FALSE;
PRBool  bvalue;
AFMKey  key;
double  value;
PRInt32 ivalue;
char* AFMFileName= aFontName.name.ToNewUTF8String(); // file we will open

  if(nsnull == AFMFileName) 
    return (success);

    if((0==strcmp(AFMFileName,"..")) || (0==strcmp(AFMFileName,"."))) {
      Recycle(AFMFileName);
      return (success);
    }

   // Open the file
  mAFMFile = fopen((const char *)AFMFileName,"r");
  Recycle(AFMFileName);

  if(nsnull != mAFMFile) {
    // create the structure to put the information in
    mPSFontInfo = new AFMFontInformation;
    memset(mPSFontInfo,0,sizeof(AFMFontInformation));

    // Check for valid AFM file
    GetKey(&key);
    if(key == kStartFontMetrics){
      GetAFMNumber(&mPSFontInfo->mFontVersion);

      while(!done){
        GetKey(&key);
        switch (key){
          case kComment:
            GetLine();
            break;
          case kStartFontMetrics:
            GetAFMNumber(&mPSFontInfo->mFontVersion);
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
            mPSFontInfo->mFontName = GetAFMString();
            break;
          case kFullName:
            mPSFontInfo->mFullName = GetAFMString();
            break;
          case kFamilyName:
            mPSFontInfo->mFamilyName = GetAFMString();
            break;
          case kWeight:
            mPSFontInfo->mWeight = GetAFMString();
            break;
          case kFontBBox:
            GetAFMNumber(&mPSFontInfo->mFontBBox_llx);
            GetAFMNumber(&mPSFontInfo->mFontBBox_lly);
            GetAFMNumber(&mPSFontInfo->mFontBBox_urx);
            GetAFMNumber(&mPSFontInfo->mFontBBox_ury);
            break;
          case kVersion:
            mPSFontInfo->mVersion = GetAFMString();
            break;
	        case kNotice:
	          mPSFontInfo->mNotice = GetAFMString();
            // we really dont want to keep this around...
            delete [] mPSFontInfo->mNotice;
            mPSFontInfo->mNotice = 0;
	          break;
	        case kEncodingScheme:
	          mPSFontInfo->mEncodingScheme = GetAFMString();
	          break;
	        case kMappingScheme:
	          GetAFMInt(&mPSFontInfo->mMappingScheme);
	          break;
	        case kEscChar:
	          GetAFMInt(&mPSFontInfo->mEscChar);
	          break;
	        case kCharacterSet:
	          mPSFontInfo->mCharacterSet = GetAFMString();
	          break;
	        case kCharacters:
	          GetAFMInt(&mPSFontInfo->mCharacters);
	          break;
	        case kIsBaseFont:
	          GetAFMBool (&mPSFontInfo->mIsBaseFont);
	          break;
	        case kVVector:
	          GetAFMNumber(&mPSFontInfo->mVVector_0);
	          GetAFMNumber(&mPSFontInfo->mVVector_1);
	          break;
	        case kIsFixedV:
	          GetAFMBool (&mPSFontInfo->mIsFixedV);
	          break;
	        case kCapHeight:
	          GetAFMNumber(&mPSFontInfo->mCapHeight);
	          break;
	        case kXHeight:
	          GetAFMNumber(&mPSFontInfo->mXHeight);
	          break;
	        case kAscender:
	          GetAFMNumber(&mPSFontInfo->mAscender);
	          break;
	        case kDescender:
	          GetAFMNumber(&mPSFontInfo->mDescender);
	          break;
	        case kStartDirection:
	          GetAFMInt(&ivalue);
	          break;
	        case kUnderlinePosition:
	          GetAFMNumber(&mPSFontInfo->mUnderlinePosition);
	          break;
	        case kUnderlineThickness:
	          GetAFMNumber(&mPSFontInfo->mUnderlineThickness);
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
	          GetAFMInt(&mPSFontInfo->mNumCharacters);     // number of charaters that follow
            mPSFontInfo->mAFMCharMetrics = new AFMscm[mPSFontInfo->mNumCharacters];
            memset(mPSFontInfo->mAFMCharMetrics,0,sizeof(AFMscm)*mPSFontInfo->mNumCharacters);
	          ReadCharMetrics (mPSFontInfo,mPSFontInfo->mNumCharacters);
	          break;
	        case kStartKernData:
	          break;
	        case kStartKernPairs:
            break;
          default:
            break;
        }
      }
    }
  fclose(mAFMFile);
  success = PR_TRUE;
  } else {
  // put in default AFM data, can't find the correct AFM file
  //memcpy(mPSFontInfo,&HelveticaAFM,sizeof(AFMFontInformation));
 // mPSFontInfo->mAFMCharMetrics = new AFMscm[mPSFontInfo->mNumCharacters];
  //memset(mPSFontInfo->mAFMCharMetrics,0,sizeof(AFMscm)*mPSFontInfo->mNumCharacters);
  //memcpy(mPSFontInfo->mAFMCharMetrics,AFMHelveticaChars,HelveticaAFM.mNumCharacters*sizeof(AFMscm));
  }

  return(success);
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
PRInt32   ch;
PRInt32   i;
PRInt32   len;

  // skip leading whitespace
  while((ch=getc(mAFMFile)) != EOF) {
    if(!ISSPACE(ch))
      break;
  }

  if(ch == EOF)
    return 0;

  ungetc(ch,mAFMFile);

  // get name
  len = (PRInt32)sizeof(mToken);
  for(i=0,ch=getc(mAFMFile);i<len && ch!=EOF && !ISSPACE(ch);i++,ch=getc(mAFMFile)){
      mToken[i] = ch;
  }

  // is line longer than the AFM specifications
  if(((PRUint32)i)>=sizeof(mToken))
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
  for (i = 0, ch = getc (mAFMFile);((PRUint32)i) < sizeof (mToken) && ch != EOF && ch != '\n';i++, ch = getc (mAFMFile)){
    mToken[i] = ch;
  }

  if (((PRUint32)i) >= sizeof (mToken)){
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
double  notyet;
char    *name;

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

        cm = &(aFontInfo->mAFMCharMetrics[i]);
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
        //GetAFMNumber(&(cm->mVv_x));
        //GetAFMNumber(&(cm->mVv_y));
        GetAFMNumber(&notyet);
        GetAFMNumber(&notyet);
        break;
      case kN:
        //cm->mName = GetAFMName();
        name = GetAFMName();
        delete [] name;
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
 *	@update 2/01/99 dwc
 */
void
nsAFMObject :: GetStringWidth(const char *aString,nscoord& aWidth,nscoord aLength)
{
char    *cptr;
PRInt32 i,idx,fwidth;
float   totallen=0.0f;

  // add up the length of the character widths, in floating to avoid roundoff
  aWidth = 0;
  cptr = (char*) aString;
  for(i=0;i<aLength;i++,cptr++){
    idx = *cptr-32;
    fwidth = (PRInt32)(mPSFontInfo->mAFMCharMetrics[idx].mW0x);
    totallen += fwidth;
  }

  // total length is in points, so convert to twips, divide by the 1000 scaling of the
  // afm measurements, and round the result.
  totallen = NSFloatPointsToTwips(totallen * mFontHeight)/1000.0f;

  aWidth = NSToIntRound(totallen);
}


/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 2/01/99 dwc
 */
void
nsAFMObject :: GetStringWidth(const PRUnichar *aString,nscoord& aWidth,nscoord aLength)
{
PRUint8   asciichar;
PRUnichar *cptr;
PRInt32   i ,fwidth,idx;
float     totallen=0.0f;

 //XXX This needs to get the aString converted to a normal cstring  DWC
 aWidth = 0;
 cptr = (PRUnichar*)aString;

  for(i=0;i<aLength;i++,cptr++){
    asciichar = (*cptr)&0x00ff;
    idx = asciichar-32;
    fwidth = (PRInt32)(mPSFontInfo->mAFMCharMetrics[idx].mW0x);
    //    if ( (*cptr == 0x0020) || (*cptr == 0x002c) )
    //   printf("fwidth = %d\n", fwidth);
    if (*cptr & 0xff00)
       fwidth = 1056;
    if ( (*cptr  == 0x0020) || (*cptr == 0x002c) )
      fwidth = 1056;  // space and comma are half size of a CJK width
    totallen += fwidth;
  }

  totallen = NSFloatPointsToTwips(totallen * mFontHeight)/1000.0f;
  aWidth = NSToIntRound(totallen);
}



#define CORRECTSTRING(d)  (d?d:"")
#define BOOLOUT(B)        (mPSFontInfo->mIsBaseFont==PR_TRUE?"PR_TRUE":"PR_FALSE")

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 3/05/99 dwc
 */
void    
nsAFMObject :: WriteFontHeaderInformation(FILE *aOutFile)
{

  // main information of the font
  fprintf(aOutFile,"%f,\n",mPSFontInfo->mFontVersion);
  fprintf(aOutFile,"\"%s\",\n",CORRECTSTRING(mPSFontInfo->mFontName));
  fprintf(aOutFile,"\"%s\",\n",CORRECTSTRING(mPSFontInfo->mFullName));
  fprintf(aOutFile,"\"%s\",\n",CORRECTSTRING(mPSFontInfo->mFamilyName));
  fprintf(aOutFile,"\"%s\",\n",CORRECTSTRING(mPSFontInfo->mWeight));
  fprintf(aOutFile,"%f,\n",mPSFontInfo->mFontBBox_llx);
  fprintf(aOutFile,"%f,\n",mPSFontInfo->mFontBBox_lly);
  fprintf(aOutFile,"%f,\n",mPSFontInfo->mFontBBox_urx);
  fprintf(aOutFile,"%f,\n",mPSFontInfo->mFontBBox_ury);
  fprintf(aOutFile,"\"%s\",\n",CORRECTSTRING(mPSFontInfo->mVersion));
  fprintf(aOutFile,"\"%s\",\n",CORRECTSTRING(mPSFontInfo->mNotice));
  fprintf(aOutFile,"\"%s\",\n",CORRECTSTRING(mPSFontInfo->mEncodingScheme));
  fprintf(aOutFile,"%d,\n",mPSFontInfo->mMappingScheme);
  fprintf(aOutFile,"%d,\n",mPSFontInfo->mEscChar);
  fprintf(aOutFile,"\"%s\",\n", CORRECTSTRING(mPSFontInfo->mCharacterSet));
  fprintf(aOutFile,"%d,\n",mPSFontInfo->mCharacters);
  fprintf(aOutFile,"%s,\n",BOOLOUT(mPSFontInfo->mIsBaseFont));
  fprintf(aOutFile,"%f,\n",mPSFontInfo->mVVector_0);
  fprintf(aOutFile,"%f,\n",mPSFontInfo->mVVector_1);
  fprintf(aOutFile,"%s,\n",BOOLOUT(mPSFontInfo->mIsFixedV));
  fprintf(aOutFile,"%f,\n",mPSFontInfo->mCapHeight);
  fprintf(aOutFile,"%f,\n",mPSFontInfo->mXHeight);
  fprintf(aOutFile,"%f,\n",mPSFontInfo->mAscender);
  fprintf(aOutFile,"%f,\n",mPSFontInfo->mDescender);
  fprintf(aOutFile,"%f,\n",mPSFontInfo->mUnderlinePosition);
  fprintf(aOutFile,"%f,\n",mPSFontInfo->mUnderlineThickness);
  fprintf(aOutFile,"%d\n",mPSFontInfo->mNumCharacters);
}

/** ---------------------------------------------------
 *  See documentation in nsAFMParser.h
 *	@update 3/05/99 dwc
 */
void    
nsAFMObject :: WriteFontCharInformation(FILE *aOutFile)
{
PRInt32 i;


  // individual font characteristics
  for(i=0;i<mPSFontInfo->mNumCharacters;i++) {
    fprintf(aOutFile,"{\n");
    fprintf(aOutFile,"%d, \n",mPSFontInfo->mAFMCharMetrics[i].mCharacter_Code);
    fprintf(aOutFile,"%f, \n",mPSFontInfo->mAFMCharMetrics[i].mW0x);
    fprintf(aOutFile,"%f, \n",mPSFontInfo->mAFMCharMetrics[i].mW0y);
    fprintf(aOutFile,"%f, \n",mPSFontInfo->mAFMCharMetrics[i].mW1x);
    fprintf(aOutFile,"%f, \n",mPSFontInfo->mAFMCharMetrics[i].mW1y);
    //fprintf(aOutFile,"\"%s\", \n", CORRECTSTRING(mPSFontInfo->mAFMCharMetrics[i].mName));
    //fprintf(aOutFile,"%f, \n",mPSFontInfo->mAFMCharMetrics[i].mVv_x);
    //fprintf(aOutFile,"%f, \n",mPSFontInfo->mAFMCharMetrics[i].mVv_y);
    fprintf(aOutFile,"%f, \n",mPSFontInfo->mAFMCharMetrics[i].mLlx);
    fprintf(aOutFile,"%f, \n",mPSFontInfo->mAFMCharMetrics[i].mLly);
    fprintf(aOutFile,"%f, \n",mPSFontInfo->mAFMCharMetrics[i].mUrx);
    fprintf(aOutFile,"%f \n",mPSFontInfo->mAFMCharMetrics[i].mUry);
    //fprintf(aOutFile,"%f, \n",mPSFontInfo->mAFMCharMetrics[i].num_ligatures);
    fprintf(aOutFile,"}\n");
    if ( i != mPSFontInfo->mNumCharacters - 1 )
	fputc( ',', aOutFile ); 
    fputc( '\n', aOutFile );
  }
}

