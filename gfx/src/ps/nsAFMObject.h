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

#ifndef nsAFMObject_h__
#define nsAFMObject_h__ 



#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"
#include "nsCRT.h"

class nsDeviceContextPS;



// AFM Key Words
typedef enum
{
  kComment,

  // File structure.
  kStartFontMetrics,
  kEndFontMetrics,
  kStartCompFontMetrics,
  kEndCompFontMetrics,
  kStartDescendent,
  kEndDescendent,
  kStartMasterFontMetrics,
  kEndMasterFontMetrics,

  // Control information.
  kMetricsSets,
  kDescendents,
  kMasters,
  kAxes,

  // Global font information. 
  kFontName,
  kFullName,
  kFamilyName,
  kWeight,
  kFontBBox,
  kVersion,
  kNotice,
  kEncodingScheme,
  kMappingScheme,
  kEscChar,
  kCharacterSet,
  kCharacters,
  kIsBaseFont,
  kVVector,
  kIsFixedV,
  kCapHeight,
  kXHeight,
  kAscender,
  kDescender,
  kWeightVector,
  kBlendDesignPositions,
  kBlendDesignMap,
  kBlendAxisTypes,

  // Writing direction information. 
  kStartDirection,
  kEndDirection,
  kUnderlinePosition,
  kUnderlineThickness,
  kItalicAngle,
  kCharWidth,
  kIsFixedPitch,

  // Individual character metrics. 
  kStartCharMetrics,
  kEndCharMetrics,
  kC,
  kCH,
  kWX,
  kW0X,
  kW1X,
  kWY,
  kW0Y,
  kW1Y,
  kW,
  kW0,
  kW1,
  kVV,
  kN,
  kB,
  kL,

  // Kerning data.
  kStartKernData,
  kEndKernData,
  kStartTrackKern,
  kEndTrackKern,
  kTrackKern,
  kStartKernPairs,
  kEndKernPairs,
  kKP,
  kKPH,
  kKPX,
  kKPY,

  // Composite character data.
  kStartComposites,
  kEndComposites,
  kCC,
  kPCC,

  // Axis information.
  kStartAxis,
  kEndAxis,
  kAxisType,
  kAxisLabel,

  // Master Design Information 
  kStartMaster,
  kEndMaster

} AFMKey;



// Single character infor for AFM character. 
struct AFM_Single_Char_Metrics
{
  PRInt32   mCharacter_Code;	// default charcode (-1 if not encoded) 
  double    mW0x;		          // character width x in writing direction 0 
  double    mW0y;		          // character width y in writing direction 0 
  double    mW1x;		          // character width x in writing direction 1 
  double    mW1y;		          // character width y in writing direction 1 
  char      *mName;	          // character name 
  double    mVv_x;	          // local VVector x 
  double    mVv_y;	          // local VVector y 

  // character bounding box. 
  double    mLlx;
  double    mLly;
  double    mUrx;
  double    mUry;

  double num_ligatures;
  //AFMLigature *ligatures;
};

typedef struct AFM_Single_Char_Metrics  AFMscm;



// Font information which we get from AFM files, this is needed for the PS output
struct fontInformation
{
  double  mFontVersion;
  char    *mFontName;
  char    *mFullName;
  char    *mFamilyName;
  char    *mWeight;
  double  mFontBBox_llx;
  double  mFontBBox_lly;
  double  mFontBBox_urx;
  double  mFontBBox_ury;
  char    *mVersion;
  char    *mNotice;
  char    *mEncodingScheme;
  PRInt32 mMappingScheme;
  PRInt32 mEscChar;
  char    *mCharacterSet;
  PRInt32 mCharacters;
  PRBool  mIsBaseFont;
  double  mVVector_0;
  double  mVVector_1;
  PRBool  mIsFixedV;
  double  mCapHeight;
  double  mXHeight;
  double  mAscender;
  double  mDescender;
  double  mUnderlinePosition;
  double  mUnderlineThickness;

  AFMscm *AFMCharMetrics;
};

typedef struct fontInformation AFMFontInformation;




class nsAFMObject 
{
public:
  nsAFMObject();
  virtual ~nsAFMObject();
  void    Init(char *aFontName,PRInt32  aFontHeight);
  void    GetStringWidth(const PRUnichar *aString,nscoord& aWidth,nscoord aLength);
  void    GetStringWidth(const char *aString,nscoord& aWidth,nscoord aLength);

protected:
  void    AFM_ReadFile(void);
  void    GetKey(AFMKey *aTheKey);
  PRInt32 GetToken(void);
  PRInt32 MatchKey(char *aKey);
  PRInt32 GetLine(void);
  char*   GetAFMString (void);
  char*   GetAFMName (void); 
  void    GetAFMInt (PRInt32 *aInt) {GetToken();*aInt = atoi (mToken);}
  void    GetAFMNumber (double *aFloat){GetToken();*aFloat = atof (mToken);}
  void    GetAFMBool (PRBool *aBool);
  void    ReadCharMetrics (AFMFontInformation *aFontInfo,PRInt32 aNumCharacters);

public:
  AFMFontInformation  *mPSFontInfo;

protected:
  FILE                *mAFMFile;          // this is the file we are reading for the AFM files
  char                mToken[256];        // this is where we put the token for reading;
  nscoord             mFontHeight;        // font height in points

};


#define NUM_KEYS (sizeof (keynames) / sizeof (struct keyname_st) - 1)

#endif
