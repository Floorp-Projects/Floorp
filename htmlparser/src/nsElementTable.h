
class CTagList {
public:
  CTagList( int aCount,
            eHTMLTags aTag1=eHTMLTag_unknown,
            eHTMLTags aTag2=eHTMLTag_unknown,
            eHTMLTags aTag3=eHTMLTag_unknown,
            eHTMLTags aTag4=eHTMLTag_unknown,
            eHTMLTags aTag5=eHTMLTag_unknown,
            eHTMLTags aTag6=eHTMLTag_unknown,
            eHTMLTags aTag7=eHTMLTag_unknown,
            eHTMLTags aTag8=eHTMLTag_unknown,
            eHTMLTags aTag9=eHTMLTag_unknown,
            eHTMLTags aTag10=eHTMLTag_unknown){
    mCount=aCount;
    mTags[0]=aTag1; mTags[1]=aTag2;
    mTags[2]=aTag3; mTags[3]=aTag4;
    mTags[4]=aTag5; mTags[5]=aTag6;
    mTags[6]=aTag7; mTags[7]=aTag8;
    mTags[8]=aTag9; mTags[9]=aTag10;  
  }

  PRInt32 GetTopmostIndexOf(nsTagStack& aTagStack);
  PRBool  Contains(eHTMLTags aTag);

  eHTMLTags mTags[10];
  int       mCount;
};

/**
 * 
 * @update	gess12/17/98
 * @param 
 * @return
 */
PRBool CTagList::Contains(eHTMLTags aTag){
  return FindTagInSet(aTag,mTags,mCount);
}


/**
 * 
 * @update	gess12/17/98
 * @param 
 * @return
 */
PRInt32 CTagList::GetTopmostIndexOf(nsTagStack& aTagStack){
  int max=aTagStack.mCount;
  int index;
  for(index=max-1;index>0;index--){
    if(FindTagInSet(aTagStack.mTags[index],mTags,mCount)) {
      return index;
    }
  }
  return kNotFound;
}


/**
 * 
 * @update	gess12/17/98
 * @param 
 * @return
 */
struct nsHTMLElement {

  static  PRBool  IsBlockElement(eHTMLTags aChild);
  static  PRBool  IsInlineElement(eHTMLTags aChild);
  static  PRBool  IsBlockCloser(eHTMLTags aChild);

  CTagList*       GetRootTags(void) const {return mRootNodes;}
  CTagList*       GetAutoCloseStartTags(void) const {return mAutocloseStart;}
  CTagList*       GetAutoCloseEndTags(void) const {return mAutocloseEnd;}

  static  PRBool  IsBlockParent(eHTMLTags aChild);
  static  PRBool  IsInlineParent(eHTMLTags aChild); 

  CTagList*       GetSpecialChildren(void) const {return mSpecialKids;}
  CTagList*       GetSpecialParents(void) const {return mSpecialParents;}
    
  eHTMLTags       GetTag(void) const {return mTagID;}
  PRBool          CanContain(eHTMLTags aChild) const;
  PRBool          CanOmitStartTag(eHTMLTags aChild) const;
  PRBool          CanOmitEndTag(eHTMLTags aChild) const;
  PRBool          CanSelfContain() const {return mSelfContained;}
  PRBool          HasBits(PRInt32 aBitset) const;

  static  PRBool  CanContain(eHTMLTags aParent,eHTMLTags aChild);
  static  PRBool  IsContainer(eHTMLTags aTag) ;
  static  PRBool  IsStyleTag(eHTMLTags aTag) ;
  static  PRBool  IsTextTag(eHTMLTags aTag);

  eHTMLTags   mTagID;
  CTagList*   mRootNodes;         //These are the tags above which you many not autoclose
  CTagList*   mAutocloseStart;    //these are the start tags that you can automatically close with this START tag
  CTagList*   mAutocloseEnd;      //these are the start tags that you can automatically close with this END tag
  int         mBits;              //defines parental and containment rules
  CTagList*   mSpecialParents;    //These are the extra tags that can contain you
  CTagList*   mSpecialKids;       //These are th extra things you can contain
  PRBool      mSelfContained;
}; 

    
/***************************************************************************** 
  Now it's time to list all the html elements all with their capabilities...
******************************************************************************/

// id, hasblockparent, hasinlineparent,isBlockContainer,isInlineContainer

//First, define the set of taglists for tags with special parents...
CTagList  gAParents(1,eHTMLTag_map);
CTagList  gInHead(1,eHTMLTag_head);
CTagList  gInTable(1,eHTMLTag_table);
CTagList  gInHTML(1,eHTMLTag_html);
CTagList  gInBody(1,eHTMLTag_body);
CTagList  gInForm(1,eHTMLTag_form);
CTagList  gInTR(1,eHTMLTag_tr);
CTagList  gInDL(1,eHTMLTag_dl);
CTagList  gInFrameset(1,eHTMLTag_frameset);
CTagList  gOptgroupParents(2,eHTMLTag_optgroup,eHTMLTag_select);
CTagList  gBodyParents(2,eHTMLTag_html,eHTMLTag_noframes);
CTagList  gColParents(2,eHTMLTag_colgroup,eHTMLTag_table);
CTagList  gFramesetParents(2,eHTMLTag_frameset,eHTMLTag_html);
CTagList  gLegendParents(1,eHTMLTag_fieldset);
CTagList  gAreaParent(1,eHTMLTag_map);
CTagList  gParamParents(2,eHTMLTag_applet,eHTMLTag_object);
CTagList  gTRParents(4,eHTMLTag_tbody,eHTMLTag_tfoot,eHTMLTag_thead,eHTMLTag_table);

//*********************************************************************************************
//  Next, define the set of taglists for tags with special kids...
//*********************************************************************************************
CTagList  gContainsText(4,eHTMLTag_text,eHTMLTag_newline,eHTMLTag_whitespace,eHTMLTag_entity);
CTagList  gContainsHTML(1,eHTMLTag_html);
CTagList  gContainsOpts(2,eHTMLTag_option,eHTMLTag_optgroup);
CTagList  gContainsParam(1,eHTMLTag_param);
CTagList  gColgroupKids(1,eHTMLTag_col); 
CTagList  gBodyKids(2,eHTMLTag_noscript,eHTMLTag_script);
CTagList  gButtonKids(2,eHTMLTag_caption,eHTMLTag_legend);
CTagList  gDLKids(2,eHTMLTag_dd,eHTMLTag_dt);
CTagList  gDTKids(1,eHTMLTag_dt);
CTagList  gFieldsetKids(1,eHTMLTag_legend);
CTagList  gFormKids(1,eHTMLTag_keygen);
CTagList  gFramesetKids(3,eHTMLTag_frame,eHTMLTag_frameset,eHTMLTag_noframes);
CTagList  gHtmlKids(6,eHTMLTag_body,eHTMLTag_frameset,eHTMLTag_head,eHTMLTag_map,eHTMLTag_noscript,eHTMLTag_script);
CTagList  gHeadKids(7,eHTMLTag_base,eHTMLTag_bgsound,eHTMLTag_link,eHTMLTag_meta,eHTMLTag_script,eHTMLTag_style,eHTMLTag_title);
CTagList  gLIKids(2,eHTMLTag_ol,eHTMLTag_ul);
CTagList  gMapKids(1,eHTMLTag_area);
CTagList  gNoframesKids(1,eHTMLTag_body);
CTagList  gPreKids(1,eHTMLTag_hr);
CTagList  gTableKids(9,eHTMLTag_caption,eHTMLTag_col,eHTMLTag_colgroup,eHTMLTag_form,eHTMLTag_map,eHTMLTag_thead,eHTMLTag_tbody,eHTMLTag_tfoot,eHTMLTag_script);
CTagList  gTableElemKids(7,eHTMLTag_form,eHTMLTag_map,eHTMLTag_noscript,eHTMLTag_script,eHTMLTag_td,eHTMLTag_th,eHTMLTag_tr);
CTagList  gTBodyKids(1,eHTMLTag_tr);
CTagList  gTRKids(6,eHTMLTag_form,eHTMLTag_map,eHTMLTag_noscript,eHTMLTag_script,eHTMLTag_td,eHTMLTag_th);
CTagList  gULKids(2,eHTMLTag_li,eHTMLTag_p);

//*********************************************************************************************
// The following tag lists are used to define common set of root notes for the HTML elements...
//*********************************************************************************************
CTagList  gRootTags(2,eHTMLTag_body,eHTMLTag_td);
CTagList  gHTMLRootTags(1,eHTMLTag_unknown);
CTagList  gLIRootTags(7,eHTMLTag_ul,eHTMLTag_ol,eHTMLTag_dir,eHTMLTag_menu,eHTMLTag_p,eHTMLTag_body,eHTMLTag_td);
CTagList  gOLRootTags(3,eHTMLTag_body,eHTMLTag_li,eHTMLTag_td);
CTagList  gTextRootTags(2,eHTMLTag_p,eHTMLTag_body);
CTagList  gTDRootTags(3,eHTMLTag_tr,eHTMLTag_tbody,eHTMLTag_table);
CTagList  gOptionRootTags(1,eHTMLTag_select);


//*********************************************************************************************
// The following tag lists are used to define the autoclose properties of the html elements...
//*********************************************************************************************
CTagList  gAutoClose(2,eHTMLTag_body,eHTMLTag_td);
CTagList  gBodyAutoClose(1,eHTMLTag_head);
CTagList  gCaptionAutoClose(1,eHTMLTag_tbody);
CTagList  gLIAutoClose(2,eHTMLTag_p,eHTMLTag_li);
CTagList  gPAutoClose(2,eHTMLTag_p,eHTMLTag_li);
CTagList  gHxAutoClose(6,eHTMLTag_h1,eHTMLTag_h2,eHTMLTag_h3,eHTMLTag_h4,eHTMLTag_h5,eHTMLTag_h6);
CTagList  gTRCloseTags(2,eHTMLTag_td,eHTMLTag_th);
CTagList  gTDCloseTags(2,eHTMLTag_td,eHTMLTag_th);
CTagList  gDTCloseTags(2,eHTMLTag_dt,eHTMLTag_dd);
CTagList  gULCloseTags(1,eHTMLTag_li);

 
static const int kNone= 0x0;
static const int kBP  = 0x1;  //has block parent
static const int kIP  = 0x2;  //has inline parent
static const int kCB  = 0x4;  //can close blocks (even if it's not a block elements)
static const int kBC  = 0x8;  //contains block child
static const int kIC  = 0x10;  //contains inline child

static const int kBPIP   = (kBP | kIP);
static const int kBPIPIC = (kBP | kIP | kIC);
static const int kBPBCIC = (kBP | kBC | kIC);
static const int kBCIC   = (kBC | kIC);
static const int kAll    = (kBPIP | kBCIC);


//*********************************************************************************************
//Lastly, bind tags with their rules, their special parents and special kids.
//*********************************************************************************************

static nsHTMLElement gHTMLElements[] = {

  //                                                               
  //                      Root          Autoclose-    Autoclose-      Container     Special       Special
  //    tag-id            Nodes         starttags     endtags         -rules        Parents       Kids

  {eHTMLTag_unknown,    &gRootTags,    0,              0,              kNone,        0,            &gContainsHTML},
  {eHTMLTag_a,          &gRootTags,    0,              0,              kBPIPIC,     &gAParents,   0},
  {eHTMLTag_abbr,       &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_acronym,    &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_address,    &gRootTags,    0,              0,              kBP|kIC},
  {eHTMLTag_applet,     &gRootTags,    0,              0,              kAll,         0,            &gContainsParam},
  {eHTMLTag_area,       &gAreaParent,  &gAutoClose,    0,              kNone},
  
  {eHTMLTag_b,          &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_base,       &gInHead,      0,              0,              kNone,        &gInHead,     0},
  {eHTMLTag_basefont,   &gRootTags,    0,              0,              kBP},
  {eHTMLTag_bdo,        &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_bgsound,    &gRootTags,    0,              0,              kBPIP,        &gInHead,     0},
  {eHTMLTag_big,        &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_blink,      &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_blockquote, &gRootTags,    0,              0,              kBPBCIC},
  {eHTMLTag_body,       &gInHTML,      &gBodyAutoClose,0,              kBCIC|kCB,    &gBodyParents,&gBodyKids},
  {eHTMLTag_br,         &gRootTags,    0,              0,              kBPIP},
  {eHTMLTag_button,     &gRootTags,    0,              0,              kAll,         0,            &gButtonKids},

  {eHTMLTag_caption,    &gRootTags,    &gCaptionAutoClose,  0,         kIC|kCB,      &gInTable,    0},
  {eHTMLTag_center,     &gRootTags,    0,              0,              kAll},
  {eHTMLTag_cite,       &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_code,       &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_col,        &gColParents,  0,              0,              kNone,        &gColParents, 0},
  {eHTMLTag_colgroup,   &gRootTags,    0,              0,              kNone,        0,            &gColgroupKids},  

  {eHTMLTag_dd,         &gInDL,        &gDTCloseTags,  0,              kBCIC|kCB,    &gInDL,       0},
  {eHTMLTag_del,        &gRootTags,    0,              0,              kAll,         0,0},
  {eHTMLTag_dfn,        &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_dir,        &gRootTags,    0,              0,              kAll,         0,            &gULKids},
  {eHTMLTag_div,        &gRootTags,    0,              0,              kBPBCIC},
  {eHTMLTag_dl,         &gRootTags,    0,              0,              kBP|kIC,      0,            &gDLKids},
  {eHTMLTag_dt,         &gInDL,        &gDTCloseTags,  0,              kIC|kCB,      &gInDL,       &gDTKids},

  {eHTMLTag_em,         &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_embed,      &gRootTags,    0,              0,              kAll,         0,            &gContainsParam},

  {eHTMLTag_fieldset,   &gRootTags,    0,              0,              kBPBCIC,   0,            &gFieldsetKids},
  {eHTMLTag_font,       &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_form,       &gRootTags,    0,              0,              kAll,         0,            &gFormKids},
  {eHTMLTag_frame,      &gInFrameset,  0,              0,              kNone,        &gInFrameset, 0},
  {eHTMLTag_frameset,   &gInHTML,      0,              0,              kNone,        &gFramesetParents,  &gFramesetKids},

  {eHTMLTag_h1,         &gRootTags,    &gHxAutoClose,  &gHxAutoClose,  kBPBCIC},
  {eHTMLTag_h2,         &gRootTags,    &gHxAutoClose,  &gHxAutoClose,  kBPBCIC},
  {eHTMLTag_h3,         &gRootTags,    &gHxAutoClose,  &gHxAutoClose,  kBPBCIC},
  {eHTMLTag_h4,         &gRootTags,    &gHxAutoClose,  &gHxAutoClose,  kBPBCIC},
  {eHTMLTag_h5,         &gRootTags,    &gHxAutoClose,  &gHxAutoClose,  kBPBCIC},
  {eHTMLTag_h6,         &gRootTags,    &gHxAutoClose,  &gHxAutoClose,  kBPBCIC},
  {eHTMLTag_head,       &gInHTML,      0,              0,              kNone,        &gInHTML,     &gHeadKids},
  {eHTMLTag_hr,         &gRootTags,    0,              0,              kBP},
  {eHTMLTag_html,       &gHTMLRootTags,&gAutoClose,    0,              kNone,        0,            &gHtmlKids},
  
  {eHTMLTag_i,          &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_iframe,     &gRootTags,    0,              0,              kAll},
  {eHTMLTag_ilayer,     &gRootTags,    0,              0,              kAll},
  {eHTMLTag_img,        &gRootTags,    0,              0,              kBPIP},  
  {eHTMLTag_input,      &gRootTags,    0,              0,              kBPIP},  //NOT DONE!!!
  {eHTMLTag_ins,        &gRootTags,    0,              0,              kAll,         &gInBody,     0},
  {eHTMLTag_isindex,    &gRootTags,    0,              0,              kBP,          &gInHead,     0},

  {eHTMLTag_kbd,        &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_keygen,     &gRootTags,    0,              0,              kNone,        &gInForm,     0},

  {eHTMLTag_label,      &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_layer,      &gRootTags,    0,              0,              kAll},
  {eHTMLTag_legend,     &gRootTags,    0,              0,              kIC,          &gLegendParents, 0},
  {eHTMLTag_li,         &gLIRootTags,  &gLIAutoClose,  0,              kAll,         0,               &gLIKids},
  {eHTMLTag_link,       &gInHead,      0,              0,              kNone,        &gInHead,     0},
  {eHTMLTag_listing,    &gRootTags,    0,              0,              kAll}, 

  {eHTMLTag_map,        &gRootTags,    0,              0,              kBPIP,        0,            &gMapKids},
  {eHTMLTag_menu,       &gRootTags,    0,              0,              kAll,         0,            &gULKids},
  {eHTMLTag_meta,       &gInHead,      0,              0,              kNone,        &gInHead,     0},
  {eHTMLTag_multicol,   &gRootTags,    0,              0,              kBPBCIC},

  {eHTMLTag_nobr,       &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_noembed,    &gRootTags,    0,              0,              kBPBCIC},
  {eHTMLTag_noframes,   &gRootTags,    0,              0,              kBPBCIC,  &gInFrameset, &gNoframesKids},
  {eHTMLTag_nolayer,    &gRootTags,    0,              0,              kBPBCIC},
  {eHTMLTag_noscript,   &gRootTags,    0,              0,              kBPBCIC},
  
  {eHTMLTag_object,     &gRootTags,    0,              0,              kAll,         0,            &gContainsParam},
  {eHTMLTag_ol,         &gOLRootTags,  0,              &gULCloseTags,  kAll,         0,            &gULKids},
  {eHTMLTag_optgroup,   &gRootTags,    0,              0,              kNone, &gOptgroupParents,   &gContainsOpts},
  {eHTMLTag_option,     &gOptionRootTags,0,              0,            kNone, &gOptgroupParents,   &gContainsText},

  //                                                               
  //                      Root          Autoclose-    Autoclose-      Container     Special       Special
  //    tag-id            Nodes         starttags     endtags         -rules        Parent        Kids

  {eHTMLTag_p,          &gRootTags,    &gPAutoClose,   0,              kBPBCIC},
  {eHTMLTag_param,      &gParamParents,0,              0,              kNone,    &gParamParents,   0},
  {eHTMLTag_plaintext,  &gRootTags,    0,              0,              kBPIP},
  {eHTMLTag_pre,        &gRootTags,    0,              0,              kBP|kIC,   0,  &gPreKids},
  {eHTMLTag_q,          &gRootTags,    0,              0,              kBPIPIC}, 

  {eHTMLTag_s,          &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_samp,       &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_script,     &gRootTags,    0,              0,              kBPIP,        &gInHead,     &gContainsText},
  {eHTMLTag_select,     &gInForm,      0,              0,              kBPIP,        0,            &gContainsOpts},
  {eHTMLTag_server,     &gRootTags,    0,              0,              kNone},
  {eHTMLTag_small,      &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_sound,      &gRootTags,    0,              0,              kBPIP,        &gInHead,     0},
  {eHTMLTag_spacer,     &gRootTags,    0,              0,              kBPIP},
  {eHTMLTag_span,       &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_strike,     &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_strong,     &gRootTags,    0,              0,              kBPIPIC,      &gInHead,     &gContainsText},
  {eHTMLTag_style,      &gInHead,      0,              0,              kNone,        &gInHead},
  {eHTMLTag_sub,        &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_sup,        &gRootTags,    0,              0,              kBPIPIC},

  {eHTMLTag_table,      &gRootTags,    0,              0,              kBP,          0,            &gTableKids},
  {eHTMLTag_tbody,      &gInTable,     0,              0,              kNone,        &gInTable,    &gTBodyKids},
  {eHTMLTag_td,         &gTDRootTags,  &gTDCloseTags,  0,              kBCIC|kCB,    &gInTR,       0},
  {eHTMLTag_textarea,   &gInForm,      0,              0,              kBPIP,        0,            &gContainsText},
  {eHTMLTag_tfoot,      &gInTable,     0,              0,              kCB,          &gInTable,    &gTableElemKids},
  {eHTMLTag_th,         &gTDRootTags,  0,              0,              kBCIC|kCB,    &gInTR,       0},
  {eHTMLTag_thead,      &gInTable,     0,              0,              kCB,          &gInTable,    &gTableElemKids},
  {eHTMLTag_title,      &gInHead,      0,              0,              kNone,        &gInHead,     &gContainsText},
  {eHTMLTag_tr,         &gTRParents,   &gTRCloseTags,  0,              kCB,          &gTRParents,  &gTRKids},
  {eHTMLTag_tt,         &gRootTags,    0,              0,              kBPIPIC},

  {eHTMLTag_u,          &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_ul,         &gOLRootTags,  0,              &gULCloseTags,  kAll,         0,            &gULKids},
  {eHTMLTag_var,        &gRootTags,    0,              0,              kBPIPIC},
  {eHTMLTag_wbr,        &gRootTags,    0,              0,              kBPIP},
  {eHTMLTag_xmp,        &gRootTags,    0,              0,              kBPIP},
  {eHTMLTag_text,       &gTextRootTags,0,             0,              kBPIP},
  {eHTMLTag_whitespace, &gTextRootTags,0,              0,              kBPIP},
  {eHTMLTag_newline,    &gTextRootTags,0,              0,              kBPIP},
  {eHTMLTag_comment,    &gRootTags,    0,              0,              kBPIP},
  {eHTMLTag_entity,     &gTextRootTags,0,              0,              kBPIP},
  {eHTMLTag_userdefined,&gRootTags,    0,              0,              kBPIP},
};

/**
 * This class is here to finalize the initialization of the
 * nsHTMLElement table.
 */
class CTableInitializer {
public:
  CTableInitializer(){
    int index=0;
    for(index=0;index<eHTMLTag_userdefined;index++){
      gHTMLElements[index].mSelfContained=PR_FALSE;
      //gHTMLElements[index].mBits&=kTC;
    }

    //now initalize tags that can contain themselves...
    int max=sizeof(gStyleTags)/sizeof(eHTMLTag_unknown);
    for(index=0;index<max;index++){
      gHTMLElements[gStyleTags[index]].mSelfContained=PR_TRUE;
    }
    gHTMLElements[eHTMLTag_a].mSelfContained=PR_FALSE;
    gHTMLElements[eHTMLTag_frameset].mSelfContained=PR_TRUE;
    gHTMLElements[eHTMLTag_ol].mSelfContained=PR_TRUE;
    gHTMLElements[eHTMLTag_ul].mSelfContained=PR_TRUE;
  }
};
CTableInitializer gTableInitializer;

/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsContainer(eHTMLTags aChild) {
  PRBool result=PR_FALSE;

  if((eHTMLTag_userdefined==aChild) || (eHTMLTag_unknown==aChild))
    result=PR_TRUE;
  else if((aChild>=eHTMLTag_unknown) & (aChild<=eHTMLTag_userdefined)){
    if((gHTMLElements[aChild].HasBits(kBC))  ||
       (gHTMLElements[aChild].HasBits(kIC)) || 
       (gHTMLElements[aChild].mSpecialKids)) {
      result=PR_TRUE;
    }
  } 
  return result;
}


/**
 * 
 * @update	gess12/17/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsBlockElement(eHTMLTags aChild){
  PRBool result=PR_FALSE;

  if((aChild>=eHTMLTag_unknown) & (aChild<=eHTMLTag_userdefined)){
    result=gHTMLElements[aChild].HasBits(kBP);
  } 
  return result;
}


/**
 * 
 * @update	gess12/17/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsBlockCloser(eHTMLTags aChild){
  PRBool result=PR_FALSE;

  if((aChild>=eHTMLTag_unknown) & (aChild<=eHTMLTag_userdefined)){
    result=gHTMLElements[aChild].HasBits(kCB|kBP);
  } 
  return result;
}


/**
 * 
 * @update	gess12/17/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsInlineElement(eHTMLTags aChild){
  PRBool result=PR_FALSE;
  if((aChild>=eHTMLTag_unknown) & (aChild<=eHTMLTag_userdefined)){
    result=gHTMLElements[aChild].HasBits(kIP);
  } 
  return result;
}


/**
 * 
 * @update	gess12/17/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsBlockParent(eHTMLTags aChild){
  PRBool result=PR_FALSE;
  if((aChild>=eHTMLTag_unknown) & (aChild<=eHTMLTag_userdefined)){
    result=gHTMLElements[aChild].HasBits(kBC);
  } 
  return result;
}


/**
 * 
 * @update	gess12/17/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsInlineParent(eHTMLTags aChild){
  PRBool result=PR_FALSE;
  if((aChild>=eHTMLTag_unknown) & (aChild<=eHTMLTag_userdefined)){
    result=gHTMLElements[aChild].HasBits(kIC);
  } 
  return result;
}

/**
 * 
 * @update	gess12/17/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::CanContain(eHTMLTags aParent,eHTMLTags aChild){
  PRBool result=PR_FALSE;
  if((aParent>=eHTMLTag_unknown) & (aParent<=eHTMLTag_userdefined)){
    result=gHTMLElements[aChild].CanContain(aChild);
  } 
  return result;
}

/**
 * 
 * @update	gess12/17/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::CanOmitEndTag(eHTMLTags aChild) const{
  PRBool result=PR_FALSE;
  return result;
}

/**
 * 
 * @update	gess12/17/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::CanOmitStartTag(eHTMLTags aChild) const{
  PRBool result=PR_FALSE;
  return result;
}

/**
 * 
 * @update	gess12/17/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::HasBits(PRInt32 aBitset) const{
  PRBool result=(mBits & aBitset);
  return result;
}

/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsStyleTag(eHTMLTags aChild) {
  PRBool result=FindTagInSet(aChild,gStyleTags,sizeof(gStyleTags)/sizeof(eHTMLTag_body));
  return result;
}

/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsTextTag(eHTMLTags aChild) {
  static eHTMLTags gTextTags[]={eHTMLTag_text,eHTMLTag_entity,eHTMLTag_newline, eHTMLTag_whitespace};
  PRBool result=FindTagInSet(aChild,gTextTags,sizeof(gTextTags)/sizeof(eHTMLTag_body));
  return result;
}

/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::CanContain(eHTMLTags aChild) const{

  if(IsContainer(mTagID)){

    if(mTagID==aChild) {
      return mSelfContained;  //not many tags can contain themselves...
    }

    if(nsHTMLElement::IsInlineElement(aChild)){
      if(nsHTMLElement::IsInlineParent(mTagID)){
        return PR_TRUE;
      }
    }

    if(nsHTMLElement::IsTextTag(aChild)) {
      if(nsHTMLElement::IsInlineParent(mTagID)){
        return PR_TRUE;
      }
    }

    if(nsHTMLElement::IsBlockElement(aChild)){
      if(nsHTMLElement::IsBlockParent(mTagID) || IsStyleTag(mTagID)){
        return PR_TRUE;
      }
    }

    if(mSpecialKids) {
      if(FindTagInSet(aChild,mSpecialKids->mTags,mSpecialKids->mCount)){
        return PR_TRUE;
      }
    }

    if(mTagID!=eHTMLTag_server){
      int x=5;
    }
  }
  
  return PR_FALSE;
}

/**
 * This method figures out what kind of child tag 
 * you're looking at. For now, it's kind of an 
 * expensive lookup. Later, I'll OPT-O-MIZE it.
 * @update  gess4/6/98
 * @param   aChildTag is who you're trying to figure out
 * @return  eHTMLChildType value
 */
eHTMLCategory GetParentModelFor(eHTMLTags aChildTag) {
  eHTMLCategory result=eHTMLCategory_unknown;

  PRBool blockChild=nsHTMLElement::IsBlockElement(aChildTag);
  PRBool inlineChild=nsHTMLElement::IsInlineElement(aChildTag);

  if(blockChild & inlineChild)
    return eHTMLCategory_blockAndInline;
  else if(blockChild)
    return eHTMLCategory_block;
  else if(inlineChild)
    return eHTMLCategory_inline;
  else if(gHTMLElements[aChildTag].mSpecialParents==&gInHead)
    return eHTMLCategory_head;
  else if(gHTMLElements[aChildTag].mSpecialParents==&gInForm)
    return eHTMLCategory_form;
  else if(gHTMLElements[aChildTag].mSpecialParents==&gInTable)
    return eHTMLCategory_table;

  else {
    switch(aChildTag){
      case eHTMLTag_li:
        result=eHTMLCategory_list;
        break;
      case eHTMLTag_html:
        result=eHTMLCategory_unknown;
        break;
      case eHTMLTag_head:
      case eHTMLTag_body:
      case eHTMLTag_unknown:
        result=eHTMLCategory_html;
        break;
      case eHTMLTag_tbody:
      case eHTMLTag_thead:
      case eHTMLTag_tfoot:
        result=eHTMLCategory_tablepart;
        break;
      case eHTMLTag_tr:
        result=eHTMLCategory_tablerow;
        break;
      case eHTMLTag_td:
      case eHTMLTag_th:
        result=eHTMLCategory_tabledata;
        break;
      case eHTMLTag_option:
      case eHTMLTag_optgroup:
        result=eHTMLCategory_options;
        break;
      case eHTMLTag_legend:
        result=eHTMLCategory_text;
        break;
      case eHTMLTag_frameset:
        result=eHTMLCategory_frameset;
        break;
      default:
        NS_ERROR("Oops");
        break;
    } //switch
  } //else

  return result;
}

/**
 * This method figures out what kind of parent tag 
 * you're looking at. For now, it's kind of an 
 * expensive lookup. Later, I'll OPT-O-MIZE it.
 * @update  gess4/6/98
 * @param   aParentTag is who you're trying to figure out
 * @return  eHTMLChildType value
 */
eHTMLCategory GetContentModelFor(eHTMLTags aTag) {
  eHTMLCategory result=eHTMLCategory_unknown;

  PRBool blockParent=nsHTMLElement::IsBlockParent(aTag);
  PRBool inlineParent=nsHTMLElement::IsInlineParent(aTag);

  if(blockParent & inlineParent) { 
    return eHTMLCategory_blockAndInline;
  }
  else if(blockParent) {
    return eHTMLCategory_block;
  }
  else if(inlineParent) {
    return eHTMLCategory_inline;
  }
  else if(gHTMLElements[aTag].mSpecialParents==&gInTable) {
    return eHTMLCategory_table;
  }
  else if((aTag==eHTMLTag_ol) || (aTag==eHTMLTag_ol)) {
    result=eHTMLCategory_list;
  }
  else {
    switch(aTag){
      case eHTMLTag_li:
        result=eHTMLCategory_list;
        break;
      case eHTMLTag_html:
      case eHTMLTag_body:
        result=eHTMLCategory_html;
        break;
      case eHTMLTag_head:
        result=eHTMLCategory_head;
        break;
      case eHTMLTag_unknown:
        result=eHTMLCategory_unknown;
        break;
      case eHTMLTag_tbody:
      case eHTMLTag_thead:
      case eHTMLTag_tfoot:
        result=eHTMLCategory_tablepart;
        break;
      case eHTMLTag_tr:
        result=eHTMLCategory_tablerow;
        break;
      case eHTMLTag_td:
      case eHTMLTag_th:
        result=eHTMLCategory_tabledata;
        break;
      case eHTMLTag_table:
        result=eHTMLCategory_table;
        break;
      case eHTMLTag_select:
        result=eHTMLCategory_options;
        break;
      case eHTMLTag_frameset:
        result=eHTMLCategory_frameset;
        break;
      default:
        if(!(blockParent || inlineParent)) {
          if(gHTMLElements[aTag].mSpecialKids==&gContainsText)
            return eHTMLCategory_text;
        }
        NS_ERROR("Oops");
        break;
    } //switch
  } //else
  return result;
}
