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


/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 */



#ifndef _NSELEMENTABLE
#define _NSELEMENTABLE

#include "nsHTMLTokens.h"
#include "nsDTDUtils.h"

class CTagList {
public:
  CTagList( int aCount,
            eHTMLTags*  aTagList=0,
            eHTMLTags aTag1=eHTMLTag_unknown,
            eHTMLTags aTag2=eHTMLTag_unknown,
            eHTMLTags aTag3=eHTMLTag_unknown,
            eHTMLTags aTag4=eHTMLTag_unknown,
            eHTMLTags aTag5=eHTMLTag_unknown) 
  {
    mCount=aCount;
    mTags[0]=aTag1; mTags[1]=aTag2; mTags[2]=aTag3; mTags[3]=aTag4; mTags[4]=aTag5;
    mTagList=aTagList;
  }

  PRInt32 GetTopmostIndexOf(nsTagStack& aTagStack);
  PRInt32 GetBottommostIndexOf(nsTagStack& aTagStack,PRInt32 aStartOffset);
  PRBool  Contains(eHTMLTags aTag);

  eHTMLTags   mTags[5];
  eHTMLTags*  mTagList;
  int         mCount;
};

//*********************************************************************************************
// The following ints define the standard groups of HTML elements...
//*********************************************************************************************


/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
struct nsHTMLElement {

  static  PRBool  IsContainerType(eHTMLTags aTag,int aType);

  static  PRBool  IsBlockElement(eHTMLTags aTag);
  static  PRBool  IsInlineElement(eHTMLTags aTag);
  static  PRBool  IsFlowElement(eHTMLTags aTag);
  static  PRBool  IsBlockCloser(eHTMLTags aTag);

  CTagList*       GetRootTags(void) const {return mRootNodes;}
  CTagList*       GetEndRootTags(void) const {return mEndRootNodes;}
  CTagList*       GetAutoCloseStartTags(void) const {return mAutocloseStart;}
  CTagList*       GetAutoCloseEndTags(void) const {return mAutocloseEnd;}
  CTagList*       GetSynonymousTags(void) const {return mSynonymousTags;}

  static  PRBool  IsBlockParent(eHTMLTags aTag);
  static  PRBool  IsInlineParent(eHTMLTags aTag); 
  static  PRBool  IsFlowParent(eHTMLTags aTag);

  CTagList*       GetSpecialChildren(void) const {return mSpecialKids;}
  CTagList*       GetSpecialParents(void) const {return mSpecialParents;}

  PRBool          IsMemberOf(PRInt32 aType) const;
  PRBool          CanContainType(PRInt32 aType) const;
  
  eHTMLTags       GetTag(void) const {return mTagID;}
  PRBool          CanContain(eHTMLTags aChild) const;
  PRBool          CanOmitStartTag(eHTMLTags aChild) const;
  PRBool          CanOmitEndTag(eHTMLTags aParent) const;
  PRBool          CanContainSelf() const;
  PRBool          HasSpecialProperty(PRInt32 aProperty) const;
 
  static  PRBool  CanContain(eHTMLTags aParent,eHTMLTags aChild);
  static  PRBool  IsContainer(eHTMLTags aTag) ;
  static  PRBool  IsStyleTag(eHTMLTags aTag) ;
  static  PRBool  IsHeadingTag(eHTMLTags aTag) ;
  static  PRBool  IsChildOfHead(eHTMLTags aTag) ;
  static  PRBool  IsTextTag(eHTMLTags aTag);

  eHTMLTags       mTagID;
  CTagList*       mRootNodes;         //These are the tags above which you many not autoclose a START tag
  CTagList*       mEndRootNodes;      //These are the tags above which you many not autoclose an END tag
  CTagList*       mAutocloseStart;    //these are the start tags that you can automatically close with this START tag
  CTagList*       mAutocloseEnd;      //these are the start tags that you can automatically close with this END tag
  CTagList*       mSynonymousTags;    //These are morally equivalent; an end tag for one can close a start tag for another (like <Hn>)
  int             mParentBits;        //defines groups that can contain this element
  int             mInclusionBits;     //defines parental and containment rules
  int             mExclusionBits;     //defines things you CANNOT contain
  int             mSpecialProperties; //used for various special purposes...
  CTagList*       mSpecialParents;    //These are the special tags that contain this tag (directly)
  CTagList*       mSpecialKids;       //These are the extra things you can contain
  eHTMLTags       mSkipTarget;        //If set, then we skip all content until this tag is seen
}; 

extern nsHTMLElement gHTMLElements[];
extern CTagList      gFramesetKids;
extern CTagList      gHeadingTags;

//special property bits...
static const int kDiscardTag      = 0x0001; //tells us to toss this tag
static const int kOmitEndTag      = 0x0002; //safely ignore end tag
static const int kLegalOpen       = 0x0004; //Lets BODY, TITLE, SCRIPT to reopen


#endif
