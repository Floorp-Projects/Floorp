/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#ifdef EDITOR

#include "editor.h"
#include "prefapi.h"
#include "intl_csi.h"

#define CONTENT_TYPE "Content-Type"

// Maximum length of "positional text" - see GetPositionalText(). 
#ifdef XP_WIN16
const int32 MAX_POSITIONAL_TEXT = 64000;
#else
const int32 MAX_POSITIONAL_TEXT =0x7FFFFFF0;
#endif

// Maximum length of text that can be pasted.
#ifdef XP_WIN16
#define MAX_PASTE_SIZE 32760
#else
#define MAX_PASTE_SIZE (1<<30)
#endif

#include "prefapi.h"

#define GET_COL TRUE
#define GET_ROW FALSE

#if defined( DEBUG_shannon )
int32	gEditorReflow = TRUE;
#endif

EDT_CharacterData *CEditBuffer::m_pCopyStyleCharacterData = 0;

//-----------------------------------------------------------------------------
// CEditBuffer
//-----------------------------------------------------------------------------

CEditElement* CEditBuffer::CreateElement(PA_Tag* pTag, CEditElement *pParent){
    CEditElement *pElement;
    // this is a HACK.  We don't understand the tag but we want
    //  to make sure that we can re-gurgitate it to layout.
    //  Save the parameter data so we can spew it back out.
    char *locked_buff;

    PA_LOCK(locked_buff, char *, pTag->data);
    if( locked_buff && *locked_buff != '>'){
        pElement = new CEditElement(pParent,pTag->type,locked_buff);
    }
    else {
        pElement = new CEditElement(pParent,pTag->type);
    }
    PA_UNLOCK(pTag->data);
    return pElement;
}

CParseState* CEditBuffer::GetParseState() {
    if ( m_parseStateStack.IsEmpty() ) {
        PushParseState();
    }
    return m_parseStateStack.Top();
}


/////////////////////////////////////////////
//EDITBUFFER/////////////////////////////////
/////////////////////////////////////////////

XP_Bool CEditBuffer::m_bEdtBufPrefInitialized=FALSE;
XP_Bool CEditBuffer::m_bMoveCursor; //no initial value should be depended on
XP_Bool CEditBuffer::m_bNewCellHasSpace = TRUE; //New cells we create have an &nbsp in them so border displays
//these variables are set by the pref_registercallback function "PrefCallback";


//callback from a change in the preferences. we must then reinitialize these prefs
int CEditBuffer::PrefCallback(const char *, void *)//static
{
  m_bEdtBufPrefInitialized=FALSE;
  CEditBuffer::InitializePrefs();//static
  return TRUE;
}


void CEditBuffer::InitializePrefs()//static
{
  if (!m_bEdtBufPrefInitialized) //this is ok if this is allready initialized, 
  {                              //this means that someone has changed 
                                 //them and we need to reset them 
    PREF_RegisterCallback("editor", CEditBuffer::PrefCallback, NULL);//no instance data, only setting statics.  we are all of the same mind here
    PREF_GetBoolPref("editor.page_updown_move_cursor",&m_bMoveCursor);
    PREF_GetBoolPref("editor.new_cell_has_space",&m_bNewCellHasSpace);
  }
  m_bEdtBufPrefInitialized=TRUE;
}



void CEditBuffer::PushParseState(){
    XP_Bool bInBody = FALSE;
    if ( ! m_parseStateStack.IsEmpty() ) {
        bInBody = m_parseStateStack.Top()->InBody();
    }
    CParseState* state = new CParseState();
    state->Reset();
    if ( bInBody ) {
        state->StartBody(); // If we were in the body before, we're still in the body.
    }
    m_parseStateStack.Push(state);
}

void CEditBuffer::PopParseState(){
    if ( m_parseStateStack.IsEmpty() ) {
        XP_ASSERT(FALSE);
    }
    else {
        CParseState* state = m_parseStateStack.Pop();
        delete state;
    }
}

void CEditBuffer::ResetParseStateStack(){
    while ( ! m_parseStateStack.IsEmpty() ) {
        PopParseState();
    }
}

//
// stolen from layout
//
#define DEF_TAB_WIDTH       8

#define REMOVECHAR(p) XP_MEMMOVE( p, p+1, XP_STRLEN(p) );

PRIVATE
void edt_ExpandTab(char *p, intn n){
    if( n != 1 ){
        XP_MEMMOVE( p+n, p+1, XP_STRLEN(p));
    }
    XP_MEMSET( p, ' ', n);
}


//
// Parse the text into a series of tags.  Each tag is at most one line in
//  length.
//
intn CEditBuffer::NormalizePreformatText(pa_DocData *pData, PA_Tag* pTag,
            intn status )
{
    char *pText;
    intn retVal = OK_CONTINUE;
    m_bInPreformat = TRUE;

    PA_LOCK(pText, char *, pTag->data);


    //
    // Calculate how big a buffer we are going to need to normalize this
    //  preformatted text.
    //
    int iTabCount = 0;
    int iLen = 0;
    int iNeedLen;
    int iSpace;

    while( *pText ){
        if( *pText == TAB ){
            iTabCount++;
        }
        pText++;
        iLen++;
    }

    PA_UNLOCK(pTag->data);
    iNeedLen = iLen + iTabCount * DEF_TAB_WIDTH+1;

    pTag->data =  (PA_Block) PA_REALLOC( pTag->data, iNeedLen );
    pTag->data_len = iNeedLen;
    PA_LOCK(pText, char *, pTag->data);

    // LTNOTE: total hack, but make sure the buffer is 0 terminated...  Probably has
    //  bad internation ramifications.. Oh Well...
    // JHPNOTE: I'll say this is a total hack. We used to write at [data_len], which
    // smashed beyond allocated memory. As far as I can see there's no need to write here,
    // since the string is known to be zero terminated now, and all the operations
    // below keep the string zero terminated. Never the less, in the interest of keeping
    // tradition alive (and because I don't want to take the chance of introducing a
    // bug, I'll leave the correct, but unnescessary, code in place.

    pText[iNeedLen-1] = 0;
    pText[iLen] = 0;

    char* pBuf = pText;
    //
    // Now actually Normalize the preformatted text
    //
    while( *pText ){
        switch( *pText ){
        // Look for CF LF, if you see it, just treat it as LF
        case CR:
            if( pText[1] == LF ){
                REMOVECHAR(pText);
                break;
            }
            else {
                *pText = LF;
            }

        case LF:
            pText++;
            m_preformatLinePos = 0;
            break;

        case VTAB:
        case FF:
            REMOVECHAR(pText);
            break;

        case TAB:
            iSpace = DEF_TAB_WIDTH - (m_preformatLinePos % DEF_TAB_WIDTH);

            edt_ExpandTab( pText, iSpace );
            pText += iSpace;
            m_preformatLinePos += iSpace;
            break;


        default:
            pText++;
            m_preformatLinePos++;
        }
    }

    XP_Bool bBreak = FALSE;
    while( *pBuf && retVal == OK_CONTINUE ){
        pText = pBuf;
        bBreak = FALSE;

        // scan for end of line.
        while( *pText && *pText != '\n' ){
            pText++;
        }


        PA_Tag *pNewTag = XP_NEW( PA_Tag );
        XP_BZERO( pNewTag, sizeof( PA_Tag ) );
        pNewTag->type = P_TEXT;

        char save = *pText;
        *pText = 0;
        edt_SetTagData(pNewTag, pBuf );
        *pText = save;

        // include the new line
        if( *pText == '\n' ){
            pText++;
            bBreak = TRUE;
        }

        pBuf = pText;
        retVal = EDT_ProcessTag( pData, pNewTag, status );
        if( bBreak ){
            pNewTag = XP_NEW( PA_Tag );
            XP_BZERO( pNewTag, sizeof( PA_Tag ) );
            pNewTag->type = P_LINEBREAK;
            retVal = EDT_ProcessTag( pData, pNewTag, status );
        }
    }

    PA_UNLOCK(pTag->data);
    m_bInPreformat = FALSE;
    return ( retVal == OK_CONTINUE) ? OK_IGNORE : retVal ;
}

static char *anchorHrefParams[] = {
    PARAM_HREF,
    0
};

static char *bodyParams[] = {
    PARAM_BACKGROUND,
    PARAM_BGCOLOR,
    PARAM_TEXT,
    PARAM_LINK,
    PARAM_ALINK,
    PARAM_VLINK,
    PARAM_NOSAVE,
    0
};

PRIVATE
void NormalizeEOLsInString(char* pStr){
    int state = 0;
    char* pOut = pStr;
    for(;; ) {
        char c = *pStr++;
        if ( c == '\0' ) break;

        int charClass = 0;
        if ( c == '\r' ) charClass = 1;
        else if ( c == '\n') charClass = 2;
        switch ( state ) {
        case 0: /* Normal */
            switch ( charClass ) {
            case 0: /* Normal char */
                *pOut++ = c;
                break;
            case 1: /* CR */
                state = 1;
                *pOut++ = '\n';
                break;
            case 2: /* LF */
                state = 2;
                *pOut++ = '\n';
                break;
            }
            break;
        case 1: /* Just saw a CR */
            switch ( charClass ) {
            case 0: /* Normal char */
                *pOut++ = c;
                state = 0;
                break;
            case 1: /* CR */
                *pOut++ = '\n';
                break;
            case 2: /* LF */
                state = 0;
                /* Swallow it, back to normal */
                break;
            }
            break;
        case 2: /* Just saw a LF */
            switch ( charClass ) {
            case 0: /* Normal char */
                *pOut++ = c;
                state = 0;
                break;
            case 1: /* CR */
                /* Swallow it, back to normal */
                state = 0;
                break;
            case 2: /* LF */
                *pOut++ = '\n';
                break;
            }
            break;
        }
    }
    *pOut = '\0';
}


PRIVATE
void NormalizeEOLsInTag(PA_Tag* pTag){
    // Convert the end-of-lines into \n characters.
    if( pTag->data ){
        char* pStr;
        PA_LOCK(pStr, char*, pTag->data);
        NormalizeEOLsInString(pStr);
        pTag->data_len = strlen( pStr );
        PA_UNLOCK( pTag->data );
    }
}

PRIVATE
XP_Bool IsComment(PA_Tag* pTag){
    // Is this a comment tag?
    XP_Bool result = FALSE;
    if ( pTag->type == P_UNKNOWN ) {
        if( pTag->data ){
            char* pStr;
            const char* kComment = "!";
            unsigned int commentLen = XP_STRLEN(kComment);
            PA_LOCK(pStr, char*, pTag->data);
            if ( pStr && XP_STRLEN(pStr) >= commentLen && XP_STRNCASECMP(pStr, kComment, commentLen) == 0 ) {
                result = TRUE;
            }
            PA_UNLOCK( pTag->data );
        }
    }
    return result;
}

PRIVATE
XP_Bool IsEmptyText(PA_Tag* pTag){
    // Is this empty white-space text?
    // Takes advantage of white-space being
    // one byte in all character sets,
    // and non-white-space characters always
    // start with a non-white-space byte.
    XP_Bool result = FALSE;
    if ( pTag->type == P_TEXT ) {
        result = TRUE;
        if( pTag->data ){
            char* pStr;
            PA_LOCK(pStr, char*, pTag->data);
            if ( pStr ) {
                while ( *pStr ) {
                    if ( ! XP_IS_SPACE(*pStr) ) {
                        result = FALSE;
                        break;
                    }
                    pStr++;
                }
            }
            PA_UNLOCK( pTag->data );
        }
    }
    return result;
}


PRIVATE
XP_Bool IsDocTypeTag(PA_Tag* pTag){
    // Is this a DocType tag?
    XP_Bool result = FALSE;
    if ( pTag->type == P_UNKNOWN ) {
        if( pTag->data ){
            char* pStr;
            const char* kDocType = "!DOCTYPE";
            unsigned int docTypeLen = XP_STRLEN(kDocType);
            PA_LOCK(pStr, char*, pTag->data);
            if ( XP_STRLEN(pStr) >= docTypeLen && XP_STRNCASECMP(pStr, kDocType, docTypeLen) == 0 ) {
                result = TRUE;
            }
            PA_UNLOCK( pTag->data );
        }
    }
    return result;
}

// To do - combine with HandleSelectionComment

XP_Bool CEditBuffer::IsSelectionComment(PA_Tag* pTag){
    XP_Bool result = FALSE;
    if ( pTag && pTag->type == P_UNKNOWN) {
        if ( pTag->data ){
            char* kStart = "!-- " EDT_SELECTION_START_COMMENT;
            char* kEnd = "!-- " EDT_SELECTION_END_COMMENT;
            char* pStr;
            PA_LOCK(pStr, char*, pTag->data);
            XP_Bool bStartComment = XP_STRLEN(pStr) >= XP_STRLEN(kStart) &&
                XP_STRNCASECMP(pStr, kStart, XP_STRLEN(kStart)) == 0;
            XP_Bool bEndComment = XP_STRLEN(pStr) >= XP_STRLEN(kEnd) &&
                XP_STRNCASECMP(pStr, kEnd,  XP_STRLEN(kEnd)) == 0;
            if ( bStartComment || bEndComment ) {
                result = TRUE;
            }
            PA_UNLOCK( pTag->data );
        }
    }
    return result;
}

XP_Bool CEditBuffer::HandleSelectionComment(PA_Tag* pTag, CEditElement*& pElement, intn& retval){
    XP_Bool result = FALSE;
    if ( pTag->type == P_UNKNOWN) {
        if ( pTag->data ){
            char* kStart = "!-- " EDT_SELECTION_START_COMMENT;
            char* kEnd = "!-- " EDT_SELECTION_END_COMMENT;
            char* kStartPlus = "!-- " EDT_SELECTION_START_COMMENT "+";
            char* kEndPlus = "!-- " EDT_SELECTION_END_COMMENT "+";
            char* pStr;
            PA_LOCK(pStr, char*, pTag->data);
            XP_Bool bStartComment = XP_STRLEN(pStr) >= XP_STRLEN(kStart) &&
                XP_STRNCASECMP(pStr, kStart, XP_STRLEN(kStart)) == 0;
            XP_Bool bEndComment = XP_STRLEN(pStr) >= XP_STRLEN(kEnd) &&
                XP_STRNCASECMP(pStr, kEnd,  XP_STRLEN(kEnd)) == 0;
            if ( bStartComment || bEndComment )
            {
                result = TRUE;
                pElement = NULL;
                if ( bStartComment && ! m_pStartSelectionAnchor ) {
                    pElement = m_pStartSelectionAnchor = new CEditInternalAnchorElement(m_pCreationCursor);
                    m_bStartSelectionStickyAfter = XP_STRLEN(pStr) >= XP_STRLEN(kStartPlus) &&
                        XP_STRNCASECMP(pStr, kStartPlus, XP_STRLEN(kStartPlus)) == 0;
                }
                else if ( bEndComment && ! m_pEndSelectionAnchor ) {
                    pElement = m_pEndSelectionAnchor = new CEditInternalAnchorElement(m_pCreationCursor);
                    m_bEndSelectionStickyAfter = XP_STRLEN(pStr) >= XP_STRLEN(kEndPlus) &&
                        XP_STRNCASECMP(pStr, kEndPlus, XP_STRLEN(kEndPlus)) == 0;
                }
                else {
                    retval = OK_IGNORE;
                }
            }
            PA_UNLOCK( pTag->data );
        }
    }

    if ( pElement ) {
        m_pCreationCursor = pElement->GetParent();
    }
    return result;
}

XP_Bool CEditBuffer::ShouldAutoStartBody(PA_Tag* pTag, int16 csid){
    CParseState* pParseState = GetParseState();
    if ( pParseState->InBody() ) {
        return FALSE;
    }
    if ( pParseState->m_inJavaScript ||
        pParseState->m_bInTitle) {
        return FALSE;
    }
    if (IsSelectionComment(pTag)){
        return TRUE;
    }
    if ( ! BitSet( edt_setAutoStartBody, pTag->type ) ) {
        return FALSE;
    }
    if ( pTag->type != P_TEXT ) {
        return TRUE;
    }
    XP_Bool result = FALSE;
    // Pure white space doesn't start a body.
    // This code takes advantage of the fact that, in all supported
    // character sets, white-space characters are single byte.
    if( pTag->data ){
        char* pStr;
        PA_LOCK(pStr, char*, pTag->data);
        if ( pStr ) {
            while ( *pStr ) {
                if (! XP_IS_SPACE(*pStr) ) {
                    result = TRUE;
                    break;
                }
                pStr = INTL_NextChar(csid, pStr);
            }
        }
        PA_UNLOCK( pTag->data );
    }
    return result;
}

intn CEditBuffer::ParseTag(pa_DocData *pData, PA_Tag* pTag, intn status){
    intn retVal = OK_CONTINUE;

    // Examine the tag  before it's modified or replaced.
    XP_Bool bRecordTag = ! IsComment(pTag) && ! IsEmptyText(pTag);
    TagType iTagType = pTag->type;
    XP_Bool bTagIsEnd = (XP_Bool) pTag->is_end;

    NormalizeEOLsInTag(pTag);

    /* P_STRIKE is a synonym for P_STRIKEOUT. Since pre-3.0 versions of
     * Navigator don't recognize P_STRIKE ( "<S>" ), we switch it to
     * P_STRIKEOUT ( "<STRIKE>" ). Also, it would complicate the
     * editor internals to track two versions of the same tag.
     * We test here so that we catch both the beginning and end tag.
     */
    if ( pTag->type == P_STRIKE ) {
        pTag->type = P_STRIKEOUT;
    }

    /* Many HTML pages don't include BODY tags. The first appearance of a
     * body-only tag is our signal to start a body.
     */
    if ( ShouldAutoStartBody(pTag, GetRAMCharSetID()) ) {
        GetParseState()->StartBody();
    }

    if( pTag->is_end && !BitSet( edt_setUnsupported, pTag->type ) &&
            pTag->type != P_UNKNOWN ){
        retVal = ParseEndTag(pTag);
    }
    else {
        retVal = ParseOpenTag(pData, pTag, status);
    }

    // Maintain the m_iLastTagType
    if ( bRecordTag ) {
        m_iLastTagType = iTagType;
        m_bLastTagIsEnd = bTagIsEnd;
    }

    return retVal;
}

intn CEditBuffer::ParseEndTag(PA_Tag* pTag){
    intn retVal = OK_CONTINUE;
 
    // If this is an end tag, match backwardly to the appropriate tag.
    //
    CEditElement *pPop = m_pCreationCursor;
    if( pTag->type == P_HEAD ){
        WriteClosingScriptTag();
        // RecordTag(pTag, TRUE);
        return retVal;
    }
    if( pTag->type == P_BODY ){
        // We ignore </BODY> in the Mail Compose window because we have
        // to deal with nested HTML documents.
        // We don't ignore it in the Page Compose window because
        // we want to preserve <FRAMESET>s and other tags that appear
        // after the </BODY>.
        if ( !IsComposeWindow() ) {
            GetParseState()->EndBody();
        }
        return retVal;
    }
    if( pTag->type == P_TITLE ){
        GetParseState()->m_bInTitle = FALSE;
        return retVal;
    }
    // libparse is nice enough to only call us with end tags if they
    // match the actual brute tag that we're in. So we
    // don't have to check if the end SCRIPT or STYLE tag matches the
    // corresponding start tag. (Other tags are passed as two text
    // tags, one for the opening '<', and the other for the rest of
    // the tag.)
    if( GetParseState()->m_inJavaScript &&
        pTag->type == GetParseState()->m_inJavaScript ){
        WriteClosingScriptTag();
        if ( GetParseState()->InBody() ){
            PopParseState();
        }
        return retVal;
    }
    if( BitSet( edt_setTextContainer, pTag->type ) || BitSet( edt_setList, pTag->type ) ){
        GetParseState()->bLastWasSpace = TRUE;
    }

    while( pPop && pTag->type != pPop->GetType() ){
        pPop = pPop->GetParent();
    }

    // if this is character formatting, pop the stack.
    if( pTag->type == P_CENTER || pTag->type == P_DIVISION ){
        if( !GetParseState()->m_formatAlignStack.IsEmpty() ){
            GetParseState()->m_formatAlignStack.Pop();
        }
        // If we are closing alignment, it forces a line break (or new
        //  paragraph)
        if( m_pCreationCursor->IsContainer() ){
            m_pCreationCursor = m_pCreationCursor->GetParent();
        }
        GetParseState()->bLastWasSpace = TRUE;

        // And for divisions, pop the division information.
        if( pTag->type == P_DIVISION && pPop && pTag->type == pPop->GetType() && pPop->GetParent() != 0 ) {
            m_pCreationCursor = pPop->GetParent();
        }
    }
    else if( BitSet( edt_setCharFormat, pTag->type ) && !GetParseState()->m_formatTypeStack.IsEmpty()){
        // The navigator is forgiving about the nesting of style end tags, e.g. </b> vs. </i>.
        // So you can say, for example,
        // <b>foo<i>bar</b>baz</i> and it will work as if you had said:
        // <b>foo<i>bar</i>baz</b>.

        // LTNOTE: This crashes sometimes??
        //delete GetParseState()->m_pNextText;
        GetParseState()->m_pNextText = 0;
        GetParseState()->m_formatTypeStack.Pop();
        GetParseState()->m_pNextText = GetParseState()->m_formatTextStack.Pop();

        // </a> pops all the HREFs. So if you say:
        // <a href="a">foo<a href="b">bar</a>baz, it's as if you said:
        // <a href="a">foo</a><a href="b">bar</a>baz
        // For fun, try this:
        // <a href="a">foo<a name="b">bar</a>baz
        // The 'bar' will look like it is a link, but it won't act like a link if you
        // press on it.
        // Anyway, we emulate this mess by clearing the hrefs out of the format text stack when we
        // close any P_ANCHOR tag

        if ( pTag->type == P_ANCHOR ) {
            if ( GetParseState()->m_pNextText ) {
                GetParseState()->m_pNextText->SetHREF( ED_LINK_ID_NONE );
            }
            TXP_PtrStack_CEditTextElement& textStack = GetParseState()->m_formatTextStack;
            for ( int i = 0; i < textStack.Size(); i++) {
                if ( textStack[i] ) {
                    textStack[i]->SetHREF( ED_LINK_ID_NONE );
                }
            }
        }
    }
    else if( pTag->type == P_PARAGRAPH ) {
        if( pPop && pTag->type == pPop->GetType() && pPop->IsContainer() ) {
            CEditContainerElement *pCont = pPop->Container();
            if ( pCont ) {
                pCont->m_bHasEndTag = TRUE;
            }
        }

        if( pPop && pTag->type == pPop->GetType() && pPop->GetParent() != 0 ) {
            m_pCreationCursor = pPop->GetParent();
        }
    }
    else if( pPop && pTag->type == pPop->GetType() && pPop->GetParent() != 0 ) {
        m_pCreationCursor = pPop->GetParent();
    }
    else{
        // We have an error.  Ignore it for now.
        //DebugBreak();
    }
    if ( pPop && (pTag->type == P_TABLE_HEADER || pTag->type == P_TABLE_DATA || pTag->type == P_CAPTION) ) {
        PopParseState();
    }
    return retVal;
}

void CEditBuffer::WriteClosingScriptTag()
{
    // Write out the closing Script tag.
    TagType type = GetParseState()->m_inJavaScript;
    if ( ! type ) return;
    char* tagName =  EDT_TagString(type);
    CStreamOutMemory* pOut = GetParseState()->GetStream();
    pOut->Write( "</", 2);
    pOut->Write( tagName, XP_STRLEN(tagName));
    pOut->Write( ">\n", 2);
    GetParseState()->m_inJavaScript = 0;
    if ( GetParseState()->InBody() ){
        RecordJavaScriptAsUnknownTag(pOut);
    }
}

PRIVATE void edt_MakeAbsoluteUsingBaseTag(CEditElement *pCreationCursor,char **ppURL) {
  // Don't do anything if NULL.
  if (!(ppURL && *ppURL)) {
    return;
  }

  // Find enclosing mail quote.
  CEditListElement *pMQuote = pCreationCursor->GetMailQuote();
  if (pMQuote) {
    EDT_ListData *pListData = pMQuote->GetData();
    if (pListData) {
      // If mail quote has a <BASE HREF=> tag.
      if (pListData->pBaseURL && *pListData->pBaseURL) {
        char *pAbs = NET_MakeAbsoluteURL(pListData->pBaseURL,*ppURL);
        if (pAbs) {
          XP_FREE(*ppURL);
          *ppURL = pAbs;
        }
      }
      CEditListElement::FreeData(pListData);
    }
  }
}

intn CEditBuffer::ParseOpenTag(pa_DocData *pData, PA_Tag* pTag, intn status){
    char *pStr;
    CEditElement* pElement = 0;
    CEditElement *pContainer;
    PA_Block buff;
    intn retVal = OK_CONTINUE;
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_pContext);
    int16 win_csid = INTL_GetCSIWinCSID(c);

    //
    // make sure all solo tags are placed in a paragraph.
    //
    if( pTag->type != P_TEXT
            && (BitSet( edt_setSoloTags,  pTag->type  )
                    || BitSet( edt_setUnsupported,  pTag->type ) )
            && !BitSet( edt_setTextContainer,  m_pCreationCursor->GetType()  )
            && m_pCreationCursor->FindContainer() == 0 ){
        //m_pCreationCursor = new CEditElement(m_pCreationCursor,P_PARAGRAPH);
        m_pCreationCursor = CEditContainerElement::NewDefaultContainer( m_pCreationCursor,
                    GetCurrentAlignment() );

    }
    if ( BitSet( edt_setUnsupported, pTag->type ) ) {
        ParseUnsupportedTag(pTag, pElement, retVal);
    }
    else
        switch(pTag->type){

        case P_LINK:
            ParseLink(pTag, pElement, retVal);
            break;

        case P_TEXT:
            if( pTag->data ){
                PA_LOCK(pStr, char*, pTag->data);

                if( GetParseState()->m_bInTitle ){
                    AppendTitle( pStr );
                    PA_UNLOCK( pTag->data );
                    return retVal;
                }
                else if( GetParseState()->m_inJavaScript ){
                    GetParseState()->GetStream()->Write( pStr, XP_STRLEN( pStr ) );
                    PA_UNLOCK( pTag->data );
                    return OK_IGNORE;
                }
                // New strategy: Format each line of imported text into a <BR> line
                // (calls EDT_ProcessTag iteratively)
                else if( m_bImportText && !m_bInPreformat ) {
                        // calls EDT_ProcessTag iteratively
                        return NormalizePreformatText( pData, pTag, status );
                }
                else if( !BitSet( edt_setFormattedText, m_pCreationCursor->GetType() )
#ifdef USE_SCRIPT
                        && !(GetParseState()->m_pNextText->m_tf & (TF_SERVER|TF_SCRIPT|TF_STYLE))
#endif
                        && !m_pCreationCursor->InFormattedText() ){
                    NormalizeText( pStr );
                }
                else {
                    if( !m_bInPreformat ){
                        //
                        // calls EDT_ProcessTag iteratively
                        //
                        // Note: We hit the "New Strategy" above when parsing
                        //       imported text, so I don't think we ever hit
                        //       this, but lets keep it to be safe.
                        return NormalizePreformatText( pData, pTag, status );
                    }
                }
                // we probably need to adjust the length in the parse tag.
                //  it works without it, but I'm not sure why.

                // if the text didn't get reduced away.
                // Strip white space in tables
                XP_Bool bGoodText = *pStr && ! (( XP_IS_SPACE(*pStr) && pStr[1] == '\0' ) &&
                    BitSet( edt_setIgnoreWhiteSpace,  m_pCreationCursor->GetType()));
                if( bGoodText ){
                    // if this text is not within a container, make one.
                    if( !BitSet( edt_setTextContainer,  m_pCreationCursor->GetType()  )
                            && m_pCreationCursor->FindContainer() == 0 ){
                        m_pCreationCursor = CEditContainerElement::NewDefaultContainer( m_pCreationCursor,
                                GetCurrentAlignment() );
                    }
                    CEditTextElement *pNew = GetParseState()->m_pNextText->CopyEmptyText(m_pCreationCursor);
                    pNew->SetText( pStr, IsMultiSpaceMode(), win_csid );

                    // create a new text element.  It adds itself as a child
                    pElement = pNew;
                }
                else {
                    retVal =  OK_IGNORE;
                }
                PA_UNLOCK( pTag->data );
            }
            break;

        //
        // Paragraphs are always inserted at the level of the current
        //  container.
        //
        case P_HEADER_1:
        case P_HEADER_2:
        case P_HEADER_3:
        case P_HEADER_4:
        case P_HEADER_5:
        case P_HEADER_6:
        case P_DESC_TITLE:
        case P_ADDRESS:
        case P_LIST_ITEM:
        case P_DESC_TEXT:
        case P_PARAGRAPH:
        ContainerCase:
            pContainer = m_pCreationCursor->FindContainerContainer();
            if( pContainer ){
                m_pCreationCursor = pContainer;
            }
            else {
                m_pCreationCursor = m_pRoot;
            }
            m_pCreationCursor = pElement = new CEditContainerElement( m_pCreationCursor,
                    pTag, GetRAMCharSetID(), GetCurrentAlignment() );
            break;

        case P_UNUM_LIST:
        case P_NUM_LIST:
        case P_DESC_LIST:
        case P_MENU:
        case P_DIRECTORY:
        case P_BLOCKQUOTE:
        case P_MQUOTE: // Mailing quotation.
            pContainer = m_pCreationCursor->FindContainerContainer();
            if( pContainer ){
                m_pCreationCursor = pContainer;
            }
            else {
                m_pCreationCursor = m_pRoot;
            }
            m_pCreationCursor = pElement = new CEditListElement( m_pCreationCursor, pTag, GetRAMCharSetID());
            break;

        case P_BODY:
            ParseBodyTag(pTag);
            break;

        case P_HEAD:
            // Silently eat this tag.
            break;

    // character formatting
        case P_STYLE:
        case P_SCRIPT:
        case P_SERVER:
            if( GetParseState()->InBody() ){
                PushParseState();
            }
            RecordTag(pTag, FALSE);
            GetParseState()->m_inJavaScript = pTag->type;
            return OK_IGNORE;
            /*
            // Save tag parameters.
            if ( GetParseState()->m_pNextText ) {
               char *locked_buff;
               PA_LOCK(locked_buff, char *, pTag->data );
               GetParseState()->m_pNextText->SetScriptExtra(locked_buff);
               PA_UNLOCK(pTag->data);
            }
            m_preformatLinePos = 0;
            // intentionally fall through.
            */
        case P_BOLD:
        case P_STRONG:
        case P_ITALIC:
        case P_EMPHASIZED:
        case P_VARIABLE:
        case P_CITATION:
        case P_FIXED:
        case P_CODE:
        case P_KEYBOARD:
        case P_SAMPLE:
        case P_SUPER:
        case P_SUB:
        case P_NOBREAK:
        case P_STRIKEOUT:
        case P_SPELL:
        case P_INLINEINPUT:
        case P_INLINEINPUTTHICK:
        case P_INLINEINPUTDOTTED:
        case P_STRIKE:
        case P_UNDERLINE:
        case P_BLINK:
            GetParseState()->m_formatTextStack.Push(GetParseState()->m_pNextText);
            GetParseState()->m_formatTypeStack.Push(pTag->type);
            GetParseState()->m_pNextText = GetParseState()->m_pNextText->CopyEmptyText();
            GetParseState()->m_pNextText->m_tf |= edt_TagType2TextFormat(pTag->type);
            break;

        case P_SMALL:
            GetParseState()->m_formatTextStack.Push(GetParseState()->m_pNextText);
            GetParseState()->m_formatTypeStack.Push(pTag->type);
            GetParseState()->m_pNextText = GetParseState()->m_pNextText->CopyEmptyText();
            if( GetParseState()->m_pNextText->m_tf & TF_FONT_SIZE ){
                GetParseState()->m_pNextText->SetFontSize(GetParseState()->m_pNextText->GetFontSize()-1);
            }
            else {
                GetParseState()->m_pNextText->SetFontSize(m_pCreationCursor->GetDefaultFontSize()-1);
            }
            break;

        case P_BIG:
            GetParseState()->m_formatTextStack.Push(GetParseState()->m_pNextText);
            GetParseState()->m_formatTypeStack.Push(pTag->type);
            GetParseState()->m_pNextText = GetParseState()->m_pNextText->CopyEmptyText();
            if( GetParseState()->m_pNextText->m_tf & TF_FONT_SIZE ){
                GetParseState()->m_pNextText->SetFontSize(GetParseState()->m_pNextText->GetFontSize()+1);
            }
            else {
                GetParseState()->m_pNextText->SetFontSize(m_pCreationCursor->GetDefaultFontSize()+1);
            }
            break;

        case P_BASEFONT:
             GetParseState()->m_baseFontSize = edt_FetchParamInt(pTag, PARAM_SIZE, 3, GetRAMCharSetID());
             GetParseState()->m_pNextText->SetFontSize(GetParseState()->m_baseFontSize);
            break;

        case P_FONT:
            {
                GetParseState()->m_formatTextStack.Push(GetParseState()->m_pNextText);
                GetParseState()->m_formatTypeStack.Push(pTag->type);
                GetParseState()->m_pNextText = GetParseState()->m_pNextText->CopyEmptyText();
                buff = PA_FetchParamValue(pTag, PARAM_SIZE, win_csid);
                if (buff != NULL) {
                    char *size_str;
                    PA_LOCK(size_str, char *, buff);
                    GetParseState()->m_pNextText->SetFontSize(LO_ChangeFontSize(
                            GetParseState()->m_baseFontSize, size_str ));
                    PA_UNLOCK(buff);
                    PA_FREE(buff);
                }
				buff = PA_FetchParamValue(pTag, PARAM_FACE, win_csid);
				if (buff != NULL)
				{
                    char* str;
				    PA_LOCK(str, char *, buff);
                    GetParseState()->m_pNextText->SetFontFace(str);
				    PA_UNLOCK(buff);
				    PA_FREE(buff);
				}

				int16 iWeight = (int16) edt_FetchParamInt(pTag, PARAM_FONT_WEIGHT, ED_FONT_WEIGHT_NORMAL, GetRAMCharSetID());
				if (iWeight != ED_FONT_WEIGHT_NORMAL)
				{
                    GetParseState()->m_pNextText->SetFontWeight(iWeight);
				}

				int16 iPointSize = (int16) edt_FetchParamInt(pTag, PARAM_POINT_SIZE, ED_FONT_POINT_SIZE_DEFAULT, GetRAMCharSetID());
				if (iPointSize != ED_FONT_POINT_SIZE_DEFAULT)
				{
                    GetParseState()->m_pNextText->SetFontPointSize(iPointSize);
				}

                ED_Color c = edt_FetchParamColor( pTag, PARAM_COLOR, GetRAMCharSetID() );
                if( c.IsDefined() ){
                    GetParseState()->m_pNextText->SetColor( c );
                }
            }
            break;

        case P_ANCHOR:
            // push the formatting stack in any case so we properly pop
            //  it in the future.
            GetParseState()->m_formatTextStack.Push(GetParseState()->m_pNextText);
            GetParseState()->m_formatTypeStack.Push(pTag->type);
            GetParseState()->m_pNextText = GetParseState()->m_pNextText->CopyEmptyText();

            pStr = edt_FetchParamString(pTag, PARAM_HREF, win_csid);
            if( pStr != NULL ){
                // If in a mail quote with a <BASE> tag, make absolute.
                edt_MakeAbsoluteUsingBaseTag(m_pCreationCursor,&pStr);

                // collect extra stuff..
                // LTNOTE:
                char* pExtra = edt_FetchParamExtras( pTag, anchorHrefParams, win_csid );
                ED_LinkId id = linkManager.Add( pStr, pExtra );
                GetParseState()->m_pNextText->SetHREF( id );
                linkManager.Free( id );         // reference counted.
                XP_FREE( pStr );
                XP_FREEIF( pExtra );
            }

            pStr = edt_FetchParamString(pTag, PARAM_NAME, win_csid);
            if( pStr ){
                // We don't catch this case above because by default
                //  an anchor is a character formatting
                if( m_pCreationCursor->FindContainer() == 0 ){
                    m_pCreationCursor = CEditContainerElement::NewDefaultContainer(
                            m_pCreationCursor, GetCurrentAlignment() );
                }
                pElement = new CEditTargetElement(m_pCreationCursor, pTag);
                m_pCreationCursor = pElement->GetParent();
                XP_FREE( pStr );
            }
            break;

        case P_IMAGE:
        case P_NEW_IMAGE:
            {
                pElement = new CEditImageElement(m_pCreationCursor, pTag, GetRAMCharSetID(),
                            GetParseState()->m_pNextText->GetHREF());
                
                // If in a mail quote with a <BASE> tag, make SRC and LOWSRC absolute.
                EDT_ImageData *pImageData = pElement->Image()->GetImageData();
                if (pImageData) {
                  // regular image
                  edt_MakeAbsoluteUsingBaseTag(m_pCreationCursor,&pImageData->pSrc);
                  // low resolution
                  edt_MakeAbsoluteUsingBaseTag(m_pCreationCursor,&pImageData->pLowSrc);

                  pElement->Image()->SetImageData(pImageData);
                  EDT_FreeImageData(pImageData);
                }
 
                m_pCreationCursor = pElement->GetParent();
                GetParseState()->bLastWasSpace = FALSE;
                break;
            }

        case P_HRULE:
            {
            pElement = new CEditHorizRuleElement(m_pCreationCursor, pTag);
            m_pCreationCursor = pElement->GetParent();
            GetParseState()->bLastWasSpace = TRUE;
            break;
            }

        case P_PLAIN_TEXT:
            if( m_bImportText )
            {
                // New strategy for importing text:
                // We will convert each imported line to a paragraph,
                //   so we can ignore the initial tag that would have
                //   resulted in PREFORMAT around all imported text 
                // (We used to fall through to insert the PREFORMAT tag)
                break;
            }
        case P_PLAIN_PIECE:
        case P_LISTING_TEXT:
        case P_PREFORMAT:
            pTag->type = P_PREFORMAT;
            m_preformatLinePos = 0;
            goto ContainerCase;


        //
        // pass these tags on so the document looks right
        //
        case P_CENTER:
            // Forces a new paragraph unless the current paragraph is empty
            if( m_pCreationCursor->IsContainer() && m_pCreationCursor->GetChild() ){
                m_pCreationCursor = m_pCreationCursor->GetParent();
            }
            GetParseState()->m_formatAlignStack.Push(ED_ALIGN_CENTER);
            pContainer = m_pCreationCursor->FindContainer();
            if( pContainer ){
                pContainer->Container()->AlignIfEmpty( ED_ALIGN_CENTER );
            }
            break;

        case P_DIVISION:
            // Forces a new paragraph unless the current paragraph is empty
            if( m_pCreationCursor->IsContainer() && m_pCreationCursor->GetChild() ){
                m_pCreationCursor = m_pCreationCursor->GetParent();
            }
            {
                pContainer = m_pCreationCursor->FindContainerContainer();
                if( pContainer ){
                    m_pCreationCursor = pContainer;
                }
                else {
                    m_pCreationCursor = m_pRoot;
                }
                CEditDivisionElement* pDivision = new CEditDivisionElement( m_pCreationCursor, pTag, GetRAMCharSetID() );

                // For historical reasons, we handle alignment in the formatAlignStack.
                // It would probably be better to handle it in the DIV tag.
                ED_Alignment eAlign = pDivision->GetAlignment();
                if ( eAlign == ED_ALIGN_DEFAULT ) {
                    eAlign = ED_ALIGN_LEFT;
                }
                else {
                    pDivision->ClearAlignment(); 
                }
                GetParseState()->m_formatAlignStack.Push( eAlign );
                pContainer = m_pCreationCursor->FindContainer();
                if( pContainer ){
                    pContainer->Container()->AlignIfEmpty( eAlign );
                }
                // Create a division container.
                m_pCreationCursor = pDivision;
                break;
            }

        case P_LINEBREAK:
            {
            // Ignore <BR> after </LI>.
            if ( m_bLastTagIsEnd && BitSet( edt_setIgnoreBreakAfterClose, m_iLastTagType ) ) {
            }
            else {
                pElement = new CEditBreakElement(m_pCreationCursor, pTag);
                m_pCreationCursor = pElement->GetParent();
            }
            GetParseState()->bLastWasSpace = TRUE;
            break;
            }

        case P_TITLE:
            GetParseState()->m_bInTitle = TRUE;
            break;

        case P_META:
            ParseMetaTag( pTag );
            break;

        case P_BASE:
            // There are two known parameters in a BASE tag:
            // HREF which sets the base URL
            // TARGET which sets the default target for links.
            // We ignore HREF when it's not in a mail quotation.
            // We preserve TARGET as part of the document properties.
            {
                char *pTarget = edt_FetchParamString(pTag,PARAM_TARGET,m_pCreationCursor->GetWinCSID());
                if ( pTarget ) {
                    SetBaseTarget(pTarget);
                }
                XP_FREEIF(pTarget);
            }
            // We only deal with P_BASE tags for mail quotation.
            {
              // Look for enclosing mail quote CEditListElement and set pBaseURL to
              // be the HREF parameter of the <BASE> tag.
              CEditListElement *pMQuote = m_pCreationCursor->GetMailQuote();
              if (pMQuote) {
                char *pHref = edt_FetchParamString(pTag,PARAM_HREF,m_pCreationCursor->GetWinCSID());
                if (pHref) {
                  EDT_ListData *pData = pMQuote->GetData();
                  if (pData) {
                    XP_FREEIF(pData->pBaseURL);
                    pData->pBaseURL = pHref;
                    pMQuote->SetData(pData);
                    CEditListElement::FreeData(pData);
                  }
                  else {
                    XP_FREE(pHref);
                  }
                }
              }
            }
            break;

        case P_TABLE:
            {
              pContainer = m_pCreationCursor->FindContainerContainer();
              if( pContainer ){
                  m_pCreationCursor = pContainer;
              }
              else {
                  m_pCreationCursor = m_pRoot;
              }


              // A table following a </P> should have an extra break before the table.  Add a NSDT.
              CEditElement *pPrev = m_pCreationCursor->GetLastChild();
              if (pPrev && pPrev->IsContainer()) {
                  CEditContainerElement *pCont = pPrev->Container();
                  if (pCont && pCont->GetType() == P_PARAGRAPH && pCont->m_bHasEndTag) {
                    CEditContainerElement *pNew;
                    pNew = CEditContainerElement::NewDefaultContainer( m_pCreationCursor, pCont->GetAlignment() );
                    // g++ thinks the following value is not used, but it's mistaken.
                    // The constructor auto-inserts the element into the tree.
                    (void) new CEditTextElement(pNew, 0);
                  }
              }


              m_pCreationCursor = pElement = new CEditTableElement( m_pCreationCursor, pTag, GetRAMCharSetID(), GetCurrentAlignment() );

              // If in a mail quote with a <BASE> tag, make table background absolute.
              EDT_TableData *pData;
              if (pElement && NULL != (pData = CEditTableElement::Cast(pElement)->GetData())) {
                edt_MakeAbsoluteUsingBaseTag(m_pCreationCursor,&pData->pBackgroundImage);
                CEditTableElement::Cast(pElement)->SetData(pData);
                CEditTableElement::FreeData(pData);
              }
            }

            break;

        case P_TABLE_ROW:
            {
                CEditTableElement* pTable = m_pCreationCursor->GetTable();
                if ( ! pTable ){
                    // We might be in a TD that someone forgot to close.
                    CEditTableCellElement* pCell = m_pCreationCursor->GetTableCell();
                    if ( pCell ){
                        pTable = pCell->GetTable();
                    }
                }
                if ( pTable ) {
                    m_pCreationCursor = pElement = new CEditTableRowElement( pTable, pTag, GetRAMCharSetID() );

                    // If in a mail quote with a <BASE> tag, make table background absolute.
                    EDT_TableRowData *pData;
                    if (pElement && NULL != (pData = CEditTableRowElement::Cast(pElement)->GetData())) {
                      edt_MakeAbsoluteUsingBaseTag(m_pCreationCursor,&pData->pBackgroundImage);
                      CEditTableRowElement::Cast(pElement)->SetData(pData);
                      CEditTableRowElement::FreeData(pData);
                    }
                }
                else {
                    XP_TRACE(("Ignoring table row. Not in table."));
                }
            }
            break;

        case P_TABLE_HEADER:
        case P_TABLE_DATA:
            {
                CEditTableElement* pTable = m_pCreationCursor->GetTable();
                if ( ! pTable ){
                    // We might be in a TD that someone forgot to close.
                    CEditTableCellElement* pCell = m_pCreationCursor->GetTableCell();
                    if ( pCell ){
                        m_pCreationCursor = pCell->GetParent();
                        pTable = m_pCreationCursor->GetTable();
                    }
                }
                if ( ! pTable ) {
                    XP_TRACE(("Ignoring table cell. Not in table."));
                }
                else {
                    CEditTableRowElement* pTableRow = m_pCreationCursor->GetTableRow();
                    if ( ! pTableRow ){
                        // They forgot to put in a table row.
                        // Create one for them.

                        pTableRow = new CEditTableRowElement();
                        pTableRow->InsertAsLastChild(pTable);
                    }
                    m_pCreationCursor = pElement = new CEditTableCellElement( pTableRow, pTag, GetRAMCharSetID() );

                    // If in a mail quote with a <BASE> tag, make table background absolute.
                    EDT_TableCellData *pData;
                    if (pElement && NULL != (pData = CEditTableCellElement::Cast(pElement)->GetData())) {
                      edt_MakeAbsoluteUsingBaseTag(m_pCreationCursor,&pData->pBackgroundImage);
                      CEditTableCellElement::Cast(pElement)->SetData(pData);
                      CEditTableCellElement::FreeData(pData);
                    }

                    PushParseState();
                }
            }
            break;

        case P_CAPTION:
            {
                CEditTableElement* pTable = m_pCreationCursor->GetTable();
                if ( pTable ) {
                    m_pCreationCursor = pElement = new CEditCaptionElement( pTable, pTag, GetRAMCharSetID() );
                    PushParseState();
                }
                else {
                    XP_TRACE(("Ignoring caption. Not in a table."));
                }
            }
            break;

        case P_LAYER:
            pContainer = m_pCreationCursor->FindContainerContainer();
            if( pContainer ){
                m_pCreationCursor = pContainer;
            }
            else {
                m_pCreationCursor = m_pRoot;
            }
            m_pCreationCursor = new CEditLayerElement( m_pCreationCursor, pTag, GetRAMCharSetID() );
            retVal =  OK_IGNORE; // Remove this when we can display layers in the editor
            break;

        // unimplemented Tags

        case P_MAX:
            m_pCreationCursor = pElement = CreateElement( pTag, m_pCreationCursor );
            if( BitSet( edt_setSoloTags, m_pCreationCursor->GetType() ) ){
                m_pCreationCursor = m_pCreationCursor->GetParent();
            }
            break;

        //
        // someones added a new tag.  Inspect pTag and look in pa_tags.h
        // At the least, add it to edt_setUnsupported
        //
        default:
            XP_TRACE(("Someone added a new tag type %d.  Inspect pTag and look in pa_tags.h", pTag->type));
            XP_ASSERT(FALSE);
        case P_UNKNOWN:
            ParseUnsupportedTag(pTag, pElement, retVal);
            break;

        // its ok to ignore these tags.
        case P_NSCP_OPEN:
        case P_NSCP_CLOSE:
        case P_NSCP_REBLOCK:
        case P_HTML:
            break;
    }
    pTag->edit_element = pElement;
    return retVal;
}

void CEditBuffer::ParseLink(PA_Tag* pTag, CEditElement*& pElement, intn& retVal){
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_pContext);
    if ( GetParseState()->InBody() ){
        ParseUnsupportedTag(pTag, pElement, retVal);
        return;
    }
    char* pRel = edt_FetchParamString(pTag, PARAM_REL, INTL_GetCSIWinCSID(c));
    if ( pRel && XP_STRCASECMP(pRel, "FONTDEF") == 0 ) {
        ParseLinkFontDef(pTag, pElement, retVal); 
        return;
    }
    ParseUnsupportedTag(pTag, pElement, retVal);
}

void CEditBuffer::ParseLinkFontDef(PA_Tag* pTag, CEditElement*& /*pElement*/, intn& /*retVal*/){
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_pContext);
    if ( m_pFontDefURL ) {
        XP_FREE(m_pFontDefURL);
    }
    m_pFontDefURL = edt_FetchParamString(pTag, PARAM_SRC, INTL_GetCSIWinCSID(c));
    m_bFontDefNoSave = edt_FetchParamBoolExist(pTag, PARAM_NOSAVE, GetRAMCharSetID());
}


void CEditBuffer::ParseUnsupportedTag(PA_Tag* pTag, CEditElement*& pElement, intn& retVal)
{
    if ( IsDocTypeTag(pTag) ) {
        return; // Ignore document's idea about it's document type.
    }

    // If we don't understand it, and it's in the head, then assume that it needs to stay in the head.
    if ( !GetParseState()->InBody() ){
        RecordTag(pTag, TRUE);
        retVal = OK_IGNORE;
        return;
    }

    if( !m_pCreationCursor->IsContainer() && m_pCreationCursor->FindContainer() == 0 ){
        m_pCreationCursor = CEditContainerElement::NewDefaultContainer(
                m_pCreationCursor, GetCurrentAlignment() );
    }
    if ( HandleSelectionComment(pTag, pElement, retVal) ){
        return;
    }
    m_pCreationCursor = pElement = new CEditIconElement( m_pCreationCursor,
        pTag->is_end ?
            EDT_ICON_UNSUPPORTED_END_TAG
        :
            EDT_ICON_UNSUPPORTED_TAG,
        pTag );
    m_pCreationCursor->Icon()->MorphTag( pTag );
    m_pCreationCursor = m_pCreationCursor->GetParent();
    return;
}

void CEditBuffer::RecordTag(PA_Tag* pTag, XP_Bool bWithLinefeed){
    CStreamOutMemory* pOut = GetParseState()->GetStream();

    // Save the tag and its parameters
    char* kScriptTag =  EDT_TagString(pTag->type);
    pOut->Write("<",1);
    if ( pTag->is_end ) {
        pOut->Write("/",1);
    }
    pOut->Write(kScriptTag, XP_STRLEN(kScriptTag));

    char *locked_buff;
    PA_LOCK(locked_buff, char *, pTag->data );
    pOut->Write(locked_buff, XP_STRLEN(locked_buff));
    PA_UNLOCK(pTag->data);
    if ( bWithLinefeed ) pOut->Write("\n", 1);
}

void CEditBuffer::RecordJavaScriptAsUnknownTag(CStreamOutMemory* pOut){
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_pContext);
    // Store the script in a Icon tag.
    PA_Tag dummyTag;
    XP_BZERO( &dummyTag, sizeof( dummyTag ) );
    dummyTag.type = P_UNKNOWN; 
    dummyTag.is_end = FALSE;

    if( !m_pCreationCursor->IsContainer() && m_pCreationCursor->FindContainer() == 0 ){
        m_pCreationCursor = CEditContainerElement::NewDefaultContainer(
                m_pCreationCursor, GetCurrentAlignment() );
    }
    m_pCreationCursor = new CEditIconElement( m_pCreationCursor,
                EDT_ICON_UNSUPPORTED_TAG,
                &dummyTag );
    m_pCreationCursor->Icon()->MorphTag( &dummyTag );
    char* pData = edt_CopyFromHuge(INTL_GetCSIWinCSID(c), pOut->GetText(), pOut->GetLen(), NULL);
    m_pCreationCursor->Icon()->SetData( pData );
    m_pCreationCursor = m_pCreationCursor->GetParent();
    XP_FREE(pData);
}

void CEditBuffer::ParseBodyTag(PA_Tag *pTag){
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_pContext);
    GetParseState()->StartBody();
    // Allow multiple body tags -- don't replace parameter a new one isn't found.
    edt_FetchParamString2(pTag, PARAM_BACKGROUND, m_pBackgroundImage, INTL_GetCSIWinCSID(c));
    m_bBackgroundNoSave = edt_FetchParamBoolExist( pTag, PARAM_NOSAVE, GetRAMCharSetID() );
    edt_FetchParamColor2( pTag, PARAM_BGCOLOR, m_colorBackground, GetRAMCharSetID());
    edt_FetchParamColor2( pTag, PARAM_TEXT, m_colorText, GetRAMCharSetID() );
    edt_FetchParamColor2( pTag, PARAM_LINK, m_colorLink, GetRAMCharSetID() );
    edt_FetchParamColor2( pTag, PARAM_ALINK, m_colorActiveLink, GetRAMCharSetID() );
    edt_FetchParamColor2( pTag, PARAM_VLINK, m_colorFollowedLink, GetRAMCharSetID() );
    //m_pBody = m_pCreationCursor = pElement = CreateElement( pTag, m_pCreationCursor );
    edt_FetchParamExtras2( pTag, bodyParams, m_pBodyExtra, GetRAMCharSetID() );
}


CEditBuffer::CEditBuffer(MWContext *pContext, XP_Bool bImportText):
        m_lifeFlag(0xbab3fac3),
        m_pContext(pContext),
        m_pRoot(0),
        m_pCurrent(0),
        m_iCurrentOffset(0),
        m_bCurrentStickyAfter(FALSE),
        m_pCreationCursor(0),
        m_colorText(ED_Color::GetUndefined()),
        m_colorBackground(ED_Color::GetUndefined()),
        m_colorLink(ED_Color::GetUndefined()),
        m_colorFollowedLink(ED_Color::GetUndefined()),
        m_colorActiveLink(ED_Color::GetUndefined()),
        m_pTitle(0),
        m_pBackgroundImage(0),
        m_bBackgroundNoSave(FALSE),
        m_pFontDefURL(0),
        m_bFontDefNoSave(FALSE),
        m_pBodyExtra(0),
        m_pLoadingImage(0),
        m_pSaveObject(0),
        m_bMultiSpaceMode(TRUE),
        m_hackFontSize(0),
        m_iDesiredX(-1),
        m_lastTopY(0),
        m_inScroll(FALSE),
        m_bBlocked(TRUE),
        m_bSelecting(0),
        m_bNoRelayout(FALSE),
        m_pSelectStart(0),
        m_iSelectOffset(0),
        m_preformatLinePos(0),
        m_bInPreformat(FALSE),
        printState(),
        linkManager(),
        m_metaData(),
        m_parseStateStack(),
        m_status(ED_ERROR_NONE),
        m_pCommandLog(0),
        m_bTyping(FALSE),
        m_pStartSelectionAnchor(0),
        m_pEndSelectionAnchor(0),
        m_bStartSelectionStickyAfter(0),
        m_bEndSelectionStickyAfter(0),
        m_bLayoutBackpointersDirty(TRUE),
        m_iFileWriteTime(0),
#ifdef DEBUG
        m_pTestManager(0),
        m_iSuppressPhantomInsertPointCheck(0),
        m_bSkipValidation(0),
#endif
        m_pSizingObject(0),
        m_finishLoadTimer(),
        m_relayoutTimer(),
        m_autoSaveTimer(),
        m_bDisplayTables(TRUE),
        m_bDummyCharacterAddedDuringLoad(FALSE),
        m_bReady(0),
        m_bPasteQuoteMode(FALSE),
        m_bPasteHTML(FALSE),
        m_bPasteHTMLWhenSavingDocument(FALSE),
        m_pPasteHTMLModeText(0),
        m_pPasteTranscoder(0),
        m_bForceDocCSID(FALSE),
        m_forceDocCSID(0),
        m_originalWinCSID(0),
        m_pSelectedLoTable(0),
        m_pSelectedEdTable(0),
        m_iNextSelectedCell(0),
        m_TableHitType(ED_HIT_NONE),
        m_pSelectedTableElement(0),
        m_pPrevExtendSelectionCell(0),
    	m_bEncrypt(PR_FALSE),
        m_pNonTextSelectedTable(0),
        m_bImportText(bImportText),
        m_bFillNewCellWithSpace(FALSE)
{
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_pContext);
    m_originalWinCSID = INTL_GetCSIWinCSID(c);
    m_pCommandLog = CGlobalHistoryGroup::GetGlobalHistoryGroup()->CreateLog(this);
    m_pStartSelectionAnchor = NULL;
    m_pEndSelectionAnchor = NULL;
    m_bStartSelectionStickyAfter = FALSE;
    m_bEndSelectionStickyAfter = FALSE;
    m_iLastTagType = -1;
    m_bLastTagIsEnd = FALSE;
    m_pBaseTarget = 0;

    // create a root element
    edt_InitBitArrays();
    m_pCreationCursor = m_pRoot = new CEditRootDocElement( this );
    ResetParseStateStack();

#ifdef DEBUG
    m_pTestManager = new CEditTestManager(this);
    m_iSuppressPhantomInsertPointCheck = FALSE;
    m_bSkipValidation = FALSE;
#endif

    m_relayoutTimer.SetEditBuffer(this);
    m_autoSaveTimer.SetEditBuffer(this);
#ifdef DEBUG_AUTO_SAVE
    m_autoSaveTimer.SetPeriod(1);
#endif

    // This is no longer changable by user
    m_pContext->display_table_borders = TRUE; // On by default.
}


CEditBuffer::~CEditBuffer(){
    m_lifeFlag = 0;

	if ( m_pContext ) {
        int32 doc_id = XP_DOCID(m_pContext);
	    lo_TopState* top_state = lo_FetchTopState(doc_id);
	    if (top_state != NULL )
	    {
    	    top_state->edit_buffer = 0;
        }
    }
#ifdef DEBUG
    delete m_pTestManager;
#endif
    delete m_pRoot;
    m_pRoot = NULL;
    // Delete meta data
    for(intn i = 0; i < m_metaData.Size(); i++ ) {
        FreeMetaData(m_metaData[i]);
    }
    ResetParseStateStack();
    CGlobalHistoryGroup::GetGlobalHistoryGroup()->DeleteLog(this);
    delete m_pPasteHTMLModeText;
    delete m_pPasteTranscoder;
    XP_FREEIF(m_pBaseTarget);
}

XP_Bool CEditBuffer::IsAlive(CEditBuffer* pBuffer){
    return pBuffer && pBuffer->m_lifeFlag == 0xbab3fac3;
}

XP_Bool CEditBuffer::IsReady(){
	return m_bReady;
}

void CEditBuffer::FixupInsertPoint(){
    CEditInsertPoint ip(m_pCurrent, m_iCurrentOffset, m_bCurrentStickyAfter);
    FixupInsertPoint(ip);
    m_pCurrent = ip.m_pElement;
    m_iCurrentOffset = ip.m_iPos;
    m_bCurrentStickyAfter = ip.m_bStickyAfter;
    XP_ASSERT(m_bCurrentStickyAfter == TRUE || m_bCurrentStickyAfter == FALSE);
}

void CEditBuffer::FixupInsertPoint(CEditInsertPoint& ip){
    if( ip.m_iPos == 0 && ! IsPhantomInsertPoint(ip) ){
        CEditLeafElement *pPrev = ip.m_pElement->PreviousLeafInContainer();
        if( pPrev && pPrev->GetLen() != 0 ){
            ip.m_pElement = pPrev;
            ip.m_iPos = pPrev->GetLen();
        }
    }
    // Since we fake up spaces at the beginning of paragraph, we might get
    //  an offset of 1 (after the fake space).
    else if( ip.m_pElement->GetLen() == 0 ){
        ip.m_iPos = 0;
    }
}

void CEditBuffer::SetInsertPoint( CEditLeafElement* pElement, int iOffset, XP_Bool bStickyAfter ){
#ifdef LAYERS
    LO_StartSelectionFromElement( m_pContext, 0, 0, NULL );
#else
    LO_StartSelectionFromElement( m_pContext, 0, 0);
#endif
    m_pCurrent = pElement;
    m_iCurrentOffset = iOffset;
    m_bCurrentStickyAfter = bStickyAfter;
    XP_ASSERT(m_bCurrentStickyAfter == TRUE || m_bCurrentStickyAfter == FALSE);
    SetCaret();
}

//
// Returns true if the insert point doesn't really exist.
//
XP_Bool CEditBuffer::IsPhantomInsertPoint(){
    if( !IsSelected() ) {
        CEditInsertPoint ip(m_pCurrent, m_iCurrentOffset, m_bCurrentStickyAfter);
        return IsPhantomInsertPoint(ip);
    }
    else {
        return FALSE;
    }
}

XP_Bool CEditBuffer::IsPhantomInsertPoint(CEditInsertPoint& ip){
    if(  ip.m_pElement
            && ip.m_pElement->IsA( P_TEXT )
            && ip.m_pElement->Text()->GetLen() == 0
            && !( ip.m_pElement->IsFirstInContainer()
                    && ip.m_pElement->LeafInContainerAfter() == 0 )
                ){
        return TRUE;
    }
    else {
        return FALSE;
    }
}

//
// Force the insertpoint to be a real insert point so we can do something.
//
void CEditBuffer::ClearPhantomInsertPoint(){
    if( IsPhantomInsertPoint() ){
        CEditLeafElement *pPrev = m_pCurrent->PreviousLeafInContainer();
        if( pPrev ){
            m_pCurrent = pPrev;
            m_iCurrentOffset = m_pCurrent->GetLen();
            m_bCurrentStickyAfter = FALSE;
        }
        else {
            CEditLeafElement *pNext = m_pCurrent->LeafInContainerAfter();
            if( pNext ){
                m_pCurrent = pNext;
                m_iCurrentOffset = 0;
                m_bCurrentStickyAfter = FALSE;
           }
            else {
                XP_ASSERT(FALSE);
            }
        }
        Reduce( m_pCurrent->FindContainer() );
    }
}

XP_Bool CEditBuffer::GetDirtyFlag(){
    return GetCommandLog()->IsDirty();
}

void CEditBuffer::DocumentStored(){
    DoneTyping();
    GetCommandLog()->DocumentStored();
}

CEditElement* CEditBuffer::FindRelayoutStart( CEditElement *pStartElement ){
    CEditElement* pOldElement = NULL;
    while ( pStartElement && pStartElement != pOldElement ) {
        pOldElement = pStartElement;
        CEditElement* pTable = pStartElement->GetTopmostTableOrLayer();
        if ( m_bDisplayTables && pTable ) {
            // If this is in a table, skip before it.
            pStartElement = pTable->PreviousLeaf();
        }
        else if( !pStartElement->IsLeaf() ){
            pStartElement = pStartElement->PreviousLeaf();
        }
        else if( pStartElement->Leaf()->GetLayoutElement() == 0 ){
           pStartElement = pStartElement->PreviousLeaf();
        }
    }
    return pStartElement;
}

static TXP_GrowableArray_LO_TableStruct edt_RelayoutTables;

// Add to current list of tables being relayed out
void EDT_AddToRelayoutTables(MWContext * /*pMWContext*/, LO_TableStruct *pLoTable )
{
    if(pLoTable)
    {
        edt_RelayoutTables.Add(pLoTable);
    }
}

void EDT_FixupTableData(MWContext *pMWContext)
{
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pMWContext, pEditBuffer);
    pEditBuffer->FixupTableData();
}

// Change Table and all cell size data to match
//  sizes calculated by Layout. Must do for all tables during Relayout()
//  else generated HTML is very misleading!
void CEditBuffer::FixupTableData()
{
//    return;
    for( int i = 0; i < edt_RelayoutTables.Size(); i++ )
    {
        LO_TableStruct *pLoTable = edt_RelayoutTables[i];
        LO_Element     *pLoTableElement = (LO_Element*)pLoTable;

        // Currently, embeded tables will cause reallocation during layout
        //   that end up reusing pointers, thus some table pointers
        //   may not be valid. Skip over them.
        // TODO: Figure out how to remove re-allocated tables from edt_RelayoutTables
        if( pLoTable->type != LO_TABLE )
            continue;

        CEditTableElement *pEdTable =
            (CEditTableElement*)edt_GetTableElementFromLO_Element(pLoTableElement, LO_TABLE );
        if(!pEdTable ) 
            continue;

        EDT_TableData *pTableData = pEdTable->GetData();
        if(!pTableData)
            return;

        // Get space between cells
        pTableData->iCellSpacing = pLoTable->inter_cell_space;

//XP_TRACE(("Fixup Table: Old iWidth = %d, iWidthPixels = %d; New iWidthPixels = %d", pTableData->iWidth, pTableData->iWidthPixels, pLoTable->width));

    	int32 iMaxWidth;
        int32 iMaxHeight;
        pEdTable->GetParentSize(m_pContext, &iMaxWidth, &iMaxHeight, pLoTable);

        // Save correct width even if bWidthDefined is FALSE
        //   (NOTE: width will NOT be save in m_pTagData if bWidthDefined == FALSE)
        pTableData->iWidthPixels = pLoTable->width;
        if( pTableData->bWidthPercent )
        {
            pTableData->iWidth = (pTableData->iWidthPixels * 100) / iMaxWidth;
        } else {
            pTableData->iWidth = pTableData->iWidthPixels;
        }

        pTableData->iHeightPixels = pLoTable->height;
        if( pTableData->bHeightPercent )
        {
            pTableData->iHeight = (pTableData->iHeightPixels * 100) / iMaxHeight;
        } else {
            pTableData->iHeight = pTableData->iHeightPixels;
        }
        // Get starting layout cell - usually first cell after table element
        LO_Element *pLoCell = pLoTableElement->lo_any.next;
        
        // Skip over non-cells
        while( pLoCell && pLoCell->type != LO_CELL )
            pLoCell = pLoCell->lo_any.next;
        
        // Get space between cell border and contents 
        // ASSUMES ALL CELLS THE SAME (Need to revisit if CSS ver.2 is implemented)
        pTableData->iCellPadding = lo_GetCellPadding(pLoCell);

        CEditTableCellElement *pEdCell;

        // This is dependable even if row and cell indexes are not set for cells
        int32 iRows = pEdTable->CountRows();
        int32 iArraySize = iRows * sizeof(int32);
        // Stores extra columns generated by ROWSPAN > 1 
        int32 *ExtraColumns = (int32*)XP_ALLOC(iArraySize);
        if( !ExtraColumns )
        {
            return;
        }
        XP_MEMSET( ExtraColumns, 0, iArraySize );

        // This may actually be a CaptionElement,
        //  but we will test for rows below
        CEditElement *pRow = pEdTable->GetChild();
        intn iRow = 0;

        // Clear existing layout data
        pEdTable->DeleteLayoutData();

        while( pRow )
        {
            // A Caption element may be a child, so test type first
            if( pRow->IsTableRow() )
            {
                pEdCell = (CEditTableCellElement*)(pRow->GetChild());

                // We will count number of actual cell locations in each row
                // Start with extra columns caused by ROWSPAN in previous rows
                int32 iColumnsInRow = ExtraColumns[iRow];

                while( pEdCell )
                {
                    intn iColSpan = pEdCell->GetColSpan();
                    intn iRowSpan = pEdCell->GetRowSpan();
                    iColumnsInRow += iColSpan;
                    
    #ifdef DEBUG
                    // Test if list scan is in sync between layout and editor objects        
                    LO_Element *pNextCell = (LO_Element*)(pEdCell->GetLoCell());
                    if( !pLoCell || pLoCell !=  pNextCell)
                    {
                        XP_TRACE(("**** pNextCell (%d) is not correct", pLoCell));
                    }
                    else
    #endif                
                    {
                        // If current cell has extra ROWSPAN,
                        //  then it will cause extra columns in following row(s)
                        if( iRowSpan > 1 )
                        {
                            for( intn j = 1; j < iRowSpan; j++ )
                            {
                                // We may overrun our array if table is "bad"
                                //   because of a ROWSPAN value that exceeds actual
                                //   number of rows. Just skip attempts to access a value too high
                                //   since there is no row to use the "ExtraColumns" anyway.
                                if( iRow+j < iRows )
                                    ExtraColumns[iRow+j] += iColSpan;
                            }
                        }
                        //  Save actual location and size data
                        EDT_TableCellData *pCellData = pEdCell->GetData();
                        if( pCellData )
                        {
                            pCellData->X = pLoCell->lo_any.x;
                            pCellData->Y = pLoCell->lo_any.y;
                            pCellData->iRow = iRow;

                            // The LO_Element's concept of cell "width" INCLUDES
                            //   the border and cell padding, but the HTML tag value excludes these,
                            //   so we must compensate here.
                            pCellData->iWidthPixels = lo_GetCellTagWidth(pLoCell);
                            pCellData->iHeightPixels = lo_GetCellTagHeight(pLoCell);

                            if( pCellData->bWidthPercent )
                            {
                                // Use "full" width of cell for % calculation
                                pCellData->iWidth = (pLoCell->lo_any.width * 100) / iMaxWidth;
                            } else {
                                pCellData->iWidth = pCellData->iWidthPixels;
                            }

                            if( pCellData->bHeightPercent )
                            {
                                pCellData->iHeight = (pLoCell->lo_any.height * 100) / iMaxHeight;
                            } else {
                                pCellData->iHeight = pCellData->iHeightPixels;
                            }
                            pEdCell->SetData(pCellData);
                            EDT_FreeTableCellData(pCellData);
                        }
                    }
                    // Add this cell to Column and Row layout data
                    // Note: Can do only after setting size params above
                    pEdTable->AddLayoutData(pEdCell, pLoCell);

                    CEditTableCellElement *pNextEdCell = (CEditTableCellElement*)(pEdCell->GetNextSibling());
                
                    // Next cell in row 
                    //  (or signal to get next row if NULL)
                    pEdCell = pNextEdCell;

                    // If cell scanning is in sync
                    //   this should be much quicker than searching for 
                    //   a LO_Element from each CEditElement or vice versa
                    if( pLoCell )
                    {
                        pLoCell = pLoCell->lo_any.next;
                        // Skip over non-cells
                        while( pLoCell && pLoCell->type != LO_CELL )
                            pLoCell = pLoCell->lo_any.next;
                    }
                }
                // Save the column count in Row
                pRow->TableRow()->SetColumns(iColumnsInRow);

                // PLEASE!!! I hope this rule holds: "We can never have an empty row"
                iRow++;
            }
            pRow = pRow->GetNextSibling();
        }
        // Safety check
        XP_ASSERT(iRow == iRows);

        // Now we know maximum number of columns to set in table
        pTableData->iColumns = pEdTable->m_ColumnLayoutData.Size();
        pTableData->iRows = iRows;
        pEdTable->SetData(pTableData);
        EDT_FreeTableData(pTableData);

        XP_FREE(ExtraColumns);
    }

    // We need to do this just once, so clear the list
    edt_RelayoutTables.Empty();
}

//
// ReflowFromElement
//
// This attempts to reflow layout elements from a given edit element. The idea is to not
// have to return to the tags for basic edit operations. Text typing is currently the main
// client for this.
//
// We do not handle typing inside tables (yet).
//
// There will be some duplication of code/functionality with Relayout.
//
void CEditBuffer::Reflow( CEditElement* pStartElement,
                            int iEditOffset,
                            CEditElement *pEndElement,
                            intn relayoutFlags ){

// Use this to skip new layout (when things go wrong!)
//    Relayout( pStartElement, iEditOffset, pEndElement, relayoutFlags );
//    return;

    CEditElement *pEdStart, *pNewStartElement;
    LO_Element *pLayoutElement;
    LO_Element *pLoStartLine;
    int iOffset;
    int32 iLineNum;

    if( m_bNoRelayout ){
        return;
    }

#if defined( DEBUG_shannon )
	// do we want to force calls through to the old relayout?
	if ( !gEditorReflow ) {
        Relayout( pStartElement, iEditOffset, pEndElement, relayoutFlags );
        return;
	}
#endif

	// if the start element can't reflow, then do a relayout
	if ( ( pStartElement != NULL ) && !pStartElement->CanReflow() ) {
        Relayout( pStartElement, iEditOffset, pEndElement, relayoutFlags );
        return;
	}
	
    // Clear the list of tables created during layout
    // This will be rebuilt as each table is encountered
    edt_RelayoutTables.Empty();

    CEditLeafElement *pBegin, *pEnd;
    ElementOffset iBeginPos, iEndPos;
    XP_Bool bFromStart;
    XP_Bool bWasSelected = IsSelected();

    if( bWasSelected )
    {
        GetSelection( pBegin, iBeginPos, pEnd, iEndPos, bFromStart );
#ifdef LAYERS
        LO_StartSelectionFromElement( m_pContext, 0, 0, NULL );
#else
        LO_StartSelectionFromElement( m_pContext, 0, 0);
#endif
    }

    pNewStartElement = FindRelayoutStart(pStartElement);
    if( pNewStartElement && pNewStartElement != pStartElement ){
        // we had to back up some.  Layout until we pass this point.
        if( pEndElement == 0 ){
            pEndElement = pStartElement;
        }
        pStartElement = pNewStartElement;
        iEditOffset = pStartElement->Leaf()->GetLen();
    }

    // If the end is in a table, move it outside of the table.
    if ( pEndElement ) {
        CEditElement* pTable = pEndElement->GetTopmostTableOrLayer();
        if ( m_bDisplayTables && pTable) {
            // If this is in a table, skip after it.
            pEndElement = pTable->GetLastMostChild()->NextLeaf();
        }
    }

    // laying out from the beginning of the document
    if( pNewStartElement == 0 ){
        if( pEndElement == 0 ){
            pEndElement = pStartElement;
        }
        iLineNum = 0;
        pEdStart = m_pRoot;
        iOffset = 0;
    }
    else {
        //
        // normal case
        //
        if( pStartElement->IsA(P_TEXT)){
            pLayoutElement = (LO_Element*)pStartElement->Text()->GetLOText( iEditOffset );
        }
        else {
            pLayoutElement = pStartElement->Leaf()->GetLayoutElement();
        }
        if( pLayoutElement == 0 ){
            // since we can't find the start element, we have to go back to the tags.
            // BRAIN DAMAGE: Is this really true - shouldn't we just be able to reflow
            // from here instead?
            XP_TRACE(("Cannot find start element - relayout from tags"));
            Relayout( pStartElement->FindContainer(), 0, pEndElement ?
                        pEndElement : pStartElement, relayoutFlags  );
            return;
        }
        //
        // Find the first element on this line.
        pLoStartLine = FirstElementOnLine( pLayoutElement, &iLineNum );

        //
        // Position the tag cursor at this position
        //
        pEdStart = pLoStartLine->lo_any.edit_element;
        iOffset = pLoStartLine->lo_any.edit_offset;
    }


    // Create a new cursor.
    CEditTagCursor cursor(this, pEdStart, iOffset, pEndElement);
    //CEditTagCursor *pCursor = new CEditTagCursor(this, m_pRoot, iOffset);

    LO_EditorReflow(m_pContext, &cursor, iLineNum, iOffset, m_bDisplayTables);


#if defined( DEBUG_shannon )
    XP_TRACE(("\n\nEDITOR REFLOW"));
	lo_PrintLayout(m_pContext);
#endif

    // Search for zero-length text elements, and remove the non-breaking spaces we put
    // in.

    CEditElement* pElement= m_pRoot;
    while ( NULL != (pElement = pElement->FindNextElement(&CEditElement::FindLeafAll,0))){
        switch ( pElement->GetElementType() ) {
        case eTextElement:
            {
				/* Am I correct that we don't need to do this? */
                CEditTextElement* pText = pElement->Text();
                intn iLen = pText->GetLen();
                if ( iLen == 0 ) {
                    LO_Element* pTextStruct = pText->GetLayoutElement();
                    if ( pTextStruct && pTextStruct->type == LO_TEXT
                            && pTextStruct->lo_text.text_len == 1
                            && pTextStruct->lo_text.text ) {
                        XP_ASSERT( pElement->Leaf()->PreviousLeafInContainer() == 0 );
                        XP_ASSERT( pElement->Leaf()->LeafInContainerAfter() == 0 );
                        // Need to strip the allocated text, because lo_bump_position cares.
                        pTextStruct->lo_text.text_len = 0;
                    }
                }
            }
            break;
        case eBreakElement:
            {
                CEditBreakElement* pBreak = pElement->Break();
                LO_Element* pBreakStruct = pBreak->GetLayoutElement();
                if ( pBreakStruct && pBreakStruct->type == LO_LINEFEED) {
                    // Need to strip the allocated text, because lo_bump_position cares.
                    pBreakStruct->lo_linefeed.break_type = LO_LINEFEED_BREAK_HARD;
                    pBreakStruct->lo_linefeed.edit_element = pElement;
                    pBreakStruct->lo_linefeed.edit_offset = 0;
                }
            }
            break;
       case eHorizRuleElement:
             {
                // The linefeeds after hrules need to be widened.
                // They are zero pixels wide by default.
                CEditHorizRuleElement* pHorizRule = (CEditHorizRuleElement*) pElement;
                LO_Element* pHRuleElement = pHorizRule->GetLayoutElement();
                if ( pHRuleElement ) {
                    LO_Element* pNext = pHRuleElement->lo_any.next;
                    if ( pNext && pNext->type == LO_LINEFEED) {
                        const int32 kMinimumWidth = 7;
                        if (pNext->lo_linefeed.width < kMinimumWidth ) {
                             pNext->lo_linefeed.width = kMinimumWidth;
                        }
                    }
                }
            }
            break;
      default:
            break;
        }
        // Set end-of-paragraph marks
        if ( pElement->GetNextSibling() == 0 ){
            // We're the last leaf in the container.
            CEditLeafElement* pLeaf = pElement->Leaf();
            LO_Element* pLastElement;
            int iOffset;
            if ( pLeaf->GetLOElementAndOffset(pLeaf->GetLen(), TRUE,
                pLastElement, iOffset) ){
                LO_Element* pNextElement = pLastElement ? pLastElement->lo_any.next : 0;
                if ( pNextElement && pNextElement->type == LO_LINEFEED ) {
                    pNextElement->lo_linefeed.break_type = LO_LINEFEED_BREAK_PARAGRAPH;
                }
            }
            else {
                // Last leaf, but we're empty. Yay. Try the previous leaf.
                CEditElement* pPrevious = pElement->GetPreviousSibling();
                if ( pPrevious ) {
                    CEditLeafElement* pLeaf = pPrevious->Leaf();
                    LO_Element* pLastElement;
                    int iOffset;
                    if ( pLeaf->GetLOElementAndOffset(pLeaf->GetLen(), TRUE,
                        pLastElement, iOffset) ){
                        LO_Element* pNextElement = pLastElement ? pLastElement->lo_any.next : 0;
                        if ( pNextElement && pNextElement->type == LO_LINEFEED ) {
                            pNextElement->lo_linefeed.break_type = LO_LINEFEED_BREAK_PARAGRAPH;
                        }
                    }
                }
            }
        }
    }
    if( (relayoutFlags & RELAYOUT_NOCARET) == 0 && !bWasSelected){
        SetCaret();
    }

    if( bWasSelected ){
        SelectRegion(pBegin, iBeginPos, pEnd, iEndPos, bFromStart );
    }

    m_bLayoutBackpointersDirty = FALSE;
    
    // For each table just layed out, readjust all table and cell width data
    //   to reflect the complicated Layout algorithm's size data
    FixupTableData();

#if defined(XP_WIN) || defined(XP_OS2)
    // This clears FE pointers to cached layout elements
    FE_FinishedRelayout(m_pContext);
#endif
}

//
// Relayout.
//
void CEditBuffer::Relayout( CEditElement* pStartElement,
                            int iEditOffset,
                            CEditElement *pEndElement,
                            intn relayoutFlags ){
    CEditElement *pEdStart, *pNewStartElement;
    CEditElement *pContainer;
    LO_Element *pLayoutElement;
    LO_Element *pLoStartLine;
    int iOffset;
    int32 iLineNum;

    if( m_bNoRelayout ){
        return;
    }
	
   // These elements will be destroyed during LO_Relayout,
    //  so remove the list -- reconstructed after laying out
    m_pSelectedLoTable = NULL;
    m_SelectedLoCells.Empty();
        
    // Clear the list of tables
    // This will be rebuilt as each table is encountered
    edt_RelayoutTables.Empty();

    CEditLeafElement *pBegin, *pEnd;
    ElementOffset iBeginPos, iEndPos;
    XP_Bool bFromStart;
    XP_Bool bWasSelected = IsSelected();

    if( bWasSelected )
    {
        GetSelection( pBegin, iBeginPos, pEnd, iEndPos, bFromStart );
#ifdef LAYERS
        LO_StartSelectionFromElement( m_pContext, 0, 0, NULL );
#else
        LO_StartSelectionFromElement( m_pContext, 0, 0);
#endif
    }
    
    // we actually may need to go back more to the container as we need to layout whole text elements in relayout
    if ( pStartElement ){
		pContainer = pStartElement->FindContainer();
		if ( pContainer ){
			pStartElement = pContainer;
			iEditOffset = 0;
		}
	}

    pNewStartElement = FindRelayoutStart(pStartElement);
    if( pNewStartElement && pNewStartElement != pStartElement ){
    
    	// we really want to get the first element of this container
		pContainer = pNewStartElement->FindContainer();
		if ( pContainer ){
			pNewStartElement = pContainer->GetChild();
		}
		
        // we had to back up some.  Layout until we pass this point.
        if( pEndElement == 0 ){
            pEndElement = pStartElement;
        }
        pStartElement = pNewStartElement;
        iEditOffset = 0;
    }

    // If the end is in a table, move it outside of the table.
    if ( pEndElement ) {
        CEditElement* pTable = pEndElement->GetTopmostTableOrLayer();
        if ( m_bDisplayTables && pTable) {
            // If this is in a table, skip after it.
            pEndElement = pTable->GetLastMostChild()->NextLeaf();
        }
    }

    // laying out from the beginning of the document
    if( pNewStartElement == 0 ){
        if( pEndElement == 0 ){
            pEndElement = pStartElement;
        }
        iLineNum = 0;
        pEdStart = m_pRoot;
        iOffset = 0;
    }
    else {
        //
        // normal case
        //
		// we always go to the leaf's layout element in relayout
		pLayoutElement = pStartElement->Leaf()->GetLayoutElement();

        if( pLayoutElement == 0 ){
            // we are sunk! try something different.
            XP_TRACE(("Yellow Alert! Can't resync, plan B"));
            Relayout( pStartElement->FindContainer(), 0, pEndElement ?
                        pEndElement : pStartElement, relayoutFlags  );
            return;
        }
        //
        // Find the first element on this line.
        pLoStartLine = FirstElementOnLine( pLayoutElement, &iLineNum );

        //
        // Position the tag cursor at this position
        //
        pEdStart = pLoStartLine->lo_any.edit_element;
        iOffset = pLoStartLine->lo_any.edit_offset;
    }


    // Create a new cursor.
    CEditTagCursor cursor(this, pEdStart, iOffset, pEndElement);
    //CEditTagCursor *pCursor = new CEditTagCursor(this, m_pRoot, iOffset);

    LO_Relayout(m_pContext, &cursor, iLineNum, iOffset, m_bDisplayTables);

#if defined( DEBUG_shannon )
    XP_TRACE(("\n\nEDITOR RELAYOUT"));
	lo_PrintLayout(m_pContext);
#endif

    // Search for zero-length text elements, and remove the non-breaking spaces we put
    // in.

    CEditElement* pElement= m_pRoot;
    while ( NULL != (pElement = pElement->FindNextElement(&CEditElement::FindLeafAll,0))){
        switch ( pElement->GetElementType() ) {
        case eTextElement:
            {
				/* Am I correct that we don't need to do this? */
                CEditTextElement* pText = pElement->Text();
                intn iLen = pText->GetLen();
                if ( iLen == 0 ) {
                    LO_Element* pTextStruct = pText->GetLayoutElement();
                    if ( pTextStruct && pTextStruct->type == LO_TEXT
                            && pTextStruct->lo_text.text_len == 1
                            && pTextStruct->lo_text.text ) {
                        XP_ASSERT( pElement->Leaf()->PreviousLeafInContainer() == 0 );
                        XP_ASSERT( pElement->Leaf()->LeafInContainerAfter() == 0 );
                        // Need to strip the allocated text, because lo_bump_position cares.
                        pTextStruct->lo_text.text_len = 0;
                    }
                }
#ifdef OLDWAY
                else {
                    // Layout stripped out all the spaces that crossed soft line breaks.
                    // Put them back in so they can be selected.
                    LO_Element* pTextStruct = pText->GetLayoutElement();
                    while ( pTextStruct && pTextStruct->type == LO_TEXT
                        && pTextStruct->lo_any.edit_element == pText ) {
                        intn iOffset = pTextStruct->lo_any.edit_offset + pTextStruct->lo_text.text_len;
                        if ( iOffset >= iLen )
                            break;
                        LO_Element* pNext = pTextStruct->lo_any.next;
                        while ( pNext && pNext->type != LO_TEXT) {
                            pNext = pNext->lo_any.next;
                        }
                        intn bytesToCopy = iLen - iOffset;
                        if ( pNext && pNext->lo_any.edit_element == pText ){
                           bytesToCopy =  pNext->lo_text.edit_offset - iOffset;
                        }
                        if ( bytesToCopy > 0 ) {
                            // Layout has stripped some characters.
                            // (Probably just a single space character.)
                            // Put them back.
                            int16 length = pTextStruct->lo_text.text_len;
                            int16 newLength = (int16) (length + bytesToCopy + 1); // +1 for '\0'
                            PA_Block newData;
                            if (pTextStruct->lo_text.text) {
                                newData  = (PA_Block) PA_REALLOC(pTextStruct->lo_text.text, newLength);
                            }
                            else {
                                newData = (PA_Block) PA_ALLOC(newLength);
                            }
                            if ( ! newData ) {
                                XP_ASSERT(FALSE); /* Out of memory. Not well tested. */
                                break;
                            }
                            pTextStruct->lo_text.text = newData;
                            pTextStruct->lo_text.text_len = (int16) (newLength - 1);
                            char *locked_buff;
                            PA_LOCK(locked_buff, char *, pTextStruct->lo_text.text);
                            if ( locked_buff ) {
                                char* source = pText->GetText();
                                for ( intn i = 0; i < bytesToCopy; i++ ){
                                    locked_buff[length + i] = source[iOffset+ i];
                                }
                                locked_buff[length + bytesToCopy] = '\0';
                            }
                            PA_UNLOCK(pNext->lo_text.text);
                            // The string is now wider. We must adjust the width.
                            LO_TextInfo text_info;
                            FE_GetTextInfo(m_pContext, &pTextStruct->lo_text, &text_info);
                            int32 delta = text_info.max_width - pTextStruct->lo_text.width;
                            pTextStruct->lo_text.width = text_info.max_width;
                            // Shrink the linefeed
                            LO_Element* pNext = pTextStruct->lo_any.next;
                            if ( pNext && pNext->type == LO_LINEFEED ) {
                                if ( pNext->lo_linefeed.width > delta ) {
                                    pNext->lo_linefeed.width -= delta;
                                    pNext->lo_linefeed.x += delta;
                                }
                            }
                        }
                        pTextStruct = pNext;
                    }
                }
#endif
            }
            break;
        case eBreakElement:
            {
                CEditBreakElement* pBreak = pElement->Break();
                LO_Element* pBreakStruct = pBreak->GetLayoutElement();
                if ( pBreakStruct && pBreakStruct->type == LO_LINEFEED) {
                    // Need to strip the allocated text, because lo_bump_position cares.
                    pBreakStruct->lo_linefeed.break_type = LO_LINEFEED_BREAK_HARD;
                    pBreakStruct->lo_linefeed.edit_element = pElement;
                    pBreakStruct->lo_linefeed.edit_offset = 0;
                }
            }
            break;
       case eHorizRuleElement:
             {
                // The linefeeds after hrules need to be widened.
                // They are zero pixels wide by default.
                CEditHorizRuleElement* pHorizRule = (CEditHorizRuleElement*) pElement;
                LO_Element* pHRuleElement = pHorizRule->GetLayoutElement();
                if ( pHRuleElement ) {
                    LO_Element* pNext = pHRuleElement->lo_any.next;
                    if ( pNext && pNext->type == LO_LINEFEED) {
                        const int32 kMinimumWidth = 7;
                        if (pNext->lo_linefeed.width < kMinimumWidth ) {
                             pNext->lo_linefeed.width = kMinimumWidth;
                        }
                    }
                }
            }
            break;
      default:
            break;
        }
        // Set end-of-paragraph marks
        if ( pElement->GetNextSibling() == 0 ){
            // We're the last leaf in the container.
            CEditLeafElement* pLeaf = pElement->Leaf();
            LO_Element* pLastElement;
            int iOffset;
            if ( pLeaf->GetLOElementAndOffset(pLeaf->GetLen(), TRUE,
                pLastElement, iOffset) ){
                LO_Element* pNextElement = pLastElement ? pLastElement->lo_any.next : 0;
                if ( pNextElement && pNextElement->type == LO_LINEFEED ) {
                    pNextElement->lo_linefeed.break_type = LO_LINEFEED_BREAK_PARAGRAPH;
                }
            }
            else {
                // Last leaf, but we're empty. Yay. Try the previous leaf.
                CEditElement* pPrevious = pElement->GetPreviousSibling();
                if ( pPrevious ) {
                    CEditLeafElement* pLeaf = pPrevious->Leaf();
                    LO_Element* pLastElement;
                    int iOffset;
                    if ( pLeaf->GetLOElementAndOffset(pLeaf->GetLen(), TRUE,
                        pLastElement, iOffset) ){
                        LO_Element* pNextElement = pLastElement ? pLastElement->lo_any.next : 0;
                        if ( pNextElement && pNextElement->type == LO_LINEFEED ) {
                            pNextElement->lo_linefeed.break_type = LO_LINEFEED_BREAK_PARAGRAPH;
                        }
                    }
                }
            }
        }
    }
    if( (relayoutFlags & RELAYOUT_NOCARET) == 0 && !bWasSelected){
        SetCaret();
    }

    if( bWasSelected ){
        SelectRegion(pBegin, iBeginPos, pEnd, iEndPos, bFromStart );
    }

    m_bLayoutBackpointersDirty = FALSE;
    
    // For each table just layed out, readjust all table and cell width data
    //   to reflect the complicated Layout algorithm's size data
    // Moved ABOVE next block (was below)
    FixupTableData();

    //XP_TRACE(("*** Resync Table Selection"));
    // Resynch table selection
    // TODO -- NOT NEEDED AFTER CHANGE SO LAYOUT ELEMENTS AREN'T DESTROYED
    if( m_pSelectedEdTable )
    {
        m_pSelectedLoTable = m_pSelectedEdTable->GetLoTable();
        SelectTable(TRUE, m_pSelectedLoTable, m_pSelectedEdTable);
    } else {
        intn iEdCount = m_SelectedEdCells.Size();
        for( int i = 0; i < iEdCount; i++ )
        {
            // Get something we can observe in debugger
            // CURRENTLY CRASHING HERE DURING DELETE SELECTED CELLS
            CEditTableCellElement *pEdCell = m_SelectedEdCells[i];
            // Get the new LO_CellStruct matching the previously-selected edit element
            LO_CellStruct *pLoCell = pEdCell->GetLoCell();
            // Resync the list and display the cell
            SelectCell(TRUE, pLoCell, pEdCell);
        }
    }

#if defined(XP_WIN) || defined(XP_OS2)
    // This clears FE pointers to cached layout elements
    FE_FinishedRelayout(m_pContext);
#endif
}

// Call the next two funcions instead of Relayout( pTable )
//  when we want to relayout entire table because we are changing
//  either the table size or cell size(s)
// These set the correct "table size mode", call Relayout,
//  then RestoreSizeMode() so the resize actually
//  happens. If we do not, fixed cell sizes can defeat attempts
//  to resize the table and vice versa.
// bChangeWidth and bChangeHeight tell us which dimension is changing

void CEditBuffer::ResizeTable(CEditTableElement *pTable, XP_Bool bChangeWidth, XP_Bool bChangeHeight)
{
    if( !pTable )
        return;

    // Convert all the cell's values to PERCENT mode
    // This is the best attempt we can make to keep current column
    //  and row dimensions. If we use Pixels, we cannot reduce
    //  the size of the table since layout uses max(Table-WIDTH, sum-of-Cell-WIDTHS)
    //  Also, percent mode results in less distortion in size
    //  of individual columns than pixel mode when increasing the width or height of table
    //  WE NEED EQUIVALENT OF CSS2's "FIXED" TABLE LAYOUT MODE TO FREEZE COL AND ROW SIZES!
    //  (See note in ResizeTableCell about column span problem
    // DON'T USE COLS MODE! TOO MESSED UP FOR COMPLICATED TABLES (COLSPAN and ROWSPAN > 1)
    int iMode = ED_MODE_CELL_PERCENT | ED_MODE_NO_COLS; //((pTable->FirstRowHasColSpan() ? ED_MODE_NO_COLS : ED_MODE_USE_COLS));
    
    if( bChangeWidth )
        iMode |= ED_MODE_USE_TABLE_WIDTH;

    if( bChangeHeight )
        iMode |= ED_MODE_USE_TABLE_HEIGHT;

    // Set size params to the above modes but saves current settings first
    pTable->SetSizeMode(m_pContext, iMode);
    Relayout(pTable, 0, pTable);
    // Restore to the size mode settings saved in SetSizeMode
    pTable->RestoreSizeMode(m_pContext);
}

void CEditBuffer::ResizeTableCell(CEditTableElement  *pTable, XP_Bool bChangeWidth, XP_Bool bChangeHeight)
{
    if( !pTable )
        return;

    // Set all cell sizes to pixel mode so each col or row are equally 
    //   treated by layout. This makes sure all the change is just in the desired col or row
    // NOTE: If COLS mode is used, but 1st row has any cells with COLSPAN>1,
    //       the column widths "under" each spanned cell will allways be
    //       apportioned equally. Thus, in general, NOT using COLS results
    //       in more deterministic column sizing.
    //       Lets try to be smart by testing if any cell in 1st row has colspan
    //       and use COLS if it doesn't
    // PROBLEM: When some cells have COLSPAN (not necessarily the first row),
    //          using COLS prevents the last column to be resized 
    //          (it seems to have a lower limit = width of column to the left)
    //          TODO: Lets not use COLS until this is fixed
    int iMode = ED_MODE_CELL_PIXELS | ED_MODE_NO_COLS; // | (pTable->FirstRowHasColSpan() ? ED_MODE_NO_COLS : ED_MODE_USE_COLS);

    // Change param only for the dimension being resized
    // Also turn OFF size param in table to remove its influence
    //   on laying out the table.
    if( bChangeWidth )
        iMode |= (ED_MODE_USE_CELL_WIDTH | ED_MODE_NO_TABLE_WIDTH);
    if( bChangeHeight )
        iMode |= (ED_MODE_USE_CELL_HEIGHT  | ED_MODE_NO_TABLE_HEIGHT);

    pTable->SetSizeMode(m_pContext, iMode);
    Relayout(pTable, 0, pTable);
    pTable->RestoreSizeMode(m_pContext);
}

// Relayout selected table or parent table if any cells are selected
void CEditBuffer::RelayoutSelectedTable()
{
    // Should be cleared, but set just to be sure
    m_bNoRelayout = FALSE;
    CEditTableElement *pTable = NULL;
    if( m_pSelectedEdTable )
    {
        pTable = m_pSelectedEdTable;
    }
    else if( m_SelectedEdCells.Size() )
    {
        pTable = m_SelectedEdCells[0]->GetParentTable();
    }

    if( pTable )
        Relayout(pTable, 0, pTable);
}


//
// Insert a character at the current edit point
//
EDT_ClipboardResult CEditBuffer::InsertChar( int newChar, XP_Bool bTyping ){
    char buffer[2];
    buffer[0] = (char) newChar;
    buffer[1] = '\0';
    return InsertChars(buffer, bTyping, TRUE);
}

EDT_ClipboardResult CEditBuffer::InsertChars( char* pNewChars, XP_Bool bTyping, XP_Bool bReduce){

#ifdef DEBUG
    if ( bTyping ) {
        for(char* pChar = pNewChars; *pChar; pChar++ ) {
            if ( m_pTestManager->Key(*pChar) )
                return EDT_COP_OK;
        }
    }
#endif

    VALIDATE_TREE(this);
    EDT_ClipboardResult result = EDT_COP_OK;
    int iChangeOffset = m_iCurrentOffset;
    StartTyping(bTyping);

    if( IsSelected() ){
        result = DeleteSelection();
        if ( result != EDT_COP_OK ) return result;
    }
    //ClearSelection();

    ClearMove(FALSE);
    if(!IsPhantomInsertPoint() ){
        FixupInsertPoint();
    }
    if( m_pCurrent->IsA( P_TEXT )){
        // If this assert ever fails, it means that we've
        // got to uncomment this and make it work.
        XP_ASSERT(IsMultiSpaceMode()); 
#if 0
        // Check for space after space case.
        if ( newChar == ' ' && !m_pCurrent->InFormattedText()
                && !IsMultiSpaceMode() ) {
            CEditInsertPoint p(m_pCurrent, m_iCurrentOffset);
            // move to the right on spacebar if we are under a space.
            if( p.IsSpace() ){
                NextChar( FALSE );
                return result;
            }
            if ( p.IsSpaceBeforeOrAfter() ) {
                return result;
            }
        }
#endif
        //
        // The edit element can choose not to insert a character if it is
        //  a space and at the insertion point there already is a space.
        //
        int32 bytesInserted = m_pCurrent->Text()->InsertChars( m_iCurrentOffset, pNewChars );
        if( bytesInserted > 0 ){

            // move past the inserted characters
            m_iCurrentOffset += bytesInserted;
            // Reduce or die unless dont reduce
            if (bReduce)
            {
                Reduce(m_pCurrent->FindContainer());
                // relay out the stream
	    		Reflow(m_pCurrent, iChangeOffset);
            }
        }
    }
    else {
        if (bReduce)
        {
            Reduce( m_pCurrent->FindContainer());
        }
        if( m_iCurrentOffset == 0 ){
            // insert before leaf case.
            CEditElement *pPrev = m_pCurrent->GetPreviousSibling();
            if( pPrev == 0 || !pPrev->IsA( P_TEXT ) ){
                pPrev = new CEditTextElement((CEditElement*)0,0);
                pPrev->InsertBefore( m_pCurrent );
            }
            m_pCurrent = pPrev->Leaf();
            m_iCurrentOffset = m_pCurrent->Text()->GetLen();
        }
        else {
            XP_ASSERT( m_iCurrentOffset == 1 );
            // insert after leaf case
            CEditLeafElement *pNext = (CEditLeafElement*) m_pCurrent->GetNextSibling();
            if( pNext == 0 || !pNext->IsA( P_TEXT ) ){
                CEditTextElement* pPrev = m_pCurrent->PreviousTextInContainer();
                if( pPrev ){
                    pNext = pPrev->CopyEmptyText();
                }
                else {
                    pNext = new CEditTextElement((CEditElement*)0,0);
                }
                pNext->InsertAfter( m_pCurrent );
            }
            m_pCurrent = pNext;
            m_iCurrentOffset = 0;
        }
        // now we have a text.  Do the actual insert.
        int32 bytesInserted = m_pCurrent->Text()->InsertChars( m_iCurrentOffset, pNewChars );
        if ( bytesInserted > 0 ){
            m_iCurrentOffset += bytesInserted;
            if ( bTyping ) {
                m_relayoutTimer.Relayout(m_pCurrent, iChangeOffset);
            }
            else {
                Relayout(m_pCurrent, iChangeOffset);
            }
        }
    }
    return result;
}


EDT_ClipboardResult CEditBuffer::DeletePreviousChar(){
#ifdef DEBUG
    if ( m_pTestManager->Backspace() )
        return EDT_COP_OK;
#endif
    return DeleteChar(FALSE);
}

EDT_ClipboardResult CEditBuffer::DeleteNextChar()
{
    return DeleteChar(TRUE);
}

EDT_ClipboardResult CEditBuffer::DeleteChar(XP_Bool bForward, XP_Bool bTyping)
{
    VALIDATE_TREE(this);
    EDT_ClipboardResult result = EDT_COP_OK;
    if( IsTableOrCellSelected() )
    {
        DoneTyping();
        BeginBatchChanges(kGroupOfChangesCommandID);
        // We assume that table or cells are selected ONLY
        //  if there's not a "normal" selection
        if( m_pSelectedEdTable )
        {
            // Delete the entire table
            // Assumes current insert point is inside the selected table
    		AdoptAndDo(new CDeleteTableCommand(this));
        } else if( m_SelectedEdCells.Size() )
        {
            // Delete all selected cells
            // This will delete the cell elements ONLY
            // if a complete row or column is selected.
            // If not, only cell contents are cleared
            // to miminize messing up table layout
            DeleteSelectedCells();
        }
        EndBatchChanges();
    } 
    else 
    {
        ClearTableAndCellSelection();
        ClearPhantomInsertPoint();
        ClearMove();
        StartTyping(bTyping);
        if( IsSelected() ){
            result = DeleteSelection();
        }
        else {
            CEditSelection selection;
            GetSelection(selection);
            if ( Move(*selection.GetEdge(bForward), bForward ) ) {
                result = CanCut(selection, TRUE);
                if ( result == EDT_COP_OK ) {
                    DeleteSelection(selection, FALSE);
                }
            }
        }
    }
    return result;
}

XP_Bool CEditBuffer::Move(CEditInsertPoint& pt, XP_Bool forward) {
    CEditLeafElement* dummyNewElement = pt.m_pElement;
    ElementOffset dummyNewOffset = pt.m_iPos;
    XP_Bool result = FALSE;
    if ( forward )
        result = NextPosition(dummyNewElement, dummyNewOffset,
            dummyNewElement, dummyNewOffset);
    else
        result = PrevPosition(dummyNewElement, dummyNewOffset,
            dummyNewElement, dummyNewOffset);
    if ( result ) {
        pt.m_pElement = dummyNewElement;
        pt.m_iPos = dummyNewOffset;
    }
    return result;
}

XP_Bool CEditBuffer::CanMove(CEditInsertPoint& pt, XP_Bool forward) {
    CEditInsertPoint test(pt);
    return Move(test, forward);
}

XP_Bool CEditBuffer::CanMove(CPersistentEditInsertPoint& pt, XP_Bool forward) {
    CPersistentEditInsertPoint test(pt);
    return Move(test, forward);
}


XP_Bool CEditBuffer::Move(CPersistentEditInsertPoint& pt, XP_Bool forward) {
    CEditInsertPoint insertPoint = PersistentToEphemeral(pt);
    XP_Bool result = Move(insertPoint, forward);
    if ( result ) {
        pt = EphemeralToPersistent(insertPoint);
    }
    return result;
}

void CEditBuffer::SelectNextChar( ){
    CEditLeafElement *pBegin, *pEnd;
    ElementOffset iBeginPos, iEndPos;
    XP_Bool bFromStart;
    XP_Bool bFound;

    if( IsSelected() ){
        GetSelection( pBegin, iBeginPos, pEnd, iEndPos, bFromStart );
        if( bFromStart ){
            bFound = NextPosition( pBegin, iBeginPos, pBegin, iBeginPos );
        }
        else {
            bFound = NextPosition( pEnd, iEndPos, pEnd, iEndPos );
        }
        if( bFound ){
            SelectRegion( pBegin, iBeginPos, pEnd, iEndPos, bFromStart, TRUE  );
        }
    }
    else {
        BeginSelection( TRUE, FALSE );
    }
}


void CEditBuffer::SelectPreviousChar( ){
    if( IsSelected() ){
        CEditLeafElement *pBegin, *pEnd;
        ElementOffset iBeginPos, iEndPos;
        XP_Bool bFound = FALSE;
        XP_Bool bFromStart;
        GetSelection( pBegin, iBeginPos, pEnd, iEndPos, bFromStart );
        if( bFromStart ){
            bFound = PrevPosition( pBegin, iBeginPos, pBegin, iBeginPos );
        }
        else {
            bFound = PrevPosition( pEnd, iEndPos, pEnd, iEndPos );
        }
        if( bFound ){
            SelectRegion( pBegin, iBeginPos, pEnd, iEndPos, bFromStart, FALSE  );
        }
    }
    else {
         BeginSelection( TRUE, TRUE );
    }
}

XP_Bool CEditBuffer::PrevPosition(CEditLeafElement *pEle, ElementOffset iOffset,
                CEditLeafElement*& pNew, ElementOffset& iNewOffset ){

    LO_Element* pElement;
    int iLayoutOffset;
    int iNewLayoutOffset;

    CEditElement* pNewEditElement;
    if( pEle->GetLOElementAndOffset( iOffset, FALSE, pElement, iLayoutOffset ) ){
        XP_Bool result = LO_PreviousPosition( m_pContext, pElement, iLayoutOffset, &pNewEditElement, &iNewLayoutOffset );
        if ( result ){
            pNew = pNewEditElement->Leaf();
            iNewOffset = iNewLayoutOffset;
        }
        return result;
    }
    else {
        return pEle->PrevPosition( iOffset, pNew, iNewOffset );
    }
}

XP_Bool CEditBuffer::NextPosition(CEditLeafElement *pEle, ElementOffset iOffset,
                CEditLeafElement*& pNew, ElementOffset& iNewOffset ){

    LO_Element* pElement;
    int iLayoutOffset;
    int iNewLayoutOffset;
    CEditElement* pNewEditElement;

    if( pEle->GetLOElementAndOffset( iOffset, FALSE, pElement, iLayoutOffset )){
        XP_Bool result = LO_NextPosition( m_pContext, pElement, iLayoutOffset, &pNewEditElement, &iNewLayoutOffset );
        // The insert point can't be the end of the document.
        if ( result && (
            ! pNewEditElement || pNewEditElement->IsEndOfDocument() ) ) {
            result = FALSE;
        }
        if ( result ){
            pNew = pNewEditElement->Leaf();
            iNewOffset = iNewLayoutOffset;
        }
        return result;
    }
    else {
        pEle->NextPosition( iOffset, pNew, iNewOffset );
        return TRUE;
    }
}

void CEditBuffer::NextChar( XP_Bool bSelect ){
    VALIDATE_TREE(this);
//    int iSavePos = m_iCurrentOffset;

    if( bSelect ){
        SelectNextChar();
        return;
    }
    ClearPhantomInsertPoint();
    ClearMove();
    ClearSelection();
    //FixupInsertPoint();
    NextPosition( m_pCurrent, m_iCurrentOffset,
                    m_pCurrent, m_iCurrentOffset );
    SetCaret();
}



XP_Bool CEditBuffer::PreviousChar( XP_Bool bSelect ){
    VALIDATE_TREE(this);

    if( bSelect ){
        SelectPreviousChar();
        return FALSE;       // no return value on select previous..
    }

    ClearPhantomInsertPoint();
    ClearMove();
    ClearSelection();
    //FixupInsertPoint();
    if( PrevPosition( m_pCurrent, m_iCurrentOffset,
                    m_pCurrent, m_iCurrentOffset )){
        SetCaret();
        return TRUE;
    }
    else {
        return FALSE;
    }
}


//
// Find out our current layout position and move up or down from here.
//
void CEditBuffer::UpDown( XP_Bool bSelect, XP_Bool bForward ){
    VALIDATE_TREE(this);
    CEditLeafElement *pEle;
    ElementOffset iOffset;
    XP_Bool bStickyAfter;
    LO_Element* pElement;
    int iLayoutOffset;

    DoneTyping();

    BeginSelection();

    if( bSelect ){
        GetInsertPoint( &pEle, &iOffset, &bStickyAfter );
    }
    else {
        // do the right thing depending on insertion point change of direction
        ClearSelection( TRUE, !bForward );
        ClearPhantomInsertPoint();
        pEle = m_pCurrent;
        iOffset = m_iCurrentOffset;
        bStickyAfter = m_bCurrentStickyAfter;
    }

    XP_ASSERT(!pEle->IsEndOfDocument());
    pEle->GetLOElementAndOffset( iOffset, bStickyAfter, pElement, iLayoutOffset );

    LO_UpDown( m_pContext, pElement, iLayoutOffset, GetDesiredX(pEle, iOffset, bStickyAfter), bSelect, bForward );

    EndSelection();
}

XP_Bool CEditBuffer::NextTableCell( XP_Bool /*bSelect*/, XP_Bool bForward, intn* pRowCounter )
{
    if( !  IsInsertPointInTable() )
        return FALSE;

    CEditTableCellElement* pTableCell = m_pCurrent->GetTableCellIgnoreSubdoc();
    if( !pTableCell )
        return FALSE;

    CEditElement *pNext;
    if(bForward)
    {
        pNext = pTableCell->GetNextSibling();
    } else {
        pNext = pTableCell->GetPreviousSibling();
    }
    if( !pNext )
    {
        // No sibling found, but we may need to check for more rows
        CEditTableRowElement* pRow = pTableCell->GetTableRowIgnoreSubdoc();
        CEditElement* pNextRow;
        if( pRow )
        {
            if(bForward)
            {
                pNextRow = pRow->GetNextSibling();
            } else {
                pNextRow = pRow->GetPreviousSibling();
            }
            if( pNextRow && pNextRow->IsTableRow() )
            {
                if(bForward)
                {
                    pNext = pNextRow->GetChild();
                    // Tell caller we wrapped to next row
                    if( pNext && pRowCounter )
                        (*pRowCounter)++;

                } else {
                    pNext = pNextRow->GetLastChild();
                    // Tell caller we wrapped to previous row
                    if( pNext && pRowCounter )
                        (*pRowCounter)--;
                }
            }
        }
    }
    if( pNext )
    {
        CEditInsertPoint insertPoint;
        insertPoint.m_pElement = pNext->GetFirstMostChild()->Leaf();
        if( insertPoint.m_pElement )
        {
            SetInsertPoint(insertPoint);
            return TRUE;
        }
    }
    return FALSE;
}

int32
CEditBuffer::GetDesiredX(CEditLeafElement* pEle, intn iOffset, XP_Bool bStickyAfter)
{
    XP_ASSERT(bStickyAfter == TRUE || bStickyAfter == FALSE);
    // m_iDesiredX is where we would move if we could.  This keeps the
    //  cursor moving, basically, straight up and down, even if there is
    //  no text or gaps
    if( m_iDesiredX == -1 ){
        LO_Element* pElement;
        int iLayoutOffset;

        // A break at offset 1 is the beginning of the next line.  We really want
        //  the end in this case.
        if( pEle->IsBreak() && iOffset == 1 ){
            iOffset = 0;
        }

        pEle->GetLOElementAndOffset( iOffset, bStickyAfter, pElement, iLayoutOffset );
        int32 x;
        int32 y;
        int32 width;
        int32 height;
        LO_GetEffectiveCoordinates(m_pContext, pElement, iLayoutOffset, &x, &y, &width, &height);
        m_iDesiredX = x;
    }
    return m_iDesiredX;
}


void CEditBuffer::NavigateChunk( XP_Bool bSelect, intn chunkType, XP_Bool bForward ){
    if ( chunkType == EDT_NA_UPDOWN ){
        UpDown(bSelect, bForward);
    }
    else {
        VALIDATE_TREE(this);
        ClearPhantomInsertPoint();
        ClearMove();    /* Arrow keys clear the up/down target position */
        BeginSelection();
        LO_NavigateChunk( m_pContext, chunkType, bSelect, bForward );
        EndSelection();
        DoneTyping();
    }
}

//cmanske: TEST FOR SELECTED TABLE/CELLS
void CEditBuffer::ClearMailQuote(){
    if ( ! m_pCurrent->InMungableMailQuote() ) {
        XP_ASSERT(FALSE);
        return;
    }
    // Outdent to get rid of MailQuote info.
    while( m_pCurrent->InMungableMailQuote() ) {
        if ( ! Outdent() ){
            /* Currently (4.0) we can't outdent in the presence of DIV tags. */
            return;
        }
    }
    // Get rid of paragraph style
    MorphContainer(P_NSDT);
    SetParagraphAlign(ED_ALIGN_DEFAULT);
    if ( m_pCurrent->IsText() ) {
        // Get rid of character styles
        EDT_CharacterData* pCharacterData = EDT_NewCharacterData();
        // We wanted to do ~0, but this causes an assert in SetCharacterData.
        pCharacterData->mask = ~(TF_SERVER|TF_SCRIPT|TF_STYLE);
        pCharacterData->values = 0; // Clear everything.
        SetCharacterData(pCharacterData);
        EDT_FreeCharacterData(pCharacterData);
    }
}

//
// Break current container into two containers.
//
EDT_ClipboardResult CEditBuffer::ReturnKey(XP_Bool bTyping, XP_Bool bIndent){
    ClearTableAndCellSelection();
    EDT_ClipboardResult result = EDT_COP_OK;
    if ( bTyping ) {
        VALIDATE_TREE(this);
#ifdef DEBUG
        if ( m_pTestManager->ReturnKey() )
            return result;
#endif
        if( IsSelected() ){
            result = DeleteSelection();
            if ( result != EDT_COP_OK ) return result;
        }
        // Check if we are in a mail quote
        CEditInsertPoint ip;
        GetInsertPoint(ip);
        if ( ip.m_pElement->InMungableMailQuote() ) {
            // Are we at the end of the mail quote? If so, move beyond
            XP_Bool bAtEndOfMailQuote = FALSE;
            if ( ip.IsEndOfContainer() ) {
                ip = ip.NextPosition();
                if ( ! ip.m_pElement->InMailQuote() ) {
                    bAtEndOfMailQuote = TRUE;
                }
            }
            result = InternalReturnKey(TRUE);
            if ( result == EDT_COP_OK ) {
                if ( ! bAtEndOfMailQuote ) {
                    StartTyping(TRUE);
                    InternalReturnKey(TRUE);
                    UpDown(FALSE, FALSE); // go back up one line.
                }
                ClearMailQuote();
            }
        }
        else {
            if( m_pCurrent && m_iCurrentOffset == 0 &&
                m_pCurrent->IsText() && m_pCurrent->GetLen() == 0 &&
                m_pCurrent->GetNextSibling() == 0 &&
                m_pCurrent->PreviousLeafInContainer() == 0 )
            {
                CEditContainerElement *pContainer;
                CEditListElement* pList;
                m_pCurrent->FindList(pContainer, pList);
                if( pContainer && pList )
                {
                    // We are at the begining of an empty element in a list item container,
                    // If we were going to Indent, just do that to current container,
                    if( bIndent )
                        Indent();
                    else                            
                        // Outdent to next higher list level
                        Outdent();

                    return EDT_COP_OK;
                }
            }
            // Fix for bug 80092 - force dirty flag on after Enter key alone is pressed
            StartTyping(TRUE);
            result = InternalReturnKey(TRUE);
//          Include this to set the end of current UNDO at the end of a paragraph (return key)
//            so Undo will only remove the last paragraph typed.
//          By not including this, multiple paragraphs typed without setting caret with mouse 
//            will all be undone together.
//            DoneTyping();

            // Indent the newly-created container one level if requested
            if( bIndent )
                Indent();
        }
    }
    else {
        StartTyping(TRUE);
        result = InternalReturnKey(TRUE);
//        DoneTyping();     // Set note above
    }
    return result;
}

// Returns the container of the first element after the split
CEditElement* CEditBuffer::SplitAtContainer(XP_Bool bUserTyped, XP_Bool bSplitAtBeginning, CEditElement*& pRelayoutStart)
{
    // ClearSelection();
    ClearPhantomInsertPoint();
    FixupInsertPoint();

    pRelayoutStart = m_pCurrent;
    CEditLeafElement *pNew,*pSplitAfter;
    CEditLeafElement *pCopyFormat;

    pSplitAfter = m_pCurrent;
    pCopyFormat = m_pCurrent;

    if( m_iCurrentOffset == 0 )
    {
        // We are at the beginning of the element

        if( m_pCurrent->PreviousLeafInContainer() == 0 )
        {
            // There are no elements before - we are at beginning of Container

            if( bSplitAtBeginning )
            {
                // Create a new empty text element
                CEditTextElement *pNew;
                pNew = m_pCurrent->CopyEmptyText();
                pNew->InsertBefore( m_pCurrent );
                pRelayoutStart = pNew->FindContainer();
                pSplitAfter = pNew;
            }
            else
            {
                // Don't split - don't create anything
                pRelayoutStart = m_pCurrent->FindContainer();
                return pRelayoutStart;
            }
        }
    }
    else 
    {
        pNew = m_pCurrent->Divide( m_iCurrentOffset )->Leaf();
        /* Get rid of space at start of new line if user typed. */
        if( bUserTyped && !pNew->InFormattedText()
                && pNew->IsA(P_TEXT)
                && pNew->Text()->GetLen()
                && pNew->Text()->GetText()[0] == ' ' )
        {
            pNew->Text()->DeleteChar(m_pContext, 0);
            CEditLeafElement *pNext = 0;
            if( pNew->Text()->GetLen() == 0
                    && (pNext = pNew->TextInContainerAfter()) != 0 )
            {
                pNew->Unlink();
                // Should we delete pNew?
                delete pNew;
                pNew = pNext;
            }
        }
        m_pCurrent = pNew;
    }
    m_iCurrentOffset = 0;
    m_pCurrent->GetParent()->Split( pSplitAfter, 0,
                &CEditElement::SplitContainerTest, 0 );


    CEditContainerElement* pContainer = m_pCurrent->FindContainer();
    if ( bUserTyped && m_pCurrent->GetLen() == 0 && m_pCurrent->TextInContainerAfter() == 0 )
    {
        // An empty paragraph. If the style is one of the
        // heading styles, set it back to normal
        TagType tagType = pContainer->GetType();
        // To do - use one of those fancy bit arrays.
        if ( tagType >= P_HEADER_1 && tagType <=  P_HEADER_6)
        {
#if EDT_DDT
            pContainer->SetType(P_NSDT);
#else
            pContainer->SetType(P_DESC_TITLE);
#endif
        }
    }
    return pContainer;
}

EDT_ClipboardResult CEditBuffer::InternalReturnKey(XP_Bool bUserTyped)
{
    EDT_ClipboardResult result = EDT_COP_CLIPBOARD_BAD;
    if( IsSelected() )
    {
        result = DeleteSelection();
        if ( result != EDT_COP_OK )
            return result;
    }

    CEditElement *pChangedElement;
    // Split at current element to create a new container (TRUE = split at beginning of element as well
    SplitAtContainer(bUserTyped, TRUE, pChangedElement);

    int iChangedOffset = (pChangedElement == m_pCurrent) ? m_iCurrentOffset : 0;
    if( pChangedElement )
    {
        if ( bUserTyped )
            Reduce(m_pRoot); // Or maybe just the two containers?
	    
	    // We currently cannot do a reflow here as the new text elements have not been
	    // created to be reflowed...
        Relayout(pChangedElement, iChangedOffset,
            (pChangedElement != m_pCurrent ? m_pCurrent : 0 ));

#if 0
        // There's a bug that after a return is pressed, the formatting is wrong
        //  for the new paragrah.  Presents a problem.  If we insert an empty
        //  text element here, we get messed up because we may not be able to
        //  position the caret.  We end up reducing and loosing a pointer to
        //  the layout element.  It is a hard problem.

        CEditTextElement *pNewText = pCopyFormat->CopyEmptyText();
        pNewText->InsertBefore(m_pCurrent);
        m_pCurrent = pNewText;
        m_iCurrentOffset = 0;
#endif
        result = EDT_COP_OK;
    }

    return result;
}

//TODO: Should this be a preference?
#define EDT_SPACES_PER_TAB  4

EDT_ClipboardResult CEditBuffer::TabKey(XP_Bool bForward, XP_Bool bForceTabChar){

    ClearTableAndCellSelection();
    if( IsInsertPointInTable() && !bForceTabChar){
        if( NextTableCell(FALSE, bForward) ){
            // This selects contents of next cell:
            //cmanske: Other programs select the contents of a cell when
            //  tab into it, but lets not do that!
            // This function will now select the cell boundary, not contents
//            SelectTableCell();
        }
        else if ( bForward ) {
            // Tabbing past last cell creates a new row
            // back-tabbing (Shift-Tab) in first cell is ignored
            BeginBatchChanges(kInsertTableRowCommandID);
            // Save current insert point so we can return
            //  there when done
            CEditInsertPoint ip;
            GetTableInsertPoint(ip);
            InsertTableRows(NULL, TRUE, 1);
            // Restore previous insert point
            //  then move to first new cell
            SetInsertPoint(ip);
            NextTableCell(FALSE, bForward);
            EndBatchChanges();
        }
        return EDT_COP_OK;
    }

    // Tab = insert some extra spaces
    // TODO: SHOULD WE USE "SPACER" TAG INSTEAD?
    EDT_ClipboardResult result;
    for( int i = 0; i < EDT_SPACES_PER_TAB; i++ ){
        if( EDT_COP_OK != (result = InsertChar(' ', FALSE)) ){
            break;
        }
    }
    return result;
}

void CEditBuffer::IndentSelection(CEditSelection& selection)
{
    CEditLeafElement *pBegin, *pEnd, *pCurrent;
    ElementOffset iBeginPos, iEndPos;
    XP_Bool bFromStart;

    // Don't relayout if we got a cell selection passed in
    XP_Bool bRelayout = selection.IsEmpty();

    // Get data from supplied selection or get current selection if empty
    GetSelection(selection, pBegin, iBeginPos, pEnd, iEndPos, bFromStart );
    pCurrent = pBegin;
    XP_Bool bDone = FALSE;
    CEditContainerElement* pLastContainer = 0;
    CEditContainerElement* pContainer = 0;
    CEditListElement* pList;
    do {
        pCurrent->FindList( pContainer, pList );
        if( pContainer != pLastContainer ){
            IndentContainer( pContainer, pList );
            pLastContainer = pContainer;
        }
        bDone = (pEnd == pCurrent );    // For most cases
        pCurrent = pCurrent->NextLeafAll();
        bDone = bDone || (iEndPos == 0 && pEnd == pCurrent ); // Pesky edge conditions!
    } while( pCurrent && !bDone );

    if( bRelayout )
    {
        // force layout stop displaying the current selection.
#ifdef LAYERS
        LO_StartSelectionFromElement( m_pContext, 0, 0, NULL );
#else
        LO_StartSelectionFromElement( m_pContext, 0, 0);
#endif
        // probably needs to be the common ancestor.
        Relayout( pBegin->FindContainer(), 0, pEnd, RELAYOUT_NOCARET );
        // Need to force selection.
        SelectRegion(pBegin, iBeginPos, pEnd, iEndPos, bFromStart );
    }
}

void CEditBuffer::Indent()
{
    VALIDATE_TREE(this);
    CEditSelection selection;
    if( IsSelected() )
    {
        IndentSelection(selection);
        return;
    }
    if( IsTableOrCellSelected() )
    {
        if( GetFirstCellSelection(selection) )
        {
            BeginBatchChanges(kChangeAttributesCommandID);
            IndentSelection(selection);
            while( GetNextCellSelection(selection) )
            {
                IndentSelection(selection);
            }
            RelayoutSelectedTable();
            EndBatchChanges();
        }
        return;
    } 

    //
    // find where cont-cont has cont
    //
    CEditContainerElement* pContainer;
    CEditListElement* pList;
    m_pCurrent->FindList( pContainer, pList );
    if( !pContainer )
        return;

    IndentContainer( pContainer, pList );

    // Get the list elemement again - it might have changed
    m_pCurrent->FindList( pContainer, pList );
    CEditElement *pStart = pContainer;
    CEditElement *pEnd = pContainer;
    XP_ASSERT(pContainer);
    CEditElement *pPrev;
    if( pList )
    {
        pStart = pList;
    } else if( (pPrev = pContainer->GetPreviousSibling()) != NULL )
    {
        pStart = pPrev;
    }
    Relayout( pStart, 0, pEnd->GetLastMostChild() );
}

void CEditBuffer::IndentContainer( CEditContainerElement *pContainer,
        CEditListElement *pList )
{
    VALIDATE_TREE(this);

    CEditElement *pPrev = pContainer->GetPreviousSibling();
    CEditElement *pNext = pContainer->GetNextSibling();
    XP_Bool bDone = FALSE;

    // Consider context (whether we are just after or before an existing list)
    //   only if we are indenting a list item
    if( pList )
    {
        //
        // case 1
        //
        //     UL:
        //         LI:
        //     LI:         <- Indenting this guy
        //     LI:
        //     LI:
        //
        //  should result in
        //
        //     UL:
        //         LI:
        //         LI:     <-end up here
        //     LI:
        //     LI:
        //
        //
        //  We also need to handle the case
        //
        //      UL:
        //          LI:
        //      LI:         <- indent this guy
        //      UL:
        //          LI:
        //          LI:
        //
        //  and the second UL becomes redundant so we eliminate it.
        //
        //      UL:
        //          LI:
        //          LI:     <-Ends up here
        //          LI:     <-frm the redundant container
        //          LI:
        //
        if( pPrev && pList->IsCompatableList(pPrev) )
        {
            CEditElement *pChild = pPrev->GetLastChild();
            if( pChild )
            {
                pContainer->Unlink();
                pContainer->InsertAfter( pChild );
            }
            else
            {
                pContainer->Unlink();
                pContainer->InsertAsFirstChild( pPrev );
            }

            if( pNext && pNext->IsList() )
                pPrev->Merge( pNext );

            bDone = TRUE;
        }
        //
        // case 2
        //
        //     UL:
        //          LI:
        //          LI:         <- Indenting this guy
        //          UL:
        //              LI:
        //          LI:
        //          LI:
        //
        //  should result in
        //
        //     UL:
        //          LI:
        //          UL:
        //              LI:     <- Ends up here.
        //              LI:
        //          LI:
        //          LI:
        //
        else if( pNext && pList->IsCompatableList(pNext) )
        {
            CEditElement *pChild = pNext->GetChild();
            if( pChild ){
                pContainer->Unlink();
                pContainer->InsertBefore( pChild );
            }
            else {
                pContainer->Unlink();
                pContainer->InsertAsFirstChild( pPrev );
            }
            bDone = TRUE;
        }
        //
        // case 3
        //
        //     UL:
        //          LI:
        //          LI:         <- Indenting this guy
        //          LI:
        //          LI:
        //
        //  should result in
        //
        //     UL:
        //          LI:
        //          UL:
        //              LI:     <- Ends up here.
        //          LI:
        //          LI:
        //
        else
        {
            CEditElement *pNewList;
            pNewList = pList->Clone(0);

            // insert the new cont-cont between the old one and the cont
            pNewList->InsertBefore( pContainer );
            pContainer->Unlink();
            pContainer->InsertAsFirstChild( pNewList);
            bDone = TRUE;
        }
    }
    //
    // case 0
    //
    //      LI:             <- Indenting this guy
    //
    // should result in
    //
    //      UL:
    //          LI:
    //
    //
    if( !bDone && pList == 0 )
    {
        PA_Tag *pTag = XP_NEW( PA_Tag );
        XP_BZERO( pTag, sizeof( PA_Tag ) );
        // Set appropriate list type for the given list item
        // (E.g., only list items should exist inside of UL, OL)
        // Use blockquote instead (like MS FrontPage)
        // Note: We pass through here when creating UL and OL
        //  lists as well, but the list type is changed 
        //  correctly later on, so this works fine
        TagType t = pContainer->GetType();
        switch ( t )
        {
            case P_LIST_ITEM:
                pTag->type = P_UNUM_LIST;
                break;
            case P_DESC_TITLE:
            case P_DESC_TEXT:
                pTag->type = P_DESC_LIST;
                break;
            default:
                pTag->type = P_BLOCKQUOTE;
                break;
        }
        CEditElement *pEle = new CEditListElement( 0, pTag, GetRAMCharSetID() );
        PA_FreeTag( pTag );

        pEle->InsertAfter( pContainer );
        pContainer->Unlink();
        pContainer->InsertAsFirstChild( pEle );
        return;
    }
}

XP_Bool CEditBuffer::OutdentSelection(CEditSelection& selection)
{
    CEditLeafElement *pBegin, *pEnd, *pCurrent;
    ElementOffset iBeginPos, iEndPos;
    XP_Bool bFromStart;

    // Don't relayout if we got a cell selection passed in
    XP_Bool bRelayout = selection.IsEmpty();

    // Get data from supplied selection or get current selection if empty
    GetSelection(selection, pBegin, iBeginPos, pEnd, iEndPos, bFromStart );
    pCurrent = pBegin;
    XP_Bool bDone = FALSE;
    CEditContainerElement* pLastContainer = 0;
    CEditContainerElement* pContainer = 0;
    CEditListElement* pList;
    do {
        pCurrent->FindList( pContainer, pList );
        if( pContainer != pLastContainer ){
            OutdentContainer( pContainer, pList );
            pLastContainer = pContainer;
        }
        bDone = (pEnd == pCurrent );    // For most cases
        pCurrent = pCurrent->NextLeafAll();
        bDone = bDone || (iEndPos == 0 && pEnd == pCurrent ); // Pesky edge conditions!
    } while( pCurrent && !bDone );

    if( bRelayout )
    {
        // force layout stop displaying the current selection.
#ifdef LAYERS
        LO_StartSelectionFromElement( m_pContext, 0, 0, NULL );
#else
        LO_StartSelectionFromElement( m_pContext, 0, 0);
#endif
        // probably needs to be the common ancestor.
        Relayout( pBegin->FindContainer(), 0, pEnd, RELAYOUT_NOCARET );
        // Need to force selection.
        SelectRegion(pBegin, iBeginPos, pEnd, iEndPos, bFromStart );
    }
    // How can we always return TRUE??
    return TRUE;
}

XP_Bool CEditBuffer::Outdent()
{
    CEditSelection selection;
    VALIDATE_TREE(this);

    if( IsSelected() ){
        return OutdentSelection(selection);
    }
    if( IsTableOrCellSelected() )
    {
        XP_Bool bResult = FALSE;
        if( GetFirstCellSelection(selection) )
        {
            BeginBatchChanges(kChangeAttributesCommandID);
            // Format first selected cell
            bResult = OutdentSelection(selection);
            // Select all other cells and format them
            while( GetNextCellSelection(selection) )
            {
                bResult = OutdentSelection(selection);
            }
            RelayoutSelectedTable();
            EndBatchChanges();
        }
        return bResult;
    } 

    CEditContainerElement* pContainer = NULL;
    CEditListElement* pList = NULL;
    m_pCurrent->FindList( pContainer, pList );
    if( !pContainer )
        return FALSE;

    OutdentContainer( pContainer, pList );
    // Get the list elemement again - it might have changed
    m_pCurrent->FindList( pContainer, pList );
    CEditElement *pStart = pContainer;
    CEditElement *pEnd = pContainer;
    XP_ASSERT(pContainer);
    CEditElement *pPrev;
    if( pList )
    {
        pStart = pList;
    } else if( (pPrev = pContainer->GetPreviousSibling()) != NULL )
    {
        pStart = pPrev;
    }
    Relayout( pStart, 0, pEnd->GetLastMostChild() );
    return TRUE;
}

void CEditBuffer::OutdentContainer( CEditContainerElement *pContainer,
        CEditListElement *pList )
{

    //
    // case 0
    //
    //      LI:     <-- outdent this guy.
    //
    if( pList == 0 )
        return;                         // no work to do

    CEditElement *pPrev = pContainer->GetPreviousSibling();
    CEditElement *pNext = pContainer->GetNextSibling();

    //
    // case 1
    //
    //      UL:
    //          LI:     <-Outdenting this guy
    //
    // should result in
    //
    //      LI:
    //
    // No previous or next siblings.  Just remove the List and
    //  put its container in its place.
    //
    if( pPrev == 0 && pNext == 0 )
    {
        pContainer->Unlink();
        pContainer->InsertAfter( pList );
        pList->Unlink();
        delete pList;
    }


    //
    // case 2
    //
    //      UL:
    //          LI:     <-Outdenting this guy
    //          LI:
    //
    //
    //      Results in:
    //
    //      LI:         <-Outdenting this guy
    //      UL:
    //          LI:
    //
    else if( pPrev == 0 )
    {
        pContainer->Unlink();
        pContainer->InsertBefore( pList );
    }
    //
    // case 3
    //
    //      UL:
    //          LI:
    //          LI:     <-Outdenting this guy
    //
    //
    //      Results in:
    //
    //      UL:
    //          LI:
    //      LI:         <-Outdenting this guy
    //
    else if( pNext == 0 )
    {
        pContainer->Unlink();
        pContainer->InsertAfter( pList );
    }

    //
    // case 4
    //
    //      UL:
    //          LI:
    //          LI:     <-Outdenting this guy
    //          LI:
    //
    //      Results in:
    //
    //      UL:
    //          LI:
    //      LI:         <-Outdenting this guy
    //      UL:
    //          LI:
    else 
    {
        // Clone the list to put following elements in it
        CEditElement *pNewList = pList->Clone(0);
        while( (pNext = pContainer->GetNextSibling()) != 0 )
        {
            pNext->Unlink();
            pNext->InsertAsLastChild( pNewList );
        }
        pContainer->Unlink();
        pContainer->InsertAfter( pList );
        pNewList->InsertAfter( pContainer );
    }
    // If we outdented so that we are no longer in the list,
    //   we must change container types that are only allowed 
    //   in a list to our "normal" container type
    TagType t = pContainer->GetType();
    if( !pContainer->GetParent()->IsList() && 
         ( t == P_LIST_ITEM ||
           t == P_DESC_TITLE ||
           t == P_DESC_TEXT ) )
    {
        pContainer->SetType(P_NSDT);
    }
}

ED_ElementType CEditBuffer::GetCurrentElementType(){
    //cmanske: VERY RISKY! Need to test the effects of this thoroughly!
    // Table and cell selection occurs when there is still a caret 
    // (but must not have anything else selected),
    if( m_pSelectedEdTable )
        return ED_ELEMENT_TABLE;

    if( m_SelectedEdCells.Size() )
        return ED_ELEMENT_CELL;
    
    CEditLeafElement *pInsertPoint;
    ElementOffset iOffset;
    XP_Bool bSingleItem;

    bSingleItem = GetPropertyPoint( &pInsertPoint, &iOffset );

    if( !bSingleItem ){
        if( IsSelected() ){
            return ED_ELEMENT_SELECTION;
        }
        else {
            return ED_ELEMENT_TEXT;
        }
    }
    else if( pInsertPoint->IsA(P_TEXT) ){
        // should never have single item text objects...
        XP_ASSERT(FALSE);
        return ED_ELEMENT_TEXT;
    }
    else if( pInsertPoint->IsImage() ){
        return ED_ELEMENT_IMAGE;
    }
    else if( pInsertPoint->IsA(P_HRULE) ){
        return ED_ELEMENT_HRULE;
    }
    else if( pInsertPoint->IsA(P_LINEBREAK )){
        return ED_ELEMENT_TEXT;
    }
    else if( pInsertPoint->GetElementType() == eTargetElement ){
        return ED_ELEMENT_TARGET;
    }
    else if( pInsertPoint->GetElementType() == eIconElement ){
        return ED_ELEMENT_UNKNOWN_TAG;
    }
    // current element type is unknown.
    XP_ASSERT( FALSE );
    return ED_ELEMENT_TEXT;     // so the compiler doesn't complain.
}

// Outdent the container until we don't have a list
void CEditBuffer::TerminateList(CEditContainerElement *pContainer)
{
    CEditElement *pParent;
    
    if( !pContainer || !(pParent = pContainer->GetParent())->IsList() )
        return;
    do {
        // Move container up one level,
        //  this will also change container type to "normal" (P_NSDT)
        //  when we outdent from the top list level
        OutdentContainer(pContainer, pParent->List() );
        pParent = pContainer->GetParent();
    }
    while( pParent && pParent->IsList() );
}


void CEditBuffer::MorphContainer( TagType t )
{
    VALIDATE_TREE(this);

    CEditSelection selection;

    // charley says this is happening.. Hmmmm...
    //XP_ASSERT( t != P_TEXT );
    if( t == P_TEXT ){
        // actually failed this assert.
        return;
    }

    // Only allow the type to be set to a text container type.
    // This is important. If we allow a different type to be
    // set, then CEditElement::SplitContainerTest will
    // potentially fail.
    if ( ! BitSet( edt_setTextContainer,  t  ) ) {
        XP_ASSERT(FALSE);
        return;
    }

    if( IsSelected() ){
        MorphContainerSelection(t, selection);
    }
    else if( IsTableOrCellSelected() )
    {
        if( GetFirstCellSelection(selection) )
        {
            BeginBatchChanges(kChangeAttributesCommandID);
            MorphContainerSelection(t, selection);
            while( GetNextCellSelection(selection) )
            {
                MorphContainerSelection(t, selection);
            }
            RelayoutSelectedTable();
            EndBatchChanges();
        }
    } 
    else {
        CEditContainerElement *pContainer;
        CEditListElement *pList;
        // Get the container and list 
        m_pCurrent->FindList(pContainer, pList);
        // HACKOLA: just poke in a new tag type.
        pContainer->SetType( t );
        if( pList )
        {
            TagType tList = pList->GetType();
            // Assure that list-type items are only contained in their proper parents. 
            // If not the right type, terminate the list
            if( ((tList == P_UNUM_LIST || tList == P_NUM_LIST || tList == P_MENU || tList == P_DIRECTORY) && 
                  t != P_LIST_ITEM) ||
                (tList == P_DESC_LIST && !(t == P_DESC_TITLE || t == P_DESC_TEXT)) )
            {
                TerminateList(pContainer);
                // Clear this -- we may have to change to a different list type below
                pList = NULL;
            }
        }
        if( !pList && (t == P_LIST_ITEM || t == P_DESC_TITLE || t == P_DESC_TEXT ) )
        {
            // We are changing to a list item that must be in a list container,
            //  but we are not currently in a list, so indent to create appropriate list
            // First reset container type to requested, since TerminateList probably changed it
            pContainer->SetType( t );
            IndentContainer(pContainer, NULL);   
        }

        // Need to relayout from previous line.  Line break distance is wrong.
        Relayout( pContainer, 0, 0 );
    }
}

void CEditBuffer::MorphContainerSelection( TagType t, CEditSelection& selection )
{
    CEditLeafElement *pBegin, *pEnd, *pCurrent;
    CEditContainerElement* pContainer = NULL;
    ED_BufferOffset/*ElementOffset*/ iBeginPos, iEndPos;
    XP_Bool bFromStart;

    // Don't relayout if we got a cell selection passed in
    XP_Bool bRelayout = selection.IsEmpty();

    // Get elements from supplied selection or get current selection
    GetSelection( selection, pBegin, iBeginPos, pEnd, iEndPos, bFromStart );

    pCurrent = pBegin;
    XP_Bool bDone = FALSE;
    do {
        pContainer = pCurrent->FindContainer();
        pContainer->SetType( t );
        // TODO: CHECK THE EFFECTS OF <BR> AFTER A LIST ITEM
        if( pContainer->GetParent()->IsList() )
        {
            // We are changing an item in a list to something other than
            //   a listitem - this means terminate the list
            if( t != P_LIST_ITEM )
                TerminateList(pContainer);
        }
        else if( t == P_LIST_ITEM )
        {
            // We are changing to a list item but
            //  we are not in a list, so create a default UNUM list
            IndentContainer(pContainer, NULL);   
        }
        bDone = (pEnd == pCurrent );    // For most cases
        pCurrent = pCurrent->NextLeafAll();
        bDone = bDone || (iEndPos == 0 && pEnd == pCurrent ); // Pesky edge conditions!
    } while( pCurrent && !bDone );

    if( bRelayout )
    {
        // force layout stop displaying the current selection.
#ifdef LAYERS
        LO_StartSelectionFromElement( m_pContext, 0, 0, NULL );
#else
        LO_StartSelectionFromElement( m_pContext, 0, 0);
#endif
        // probably needs to be the common ancestor.
        Relayout( pBegin->FindContainer(), 0, pEnd, RELAYOUT_NOCARET );
        // Need to force selection.
        SelectRegion(pBegin, iBeginPos, pEnd, iEndPos, bFromStart );
    }
}


void CEditBuffer::MorphListContainer( TagType t )
{
    CEditSelection selection;
    if( IsTableOrCellSelected() )
    {
        if( GetFirstCellSelection(selection) )
        {
            BeginBatchChanges(kChangeAttributesCommandID);
            MorphListContainer2(t, selection);
            while( GetNextCellSelection(selection) )
            {
                MorphListContainer2(t, selection);
            }
            RelayoutSelectedTable();
            EndBatchChanges();
        }
    }
    else
    {
        MorphListContainer2(t, selection);
    }
}

void CEditBuffer::MorphListContainer2( TagType t, CEditSelection& /*selection*/ )
{
    //TODO: HANDLE LISTS IN SELECTION
    EDT_ListData * pListData = GetListData();
    if( !pListData ){
        // We don't have container -- start one
        Indent();
        pListData = GetListData();
        // Block quote can be any paragraph style,
        // Description list should contain only <DT> and <DD>,
        //   others must be list item
    } else if( t != P_BLOCKQUOTE && t != P_DESC_LIST ){
        MorphContainer(P_LIST_ITEM);
    }
    if( pListData ){
        pListData->iTagType = t;
        if( t != P_BLOCKQUOTE ){
            pListData->eType = ED_LIST_TYPE_DEFAULT;
        }
        SetListData(pListData);
        EDT_FreeListData(pListData);
    }
}

void CEditBuffer::ToggleList(intn iTagType)
{
    CEditSelection selection;
    if( IsTableOrCellSelected() )
    {
        if( GetFirstCellSelection(selection) )
        {
            BeginBatchChanges(kChangeAttributesCommandID);
            ToggleList2(iTagType, selection);
            while( GetNextCellSelection(selection) )
            {
                ToggleList2(iTagType, selection);
            }
            RelayoutSelectedTable();
            EndBatchChanges();
        }
    } else {
        ToggleList2(iTagType, selection);
    }
}

// This doesn't seem to pay attention to selection - probably source of some bugs
void CEditBuffer::ToggleList2(intn iTagType, CEditSelection& /*selection*/)
{
    EDT_ListData * pData = NULL;
	TagType nParagraphFormat = GetParagraphFormatting();
    XP_Bool bIsMyList = FALSE;
    XP_Bool bIsDescList = FALSE;

    if ( nParagraphFormat == P_LIST_ITEM || iTagType == P_DESC_LIST ) {
        pData = GetListData();
        bIsMyList = ( pData && pData->iTagType == iTagType );
    }
    bIsDescList = bIsMyList && (iTagType == P_DESC_LIST);

    if ( (nParagraphFormat == P_LIST_ITEM || bIsDescList)
         && bIsMyList ) {
        // WILL THIS ALONE UNDO THE LIST?
        MorphContainer(P_NSDT);
        Outdent();
    } else {
        if ( !pData ) {
            // Must be indendented
            Indent();
            // Create a numbered list item ONLY if not Description list
            if (nParagraphFormat != P_LIST_ITEM && iTagType != P_DESC_LIST) {
                MorphContainer(P_LIST_ITEM);
            }
            pData = GetListData();
        } else if (nParagraphFormat == P_LIST_ITEM && iTagType == P_DESC_LIST ){
            // We are converting an existing list item into
            //   a description, so remove the list item
            MorphContainer(P_DESC_TITLE);
            pData = GetListData();
        }


        if ( pData && (pData->iTagType != iTagType) ){
            pData->iTagType = iTagType;
            pData->eType = ED_LIST_TYPE_DEFAULT;
            SetListData(pData);
        }
    }
    if ( pData ) {
        EDT_FreeListData(pData);
    }
}

void CEditBuffer::SetParagraphAlign( ED_Alignment eAlign ){
    VALIDATE_TREE(this);
    XP_Bool bDone;

    // To avoid extra HTML params, use default
    //  for left align since that's how layout will do it
    //cmanske: Windows FE was doing this - moved logic here for consistency
    // TODO: This is messy for table cells: We need to check if ROW
    //   has set alignment and NOT do this if it isn't already ED_ALIGN_DEFAULT,
    //   otherwise cell gets the row's alignment
    if( eAlign == ED_ALIGN_LEFT )
        eAlign = ED_ALIGN_DEFAULT;

    if( IsSelected() ){
        CEditLeafElement *pBegin, *pEnd, *pCurrent;
        CEditContainerElement *pContainer = NULL;
        ElementOffset iBeginPos, iEndPos;
        XP_Bool bFromStart;
        GetSelection( pBegin, iBeginPos, pEnd, iEndPos, bFromStart );
        pCurrent = pBegin;
        bDone = FALSE;
        do {
            pContainer = pCurrent->FindContainer();
            pContainer->Container()->SetAlignment( eAlign );

            bDone = (pEnd == pCurrent );    // For most cases
            pCurrent = pCurrent->NextLeafAll();
            bDone = bDone || (iEndPos == 0 && pEnd == pCurrent ); // Pesky edge conditions!
        } while( pCurrent && !bDone );

        // force layout stop displaying the current selection.
#ifdef LAYERS
        LO_StartSelectionFromElement( m_pContext, 0, 0, NULL );
#else
        LO_StartSelectionFromElement( m_pContext, 0, 0);
#endif
        // probably needs to be the common ancestor.
        Relayout( pBegin->FindContainer(), 0, pEnd, RELAYOUT_NOCARET );
        // Need to force selection.
        SelectRegion(pBegin, iBeginPos, pEnd, iEndPos, bFromStart );
    }
    else if( m_pSelectedEdTable )
    {
        SetTableAlign(eAlign);
    }
    else if( m_SelectedEdCells.Size() )
    {
        // Tables are weird - must use ABSCENTER
        if( eAlign == ED_ALIGN_CENTER )
            eAlign = ED_ALIGN_ABSCENTER;

        int iCount = m_SelectedEdCells.Size();
        for( int i = 0; i < iCount; i++ )
        {
            // TODO: CHECK FOR ROW AND SET ALIGNMENT THERE INSTEAD
            EDT_TableCellData *pData = m_SelectedEdCells[i]->GetData();
            if( pData )
            {
                pData->align = eAlign;
                m_SelectedEdCells[i]->SetData( pData );
                EDT_FreeTableCellData(pData);
            }
        }
        CEditTableElement *pTable = m_SelectedEdCells[0]->GetTableIgnoreSubdoc();
        if( pTable )
            Relayout( pTable, 0 );
    }
    else {
        CEditElement *pContainer = m_pCurrent->FindContainer();
        // HACKOLA: just poke in a new tag type.
        pContainer->Container()->SetAlignment( eAlign );
        // Need to Relayout from previous line.  Line break distance is wrong.
        Relayout( pContainer, 0, pContainer->GetLastMostChild() );
    }
}

ED_Alignment CEditBuffer::GetParagraphAlign( ){
    CEditLeafElement *pInsertPoint;
    ElementOffset iOffset;

    GetPropertyPoint( &pInsertPoint, &iOffset );
    if( pInsertPoint ) {
        CEditContainerElement* pCont = pInsertPoint->FindContainer();
        if ( pCont != 0){
            return pCont->GetAlignment();
        }
    }
    return ED_ALIGN_DEFAULT;
}

void CEditBuffer::SetTableAlign( ED_Alignment eAlign )
{
    if( IsInsertPointInTable() )
    {
        // To avoid extra HTML params, use default
        //  for left align since that's how layout will do it
        if( eAlign == ED_ALIGN_LEFT )
            eAlign = ED_ALIGN_DEFAULT;
        
        // Tables need ABSCENTER or they don't center!
        if( eAlign == ED_ALIGN_CENTER )
            eAlign = ED_ALIGN_ABSCENTER;

        EDT_TableData* pData = GetTableData();
        if( pData )
        {
            if( eAlign != pData->align )
            {
                pData->align = eAlign;
                SetTableData(pData);
            }
            EDT_FreeTableData(pData);
        }
    }        
}

void CEditBuffer::SetFontPointSize( int iPoints )
{
    EDT_CharacterData * pData;
    CEditSelection selection;

    if( IsTableOrCellSelected() )
    {
        // Format each cell's contents
        if( GetFirstCellSelection(selection) )
        {
            BeginBatchChanges(kChangeAttributesCommandID);
            pData = GetCharacterDataSelection(0, selection);
            if( pData )
            {
                pData->iPointSize = iPoints;
                SetCharacterDataSelection(pData, selection, FALSE);
                EDT_FreeCharacterData(pData);
                while( GetNextCellSelection(selection) )
                {
                    pData = GetCharacterDataSelection(pData, selection);
                    if( pData )
                    {
                        pData->iPointSize = iPoints;
                        SetCharacterDataSelection(pData, selection, FALSE);
                        EDT_FreeCharacterData(pData);
                    }
                }
            }
            RelayoutSelectedTable();
            EndBatchChanges();
        }
        return;
    } 

    pData = GetCharacterData();
    if( pData ){
        pData->mask = pData->values = TF_FONT_POINT_SIZE;
        pData->iPointSize = (int16)iPoints;
        // This handles selected tables correctly
        SetCharacterData(pData);
        EDT_FreeCharacterData(pData);
    }
}

void CEditBuffer::SetFontSize( int iSize, XP_Bool bRelative )
{
    VALIDATE_TREE(this);

    CEditSelection selection;
    GetSelection(selection);
    if ( selection.IsContainerEnd() )
        return;
    else if( selection.AnyLeavesSelected() )
    {
        BeginBatchChanges(kChangeAttributesCommandID);
        SetFontSizeSelection( iSize, bRelative, selection, TRUE );
        EndBatchChanges();
        return;
    }
    if( IsTableOrCellSelected() )
    {
        if( GetFirstCellSelection(selection) )
        {
            BeginBatchChanges(kChangeAttributesCommandID);
            // Format first selected cell
            SetFontSizeSelection( iSize, bRelative, selection, FALSE );
            // Select all other cells and format them
            while( GetNextCellSelection(selection) )
            {
                SetFontSizeSelection( iSize, bRelative, selection, FALSE );
            }
            RelayoutSelectedTable();
            EndBatchChanges();
        }
        return;
    } 

    FixupInsertPoint();
    CEditLeafElement *pRight = m_pCurrent->Divide( m_iCurrentOffset )->Leaf();
    if( pRight->IsA(P_TEXT)
            && pRight->Text()->GetLen() == 0 )
    {
    //    && m_pCurrent == pRight
    // Bug 27891, above term in the conditional test should not be here, e.g.
    // in CEditBuffer::SetCharacterData() it is not there.
        pRight->Text()->SetFontSize(iSize, bRelative);
        m_pCurrent = pRight;
        m_iCurrentOffset = 0;
    }
    else 
    {
        CEditTextElement *pNew = m_pCurrent->CopyEmptyText();
        pNew->InsertBefore(pRight);
        pNew->SetFontSize( iSize, bRelative );
        m_pCurrent = pNew;
        m_iCurrentOffset = 0;
    }
    Relayout( m_pCurrent, 0, 0 );
}

void CEditBuffer::SetFontSizeSelection( int iSize, XP_Bool bRelative, CEditSelection& selection, XP_Bool bRelayout)
{
    VALIDATE_TREE(this);
    CEditLeafElement *pCurrent, *pEnd, *pBegin, *pNext;
    ED_BufferOffset iBeginPos, iEndPos;
    XP_Bool bFromStart;

    // Get data from supplied selection or get current selection
    GetSelection( selection, pBegin, iBeginPos, pEnd, iEndPos, bFromStart );

    //
    // Guarantee that the text blocks of the beginning and end of selection
    //  are atomic.
    //
    MakeSelectionEndPoints(selection, pBegin, pEnd );
    pCurrent = pBegin;

    while( pCurrent != pEnd )
    {
        pNext = pCurrent->NextLeafAll();
        if( pCurrent->IsA( P_TEXT ) &&
            ED_IS_NOT_SCRIPT(pCurrent) )
        {
            // Set new size if its still inside allowable range
            pCurrent->Text()->SetFontSize(iSize, bRelative);
        }
        pCurrent = pNext;
    }
    if( bRelayout )
    {
        // force layout stop displaying the current selection.
#ifdef LAYERS
        LO_StartSelectionFromElement( m_pContext, 0, 0, NULL);
#else
        LO_StartSelectionFromElement( m_pContext, 0, 0);
#endif
        // probably needs to be the common ancestor.
        Relayout( pBegin, 0, pEnd, RELAYOUT_NOCARET );
        // Need to force selection.
        SelectRegion(pBegin, 0, pEnd, 0);
    }
    //TODO: Should we do this for cell "selection"?
    Reduce(pBegin->GetCommonAncestor(pEnd));
}

void CEditBuffer::SetFontColor( ED_Color iColor )
{
    // ToDo: Eliminate this function in favor of SetCharacterData
    VALIDATE_TREE(this);

    CEditSelection selection;
    GetSelection(selection);
    if ( selection.IsContainerEnd() )
        return;
    else if( selection.AnyLeavesSelected() )
    {
        BeginBatchChanges(kChangeAttributesCommandID);
        SetFontColorSelection( iColor, selection, TRUE );
        EndBatchChanges();
        return;
    }
    if( IsTableOrCellSelected() )
    {
        // Format each cell
        if( GetFirstCellSelection(selection) )
        {
            BeginBatchChanges(kChangeAttributesCommandID);
            // FALSE = don't relayout
            SetFontColorSelection( iColor, selection, FALSE );
            while( GetNextCellSelection(selection) )
            {
                SetFontColorSelection( iColor, selection, FALSE );
            }
            RelayoutSelectedTable();
            EndBatchChanges();
        }
        return;
    } 

    FixupInsertPoint();
    CEditLeafElement *pRight = m_pCurrent->Divide( m_iCurrentOffset )->Leaf();
    if( pRight->IsA(P_TEXT)
            && pRight->Leaf()->GetLen() == 0 ) {
    //    && m_pCurrent == pRight
    // Bug 27891, this term in the conditional test should not be here, e.g.
    // in CEditBuffer::SetCharacterData() it is not there.
        pRight->Text()->SetColor( iColor );
        m_pCurrent = pRight;
        m_iCurrentOffset = 0;
    }
    else {
        CEditTextElement *pNew = m_pCurrent->CopyEmptyText();
        pNew->InsertBefore(pRight);
        pNew->SetColor( iColor );
        m_pCurrent = pNew;
        m_iCurrentOffset = 0;
    }
    Relayout( m_pCurrent, 0, 0 );
}

void CEditBuffer::SetFontColorSelection( ED_Color iColor, CEditSelection& selection, XP_Bool bRelayout )
{
    VALIDATE_TREE(this);
    CEditLeafElement *pCurrent, *pEnd, *pBegin, *pNext;
    ED_BufferOffset iBeginPos, iEndPos;
    XP_Bool bFromStart;

    // Get data from supplied selection or get current selection
    GetSelection( selection, pBegin, iBeginPos, pEnd, iEndPos, bFromStart );

    //
    // Guarantee that the text blocks of the beginning and end of selection
    //  are atomic.
    //
    MakeSelectionEndPoints(selection, pBegin, pEnd );
    pCurrent = pBegin;

    while( pCurrent != pEnd ){
        pNext = pCurrent->NextLeafAll();
        if( pCurrent->IsA( P_TEXT ) ){
            pCurrent->Text()->SetColor( iColor );
        }
        pCurrent = pNext;
    }
    if( bRelayout )
    {
        // force layout stop displaying the current selection.
#ifdef LAYERS
        LO_StartSelectionFromElement( m_pContext, 0, 0, NULL);
#else
        LO_StartSelectionFromElement( m_pContext, 0, 0);
#endif
        // probably needs to be the common ancestor.
        Relayout( pBegin, 0, pEnd, RELAYOUT_NOCARET );
        // Need to force selection.
        SelectRegion(pBegin, 0, pEnd, 0);
    }
    Reduce(pBegin->GetCommonAncestor(pEnd));
}

ED_ElementType CEditBuffer::GetBackgroundColor(LO_Color *pColor)
{
    if( !pColor )
        return ED_ELEMENT_NONE;

    XP_Bool bDefaultColors = FALSE;
    ED_ElementType type = ED_ELEMENT_TEXT;

    if( m_pSelectedEdTable )
    {
        // A table is selected
        type = ED_ELEMENT_TABLE;

        EDT_TableData *pData = GetTableData();
        if( pData && pData->pColorBackground )
        {
            pColor->red = pData->pColorBackground->red;
            pColor->green = pData->pColorBackground->green;
            pColor->blue = pData->pColorBackground->blue;
        } else {
            bDefaultColors = TRUE;
        }
    } else if( m_SelectedEdCells.Size() || IsInsertPointInTableCell() )
    {
        // Table cell(s) are selected or caret is in a cell
        type = ED_ELEMENT_CELL;

        EDT_TableCellData *pData = GetTableCellData();
        if( pData && pData->pColorBackground )
        {
            pColor->red = pData->pColorBackground->red;
            pColor->green = pData->pColorBackground->green;
            pColor->blue = pData->pColorBackground->blue;
        } else {
            bDefaultColors = TRUE;
        }
    } else {
        EDT_PageData *pData = GetPageData();
        if( pData && pData->pColorBackground )
        {
            pColor->red = pData->pColorBackground->red;
            pColor->green = pData->pColorBackground->green;
            pColor->blue = pData->pColorBackground->blue;
        } else {
            bDefaultColors = TRUE;
        }
    }

    if( bDefaultColors )
    {
        pColor->red = lo_master_colors[LO_COLOR_BG].red;
        pColor->green = lo_master_colors[LO_COLOR_BG].green;
        pColor->blue= lo_master_colors[LO_COLOR_BG].blue;
    }
    return type;
}

void CEditBuffer::SetBackgroundColor(LO_Color *pColor)
{
    if( m_pSelectedEdTable )
    {
        // A table is selected
        EDT_TableData *pData = GetTableData();
        if( pData )
        {
            if( pColor )
            {
                // Set the color - create struct if not already set
                if( !pData->pColorBackground )
                    pData->pColorBackground = XP_NEW( LO_Color );

                if( pData->pColorBackground )
                {
                    pData->pColorBackground->red = pColor->red;
                    pData->pColorBackground->green = pColor->green;
                    pData->pColorBackground->blue = pColor->blue;
                }
            } else {
                // We are clearing the current color
                XP_FREEIF(pData->pColorBackground);
            }
            SetTableData(pData);
            EDT_FreeTableData(pData);
        }
    } else if( m_SelectedEdCells.Size() || IsInsertPointInTableCell() )
    {
        // Table cells are selected or caret is in table
        EDT_TableCellData *pData = GetTableCellData();
        if( pData )
        {
            // We are only interested in setting the background color
            pData->mask = CF_BACK_COLOR;

            if( pColor )
            {
                if( !pData->pColorBackground )
                    pData->pColorBackground = XP_NEW( LO_Color );

                if( pData->pColorBackground )
                {
                    pData->pColorBackground->red = pColor->red;
                    pData->pColorBackground->green = pColor->green;
                    pData->pColorBackground->blue = pColor->blue;
                }
            } else {
                XP_FREEIF(pData->pColorBackground);
            }
            SetTableCellData(pData);
            EDT_FreeTableCellData(pData);
        }
    } else {
        EDT_PageData *pData = GetPageData();
        if( pData )
        {
            if( pColor )
            {
                if( !pData->pColorBackground )
                    pData->pColorBackground = XP_NEW( LO_Color );

                if( pData->pColorBackground )
                {
                    pData->pColorBackground->red = pColor->red;
                    pData->pColorBackground->green = pColor->green;
                    pData->pColorBackground->blue = pColor->blue;
                }
            } else {
                XP_FREEIF(pData->pColorBackground);
            }
            SetPageData(pData);
            EDT_FreePageData(pData);
        }
    }
}

void CEditBuffer::GetHREFData( EDT_HREFData *pData ){
    //TODO: Rewrite GetHREF to handle the target!
    ED_LinkId id = GetHREFLinkID();
    char* pURL = NULL;
    char* pExtra = NULL;
    if( id != ED_LINK_ID_NONE ){
        EDT_HREFData* pOriginal = linkManager.GetHREFData(id);
        if ( pOriginal ) {
            pURL = pOriginal->pURL;
            pExtra = pOriginal->pExtra;
        }
    }
    pData->pURL = pURL ? XP_STRDUP(pURL) : 0;
    pData->pExtra = pExtra ? XP_STRDUP(pExtra) : 0;
}

void CEditBuffer::SetHREFData( EDT_HREFData *pData ){
    SetHREF(pData->pURL, pData->pExtra);
}

void CEditBuffer::SetHREF( char *pHref, char *pExtra ){
    VALIDATE_TREE(this);
    ED_LinkId id;
    ED_LinkId oldId;
    CEditElement *pPrev, *pNext, *pStart, *pEnd;
    XP_Bool bSame;
    pStart = m_pCurrent;
    pEnd = 0;

    //
    // if we are setting link data and the current selection or word
    //  has extra data, we should preserve it.
    //
    if( pHref && *pHref!= '\0'){
        EDT_CharacterData* pCD = 0;
        if( pExtra == 0 ){
            pCD = GetCharacterData();
            if( pCD
                    && (pCD->mask & TF_HREF)
                    && (pCD->values & TF_HREF)
                    && pCD->pHREFData
                    && pCD->pHREFData->pExtra ){
                pExtra = pCD->pHREFData->pExtra;
            }
        }
        id  = linkManager.Add( pHref, pExtra );
        if( pCD ){
            EDT_FreeCharacterData( pCD );
        }
    }
    else {
        id = ED_LINK_ID_NONE;
    }

    CEditSelection selection;
    GetSelection(selection);
    if ( selection.IsContainerEnd() )
        return;
    else if( selection.AnyLeavesSelected() ){
        BeginBatchChanges(kChangeAttributesCommandID);
        SetHREFSelection( id );
        EndBatchChanges();
    }
    else {
        BeginBatchChanges(kChangeAttributesCommandID);
        //FixupInsertPoint();
        // check all text around us.
        oldId = m_pCurrent->GetHREF();
        m_pCurrent->SetHREF( id );
        pPrev = m_pCurrent->PreviousLeafInContainer();
        bSame = TRUE;
        while( pPrev && bSame ){
            bSame = pPrev->Leaf()->GetHREF() == oldId;
            if( bSame ){
                pPrev->Leaf()->SetHREF( id );
                pStart = pPrev;
            }
            pPrev = pPrev->PreviousLeafInContainer();
        }
        pNext = m_pCurrent->LeafInContainerAfter();
        bSame = TRUE;
        while( pNext && bSame ){
            bSame = pNext->Leaf()->GetHREF() == oldId;
            if( bSame ){
                pNext->Leaf()->SetHREF( id );
                pEnd = pNext;
            }
            pNext = pNext->LeafInContainerAfter();
        }
        Relayout( pStart, 0, pEnd );
        EndBatchChanges();
    }
    if( id != ED_LINK_ID_NONE ){
        linkManager.Free(id);
    }
}

// Use this only when setting one attribute at a time
// Always make Superscript and Subscript mutually exclusive - very tricky!
void CEditBuffer::FormatCharacter( ED_TextFormat tf ){
    VALIDATE_TREE(this);
    CEditSelection selection;
    GetSelection(selection);
    if ( selection.IsContainerEnd() )
        return;

    if( selection.AnyLeavesSelected() ){
        BeginBatchChanges(kChangeAttributesCommandID);
        FormatCharacterSelection(tf, selection, TRUE);
        EndBatchChanges();
        return;
    }
    if( IsTableOrCellSelected() )
    {
        // Default is to clear existing attribute
        // Use this if ANY cell has ANY bold elements at all
        intn iSetFormat = ED_CLEAR_FORMAT;

        // Get each cell contents as a selection
        //   and determine if we will set or clear the attribute
        if( GetFirstCellSelection(selection) )
        {
            // FALSE means we don't need to save/restore insert point
            XP_Bool bSetFormat = GetSetStateForCharacterSelection(tf, selection);
            if( bSetFormat )
            {
                XP_Bool bFoundCell; 
                while( (bFoundCell = GetNextCellSelection(selection)) == TRUE )
                {
                    // As soon as we hit a cell that wants us to clear format,
                    //  we are done -- we will clear format for ALL cells
                    if( !GetSetStateForCharacterSelection(tf, selection) )
                        break;
                }
                // If we ran through all cells without finding one
                //   to clear, then we should SET the format for all cells
                if( !bFoundCell )                
                    iSetFormat = ED_SET_FORMAT;
            }

            // Go through cells again and set the character format
            GetFirstCellSelection(selection);
            BeginBatchChanges(kChangeAttributesCommandID);
            // Format first cell
            FormatCharacterSelection(tf, selection, FALSE, iSetFormat );
            // Format all other cells
            while( GetNextCellSelection(selection) )
            {
                FormatCharacterSelection(tf, selection, FALSE, iSetFormat);
            }
            RelayoutSelectedTable();
            EndBatchChanges();
        }
        return;
    } 

    FixupInsertPoint();
    CEditLeafElement *pRight = m_pCurrent->Divide( m_iCurrentOffset )->Leaf();
    if( pRight->IsA(P_TEXT) && pRight->Leaf()->GetLen() == 0 ){
        if( tf == TF_NONE ){
            pRight->Text()->ClearFormatting();
        }
#ifdef USE_SCRIPT
        else if( tf == TF_SERVER || tf == TF_SCRIPT || tf == TF_STYLE ){
            pRight->Text()->ClearFormatting();
            pRight->Text()->m_tf = tf;
        }
#endif
        // If we are in either Java Script type, IGNORE change in format
        else if( ED_IS_NOT_SCRIPT(pRight) ){
            // Make superscript and subscript mutually-exclusive
            // if already either one type, and we are setting the opposite,
            //  clear this bit first
            if( tf == TF_SUPER && (pRight->Text()->m_tf & TF_SUB) ){
                pRight->Text()->m_tf &= ~TF_SUB;
            } else if( tf == TF_SUB && (pRight->Text()->m_tf & TF_SUPER) ){
                pRight->Text()->m_tf &= ~TF_SUPER;
            }
            // Fixed font attribute overrides font face
            if( tf == TF_FIXED && (pRight->Text()->m_tf & TF_FONT_FACE) ){
                pRight->Text()->m_tf &= ~TF_FONT_FACE;
            }
            pRight->Text()->m_tf ^= tf;
        }
        m_pCurrent = pRight;
        m_iCurrentOffset = 0;
    }
    else {
        // We are making new text element, so we don't need
        //   to worry about existing Java Script or Super/Subscript stuff
        CEditTextElement *pNew = m_pCurrent->CopyEmptyText();
        pNew->InsertBefore(pRight);
        if( tf == TF_NONE ){
            pNew->ClearFormatting();
        }
        else {
            if( tf == TF_SUPER && (pNew->Text()->m_tf & TF_SUB) ){
                pNew->Text()->m_tf &= ~TF_SUB;
            } else if( tf == TF_SUB && (pNew->Text()->m_tf & TF_SUPER) ){
                pNew->Text()->m_tf &= ~TF_SUPER;
            }
            pNew->m_tf ^= tf;
        }
        m_pCurrent = pNew;
        m_iCurrentOffset = 0;
    }
    Relayout( m_pCurrent, 0, 0 );
}

XP_Bool CEditBuffer::GetSetStateForCharacterSelection( ED_TextFormat tf, CEditSelection& selection )
{
    if( selection.IsEmpty() )
        return FALSE;
    //
    // Guarantee that the text blocks of the beginning and end of selection
    //  are atomic.
    //
//    MakeSelectionEndPoints(selection, pBegin, pEnd );
    // To be more efficient, do just what is necessary, since we only need the first element
    CEditLeafElement *pBegin = selection.m_start.m_pElement->Divide( selection.m_start.m_iPos )->Leaf();

    // We set the attribute if first element doesn't already have it,
    //   else we will clear existing attribute
    return !(pBegin->Text()->m_tf & tf);
}

void CEditBuffer::FormatCharacterSelection( ED_TextFormat tf, CEditSelection& selection,  XP_Bool bRelayout, intn iSetState )
{
    VALIDATE_TREE(this);
    CEditLeafElement *pCurrent, *pEnd, *pBegin, *pNext;
    XP_Bool bSet = (iSetState == ED_SET_FORMAT);
    // Passed in value determines if we figure out whether to set below,
    //   or just use state passed in.
    XP_Bool bHaveSet = (iSetState != ED_GET_FORMAT);

    // Get current selection if supplied with empty selection
    if( selection.IsEmpty() )
    {
        GetSelection(selection);
        bRelayout = TRUE;
    }

    //
    // Guarantee that the text blocks of the beginning and end of selection
    //  are atomic.
    //
    MakeSelectionEndPoints(selection, pBegin, pEnd );

    pCurrent = pBegin;

    while( pCurrent != pEnd && pCurrent )
    {
        pNext = pCurrent->NextLeafAll();
        XP_ASSERT(pNext);

        if( pCurrent->IsA( P_TEXT ) )
        {
            // figure out whether we are setting or clearing...
            if( !bHaveSet )
            {
                bSet = !(pCurrent->Text()->m_tf & tf);
                bHaveSet = TRUE;
            }

            if( tf == TF_NONE )
            {
                pCurrent->Text()->ClearFormatting();
            }
#ifdef USE_SCRIPT
            else if (tf == TF_SERVER || tf == TF_SCRIPT || tf == TF_STYLE)
            {
                pCurrent->Text()->ClearFormatting();
                pCurrent->Text()->m_tf = tf;
            }
#endif
            else 
            {
                if( bSet && pCurrent->IsA( P_TEXT ) )
                {
                    // If we are in either Java Script type, IGNORE change in format
                    if( ED_IS_NOT_SCRIPT(pCurrent) )
                    {
                        // Make superscript and subscript mutually-exclusive
                        if( tf == TF_SUPER && (pCurrent->Text()->m_tf & TF_SUB) )
                        {
                            pCurrent->Text()->m_tf &= ~TF_SUB;
                        } else if( tf == TF_SUB && (pCurrent->Text()->m_tf & TF_SUPER) )
                        {
                            pCurrent->Text()->m_tf &= ~TF_SUPER;
                        }
                        pCurrent->Text()->m_tf |= tf;
                    }
                    // Fixed font attribute overrides font face
                    if( tf == TF_FIXED && (pCurrent->Text()->m_tf & TF_FONT_FACE) )
                    {
                        pCurrent->Text()->m_tf &= ~TF_FONT_FACE;
                    }
                }
                else 
                {
                    pCurrent->Text()->m_tf &= ~tf;
                }
            }
        }
        else if( pCurrent->IsImage() && tf == TF_NONE )
        {
            // Clear the link for the image within the selection
            pCurrent->Image()->SetHREF( ED_LINK_ID_NONE );
        }
        pCurrent = pNext;
    }
	CEditSelection tmp(pBegin, 0, pEnd, 0);
    
    // Only do this (which basically does relayout)
    //  if working with current selection, not cell selection
    if( bRelayout )
        RepairAndSet(tmp);
}

void CEditBuffer::SetCharacterData( EDT_CharacterData *pData )
{
    VALIDATE_TREE(this);
    CEditSelection selection;
    GetSelection(selection);
    if ( selection.IsContainerEnd() )
        return;
    else if( selection.AnyLeavesSelected() ){
        BeginBatchChanges(kChangeAttributesCommandID);
        SetCharacterDataSelection( pData, selection, TRUE );
        EndBatchChanges();
        return;
    }
    
    if( IsTableOrCellSelected() )
    {
        // Select each cell and format the contents
        if( GetFirstCellSelection(selection) )
        {
            BeginBatchChanges(kChangeAttributesCommandID);
            // Format first selected cell, don't relayout yet
            SetCharacterDataSelection(pData, selection, FALSE);

            // Select all other cells and format them
            while( GetNextCellSelection(selection) )
            {
                SetCharacterDataSelection(pData, selection, FALSE);
            }
            RelayoutSelectedTable();
            EndBatchChanges();
        }
        return;
    } 

    FixupInsertPoint();
    CEditLeafElement *pRight = m_pCurrent->Divide( m_iCurrentOffset )->Leaf();
    if( pRight->IsA(P_TEXT) && pRight->Leaf()->GetLen() == 0 ){
        pRight->Text()->SetData( pData );
        m_pCurrent = pRight;
        m_iCurrentOffset = 0;
    }
    else {
        CEditTextElement *pNew = m_pCurrent->CopyEmptyText();
        pNew->InsertBefore(pRight);
        pNew->SetData( pData );
        m_pCurrent = pNew;
        m_iCurrentOffset = 0;
    }
    Relayout( m_pCurrent, 0, 0 );
}

void CEditBuffer::SetCharacterDataSelection( EDT_CharacterData *pData, CEditSelection& selection, XP_Bool bRelayout )
{
    VALIDATE_TREE(this);
    CEditLeafElement *pCurrent, *pEnd, *pBegin, *pNext;

    //
    // Guarantee that the text blocks of the beginning and end of selection
    //  are atomic.
    //
    MakeSelectionEndPoints( selection, pBegin, pEnd );
    pCurrent = pBegin;

    while( pCurrent != pEnd ){
        pNext = pCurrent->NextLeafAll();
        if( pCurrent->IsA( P_TEXT ) ){
            pCurrent->Text()->SetData( pData );
        }
        // Set just the HREF for an image
        else if( pCurrent->IsImage() &&
                 (pData->mask & TF_HREF) ){
            pCurrent->Image()->SetHREF( pData->linkId );
        }
        pCurrent = pNext;
    }

    if( bRelayout && !m_bNoRelayout )
    {
        // force layout stop displaying the current selection.
    #ifdef LAYERS
        LO_StartSelectionFromElement( m_pContext, 0, 0, NULL );
    #else
        LO_StartSelectionFromElement( m_pContext, 0, 0);
    #endif
        // probably needs to be the common ancestor.
        Relayout( pBegin, 0, pEnd, RELAYOUT_NOCARET );
        // Need to force selection.
        SelectRegion(pBegin, 0, pEnd, 0);
    }
    Reduce(pBegin->GetCommonAncestor(pEnd));
}

void CEditBuffer::SetCharacterDataAtOffset( EDT_CharacterData *pData,
                   ED_BufferOffset iBufOffset, int32 iLen ){

#if 0
    XP_ASSERT( m_bNoRelayout );  //put this back in when edt_paste does not relayout everything
#endif
    int32 i = iBufOffset+iLen;
    CPersistentEditSelection perSel;
    perSel = CPersistentEditSelection( CPersistentEditInsertPoint( iBufOffset ),
                CPersistentEditInsertPoint( i ) );

    CEditSelection sel = PersistentToEphemeral( perSel );

    SetCharacterDataSelection( pData, sel, TRUE );
}

void CEditBuffer::SetRefresh( XP_Bool bRefreshOn ){
    if( bRefreshOn ){
        m_bNoRelayout = FALSE;
        RefreshLayout();
    }
    else {
        m_bNoRelayout = TRUE;
    }
}


// Separated from GetCharacterData
// If pData is supplied, then multiple-selections may be combined
EDT_CharacterData* CEditBuffer::GetCharacterDataSelection(EDT_CharacterData *pData, CEditSelection& selection)
{
    // Get current selection if not supplied
    if( selection.IsEmpty() )
        GetSelection( selection );

    CEditElement *pElement = selection.m_start.m_pElement;

    while( pElement && pElement != selection.m_end.m_pElement ){
        if( pElement->IsA( P_TEXT ) ){
            pElement->Text()->MaskData( pData );
        }
        else if( pElement->IsImage() ){
            // We report the HREF status if we find an image
            pElement->Image()->MaskData( pData );
        }
        pElement = pElement->NextLeaf();
    }

    // if the selection ends in the middle of a text block.
    if( pElement && selection.m_end.m_iPos != 0
            && selection.m_end.m_pElement->IsA(P_TEXT) ){
        pElement->Text()->MaskData( pData );
    }

    if( pData == 0 ){
        return EDT_NewCharacterData();
    }
    return pData;
}

EDT_CharacterData* CEditBuffer::GetCharacterData()
{
	CEditElement *pElement = m_pCurrent;
    CEditSelection selection;

	// The following assert fails once at program startup.
	// XP_ASSERT(pElement || IsSelected());

    //
    // While selecting, we may not have a valid region
    //
    if( IsSelecting() ){
        return EDT_NewCharacterData();
    }

    if( IsSelected() ){
        return GetCharacterDataSelection(0, selection);
    }
    if( IsTableOrCellSelected() )
    {
        EDT_CharacterData *pData = 0;
        if( GetFirstCellSelection(selection) )
        {
            // Start with data from first cell
            pData = GetCharacterDataSelection(pData, selection);

            // Pass in pData to combine with other cell's data
            while( GetNextCellSelection(selection) )
                GetCharacterDataSelection(pData, selection);
        }
        return pData;
    } 

    if( pElement && pElement->IsA(P_TEXT ))
    {
        return pElement->Text()->GetData();;
    }

	// Try to find the previous text element and use its characterData
	if (pElement) {
		CEditTextElement *tElement = pElement->PreviousTextInContainer();
		if ( tElement){
			return tElement->GetData();
		}
	}

	// Create a dummy text element from scratch to guarantee
	// that we get the same EDT_CharacterData as when creating a new
	// CEditTextElement during typing.
	CEditTextElement dummy( (CEditElement *)0,0);
	return dummy.GetData();

	// return EDT_NewCharacterData();
}

char *CEditBuffer::GetTabDelimitedTextFromSelectedCells()
{
    char *pText = NULL;
    if( IsTableOrCellSelected() )
    {
        // Get each cell contents to examine - as if it were selected
        CEditTableCellElement *pCell = GetFirstSelectedCell();
        if( pCell )
        {
            // Get text from cell: TRUE = join paragraphs (no embeded CR/LF)
            pText = pCell->GetText(TRUE);
            if( pText )
            {
                intn iPrevCounter = 0;
                intn iRowCounter = 0;
                while( (pCell = GetNextSelectedCell(&iRowCounter)) != NULL )
                {
                    if( iRowCounter != iPrevCounter )
                    {
                        // We are in next row - so add row delimiter to previous text
                        pText = edt_AppendEndOfLine( pText);
                        iPrevCounter = iRowCounter;
                    } else {
                        // Next cell is in same row, so add TAB delimeter
                        pText = PR_sprintf_append( pText, "\t" );
                    }

                    // Append next cell's text
                    char *pCellText = pCell->GetText();
                    if( pCellText && *pCellText )
                        pText = PR_sprintf_append( pText, pCellText );
                }
                // Add last row delimiter at the end
                pText = edt_AppendEndOfLine( pText);
            }
        }
    } 
    return pText;
}

XP_Bool CEditBuffer::CanConvertTextToTable()
{
    if( IsSelected() )
    {    
        CEditLeafElement *pEndLeaf, *pBeginLeaf;
        ED_BufferOffset iBeginPos, iEndPos;
        XP_Bool bFromStart;
        // Get current selection
        CEditSelection selection;
        GetSelection( selection, pBeginLeaf, iBeginPos, pEndLeaf, iEndPos, bFromStart );
        if( selection.m_start.m_pElement && selection.m_end.m_pElement )
        {
            CEditTableCellElement *pStartCell = selection.m_start.m_pElement->GetParentTableCell();
            CEditTableCellElement *pEndCell =  selection.m_end.m_pElement->GetParentTableCell();
            
            // We can convert if neither element is inside a table
            //   or both are inside the same cell
            if( (!pStartCell && !pEndCell) || pStartCell == pEndCell )
                return TRUE;
        }
    }
    return FALSE;
}

CEditElement *GetCommonParent(CEditElement *pElement, CEditElement *pCommonAncestor)
{
    CEditElement *pParent = pElement;
    CEditElement *pCommonParent = pElement;

    while( (pParent = pParent->GetParent()) != NULL && pParent != pCommonAncestor )
        pCommonParent = pParent;

    return pCommonParent;
}

// Convert Selected text into a table (put each paragraph in separate cell)
// Number of rows is automatic - creates as many as needed
void CEditBuffer::ConvertTextToTable(intn iColumns)
{
    if( !CanConvertTextToTable() )
        return;
    
    // We need at least one column
    iColumns = max(1, iColumns);

    VALIDATE_TREE(this);
    CEditLeafElement *pEndLeaf, *pBeginLeaf;
    ED_BufferOffset iBeginPos, iEndPos;
    XP_Bool bFromStart;
    
    // Get current selection
    CEditSelection selection;
    GetSelection( selection, pBeginLeaf, iBeginPos, pEndLeaf, iEndPos, bFromStart );

    // Guarantee that the text blocks of the beginning and end of selection
    //  are atomic.
    MakeSelectionEndPoints(selection, pBeginLeaf, pEndLeaf );
    // Check for end of doc and backup to previous leaf
    if (selection.m_end.m_pElement && selection.m_end.m_pElement->IsEndOfDocument() )
    {
        selection.m_end.m_pElement = selection.m_end.m_pElement->PreviousLeaf();
        selection.m_end.m_iPos = selection.m_end.m_pElement->GetLen();
    }
    
    if( !selection.m_start.m_pElement || !selection.m_end.m_pElement )
        return;    

    CEditElement* pCommonAncestor = selection.m_start.m_pElement->GetCommonAncestor(selection.m_end.m_pElement);

    // Create and insert a table with 1 row 
    EDT_TableData *pData = EDT_NewTableData();
    if( !pData )
        return;
    pData->iColumns = iColumns;

    BeginBatchChanges(kGroupOfChangesCommandID);

    // Suppress relayout until we are all done
    m_bNoRelayout = TRUE;

    CEditElement *pCurrent, *pEnd, *pBegin, *pNext;

    // Save the start insert point,
    //  then set insert point to end and break into new containers
    CEditInsertPoint save_start = selection.m_start;
    selection.m_start = selection.m_end;
    SetSelection(selection);

    // TODO: Go to topmost parent, but we need to split at the 
    //       same level as the "common parent", not at nearest 
    //       container, as done in InternalReturnKey()
//    pEnd = GetCommonParent(selection.m_end.m_pElement, pCommonAncestor);
    // For initial testing, use just containers
    InternalReturnKey(FALSE);
    
    // Get container of this last element
    pEnd = (CEditElement*)selection.m_end.m_pElement->FindContainer();
    if( !pEnd )
        return;    

    // Insert the table at beginning of original selection
    // This will do necessary splitting of containers
    //   and set insert point to first cell in table
    selection.m_end = selection.m_start = save_start;
    SetSelection(selection);
    CEditTableElement *pTable = InsertTable(pData);
    EDT_FreeTableData(pData);

    // Get container after the table just inserted
    pBegin = pTable->GetNextSibling();
    if( !pBegin )
        return;
    // DONT do this when coalescing containers such as lists
    pBegin = pBegin->FindContainer();
    if( !pBegin )
        return;    

    pCurrent = pBegin;

    // Put each container in a different cell
    XP_Bool bDone = FALSE;
    CEditTableCellElement *pFirstCell = NULL;
    CEditTableCellElement *pCell = NULL;
    CEditTableElement *pExistingTable = pCurrent->GetParentTable();

    while( !bDone )
    {
        // Search down element tree to find the end container,
        //  since we might be working at a higher level
        CEditElement *pContainer = pCurrent;
        if( pContainer )
            while (pContainer != pEnd && pContainer->GetChild() )
                pContainer = pContainer->GetChild();

        bDone = (pCurrent == NULL ) || (pContainer == pEnd);

        pNext = (CEditElement*)pCurrent->GetNextSibling();
        if( !pNext )
            pNext = (CEditElement*)pCurrent->NextContainer();

        // Move container into table cell,
        //  inserting it as first container in the cell
        pCell = m_pCurrent->GetParentTableCell();
        if( pCell )
        {
            // Save first cell to move caret when done
            if( !pFirstCell )
                pFirstCell = pCell;

            pCurrent->Unlink();

            // Insert the container and its children as the 1st child of current cell
            pCurrent->InsertAsFirstChild(pCell);

            // Set insert point inside first leaf element just moved,
            //   unless it was a table
            if( !pCurrent->IsTable() )
            {
                CEditElement *pFirstChild = pCurrent->GetFirstMostChild();
                CEditInsertPoint ip(pFirstChild, 0);
                SetInsertPoint(ip);

                // Delete the default container created with the table
                //    which contained the caret moved there by InsertTable()
                CEditElement *pDefaultContainer = pCurrent->GetNextSibling();
                if( pDefaultContainer )
                    delete pDefaultContainer;
            }
        } else {
            // Should never be here:
            XP_ASSERT(TRUE);
        }

        // Move to the next cell to insert next container, if it exists
        if( !bDone && !NextTableCell(FALSE, TRUE) )
        {
            // We need to create a new row
            
            // Get current row
            CEditElement* pRow = pCell->GetParent();
            if( !pRow )
                break;

            CEditTableRowElement* pNewRow = new CEditTableRowElement(iColumns);
            if( !pNewRow )
                break;

            // Insert new row after current
            pNewRow->InsertAfter(pRow);
            // Initialize cell contents
            pNewRow->FinishedLoad( this );

            // Now move to the first cell in this new row
            if( !NextTableCell(FALSE, TRUE) )
                break;
        }
        if( pNext == NULL ) // Should never happen?
            break;

        // Check if next element is inside a table
        //   abort if it isn't same as where starting element was
        CEditTableElement *pTable = pNext->GetParentTable();
        if( pTable )
        {
            // We probably want to put the entire table in next cell...
            pNext = (CEditElement*)pTable;

            // ..but first walk up nested tables to find appropriate level
            while( (pTable = pTable->GetParentTable()) != pExistingTable )
            {
                if( pTable )
                    pNext = (CEditElement*)pTable;
            }
        }
        pCurrent = pNext;
    }

    if( pFirstCell )
        SetTableInsertPoint(pFirstCell);

    SetFillNewCellWithSpace();

    // Finish initializing the remaining blank cells in the last row
    while( (pCell = (CEditTableCellElement*)pCell->GetNextSibling()) != NULL )
        pCell->FinishedLoad(this);

    ClearFillNewCellWithSpace();

    m_bNoRelayout = FALSE;

    Relayout( pBegin, 0, pEnd );

    EndBatchChanges();
}

// Convert the table into text - unravel existing paragraphs in cells
void CEditBuffer::ConvertTableToText()
{
    CEditTableElement *pTable;
    
    // Get the table to convert
    if( m_pSelectedEdTable )
    {
        pTable = m_pSelectedEdTable;
    }
    else
    {
        CEditInsertPoint ip;
        GetTableInsertPoint(ip);
        if( ip.m_pElement )
            pTable = ip.m_pElement->GetParentTable();
    }
    if( pTable )
    {
        // Be sure we don't have anything in this table selected
        ClearTableAndCellSelection();

        // Get the previous container to append to
        CEditContainerElement* pPrevContainer = pTable->PreviousContainer();
        CEditElement* pNextContainer = NULL;
        CEditElement* pLastContainer = NULL;
        if( !pPrevContainer )
            return; //TODO: CAN THIS HAPPEN??? HANLDE IT IF YES!

        CEditElement* pAppendContainer = pPrevContainer;
        CEditElement* pFirstContainer = NULL;
        CEditElement *pContainer = NULL;

        // Move all containers in each cell to previous container
        CEditTableCellElement *pCell = pTable->GetFirstCell();
        if( pCell )
        {
            BeginBatchChanges(kGroupOfChangesCommandID);
            while( pCell )
            {
                // Get first container of the cell
                pContainer = pCell->GetChild();
                XP_ASSERT(pContainer->IsContainer());

                // Save very first container while we're here
                if( pFirstContainer == NULL )
                    pFirstContainer = pContainer;

                // Move all containers
                while( pContainer )
                {
                    CEditElement* pNextContainer = pContainer->GetNextSibling();
                    pContainer->Unlink();
                    pContainer->InsertAfter(pAppendContainer);
                    pAppendContainer = pContainer;

                    // Save the last container moved
                    if( pContainer )
                        pLastContainer = pContainer;

                    pContainer = pNextContainer;
                }
                pCell = pTable->GetNextCellInTable();
            }
            
            // Connect the last moved container to
            //   the next one after table we are deleting
            if( pLastContainer )
            {
                pNextContainer = pLastContainer->NextContainer();
            }
            delete pTable;

            if( pNextContainer )
                pLastContainer->SetNextSibling(pNextContainer);


            if( pNextContainer && pLastContainer )
                pLastContainer->SetNextSibling(pNextContainer);

            // Move insert point to first moved element
            if( pFirstContainer )
            {
                CEditInsertPoint ip(pFirstContainer->GetFirstMostChild(), 0);
                SetInsertPoint(ip);
            }        
            if( pNextContainer )
            {
                Relayout(pPrevContainer->GetFirstMostChild(), 0, pNextContainer->GetFirstMostChild(), 0);
            } else {
                // Deep doodoo if we didn't get all containers around the table!
                XP_ASSERT(FALSE);
            }

#ifdef DEBUG
            m_pRoot->ValidateTree();
#endif

            EndBatchChanges();
        }
    }
}

// Save the character and paragraph style of selection or at caret
void CEditBuffer::CopyStyle()
{
    if( m_pCopyStyleCharacterData )
        EDT_FreeCharacterData(m_pCopyStyleCharacterData);
    
    m_pCopyStyleCharacterData = GetCharacterData();
}

// Apply the style to selection or at caret. Use bApplyStyle = FALSE to cancel
void CEditBuffer::PasteStyle(XP_Bool bApplyStyle)
{
    if( m_pCopyStyleCharacterData )
    {
        if( bApplyStyle )
        {
            SetCharacterData(m_pCopyStyleCharacterData);
        }
        EDT_FreeCharacterData(m_pCopyStyleCharacterData);
        m_pCopyStyleCharacterData = NULL;
    }
}

EDT_PageData* CEditBuffer::GetPageData(){
    EDT_PageData* pData = EDT_NewPageData();

    pData->pColorBackground = edt_MakeLoColor( m_colorBackground );
    pData->bBackgroundNoSave = m_bBackgroundNoSave;
    pData->pColorText = edt_MakeLoColor( m_colorText );
    pData->pColorLink = edt_MakeLoColor( m_colorLink );
    pData->pColorFollowedLink = edt_MakeLoColor( m_colorFollowedLink );
    pData->pColorActiveLink = edt_MakeLoColor( m_colorActiveLink );

    pData->pTitle = XP_STRDUP( ( m_pTitle ? m_pTitle : "" ) );
    pData->pBackgroundImage = ( m_pBackgroundImage
                    ? XP_STRDUP( m_pBackgroundImage )
                    : 0 );
    pData->pFontDefURL = ( m_pFontDefURL
                    ? XP_STRDUP( m_pFontDefURL )
                    : 0 );
    pData->bFontDefNoSave = m_bFontDefNoSave;
    return pData;
}

void CEditBuffer::FreePageData( EDT_PageData* pData ){
    if ( pData->pColorBackground ) XP_FREE( pData->pColorBackground );
    if ( pData->pColorText ) XP_FREE( pData->pColorText );
    if ( pData->pColorLink ) XP_FREE( pData->pColorLink );
    if ( pData->pColorFollowedLink ) XP_FREE( pData->pColorFollowedLink );
    if ( pData->pColorActiveLink ) XP_FREE( pData->pColorActiveLink );
    if ( pData->pTitle ) XP_FREE( pData->pTitle );
    if ( pData->pBackgroundImage ) XP_FREE( pData->pBackgroundImage );
    if ( pData->pFontDefURL ) XP_FREE( pData->pFontDefURL );

    XP_FREE( pData );
}

void CEditBuffer::SetPageData( EDT_PageData* pData ){
    m_colorBackground = EDT_LO_COLOR( pData->pColorBackground );
    m_colorLink = EDT_LO_COLOR( pData->pColorLink );
    m_colorText = EDT_LO_COLOR( pData->pColorText );
    m_colorFollowedLink = EDT_LO_COLOR( pData->pColorFollowedLink );
    m_colorActiveLink = EDT_LO_COLOR( pData->pColorActiveLink );

    // While we would like to change doc title only if it
    //   is really different, there is the case when both existing
    //   m_pTitle and the new pData->pTitle are NULL and we 
    //   need to always change the title in that case since
    //   we may be changing an existing page with no title
    //   into an "Untitled" page. This happens when we open a template file
    //   or we import a text file
    XP_Bool bNoChange = FALSE;

    if( m_pTitle && pData->pTitle && 0 == XP_STRCMP(m_pTitle, pData->pTitle) )
    {
        bNoChange = TRUE;
    }

    if( !bNoChange )
    {
        if( m_pTitle ) XP_FREE( m_pTitle );
        m_pTitle = (pData->pTitle ? XP_STRDUP( pData->pTitle ) : NULL);
        FE_SetDocTitle( m_pContext, m_pTitle );
    }
    
    //    If there was an image, and now there isn't remove it.   
    if(m_pBackgroundImage != NULL)  {
        XP_FREE(m_pBackgroundImage);
        m_pBackgroundImage = NULL;
    }

    if( pData->pBackgroundImage ){  
        m_pBackgroundImage = XP_STRDUP(pData->pBackgroundImage);  
    }  
    m_bBackgroundNoSave = pData->bBackgroundNoSave;

    // Font reference
    if(m_pFontDefURL != NULL)  {
        XP_FREE(m_pFontDefURL);
        m_pFontDefURL = NULL;
    }
    if( pData->pFontDefURL ){  
        m_pFontDefURL = XP_STRDUP(pData->pFontDefURL);  
    }  
    m_bFontDefNoSave = pData->bFontDefNoSave;

    // Note: all LO_SetDocumentColor calls
    //       and LO_SetBackgroundImage are done in RefreshLayout

    // Clear bit used to set color only once when multiple colors
    //   are in mail message. Fix for bug 64850
    // If we don't, color doesn't change when user wants to!
    lo_TopState *top_state = lo_FetchTopState(XP_DOCID(m_pContext));
    if( top_state ){
        top_state->body_attr &= ~BODY_ATTR_TEXT;
    }
    RefreshLayout();
}

void CEditBuffer::SetImageAsBackground()
{
    EDT_PageData * pPageData = GetPageData();
    if (pPageData == NULL) {
        return;
    }
    EDT_ImageData* pImageData = NULL;
    
    if( IsSelected() && GetCurrentElementType() == ED_ELEMENT_IMAGE ){
        pImageData = GetImageData();
        if( pImageData ){
            XP_FREEIF(pPageData->pBackgroundImage);
            pPageData->pBackgroundImage = XP_STRDUP(pImageData->pSrc);

            BeginBatchChanges(kGroupOfChangesCommandID);
            // Delete image from page
//            DeleteSelection();
            SetPageData(pPageData);

            EDT_FreeImageData(pImageData);
            EndBatchChanges();
        }
    }
    EDT_FreePageData(pPageData);
}

// MetaData

intn CEditBuffer::MetaDataCount( ){
    intn i = 0;
    intn iRetVal = 0;
    while( i < m_metaData.Size() ){
        if( m_metaData[i] != 0 ){
            iRetVal++;
        }
        i++;
    }
    return iRetVal;
}

intn CEditBuffer::FindMetaData( EDT_MetaData *pMetaData ){
    return FindMetaData(pMetaData->bHttpEquiv, pMetaData->pName);
}

intn CEditBuffer::FindMetaData(XP_Bool bHttpEquiv, char* pName){
    intn i = 0;
    intn iRetVal = 0;
    while( i < m_metaData.Size() ){
        if( m_metaData[i] != 0 ){
            if( (!!bHttpEquiv == !!m_metaData[i]->bHttpEquiv)
                    && XP_STRCMP( m_metaData[i]->pName, pName) == 0 ){
                return iRetVal;
            }
            iRetVal++;
        }
        i++;
    }
    return -1;
}

intn CEditBuffer::FindMetaDataIndex( EDT_MetaData *pMetaData ){
    intn i = 0;
    while( i < m_metaData.Size() ){
        if( m_metaData[i] != 0 ){
            if( (!!pMetaData->bHttpEquiv == !!m_metaData[i]->bHttpEquiv)
                    && XP_STRCMP( m_metaData[i]->pName, pMetaData->pName) == 0 ){
                return i;
            }
        }
        i++;
    }
    return -1;
}

EDT_MetaData* CEditBuffer::MakeMetaData( XP_Bool bHttpEquiv, char *pName, char*pContent ){
    EDT_MetaData *pData = XP_NEW( EDT_MetaData );
    XP_BZERO( pData, sizeof( EDT_MetaData ) );

    pData->bHttpEquiv = bHttpEquiv;
    pData->pName = (pName
                            ? XP_STRDUP(pName)
                            : 0);
    pData->pContent = (pContent
                            ? XP_STRDUP(pContent)
                            : 0);
    return pData;
}

EDT_MetaData* CEditBuffer::GetMetaData( int n ){
    intn i = 0;
//    intn iRetVal = 0;
    n++;
    while( n ){
        if ( i >= m_metaData.Size() ) {
            return NULL;
        }
        if( m_metaData[i] != 0 ){
            n--;
        }
        i++;
    }
    i--;
    if ( i < 0 ) {
        return NULL;
    }
    return MakeMetaData( m_metaData[i]->bHttpEquiv,
                         m_metaData[i]->pName,
                         m_metaData[i]->pContent );
}

void CEditBuffer::SetMetaData( EDT_MetaData *pData ){
    intn i = FindMetaDataIndex( pData );
    if ( ! (pData && pData->pContent && *(pData->pContent)) ) {
        // We've been asked to erase an entry
        if ( i == -1 ) {
            // We've been asked to erase an entry that we don't have. So just return.
            return;
        }
        else {
            // Erase the entry.
            FreeMetaData( m_metaData[i] );
            m_metaData[i] = NULL;
        }
    }
    else {
        EDT_MetaData* pNew = MakeMetaData( pData->bHttpEquiv, pData->pName, pData->pContent );
        if( i == -1 ){
            // Add an entry
            m_metaData.Add( pNew );
        }
        else {
            // Replace an entry
            FreeMetaData( m_metaData[i] );
            m_metaData[i] = pNew;
        }
    }
}

void CEditBuffer::DeleteMetaData( EDT_MetaData *pData ){
    intn i = FindMetaDataIndex( pData );
    if( i != -1 ){
        FreeMetaData( m_metaData[i] );
        m_metaData[i] = 0;
    }
}

void CEditBuffer::FreeMetaData( EDT_MetaData *pData ){
    if ( pData ) {
        if( pData->pName ) XP_FREE( pData->pName );
        if( pData->pContent ) XP_FREE( pData->pContent );
        XP_FREE( pData );
    }
}

void CEditBuffer::ParseMetaTag( PA_Tag *pTag ){
    XP_Bool bHttpEquiv = TRUE;
    char *pName;
    char *pContent;
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_pContext);
    int16 win_csid = INTL_GetCSIWinCSID(c);

    pContent = edt_FetchParamString( pTag, PARAM_CONTENT, win_csid );
    pName = edt_FetchParamString( pTag, PARAM_HTTP_EQUIV, win_csid );

    // if we didn't get http-equiv, try for name=
    if( pName == 0 ){
        bHttpEquiv = FALSE;
        pName = edt_FetchParamString( pTag, PARAM_NAME, win_csid );
    }

    // if we got one or the other, add it to the list of meta tags.
    if( pName ){
        EDT_MetaData *pData = MakeMetaData( bHttpEquiv, pName, pContent );
        SetMetaData( pData );
        FreeMetaData( pData );
    }

    if( pName ) XP_FREE( pName );
    if( pContent ) XP_FREE( pContent );
}


// Image

EDT_ImageData* CEditBuffer::GetImageData(){
    CEditLeafElement *pInsertPoint;
    ElementOffset iOffset;
    XP_Bool bSingleItem;

    bSingleItem = GetPropertyPoint( &pInsertPoint, &iOffset );
    XP_ASSERT( bSingleItem );

    XP_ASSERT( pInsertPoint->IsA(P_IMAGE) );

    return pInsertPoint->Image()->GetImageData( );
}

int32 CEditBuffer::GetDefaultBorderWidth(){
    CEditLeafElement *pInsertPoint;
    ElementOffset iOffset;
    XP_Bool bSingleItem;

    // Just check for consistency, should both have a link or both not.
    bSingleItem = GetPropertyPoint( &pInsertPoint, &iOffset );
    if (bSingleItem && pInsertPoint->IsA(P_IMAGE)) {
        ED_LinkId fromImage = pInsertPoint->Image()->GetHREF();
        char *fromEdtBuf = GetHREF();
        XP_ASSERT((fromImage && fromEdtBuf) || (!fromImage && !fromEdtBuf));
    }

    // Duplicating code in CEditImageElement::GetDefaultBorder, but at
    // least its all in the back end now.
    if (GetHREF()) {
        return 2;
    }
    else {
        return 0;
    }
}


#define XP_STRCMP_NULL( a, b ) XP_STRCMP( (a)?(a):"", (b)?(b):"" )

void CEditBuffer::SetImageData( EDT_ImageData* pData, XP_Bool bKeepImagesWithDoc ){
    VALIDATE_TREE(this);
    ClearSelection( TRUE, TRUE );

    XP_ASSERT( m_pCurrent->IsA(P_IMAGE) );

    // This function is only called when changing attributes of an image, not
    //   when inserting an image.
    //
    // There are 4 cases...
    //
    // 1) They change the src of the image, but not the size of the image
    //      (assume the dimensions of the new image).
    // 2) They change the src and the size of the image
    // 3) They change just the size of the image.
    // 4) They reset the size of the image.
    //

    CEditImageElement *pImage = m_pCurrent->Image();
    EDT_ImageData *pOldData = m_pCurrent->Image()->GetImageData();

    // Case 1, 2, or 4
    if( (XP_STRCMP( pOldData->pSrc, pData->pSrc ) != 0 )
            || XP_STRCMP_NULL( pOldData->pLowSrc, pData->pLowSrc )
            || (pData->iHeight == 0 || pData->iWidth== 0) ){


        // if they touched height or width, assume they know what they
        //  are doing.
        //
        if( pData->iHeight == pOldData->iHeight
                && pData->iWidth == pOldData->iWidth ){

            // LoadImage will get the image size from the image.
            pData->iHeight = 0;
            pData->iWidth = 0;
        }

        LoadImage( pData, bKeepImagesWithDoc, TRUE );
        edt_FreeImageData( pOldData );
    }
    else {
        m_pCurrent->Image()->SetImageData( pData );
        Relayout( m_pCurrent->FindContainer(), 0, m_pCurrent, RELAYOUT_NOCARET );
        SelectCurrentElement();
    }
    // Also set the HREF if it exists
    if( pData->pHREFData ){
        if( !pData->pHREFData->pURL || XP_STRLEN(pData->pHREFData->pURL) == 0 ){
            pImage->SetHREF( ED_LINK_ID_NONE );
        } else {
            // Add link data to the image
            pImage->SetHREF( linkManager.Add( pData->pHREFData->pURL,
                                              pData->pHREFData->pExtra) );
        }
    }
}

void CEditBuffer::LoadImage( EDT_ImageData* pData, XP_Bool /*bKeepImagesWithDoc*/,
            XP_Bool bReplaceImage ){

    XP_Bool bMakeAbsolute = TRUE;

    if( m_pLoadingImage != 0 ){
        XP_ASSERT(0);       // can only be loading one image at a time.
        return;
    }

    if( bMakeAbsolute ){
        m_pLoadingImage = new CEditImageLoader( this, pData, bReplaceImage );
        m_pLoadingImage->LoadImage();
    }

}

//
// need to do a better job of splitting and inserting in the right place.
//
void CEditBuffer::InsertImage( EDT_ImageData* pData ){
    if( IsSelected() ){
        ClearSelection();
    }
    CEditImageElement *pImage = new CEditImageElement(0);
    pImage->SetImageData( pData );
    // Add link data to the image
    if( pData->pHREFData && pData->pHREFData->pURL &&
        XP_STRLEN(pData->pHREFData->pURL) > 0){
        pImage->SetHREF( linkManager.Add( pData->pHREFData->pURL,
                                          pData->pHREFData->pExtra) );
    }
    InsertLeaf( pImage );
}


void CEditBuffer::InsertLeaf( CEditLeafElement *pLeaf ){
    VALIDATE_TREE(this);
    FixupInsertPoint();
    if( m_iCurrentOffset == 0 ){
        pLeaf->InsertBefore( m_pCurrent );
    }
    else {
        m_pCurrent->Divide(m_iCurrentOffset);
        pLeaf->InsertAfter( m_pCurrent );
    }
    m_pCurrent = pLeaf;
    m_iCurrentOffset = 1;

    if ( ! m_bNoRelayout ){
        Reduce(pLeaf->FindContainer());
    }

    // we could be doing a better job here.  We probably repaint too much...
    CEditElement *pContainer = m_pCurrent->FindContainer();
    Relayout( pContainer, 0, pContainer->GetLastMostChild() );
}

void CEditBuffer::InsertNonLeaf( CEditElement *pElement){
    VALIDATE_TREE(this);
    FixupInsertPoint();
    XP_ASSERT( ! pElement->IsLeaf());
    CEditInsertPoint start(m_pCurrent, m_iCurrentOffset);
	CEditLeafElement* pRight = NULL;
    
#if 0
    //TODO: Something like this
    // Don't do InternalReturnKey if at beginning of empty container?
    CEditElement* pParent = pElement->GetParent();
    if( pParent && pParent->IsContainer() && 
        pElement->IsText() &&
        pElement->GetNextSibling() == NULL )
    {
        char* pText = pElement->Text()->GetText();
    } else {
        InternalReturnKey(FALSE);
    }
#else
    InternalReturnKey(FALSE);
#endif

    pRight = m_pCurrent;
    CEditLeafElement* pLeft = pRight->PreviousLeaf(); // Will be NULL at start of document
    pElement->InsertBefore(pRight->FindContainer());
    pElement->FinishedLoad(this);

#ifdef DEBUG
    m_pRoot->ValidateTree();
#endif
	// Put cursor at the end of what has been inserted

    if ( pRight->IsEndOfDocument() ) {
        m_pCurrent = pRight->PreviousLeaf();
        m_iCurrentOffset = m_pCurrent->GetLen();
    }
    else {
        m_pCurrent = pRight;
        m_iCurrentOffset = 0;
   }

    Relayout( pElement, 0, pRight );
}

// Table stuff

void CEditBuffer::GetTableInsertPoint(CEditInsertPoint& ip){
    GetInsertPoint(ip);
    CEditSelection s;
    GetSelection(s);
    if ( ! s.IsInsertPoint() && s.m_end == ip && ip.IsStartOfContainer()){
        ip = ip.PreviousPosition();
    };
}

XP_Bool CEditBuffer::SetTableInsertPoint(CEditTableCellElement *pCell, XP_Bool bStartOfCell )
{
    // Move the insert point to new cell,
    if( pCell )
    {
        CEditElement *pChild;
        if( bStartOfCell )
            pChild = pCell->GetFirstMostChild();
        else
            pChild = pCell->GetLastMostChild();
        
        if( pChild && pChild->IsLeaf() )
        {
            CEditInsertPoint ip(pChild, bStartOfCell ? 0 : pChild->Leaf()->GetLen());
            SetInsertPoint(ip);
            return TRUE;
#ifdef DEBUG
        } else {
            XP_ASSERT(TRUE);
#endif
        }
    }
    return FALSE;
}

XP_Bool CEditBuffer::IsInsertPointInTable(){
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    return ip.m_pElement->GetTableIgnoreSubdoc() != NULL;
}

XP_Bool CEditBuffer::IsInsertPointInNestedTable(){
    XP_Bool result = FALSE;
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableElement* pFirstTable = ip.m_pElement->GetTableIgnoreSubdoc();
    if ( pFirstTable ) {
        CEditTableElement* pSecondTable = pFirstTable->GetParent()->GetTableIgnoreSubdoc();
        if ( pSecondTable ) {
            result = TRUE;
        }
    }
    return result;
}

EDT_TableData* CEditBuffer::GetTableData(){
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableElement* pTable = ip.m_pElement->GetTableIgnoreSubdoc();
    if ( pTable ){
        return pTable->GetData();
    }
    else {
        return NULL;
    }
}

// NOTE: Caller should manage BeginBatchChanges/EndBatchChanges
void CEditBuffer::SetTableData(EDT_TableData *pData)
{
    VALIDATE_TREE(this);
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableElement* pTable = ip.m_pElement->GetTableIgnoreSubdoc();
    if ( pTable )
    {
        XP_Bool bChangeWidth = FALSE;
        XP_Bool bChangeHeight = FALSE; 
        int32 iMaxWidth = 0;
        int32 iMaxHeight;                
        intn iCurrentCols = pTable->GetColumns();
        intn iCurrentRows = pTable->GetRows();
        CEditTableCellElement *pCell = NULL;
        XP_Bool bMovedToLastCell = FALSE;

        // Be sure we have at least 1 row and column
        pData->iColumns = max(1, pData->iColumns);
        pData->iRows = max(1, pData->iRows);

        // Add new rows or columns as needed
        // Note: InsertTableColumns and InsertTableRows will call Relayout() 
        if( pData->iColumns > iCurrentCols )
        {
            // Append new columns to end of table
            // Get first cell in last column
            pCell = pTable->GetFirstCellAtColumnIndex(iCurrentCols-1);
            if( pCell )
            {
                SetTableInsertPoint(pCell);
                InsertTableColumns(NULL, TRUE, pData->iColumns - iCurrentCols);
                bMovedToLastCell = TRUE;
            }
        } 
        else if( pData->iColumns < iCurrentCols )
        {
            // Delete columns from right side of table
            // TODO: PUT UP FE_CONFIRM DIALOG TO ASK USER
            //       IF IT IS OK TO DELETE WHEN REDUCING SIZE?
            // (or maybe let each front end do that?)
            
            // Move to first cell in new last column
            pCell = pTable->GetFirstCellAtColumnIndex(pData->iColumns-1);
            if( pCell )
            {
                // Delete commands assume caret is in the delete column
                SetTableInsertPoint(pCell);

                // This will relayout the table and move caret appropriately
        		AdoptAndDo(new CDeleteTableColumnCommand(this, iCurrentCols - pData->iColumns));

                // Don't try to go back to original insert point,
                //  it may have been deleted
                bMovedToLastCell = FALSE;
            } else {
                // TODO: PUT UP AN FE_ALERT MESSAGE TO TELL USER WHY 
                //        THEY COULDN'T REDUCE COLUMNS?
                pData->iColumns = iCurrentCols;
                XP_TRACE(("CEditBuffer::SetTableData: Failed to delete columns to reduce table size"));
            }
        }

        if( pData->iRows > iCurrentRows )
        {
            // Move to first cell in last row of table
            pCell = pTable->GetFirstCellAtRowIndex(iCurrentRows-1);
            if( pCell )
            {
                SetTableInsertPoint(pCell);
                // Append new rows to end of table
                InsertTableRows(NULL, TRUE, pData->iRows - iCurrentRows);
                bMovedToLastCell = TRUE;
            }
        }
        else if( pData->iRows < iCurrentRows )
        {
            // Delete rows from bottom of table
            // Move to first cell in new last row
            pCell = pTable->GetFirstCellAtRowIndex(pData->iRows-1);
            if( pCell )
            {
                // Delete commands assume caret is in the delete row
                SetTableInsertPoint(pCell);

                // This will relayout the table and move caret appropriately
        		AdoptAndDo(new CDeleteTableRowCommand(this, iCurrentRows - pData->iRows));

                // Don't try to go back to original insert point,
                //  it may have been deleted
                bMovedToLastCell = FALSE;
            } else {
                // TODO: PUT UP AN FE_ALERT MESSAGE TO TELL USER WHY 
                //        THEY COULDN'T REDUCE ROWS?
                pData->iRows = iCurrentRows;
                XP_TRACE(("CEditBuffer::SetTableData: Failed to delete rows to reduce table size"));
            }
        }

        // Move back to where we were if we expanded size
        if( bMovedToLastCell )
            SetInsertPoint(ip);

        // Calculate the Pixel values - FEs are only required to set iWidth and iHeight
        // Determine if we are changing the table width or height
        if( pData->bWidthDefined )
        {
            if( pData->bWidthPercent )
            {
                pTable->GetParentSize(m_pContext, &iMaxWidth, &iMaxHeight);
                pData->iWidthPixels = (pData->iWidth * iMaxWidth) / 100;
            }
            else
                pData->iWidthPixels = pData->iWidth;

            bChangeWidth = (pData->iWidthPixels != pTable->GetWidth());
        }
        if( pData->bHeightDefined )
        {
            if( pData->bHeightPercent )
            {
                if( iMaxWidth == 0 )
                    pTable->GetParentSize(m_pContext, &iMaxWidth, &iMaxHeight);
                pData->iHeightPixels = (pData->iHeight * iMaxHeight) / 100;
            }
            else
                pData->iHeightPixels = pData->iHeight;

            bChangeHeight = (pData->iHeightPixels != pTable->GetHeight());
        }

        pTable->SetData( pData );

        if( bChangeWidth || bChangeHeight )
        {
            // This will change cell data to allow table resize to work correctly,
            //  then restore previous settings
            ResizeTable( pTable, bChangeWidth, bChangeHeight );
        } else {
            // Normal relayout is OK if not resizing
            Relayout( pTable, 0 );
        }
        // Update the size data to reflect changes made by Layout
        pData->iWidth = pTable->GetWidthOrPercent();
        pData->iWidthPixels = pTable->GetWidth();
        pData->iHeight = pTable->GetHeightOrPercent();
        pData->iHeightPixels = pTable->GetHeight();
    }
}

CEditTableElement* CEditBuffer::InsertTable(EDT_TableData *pData){
    CEditTableElement *pTable = new CEditTableElement(pData->iColumns, pData->iRows);
    if( pTable ){
        pTable->SetData( pData );
        SetFillNewCellWithSpace();
        pTable->FinishedLoad(this); // Sets default paragraphs for all the cells
        if( IsSelected() ){
            ClearSelection();
        }
        InsertNonLeaf(pTable);
        // Set insert point inside first cell in table
        CEditElement *pFirstChild = pTable->GetFirstMostChild();
        CEditInsertPoint ip(pFirstChild, 0);
        SetInsertPoint(ip);
        ClearFillNewCellWithSpace();
        return pTable;
    }
    return NULL;
}

void CEditBuffer::DeleteTable(){
    VALIDATE_TREE(this);
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableElement* pTable = ip.m_pElement->GetTableIgnoreSubdoc();
    if ( pTable ){
        BeginBatchChanges(kGroupOfChangesCommandID);
		AdoptAndDo(new CDeleteTableCommand(this));
        EndBatchChanges();
    }
}

/* Use to enable/disable Merge Cells feature. 
 * Selected cells can be merged only if in a continuous set
 * within the same row or column
*/
ED_MergeType CEditBuffer::GetMergeTableCellsType()
{
    // Need at least 2 cells to merge
    CEditTableCellElement* pTableCell = NULL;
    int iCount = m_SelectedEdCells.Size();
    if( iCount == 1 )
    {
        pTableCell = m_SelectedEdCells[0];
    } else if(iCount == 0 ){
        CEditInsertPoint ip;
        GetTableInsertPoint(ip);
        pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    }
    if( pTableCell && pTableCell->GetNextSibling() )
    {
        // We have a single cell with a neighbor to the right
        return ED_MERGE_NEXT_CELL;
    }
    
    // Nothing to merge
    if( iCount < 2 )
        return ED_MERGE_NONE;
#if 0
//  TODO: REVISIT THIS
    // If any selected cell is not inline with all others,
    //  don't allow merging
    int32 left = m_SelectedLoCells[0]->x;
    int32 top = m_SelectedLoCells[0]->y;
    XP_Bool bSameTop = (m_SelectedLoCells[1]->y == top);
    XP_Bool bSameLeft = (m_SelectedLoCells[1]->x == left);

    // Neither is the same - quit
    if( !bSameLeft && !bSameTop )
        return ED_MERGE_NONE;

    for( int i = 2; i < iCount; i++ )
    {
        if( (bSameLeft && m_SelectedLoCells[i]->x != left) || 
            (bSameTop &&  m_SelectedLoCells[i]->y != top) )
        {
            return ED_MERGE_NONE;
        }
    }
#endif
    return ED_MERGE_SELECTED_CELLS;
}

/* Use to enable/disable Split Cell feature. 
 * Current cell (containing caret) can be split 
 * only if it has COLSPAN or ROWSPAN
*/
XP_Bool CEditBuffer::CanSplitTableCell()
{
    XP_Bool bResult = FALSE;
    // We can split a cell if single cell is selected
    // and/or caret is inside a cell with COLSPAN or ROWSPAN
    if( m_SelectedLoCells.Size() <= 1 )
    {
        EDT_TableCellData *pData = GetTableCellData();
        if( pData )
        {
            bResult = (pData->iColSpan > 1) || (pData->iRowSpan > 1);
            EDT_FreeTableCellData(pData);
        }
    }
    return bResult;
}

/* Set appropriate COLSPAN or ROWSPAN and move all
 * cell contents into first cell of set
 */
void CEditBuffer::MergeTableCells()
{
    // Don't attempt if not allowed
    ED_MergeType MergeType = GetMergeTableCellsType();
    //if( MergeType == ED_MERGE_NONE )
    //    return;
    int iCount = m_SelectedEdCells.Size();

    // Be sure selection is ordered properly
    SortSelectedCells();
    
    // Set beginning of UNDO block
    BeginBatchChanges(kGroupOfChangesCommandID);

    int32 x = 0;
    int32 y = 0;

    // This is the cell we will merge into
    CEditTableCellElement *pFirstCell = NULL;
    if( iCount > 0 )
    {
        // We have selected cell as first
        pFirstCell = m_SelectedEdCells[0];
        // Save the x and y of first cell
        //  to figure out COL or ROW span
        x = m_SelectedLoCells[0]->x;
        y = m_SelectedLoCells[0]->y;
    } else {
        // Get current cell at caret 
        CEditInsertPoint ip;
        GetTableInsertPoint(ip);
        pFirstCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
        if(pFirstCell )
        {
            LO_CellStruct *pLoCell = GetLoCell(pFirstCell);
            if(pLoCell)
            {
                x = pLoCell->x;
                y = pLoCell->y;
            }
        }
    }

    EDT_TableCellData *pFirstCellData = pFirstCell->GetData();
    if(!pFirstCellData)
        return;
    EDT_TableCellData *pNextCellData = NULL;

    if( iCount > 1 )
    {
        // We are merging all selected cells
        for( int i = 1; i < iCount; i++ )
        {
            // We must add all COLSPANs of all cells to be merged
            pNextCellData = m_SelectedEdCells[i]->GetData();
            if( pNextCellData )
            {
                // If y value is same, next cell is in same row, so do ColSpan
                if( y == m_SelectedLoCells[i]->y )
                {
                    pFirstCellData->iColSpan += pNextCellData->iColSpan;
                }

                // If x value is same, next cell is in same col, so do RowSpan
                // Note: This will work for strange selection sets, but if all
                //  cells in largest bounding rect aren't selected, we can get some strange results!
                if( x == m_SelectedLoCells[i]->x )
                {
                    pFirstCellData->iRowSpan += pNextCellData->iRowSpan;
                }
                EDT_FreeTableCellData(pNextCellData);
            }
            pFirstCell->MergeCells(m_SelectedEdCells[i]);
        }
    } else {
        // We are merging with just the next cell to the right
        CEditTableCellElement *pNextCell = (CEditTableCellElement*)pFirstCell->GetNextSibling();
        pNextCellData = pNextCell->GetData();
        if( pNextCellData )
        {
            pFirstCellData->iColSpan += pNextCellData->iColSpan;
            EDT_FreeTableCellData(pNextCellData);
        }
        pFirstCell->MergeCells( pNextCell );
    }
    // Set the SPAN data for the merged cell
    pFirstCell->SetData(pFirstCellData);
    EDT_FreeTableCellData(pFirstCellData);

    // Be sure insert point is in first cell, 
    //   not one of the cells that was deleted
    //   (else we crash in Relayout)
    SetTableInsertPoint(pFirstCell);
    
    ClearTableAndCellSelection();
    
    // Relayout the entire table
    Relayout(pFirstCell->GetParentTable(), 0);
    EndBatchChanges();
}

/* Separate paragraphs into separate cells,
 * removing COLSPAN or ROWSPAN
*/
void CEditBuffer::SplitTableCell()
{
    //TODO: WRITE THIS!
}


XP_Bool CEditBuffer::IsInsertPointInTableCaption(){
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    // We allow the insert point to be in a table with a table caption.
    CEditTableElement* pTable = ip.m_pElement->GetTableIgnoreSubdoc();
    CEditCaptionElement* pTableCaption = pTable ? pTable->GetCaption() : 0;
    return pTableCaption != NULL;
}

EDT_TableCaptionData* CEditBuffer::GetTableCaptionData(){
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    // We allow the insert point to be in a table with a table caption.
    CEditTableElement* pTable = ip.m_pElement->GetTableIgnoreSubdoc();
    CEditCaptionElement* pTableCaption = pTable ? pTable->GetCaption() : 0;
    if ( pTableCaption ){
        return pTableCaption->GetData();
    }
    else {
        return NULL;
    }
}

void CEditBuffer::SetTableCaptionData(EDT_TableCaptionData *pData){
    VALIDATE_TREE(this);
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableElement* pTable = ip.m_pElement->GetTableIgnoreSubdoc();
    CEditCaptionElement* pTableCaption = pTable ? pTable->GetCaption() : 0;
    if ( pTableCaption ){
        pTableCaption->SetData( pData );
        pTable->FinishedLoad(this); // Can move caption up or down.
        Relayout( pTable, 0 );
    }
}

void CEditBuffer::InsertTableCaption(EDT_TableCaptionData *pData){
    VALIDATE_TREE(this);
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableElement* pTable = ip.m_pElement->GetTableIgnoreSubdoc();
    if ( pTable ){
        if ( ! pTable->GetCaption() ) {
            // CInsertTableCaptionCommand actually performs the operation in the
            // constructor of the command. So in order for save-based undo to
            // work, we must wrap its constructor in BeginBatchChanges.
            BeginBatchChanges(kGroupOfChangesCommandID /*kInsertTableCaptionCommandID*/);
            AdoptAndDo(new CInsertTableCaptionCommand(this, pData));
            EndBatchChanges();
        }
    }
}

void CEditBuffer::DeleteTableCaption(){
    VALIDATE_TREE(this);
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableElement* pTable = ip.m_pElement->GetTableIgnoreSubdoc();
    CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if ( pTable ){
        if ( pTable->GetCaption() ) {
            BeginBatchChanges(kGroupOfChangesCommandID);
            AdoptAndDo(new CDeleteTableCaptionCommand(this));
            EndBatchChanges();
        }
    }
}

XP_Bool CEditBuffer::IsInsertPointInTableRow(){
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    return ip.m_pElement->GetTableRowIgnoreSubdoc() != NULL;
}

EDT_TableRowData* CEditBuffer::GetTableRowData(){
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableRowElement* pTableRow = ip.m_pElement->GetTableRowIgnoreSubdoc();
    if ( pTableRow ){
        return pTableRow->GetData();
    }
    else {
        return NULL;
    }
}

void CEditBuffer::SetTableRowData(EDT_TableRowData *pData){
    VALIDATE_TREE(this);
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableRowElement* pTableRow = ip.m_pElement->GetTableRowIgnoreSubdoc();
    if ( pTableRow ){
        pTableRow->SetData( pData );
        Relayout( pTableRow, 0 );
    }
}

intn CEditBuffer::GetNumberOfSelectedRows()
{
    intn iRows = 0;
    int iSize = m_SelectedLoCells.Size();
    if( iSize && m_TableHitType == ED_HIT_SEL_ROW )
    {
        SortSelectedCells();

        iRows = 1;
        int32 iCurrentTop = m_SelectedLoCells[0]->y;
        for(int i = 2; i < iSize; i++ )
        {
            // Different row when y value changes            
            if( iCurrentTop != m_SelectedLoCells[i]->y )
            {
                iRows++;
                iCurrentTop = m_SelectedLoCells[i]->y;
            }
        }
    }
    return iRows;
}

intn CEditBuffer::GetNumberOfSelectedColumns()
{
    intn iCols = 0;
    int iSize = m_SelectedLoCells.Size();
    if( iSize && m_TableHitType == ED_HIT_SEL_COL )
    {
        // Finding number of columns is more complicated 
        //  than rows because ordering is rows first,
        //  and because of COLSPAN and ROWSPAN
        TXP_GrowableArray_LO_CellStruct  UniqueXCells;
        UniqueXCells.Add(m_SelectedLoCells[0]);

        for(int i=2; i < iSize; i++ )
        {
            intn j;
            // Check if next cell has different x than others
            intn iUniqueCount = UniqueXCells.Size();
            for( j = 0; j < iUniqueCount; j++ )
            {
                if( UniqueXCells[j]->x == m_SelectedLoCells[i]->x )
                    break;
            }
            // We didn't find it, so add to list
            if( j == iUniqueCount )
                UniqueXCells.Add(m_SelectedLoCells[i]);
        }
        iCols = UniqueXCells.Size();
    }
    return iCols;
}

void CEditBuffer::InsertTableRows(EDT_TableRowData *pData, XP_Bool bAfterCurrentRow, intn number)
{
    VALIDATE_TREE(this);

    if( number <= 0 )
    {
        // Get number of rows from the table selection
        number = GetNumberOfSelectedRows();

        if( number )
        {        
            // Move to appropriate cell
            if( bAfterCurrentRow )
                MoveToLastSelectedCell();
            else
                MoveToFirstSelectedCell();
        }
    }

    // If none or just one selected cell, then insert 1 row
    if( number == 0 && m_SelectedLoCells.Size() <= 1 )
        number = 1;

    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if ( pTableCell ){
        // CInsertTableRowCommand actually performs the operation in the
        // constructor of the command. So in order for save-based undo to
        // work, we must wrap its constructor in BeginBatchChanges.
// Caller should manage UNDO buffer
//        BeginBatchChanges(kGroupOfChangesCommandID/*kInsertTableRowCommandID*/);
        ClearTableAndCellSelection();
        SetFillNewCellWithSpace();
        AdoptAndDo(new CInsertTableRowCommand(this, pData, bAfterCurrentRow, number));
        ClearFillNewCellWithSpace();
//        EndBatchChanges();
    }
}

void CEditBuffer::DeleteTableRows(intn number){
    VALIDATE_TREE(this);
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if ( pTableCell ){
        BeginBatchChanges(kGroupOfChangesCommandID);
        // If we have entire rows selected, use that for number to delete
        if( m_SelectedEdCells.Size() )
        {
            if( ED_HIT_SEL_ROW == GetTableSelectionType() )
            {
                // All selected cells are in rows,
                //   so count how many to delete
                CEditTableElement *pTable = pTableCell->GetParentTable();
                if( !pTable )
                    return; // Not likely!
                
                number = 0;
                for( intn i = 0; i < pTable->m_RowLayoutData.Size(); i++ )
                {
                    // Get the first cell in each row
                    CEditTableCellElement *pCell = pTable->m_RowLayoutData[i]->pEdCell;

                    if( pCell && pCell->IsSelected() )
                    {
                        number++;
                    }
                    else if( number )
                    {
                        // We are done when we find the first
                        // row that is not selected
                        // NOTE: We don't try to handle non-contiguous selected rows
                        break;
                    }
                }
            }
        }
        if( number )
            AdoptAndDo(new CDeleteTableRowCommand(this, number));

        EndBatchChanges();
    }
}

void CEditBuffer::SyncCursor(CEditLayerElement* pLayer)
{
    CEditInsertPoint insertPoint;
    insertPoint.m_pElement = m_pRoot->GetFirstMostChild()->Leaf();
    if ( pLayer && pLayer->GetFirstMostChild()->IsLeaf() ) {
        insertPoint.m_pElement = pLayer->GetFirstMostChild()->Leaf();
    }

    SetInsertPoint(insertPoint);
}

XP_Bool CEditBuffer::IsInsertPointInTableCell(){
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    return ip.m_pElement->GetTableCellIgnoreSubdoc() != NULL;
}

// For cell selection, return:
//    ED_HIT_SEL_ROW if >= 1 complete row is selected (including all cells in table)
//    ED_HIT_SEL_COL if >= 1 complete column is selected,
//    else ED_HIT_SEL_CELL if any cell is selected.
ED_HitType CEditBuffer::GetTableSelectionType()
{
    if( m_pSelectedEdTable )
        return ED_HIT_SEL_TABLE;

    intn iSelectedCount = m_SelectedLoCells.Size();
    
    if( iSelectedCount == 0 )
        return ED_HIT_NONE;
    
    if( iSelectedCount == 1 )
        return ED_HIT_SEL_CELL;

    // Count number of cells in table to quickly determine if
    //  ALL cells in table are selected
    if( iSelectedCount == lo_GetNumberOfCellsInTable(lo_GetParentTable(m_pContext, (LO_Element*)m_SelectedLoCells[0])) )
        return ED_HIT_SEL_ROW; // ED_HIT_SEL_ALL_CELLS;

    // Save the unique X and Y values we encounter,
    //   they define columns and rows
    // This avoids duplicate checks for each cell in row or col
    TXP_GrowableArray_int32  UniqueX;
    TXP_GrowableArray_int32  UniqueY;

    ED_HitType iNewSelType = ED_HIT_NONE;
    XP_Bool bColSelected = FALSE;
    XP_Bool bRowSelected = FALSE;

    for( intn i = 0; i < iSelectedCount; i++ )
    {
        XP_Bool bCheckCellInCol = TRUE;
        XP_Bool bCheckCellInRow = TRUE;
        
        intn j;
        // Check if we already did this column
        for( j = 0; j < UniqueX.Size(); j++ )
        {
            if( m_SelectedLoCells[i]->x == UniqueX[j] )
            {
                bCheckCellInCol = FALSE;
                break;
            }
        }
        if( !bCheckCellInCol )
            UniqueX.Add(m_SelectedLoCells[i]->x);

        // Check if we already did this row
        for( j = 0; j < UniqueY.Size(); j++ )
        {
            if( m_SelectedLoCells[i]->y == UniqueY[j] )
            {
                bCheckCellInRow = FALSE;
                break;
            }
        }
        if( !bCheckCellInRow )
            UniqueY.Add(m_SelectedLoCells[i]->y);

        if( (bCheckCellInCol || bCheckCellInRow) && iNewSelType == ED_HIT_NONE )
        {
            if( lo_AllCellsSelectedInColumnOrRow( m_SelectedLoCells[i], FALSE ) ) // Row
            {
                if( bColSelected )
                {
                    // Can't have both row and columns selected
                    iNewSelType = ED_HIT_SEL_CELL;
                    break;
                }
                else
                    bRowSelected = TRUE;
            } 
            else if( lo_AllCellsSelectedInColumnOrRow( m_SelectedLoCells[i], TRUE ) ) // Column
            {
                if( bRowSelected )
                {
                    // Can't have both row and columns selected
                    iNewSelType = ED_HIT_SEL_CELL;
                    break;
                }
                else
                    bColSelected = TRUE;
            } 
            else 
            {
                // Neither row or column is fully selected
                iNewSelType = ED_HIT_SEL_CELL;
                break;
            }
        }
    }
    if( iNewSelType == ED_HIT_NONE )
    {
        // We made it through all cells,
        //  so one of these must be TRUE
        if( bRowSelected )
            iNewSelType = ED_HIT_SEL_ROW;
        else if( bColSelected )
            iNewSelType = ED_HIT_SEL_COL;
    }
    return iNewSelType;
}

EDT_TableCellData* CEditBuffer::GetTableCellData()
{
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableCellElement* pCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if ( pCell )
    {
        // Get data for the current cell (containing the caret)
        EDT_TableCellData *pData = pCell->GetData();

        XP_ASSERT(pData); //Should never fail
        if( !pData )
            return NULL;

        // Used by front end to figure modify Cell Properties
        //  dialog based on number of selected cells
        pData->iSelectedCount = m_SelectedEdCells.Size();

        if( pData->iSelectedCount > 1 )
        {
            // This figures out if selection comprises just rows or columns
            pData->iSelectionType = GetTableSelectionType();;

            // Go through all selected cells to set mask bits 
            //  that tell what attributes are the same 
            //  for all selected cells
            for( intn i = 0; i < pData->iSelectedCount; i++ )
            {
                // Skip current cell when comparing data
                if( m_SelectedEdCells[i] != pCell )
                {
                    m_SelectedEdCells[i]->MaskData(pData);
                }
            }

            // Override ColSpan and RowSpan mask bit
            //  to NOT allow changing it when > 1 cell is selected
            // TODO: MAYBE ALLOW CHANGING THIS IF NOT A FULL ROW OR COL SELECTED???
            pData->mask &= ~(CF_COLSPAN | CF_ROWSPAN);
        }
        return pData;
    }
    else {
        return NULL;
    }
}

static void edt_CopyLoColor( LO_Color **ppDestColor, LO_Color *pSourceColor )
{
    if( ppDestColor )
    {
        if( !pSourceColor )
        {
            XP_FREEIF(*ppDestColor);
            return;
        }

        // Create new struct if it doesn't exist        
        if( !*ppDestColor )
            *ppDestColor = XP_NEW(LO_Color);
        XP_ASSERT(*ppDestColor);

        if( *ppDestColor )
        {
            (*ppDestColor)->red = pSourceColor->red;
            (*ppDestColor)->green = pSourceColor->green;
            (*ppDestColor)->blue = pSourceColor->blue;
        }
    }
}

static void edt_CopyTableCellData( EDT_TableCellData *pDestData, EDT_TableCellData *pSourceData )
{
    if( pDestData && pSourceData )
    {
        // Change data only for attributes whose bit is set in data mask
        if( pSourceData->mask & CF_ALIGN )
            pDestData->align = pSourceData->align;

        if( pSourceData->mask & CF_VALIGN )
            pDestData->valign = pSourceData->valign;

        if( pSourceData->mask & CF_COLSPAN )
            pDestData->iColSpan = pSourceData->iColSpan;

        if( pSourceData->mask & CF_ROWSPAN )
            pDestData->iRowSpan = pSourceData->iRowSpan;

        if( pSourceData->mask & CF_HEADER )
            pDestData->bHeader = pSourceData->bHeader;

        if( pSourceData->mask & CF_NOWRAP )
            pDestData->bNoWrap = pSourceData->bNoWrap;

        if( pSourceData->mask & CF_BACK_NOSAVE )
            pDestData->bBackgroundNoSave = pSourceData->bBackgroundNoSave;

        if( pSourceData->mask & CF_WIDTH )
        {
            pDestData->bWidthDefined = pSourceData->bWidthDefined;
            pDestData->iWidth = pSourceData->iWidth;
            pDestData->iWidthPixels = pSourceData->iWidthPixels;
            pDestData->bWidthPercent = pSourceData->bWidthPercent;
        }

        if( pSourceData->mask & CF_HEIGHT )
        {
            pDestData->bHeightDefined = pSourceData->bHeightDefined;
            pDestData->iHeight = pSourceData->iHeight;
            pDestData->iHeightPixels = pSourceData->iHeightPixels;
            pDestData->bHeightPercent = pSourceData->bHeightPercent;
        }

        if( pSourceData->mask & CF_BACK_COLOR )
            edt_CopyLoColor( &pDestData->pColorBackground, pSourceData->pColorBackground);

        if( pSourceData->mask & CF_BACK_IMAGE )
        {
            XP_FREEIF(pDestData->pBackgroundImage);
            if( pSourceData->pBackgroundImage )
                pDestData->pBackgroundImage = XP_STRDUP(pSourceData->pBackgroundImage);
        }

        if( pSourceData->mask & CF_EXTRA_HTML )
        {
            XP_FREEIF(pDestData->pExtra);
            if( pSourceData->pExtra )
                pDestData->pExtra = XP_STRDUP(pSourceData->pExtra);
        }
    }
}

void CEditBuffer::ChangeTableSelection(ED_HitType iHitType, ED_MoveSelType iMoveType, EDT_TableCellData *pData)
{
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableCellElement* pCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if( !pCell )
        return;

    CEditTableElement *pTable = pCell->GetTable();
    XP_ASSERT(pTable);

    // Clear existing selection - we will always reselect
    ClearTableAndCellSelection();

    // First see if we are changing the current selection type
    if( iHitType != ED_HIT_NONE && pData && iHitType != pData->iSelectionType )
    {
        // This is relatively simple as long as we can trust that the
        //   caret is in the "focus cell" 
        SelectTableElement( pCell, iHitType );
    }

    CEditTableCellElement *pNextCell = NULL;
    CEditTableCellElement *pCurrentCell = pCell;
    int32 iCurrentX = pCell->GetX();
    int32 iCurrentY = pCell->GetY();

    if( iMoveType == ED_MOVE_PREV )
    {
        switch (iHitType )
        {
            case ED_HIT_SEL_CELL:
                pCell = pCell->GetPreviousCellInTable();
                if( !pCell )
                {
                    pCell = pCurrentCell;
                    // We found beginning of table - find last cell in table
                    while( (pNextCell = pCell->GetNextCellInTable()) != NULL )
                    {
                        pCell = pNextCell;
                    }
                }
                break;

            case ED_HIT_SEL_COL:
                // Previous column contains 
                //   the previous cell in current row
                pCell = pTable->GetPreviousCellInRow(pCurrentCell);
                if( !pCell )
                    pCell = pTable->GetLastCellInRow(pCurrentCell);
                break;

            case ED_HIT_SEL_ROW:
                // Previous row contains 
                //   the cell cell in current column
                pCell = pTable->GetPreviousCellInColumn(pCurrentCell);

                if( !pCell )
                    pCell = pTable->GetLastCellInColumn(pCurrentCell);
                break;
        }
    } 
    else if( iMoveType == ED_MOVE_NEXT )
    {
        switch (iHitType )
        {
            case ED_HIT_SEL_CELL:
                // Move to Next Cell
                pCell = pCell->GetNextCellInTable();
                if( !pCell )
                    // At end of table - wrap to first cell
                    pCell = pTable->GetFirstCell();
                break;

            case ED_HIT_SEL_COL:
                pCell = pTable->GetNextCellInRow(pCurrentCell);
                if( !pCell )
                    // At last column -- wrap to first cell of current row
                    pCell = pTable->GetFirstCellInRow(pCurrentCell);
                break;

            case ED_HIT_SEL_ROW:
                pCell = pTable->GetNextCellInColumn(pCurrentCell);
                if( !pCell )
                    // At last row end -- wrap to first cell of current column
                    pCell = pTable->GetFirstCellInColumn(pCurrentCell);
                break;
        }
    }

    if( pCell )
    {
        if( iMoveType != ED_MOVE_NONE &&
            (iHitType == ED_HIT_SEL_ROW || iHitType == ED_HIT_SEL_COL) )
        {
            // We need to select the column or row we are moving to
            // NOTE: This will move caret to this cell
            SelectTableElement( pCell, iHitType );
        } else {
            // Move caret to new focus cell
            SetTableInsertPoint(pCell);
        }

        if( m_SelectedEdCells.Size() > 1 )
        {
            // NOTE: If previous selection was a set of cells or row or column,
            //   and new iHitType == ED_HIT_SEL_CELL,
            //   then the new selection will be the single cell after the
            //   focus cell of the current selection
            // This does NOT change selected cells, just marks
            //   the non-focus cells with LO_ELE_SELECTED_SPECIAL
            //   and updates iSelectionType and iSelectedCount in pData
            DisplaySpecialCellSelection(pCell, pData);
        } else 
        {
            // Select just one new cell in table...
            if( !pCell->IsSelected() )
            {
                SelectTableElement( pCell, ED_HIT_SEL_CELL );
                // ... but retain requested selection type
                m_TableHitType = iHitType;
            }

            // Change cell data to reflect single cell selection...
            if( pData )
            {
                pData->iSelectionType = iHitType; // ...but use requested type
                pData->iSelectedCount = 1;
            }
        }

        if( pData )
        {
            // Fill supplied struct with data for next cell
            EDT_TableCellData *pNextData = pCell->GetData();
            if( pNextData )
            {
                // Set all bits in mask so all values are copied
                // This does not change current pData->mask
                pNextData->mask = -1;
                edt_CopyTableCellData(pData, pNextData);
                EDT_FreeTableCellData(pNextData);
            }
        }
    }
}

static void edt_SetTableCellData( CEditTableCellElement *pCell, EDT_TableCellData *pData )
{
    XP_ASSERT(pCell && pData);
    if(!pCell || !pData )
        return;

    // Get current data before setting new stuff
    EDT_TableCellData *pCurrentData = pCell->GetData();
    XP_ASSERT( pCurrentData );
    if( !pCurrentData )
        return;
    
    // If we are changing column width or row height,
    //  its best to set it for all cells within that column or row.
    // While this fattens document, it confers better table stability
    //   when cells, columns, or rows are moved around because each cell knows its size
    // Note: If pData->bWidthDefined or pData->bHeightDefined is changed to FALSE, 
    //    then the that param will be cleared for entire column or row
    if( (pData->mask & CF_WIDTH) && 
        ( (!pData->bWidthDefined && pCurrentData->bWidthDefined ) ||
          (pData->bWidthDefined && pData->iWidthPixels !=  pCurrentData->iWidthPixels) ) )
    {
        pCell->SetColumnWidthLeft(pCell->GetParentTable(), pCell, pData); 
    }

    if( (pData->mask & CF_HEIGHT) &&
        ( (!pData->bHeightDefined && pCurrentData->bHeightDefined) ||
          (pData->bHeightDefined && pData->iHeightPixels !=  pCurrentData->iHeightPixels) ) )
    {
        pCell->SetRowHeightTop(pCell->GetParentTable(), pCell, pData); 
    }

    // Copy new data to current cell 
    // Note: Never do this BEFORE SetColumnWidthLeft or SetRowHeightTop since
    //       they use pCell's current width to determine how much to resize
    edt_CopyTableCellData( pCurrentData, pData );

    pCell->SetData( pCurrentData );

    // Try to be smart and automatically delete empty cells
    //  when user is increasing COLSPAN relative to previous value
    //TODO: NEED TO ACCOUNT FOR ROWSPAN AS WELL
    if( (pData->mask & CF_COLSPAN) && pData->iColSpan > 1 )
    {
        // Check ROWSPAN and COLSPAN
        if( pCurrentData )
        {
            int iDeleteCount = pData->iColSpan - pCurrentData->iColSpan;
            int i = 0;

            pCell = (CEditTableCellElement*)pCell->GetNextSibling();
            while( pCell && i < iDeleteCount )
            {
                CEditTableCellElement *pNextCell = 
                        (CEditTableCellElement*)pCell->GetNextSibling();

                if( pCell->IsEmpty() )
                {
                    i++;
                    delete pCell;
                }    
                pCell = pNextCell;
            }
        }
    }

    EDT_FreeTableCellData(pCurrentData);
}

void CEditBuffer::SetTableCellData(EDT_TableCellData *pData)
{
    VALIDATE_TREE(this);
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableCellElement* pCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if( !pCell )
        return;    

    CEditTableElement *pTable = pCell->GetTable();
    if( !pTable )
        return;

    // FEs are only required to set the pData->iWidth or pData->iHeight params
    //  so we need to calculate iWidthPixels and iHeightPixels from that
    pCell->CalcPixelWidth(pData);
    pCell->CalcPixelHeight(pData);
    
    BeginBatchChanges(kGroupOfChangesCommandID);

    XP_Bool bCurrentCellFound = FALSE;
    XP_Bool bChangeWidth = FALSE;
    XP_Bool bChangeHeight = FALSE;

    intn iCellCount = m_SelectedEdCells.Size();
    if( iCellCount > 1 )
    {
        // Set data for all selected cells
        for( intn i = 0; i < iCellCount; i++ )
        {
            if( pData->bWidthDefined && m_SelectedEdCells[i]->GetWidth() != pData->iWidthPixels )
                bChangeWidth = TRUE;

            if( pData->bHeightDefined && m_SelectedEdCells[i]->GetHeight() != pData->iHeightPixels )
                bChangeHeight = TRUE;

            edt_SetTableCellData( m_SelectedEdCells[i], pData );
            if( m_SelectedEdCells[i] == pCell )
                bCurrentCellFound = TRUE;
        }
        // Current cell should always be in selected cell set,
        //  so check for that
        XP_ASSERT( bCurrentCellFound );
    } 
    else {
        if( pData->bWidthDefined && pCell->GetWidth() != pData->iWidthPixels )
            bChangeWidth = TRUE;

        if( pData->bHeightDefined && pCell->GetHeight() != pData->iHeightPixels )
            bChangeHeight = TRUE;

        // Set data for just current cell
        edt_SetTableCellData( pCell, pData );
    }

    // We must use the special resize/relayout method if changing size
    if( bChangeWidth || bChangeHeight )
        ResizeTableCell( pTable, bChangeWidth, bChangeHeight );
    else
        Relayout( pTable, 0 );

    // Adjust caller's data - possibly different size data 
    //   as a result of Relayout()
    pData->iWidth = pCell->GetWidthOrPercent();
    pData->iWidthPixels = pCell->GetWidth();
    pData->iHeight = pCell->GetHeightOrPercent();
    pData->iHeightPixels = pCell->GetHeight();

    EndBatchChanges();
}

void CEditBuffer::InsertTableColumns(EDT_TableCellData *pData, XP_Bool bAfterCurrentCell, intn number){
    VALIDATE_TREE(this);
    if( number <= 0 )
    {
        // Get number of columns from the table selection
        number = GetNumberOfSelectedColumns();
        if( number )
        {        
            // Move to appropriate cell
            if( bAfterCurrentCell )
                MoveToLastSelectedCell();
            else
                MoveToFirstSelectedCell();
        }
    }
    // If none or just one selected cell, then insert 1 row
    if( number == 0 && m_SelectedLoCells.Size() <= 1 )
        number = 1;

    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if ( pTableCell ){
        // CInsertTableColumnCommand actually performs the operation in the
        // constructor of the command. So in order for save-based undo to
        // work, we must wrap its constructor in BeginBatchChanges.
        // Note: Caller must do BeginBatchChanges()
        ClearTableAndCellSelection();
        SetFillNewCellWithSpace();
        AdoptAndDo(new CInsertTableColumnCommand(this, pData, bAfterCurrentCell, number));
        ClearFillNewCellWithSpace();
    }
}

void CEditBuffer::DeleteTableColumns(intn number){
    VALIDATE_TREE(this);
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if ( pTableCell ){
        BeginBatchChanges(kGroupOfChangesCommandID);
        // If we have entire rows selected, use that for number to delete
        if( m_SelectedEdCells.Size() )
        {
            if( ED_HIT_SEL_COL == GetTableSelectionType() )
            {
                // All selected cells are in columns,
                //   so count how many to delete
                CEditTableElement *pTable = pTableCell->GetParentTable();
                if( !pTable )
                    return; // Not likely!
                
                number = 0;
                for( intn i = 0; i < pTable->m_ColumnLayoutData.Size(); i++ )
                {
                    // Get the first cell in each column
                    CEditTableCellElement *pCell = pTable->m_ColumnLayoutData[i]->pEdCell;

                    if( pCell && pCell->IsSelected() )
                    {
                        number++;
                    }
                    else if( number )
                    {
                        // We are done when we find the first
                        // column that is not selected
                        // NOTE: We don't try to handle non-contiguous selected columns
                        break;
                    }
                }
            }
        }
        if( number )
    		AdoptAndDo(new CDeleteTableColumnCommand(this, number));
        EndBatchChanges();
    }
}

void CEditBuffer::InsertTableCells(EDT_TableCellData* /* pData */, XP_Bool bAfterCurrentCell, intn number){
    VALIDATE_TREE(this);
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if ( pTableCell ){
        // CInsertTableCellCommand actually performs the operation in the
        // constructor of the command. So in order for save-based undo to
        // work, we must wrap its constructor in BeginBatchChanges.
        BeginBatchChanges(kGroupOfChangesCommandID /*kInsertTableCellCommandID*/);
        SetFillNewCellWithSpace();
		AdoptAndDo(new CInsertTableCellCommand(this, bAfterCurrentCell, number));
        ClearFillNewCellWithSpace();
        EndBatchChanges();
    }
}

void CEditBuffer::DeleteTableCells(intn number){
    VALIDATE_TREE(this);
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if ( pTableCell ){
        BeginBatchChanges(kGroupOfChangesCommandID);
		AdoptAndDo(new CDeleteTableCellCommand(this, number));
        EndBatchChanges();
    }
}

// Layer stuff

XP_Bool CEditBuffer::IsInsertPointInLayer(){
    CEditInsertPoint ip;
    GetInsertPoint(ip);
    return ip.m_pElement->GetLayerIgnoreSubdoc() != NULL;
}

EDT_LayerData* CEditBuffer::GetLayerData(){
    CEditInsertPoint ip;
    GetInsertPoint(ip);
    CEditLayerElement* pLayer = ip.m_pElement->GetLayerIgnoreSubdoc();
    if ( pLayer ){
        return pLayer->GetData();
    }
    else {
        return NULL;
    }
}

void CEditBuffer::SetLayerData(EDT_LayerData *pData){
    VALIDATE_TREE(this);
    CEditInsertPoint ip;
    GetInsertPoint(ip);
    CEditLayerElement* pLayer = ip.m_pElement->GetLayerIgnoreSubdoc();
    if ( pLayer ){
        pLayer->SetData( pData );
        Relayout( pLayer, 0 );
    }
}

void CEditBuffer::InsertLayer(EDT_LayerData *pData){
    CEditLayerElement *pLayer = new CEditLayerElement();
    if( pLayer ){
        pLayer->SetData( pData );
        pLayer->FinishedLoad(this); // Sets default paragraphs for all the cells
        InsertNonLeaf(pLayer);
        SyncCursor(pLayer);
    }
}

void CEditBuffer::DeleteLayer(){
    VALIDATE_TREE(this);
    CEditInsertPoint ip;
    GetInsertPoint(ip);
    CEditLayerElement* pLayer = ip.m_pElement->GetLayerIgnoreSubdoc();
    if ( pLayer ){
        CEditInsertPoint ip;
        GetInsertPoint(ip);
		CEditElement* pRefreshStart = pLayer->GetFirstMostChild()->PreviousLeaf();
		CEditInsertPoint replacePoint(pLayer->GetLastMostChild()->NextLeaf(), 0);
		SetInsertPoint(replacePoint);
		delete pLayer;
		Relayout(pRefreshStart, 0, replacePoint.m_pElement);
    }
}

EDT_HorizRuleData* CEditBuffer::GetHorizRuleData(){
    CEditLeafElement *pInsertPoint;
    ElementOffset iOffset;
    GetPropertyPoint( &pInsertPoint, &iOffset );
    XP_ASSERT( pInsertPoint->IsA(P_HRULE) );
    return pInsertPoint->HorizRule()->GetData( );
}

void CEditBuffer::SetHorizRuleData( EDT_HorizRuleData* pData ){
    ClearSelection( TRUE, TRUE );
    XP_ASSERT( m_pCurrent->IsA(P_HRULE) );
    m_pCurrent->HorizRule()->SetData( pData );
    Relayout( m_pCurrent->FindContainer(), 0, m_pCurrent, RELAYOUT_NOCARET );
    SelectCurrentElement();
}

void CEditBuffer::InsertHorizRule( EDT_HorizRuleData* pData ){
    if( IsSelected() ){
        ClearSelection();
    }
    BeginBatchChanges(kInsertHorizRuleCommandID);
    CEditHorizRuleElement *pHorizRule = new CEditHorizRuleElement(0);
    pHorizRule->SetData( pData );
    InsertLeaf( pHorizRule );
    EndBatchChanges();
}

char* CEditBuffer::GetTargetData(){
    CEditLeafElement *pInsertPoint;
    ElementOffset iOffset;
    XP_Bool bSingleItem;

    bSingleItem = GetPropertyPoint( &pInsertPoint, &iOffset );
    XP_ASSERT( bSingleItem );

    XP_ASSERT( pInsertPoint->GetElementType() == eTargetElement );
    return pInsertPoint->Target()->GetName( );
}

void CEditBuffer::SetTargetData( char* pData ){
    ClearSelection( TRUE, TRUE );
    if ( m_pCurrent->GetElementType() == eTargetElement ){
        m_pCurrent->Target()->SetName( pData, GetRAMCharSetID() );
        Relayout( m_pCurrent->FindContainer(), 0, m_pCurrent, RELAYOUT_NOCARET );
        SelectCurrentElement();
    }
    else {
        XP_ASSERT(FALSE);
    }
}

void CEditBuffer::InsertTarget( char* pData ){
    if( IsSelected() ){
        ClearSelection(TRUE, TRUE);
    }
    int16 csid = GetRAMCharSetID();
    CEditTargetElement *pTarget = new CEditTargetElement(0, 0, csid);
    pTarget->SetName( pData, csid );
    InsertLeaf( pTarget );
}

char* CEditBuffer::GetAllDocumentTargets(){
    intn iSize = 500;
    int iCur = 0;
    char *pBuf = (char*)XP_ALLOC( iSize );
    CEditElement *pNext = m_pRoot;

    pBuf[0] = 0;
    pBuf[1] = 0;

    while(NULL != (pNext = pNext->FindNextElement( &CEditElement::FindTarget, 0 )) ){
        char *pName = pNext->Target()->GetName();
        if( pName && *pName ){
            int iLen = XP_STRLEN( pName );
            if( iCur + iLen + 2 > iSize ){
                iSize = iSize + iSize;
                pBuf = (char*)XP_REALLOC( pBuf, iSize );
            }
            XP_STRCPY( &pBuf[iCur], pName );
            iCur += iLen+1;
        }
    }
    pBuf[iCur] = 0;
    return pBuf;
}

#define LINE_BUFFER_SIZE  4096

char* CEditBuffer::GetAllDocumentTargetsInFile(char *pHref){
    intn iSize = 500;
    int iCur = 0;
    char *pBuf = (char*)XP_ALLOC( iSize );
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_pContext);
//    CEditElement *pNext = m_pRoot;
    pBuf[0] = 0;
    pBuf[1] = 0;

    char *pFilename = NULL;

// The version for windows is the old bad code that only works
// right for windows.
// The UNIX/MAC code is XP and should really be used for
// windows also.  We're just scared to change it this late
// in the cycle. Bug 50888
#ifdef XP_WIN
    // First check if URL is a local file that exists
    XP_StatStruct stat;
    XP_Bool bFreeFilename = FALSE;

#if defined(XP_MAC) || defined(XP_UNIX) || defined(XP_OS2)
    if ( -1 != XP_Stat(pHref, &stat, xpURL) &&
        stat.st_mode & S_IFREG ) {
#else
    if ( -1 != XP_Stat(pHref, &stat, xpURL) &&
        stat.st_mode & _S_IFREG ) {
#endif
        // We can use unprocessed URL,
        //   and we don't need to free it
        pFilename = pHref;
    }
    else {
        // We probably have a URL,
        //  get absolute URL path then convert to local format
        char *pAbsolute = NET_MakeAbsoluteURL( LO_GetBaseURL(m_pContext ), pHref );

        if( !pAbsolute ||
            !XP_ConvertUrlToLocalFile(pAbsolute, &pFilename) ){
            if( pFilename ) XP_FREE(pFilename);
            if( pAbsolute ) XP_FREE(pAbsolute);
            return NULL;
        }
        XP_FREE(pAbsolute);
        // We need to free the filename made for us
        bFreeFilename = TRUE;
    }

#else
    // pFilename is in xpURL format.

    // First check if URL is a local file that exists
    char *pURL = XP_PlatformFileToURL(pHref);
    if (pURL && XP_ConvertUrlToLocalFile(pURL,NULL)) {
        pFilename = NET_ParseURL(pURL,GET_PATH_PART);
        XP_FREE(pURL);
    }
    else {
        XP_FREEIF(pURL);

        // We probably have a URL,
        //  get absolute URL path then convert to local format
        char *pAbsolute = NET_MakeAbsoluteURL( LO_GetBaseURL(m_pContext ), pHref );

        if( pAbsolute &&
            XP_ConvertUrlToLocalFile(pAbsolute,NULL) ) {
            pFilename = NET_ParseURL(pAbsolute,GET_PATH_PART);
        }

        XP_FREEIF(pAbsolute);

    }
    if (!pFilename) {
      return NULL;
    }
#endif

    // Open local file
    XP_File file = XP_FileOpen( pFilename, xpURL, XP_FILE_READ );
    if( !file ) {

        // Same story as above. Should use the non-windows version.
#ifdef XP_WIN
        if( pFilename && bFreeFilename ){
            XP_FREE(pFilename);
        }
#else
        XP_FREEIF(pFilename);
#endif
        return NULL;
    }

	char    pFileBuf[LINE_BUFFER_SIZE];
    char   *ptr, *pEnd, *pStart;
    size_t  count;
    PA_Tag *pTag;
    char   *pName;

    // Read unformated chunks
    while( 0 < (count = fread(pFileBuf, 1, LINE_BUFFER_SIZE, file)) ){
        // Scan from the end to find end of last tag in block
        ptr = pFileBuf + count -1;
        while( (ptr > pFileBuf) && (*ptr != '>') ) ptr--;

        //Bloody unlikely, but we didn't find a tag!
        if( ptr == pFileBuf ) continue;


        // Move file pointer back so next read starts
        //   1 char after the region just found
        int iBack = int(ptr - pFileBuf) -  int(count) + 1;
        fseek( file, iBack, SEEK_CUR );

        // Save the end of "tagged" region
        //  and reset to beginning
        pEnd = ptr;
        ptr = pFileBuf;

FIND_TAG:
        // Scan to beginning of any tag
        while( (ptr < pEnd) && (*ptr != '<') ) ptr++;
        if( ptr == pEnd ) continue;

        // Save start of tag
        pStart = ptr;

        // Skip over whitespace before tag name
        ptr++;
        while( (ptr < pEnd) && (XP_IS_SPACE(*ptr)) ) ptr++;
        if( ptr == pEnd ) continue;

        // Check for Anchor tag
        if( ((*ptr == 'a') || (*ptr == 'A')) &&
             XP_IS_SPACE(*(ptr+1)) ){
            // Find end of the tag
            while( (ptr < pEnd) && (*ptr != '>') ) ptr++;
            if( ptr == pEnd ) continue;

            // Parse into tag struct so we can use
            //   edt_FetchParamString to do the tricky stuff
            // Kludge city. pa_CreateMDLTag needs a pa_DocData solely to
            // look at the line count.
            {
                pa_DocData doc_data;
                doc_data.newline_count = 0;
                pTag = pa_CreateMDLTag(&doc_data, pStart, (ptr - pStart)+1 );
            }
            if( pTag ){
                if( pTag->type == P_ANCHOR ){
                    pName = edt_FetchParamString(pTag, PARAM_NAME, INTL_GetCSIWinCSID(c));
                    if( pName ){
                        // We found a Name
                        int iLen = XP_STRLEN( pName );
                        if( iCur + iLen + 2 > iSize ){
                            iSize = iSize + iSize;
                            pBuf = (char*)XP_REALLOC( pBuf, iSize );
                        }
                        XP_STRCPY( &pBuf[iCur], pName );
                        iCur += iLen+1;
                        XP_FREE(pName);
                    }
                }
                PA_FreeTag(pTag);
                // Move past the tag we found and search again
                ptr++;
                goto FIND_TAG;
            }
            else {
                // We couldn't find a complete tag, get another block
                continue;
            }
        } else {
            // Not an anchor tag, try again
            ptr++;
            goto FIND_TAG;
        }
    }
    XP_FileClose(file);

// Same story as above. Should use the non-windows version.
#ifdef XP_WIN
    if( pFilename && bFreeFilename ){
        XP_FREE(pFilename);
    }
#else
    XP_FREEIF(pFilename);
#endif

    pBuf[iCur] = 0;

    return pBuf;
}

#ifdef FIND_REPLACE
XP_Bool CEditBuffer::FindAndReplace( EDT_FindAndReplaceData *pData ){
    return FALSE;
}
#endif



class CStretchBuffer {
private:
    char *m_pBuffer;
    intn m_iSize;
    intn m_iCur;

public:
    CStretchBuffer();
    ~CStretchBuffer(){}        // intentionall don't destroy the buffer.
    void Add( char *pString );
    char* GetBuffer(){ return m_pBuffer; }
    // if index non-NULL, return the zero-based index of the found
    // string.
    XP_Bool Contains( char* p, int *pIndex = NULL );
};

CStretchBuffer::CStretchBuffer(){
    m_pBuffer = (char*)XP_ALLOC( 512 );
    m_iSize = 512;
    m_iCur = 0;
    m_pBuffer[0] = 0;
    m_pBuffer[1] = 0;
}

void CStretchBuffer::Add( char *p ){
    if( p == 0 || *p == 0 ){
        return;
    }
    int iLen = XP_STRLEN( p );
    while( m_iCur + iLen + 2 > m_iSize ){
        m_iSize = m_iSize + m_iSize;
        m_pBuffer = (char*)XP_REALLOC( m_pBuffer, m_iSize );
    }
    XP_STRCPY( &m_pBuffer[m_iCur], p );
    m_iCur += iLen+1;
    m_pBuffer[m_iCur] = 0;
}

XP_Bool CStretchBuffer::Contains( char* p, int *pIndex ){
    if (pIndex) {
      *pIndex = 0;
    }
    char *s = GetBuffer();
    while( s && *s ){
        // Assume we are dealing with absolute URLs only.
        if( EDT_IsSameURL(s,p,NULL,NULL) ){
            return TRUE;
        }
        s += XP_STRLEN(s)+1;
        if (pIndex) {
          (*pIndex)++;
        }
    }
    return FALSE;
}


// Helper for CEditBuffer::GetAllDocumentFiles.  If pImageURL is not in buf, add it and add 
// element to the "selected" list.  If pImageURL is in buf and was previously unselected, we may
// now select it.
//
// Analogous to CEditSaveObject::CheckAddFile()
PRIVATE
void AddToBufferUnique(CStretchBuffer &buf, TXP_GrowableArray_int32 &selected,
            char *pBaseURL, char *pImageURL, XP_Bool bSelected) {
  //XP_Bool bSelected = !bNoSave;
  char *pFilename = NET_MakeAbsoluteURL( pBaseURL, pImageURL);
  if (pFilename) {
     int index;
    // Don't insert duplicates.
     if (!buf.Contains( pFilename, &index )) {
       // keep buf and selected the same size.
       buf.Add( pFilename );
       selected.Add((int32)bSelected); 
     }
     else if (bSelected) {
        // If wasn't selected before, it is now.
        selected[index] = (int32)bSelected;
     }
     XP_FREE(pFilename);
  }
}

// CEditBuffer::GetAllDocumentFiles() has been significantly changed.
// The returned files do not have to be in the same directory, and are not
// checked for existence.  hardts
//
// If ppSelected is passed in, it will be set to an array of length equal to the number
// of strings in the returned value.  For each image, it tells whether, by default, it should be 
// published/saved along with the document.  E.g. looks at the NOSAVE attribute.
// ppSelected may be NULL.
// If no images found, no memory will be allocated for ppSelected.
char* CEditBuffer::GetAllDocumentFiles(XP_Bool **ppSelected,XP_Bool bKeepImagesWithDoc){
//////// WARNING: if you change this function, fix CEditSaveObject::AddAllFiles() and CEditSaveObject::FixupLinks() also.
    TXP_GrowableArray_int32 selected;

    CEditElement *pNext = m_pRoot;
    CStretchBuffer buf;
    char *pRetVal = 0;

    char *pDocURL = edt_GetDocRelativeBaseURL(m_pContext);
    if( !pDocURL ){
        return NULL;
    }

    // Font Ref.
    if( m_pFontDefURL && *m_pFontDefURL) {
      AddToBufferUnique(buf,selected,pDocURL,m_pFontDefURL,
            bKeepImagesWithDoc && !m_bFontDefNoSave);
    }

    // Background image.
    if( m_pBackgroundImage && *m_pBackgroundImage) {
      AddToBufferUnique(buf,selected,pDocURL,m_pBackgroundImage,
            bKeepImagesWithDoc && !m_bBackgroundNoSave);
    }

    // Regular images.
    while(NULL != (pNext = pNext->FindNextElement( &CEditElement::FindImage, 0 )) ){
        EDT_ImageData *pData = pNext->Image()->GetImageData();
        if( pData && pData->pSrc && *pData->pSrc) {
          AddToBufferUnique(buf,selected,pDocURL,pData->pSrc,
                bKeepImagesWithDoc && !pData->bNoSave);
        }

        if( pData && pData->pLowSrc && *pData->pLowSrc) {
          AddToBufferUnique(buf,selected,pDocURL,pData->pLowSrc,
                bKeepImagesWithDoc && !pData->bNoSave);
        }
        EDT_FreeImageData( pData );
    }

    
    //// Sure would be nice to abstract adding all the different types of table
    //// backgrounds.
    // table backgrounds <table>
    pNext = m_pRoot;
    while(NULL != (pNext = pNext->FindNextElement( &CEditElement::FindTable, 0 )) ){
      EDT_TableData *pData = ((CEditTableElement *)pNext)->GetData();
      if (pData) {
        if (pData->pBackgroundImage && *pData->pBackgroundImage) {
          AddToBufferUnique(buf,selected,pDocURL,pData->pBackgroundImage,
                  bKeepImagesWithDoc && !pData->bBackgroundNoSave);
        }
        CEditTableElement::FreeData(pData);
      }
    }
    // table row backgrounds <tr>
    pNext = m_pRoot;
    while(NULL != (pNext = pNext->FindNextElement( &CEditElement::FindTableRow, 0 )) ){
      EDT_TableRowData *pData = ((CEditTableRowElement *)pNext)->GetData();
      if (pData) {
        if (pData->pBackgroundImage && *pData->pBackgroundImage) {
          AddToBufferUnique(buf,selected,pDocURL,pData->pBackgroundImage,
                  bKeepImagesWithDoc && !pData->bBackgroundNoSave);
        }
        CEditTableRowElement::FreeData(pData);
      }
    }
    // table cell backgrounds <td> <th>
    pNext = m_pRoot;
    while(NULL != (pNext = pNext->FindNextElement( &CEditElement::FindTableCell, 0 )) ){
      EDT_TableCellData *pData = ((CEditTableCellElement *)pNext)->GetData();
      if (pData) {
        if (pData->pBackgroundImage && *pData->pBackgroundImage) {
          AddToBufferUnique(buf,selected,pDocURL,pData->pBackgroundImage,
                  bKeepImagesWithDoc && !pData->bBackgroundNoSave);
        }
        CEditTableCellElement::FreeData(pData);
      }
    }


    // UnknownHTML tags with LOCALDATA attribute.
    pNext = m_pRoot;
    while(NULL != (pNext = pNext->FindNextElement( &CEditElement::FindUnknownHTML, 0 )) ){
      CEditIconElement *pIcon = CEditIconElement::Cast(pNext);
      if (pIcon) {
        char **pMimeTypes;
        char **pURLs;
        int count = pIcon->ParseLocalData(&pMimeTypes,&pURLs);

        // Maybe should make a check that pURLs[n] is a relative URL
        // in current directory.
        for (int n = 0; n < count; n++) {
          AddToBufferUnique(buf,selected,pDocURL,pURLs[n],TRUE);
        }
        CEditIconElement::FreeLocalDataLists(pMimeTypes,pURLs,count);
      }
    } // while

    pRetVal = buf.GetBuffer();

    // If no files found, return NULL
    if( pRetVal && *pRetVal == '\0' ){
        XP_FREE(pRetVal);
        pRetVal = NULL;
    }

    // Create ppSelected list if it was passed in.
    if (ppSelected) {
      if (pRetVal) {
        // allocate memory.
        *ppSelected = (XP_Bool *)XP_ALLOC(selected.Size() * sizeof(XP_Bool));
        if (!*ppSelected) {
          // Out of memory, not really the best thing to do.
          return pRetVal;
        }
        // Fill the array.
        for (int n  = 0; n < selected.Size(); n++) {
          (*ppSelected)[n] = (XP_Bool)selected[n];
        }
      }
      // No images in doc, set ppSelected to NULL.
      else {
        *ppSelected = NULL;
      }
    }

    XP_FREE(pDocURL);
    return pRetVal;
}

char* CEditBuffer::GetUnknownTagData(){
    CEditLeafElement *pInsertPoint;
    ElementOffset iOffset;
    XP_Bool bSingleItem;

    bSingleItem = GetPropertyPoint( &pInsertPoint, &iOffset );
    XP_ASSERT( bSingleItem );

    XP_ASSERT( pInsertPoint->IsIcon() );
    return pInsertPoint->Icon()->GetData( );
}

void CEditBuffer::SetUnknownTagData( char* pData ){
    ClearSelection( TRUE, TRUE );
    XP_ASSERT( m_pCurrent->GetElementType() == eIconElement );
    m_pCurrent->Icon()->SetData( pData );
    Relayout( m_pCurrent->FindContainer(), 0, m_pCurrent, RELAYOUT_NOCARET );
    SelectCurrentElement();
}

void CEditBuffer::InsertUnknownTag( char *pData ){
    if( IsSelected() ){
        ClearSelection();
    }
    if ( ! pData ) {
        XP_ASSERT(FALSE);
        return;
    }

    // This should probably be done elsewhere.  Remove trailing whitespace. HARDTS.
    int32 iLen = XP_STRLEN(pData);
    if (iLen > 0) {
      char *pLast = pData + (iLen - 1);
      while (pLast >= pData && XP_IS_SPACE(*pLast))
        pLast--;
      *(pLast+1) = '\0';
    }

    NormalizeEOLsInString(pData);


    XP_Bool bEndTag = ( iLen > 1 && pData[1] == '/' );
    CEditIconElement *pUnknownTag = new CEditIconElement(0,
        bEndTag ? EDT_ICON_UNSUPPORTED_END_TAG :EDT_ICON_UNSUPPORTED_TAG);
    pUnknownTag->SetData( pData );
    InsertLeaf( pUnknownTag );
}

EDT_ListData* CEditBuffer::GetListData(){
    CEditContainerElement *pContainer;
    CEditListElement *pList;
    if( IsSelected() ){
        CEditContainerElement *pEndContainer;
        CEditListElement *pEndList;
        CEditSelection selection;

        // LTNOTE: this is a hack.  It doesen't handle a bunch of cases.
        // It needs to be able to handle multiple lists selected.
        // It should work a little better.
        //cmanske: TODO: FIX LISTS TO HANDLE SELECTIONS
        GetSelection( selection );
        selection.m_start.m_pElement->FindList( pContainer, pList );
        selection.GetClosedEndContainer()->GetLastMostChild()->FindList( pEndContainer, pEndList );
        if( pList != pEndList ){
            return 0;
        }
    }
    else {
        m_pCurrent->FindList( pContainer, pList );
    }
    if( pList ){
        return pList->GetData( );
    }
    else {
        return 0;
    }
}

void CEditBuffer::SetListData( EDT_ListData* pData ){
    VALIDATE_TREE(this);
    CEditContainerElement *pContainer;
    CEditListElement *pList;

    if( IsSelected() ){
        CEditLeafElement *pBegin, *pEnd, *pCurrent;
        ElementOffset iBeginPos, iEndPos;
        XP_Bool bFromStart;
        GetSelection( pBegin, iBeginPos, pEnd, iEndPos, bFromStart );
        pCurrent = pBegin;
        XP_Bool bDone = FALSE;
        do {
            pCurrent->FindList( pContainer, pList );
            if( pList ){
                pList->SetData( pData );
            }

            bDone = (pEnd == pCurrent );    // For most cases
            pCurrent = pCurrent->NextLeafAll();
            bDone = bDone || (iEndPos == 0 && pEnd == pCurrent ); // Pesky edge conditions!
        } while( pCurrent && !bDone );

        // force layout stop displaying the current selection.
#ifdef LAYERS
        LO_StartSelectionFromElement( m_pContext, 0, 0, NULL);
#else
        LO_StartSelectionFromElement( m_pContext, 0, 0);
#endif

        CEditElement *pLast = pEnd;
        CEditElement *pFirst = pBegin->FindContainer();
        if( pBegin )
        {
            pBegin->FindList( pContainer, pList);
            if( pList ) pFirst = pList;
        }
        if( pEnd ){
            pEnd->FindList( pContainer, pList );
            if( pList ) pLast = pList->GetLastMostChild();
        }

        Relayout( pFirst, 0, pLast, RELAYOUT_NOCARET );
        // Need to force selection.
        SelectRegion(pBegin, iBeginPos, pEnd, iEndPos, bFromStart );
    }
    else {

        m_pCurrent->FindList( pContainer, pList );
        if( pList ){
            pList->SetData( pData );
            Relayout( pList, 0, pList->GetLastMostChild() );
        }
    }
}

void CEditBuffer::InsertBreak( ED_BreakType eBreak, XP_Bool bTyping ){
    VALIDATE_TREE(this);

    if( IsSelected() ){
        // ToDo: Consider cutting the selection here, like InsertChar does.
        ClearSelection();
    }

    StartTyping(bTyping);
    PA_Tag *pTag = XP_NEW( PA_Tag );
    XP_BZERO( pTag, sizeof( PA_Tag ) );
    pTag->type = P_LINEBREAK;

    switch( eBreak ){
        case ED_BREAK_NORMAL:
            break;
        case ED_BREAK_LEFT:
            edt_SetTagData( pTag, "CLEAR=LEFT>" );
            break;
        case ED_BREAK_RIGHT:
            edt_SetTagData( pTag, "CLEAR=RIGHT>" );
            break;
        case ED_BREAK_BOTH:
            edt_SetTagData( pTag, "CLEAR=BOTH>" );
            break;
    }
    CEditBreakElement *pBreak = new CEditBreakElement( 0, pTag );
    InsertLeaf(pBreak);
    FixupSpace(bTyping);
    Reduce(pBreak->GetParent());
}

EDT_ClipboardResult CEditBuffer::CanCut(XP_Bool bStrictChecking){
    CEditSelection selection;
    GetSelection(selection);
    return CanCut(selection, bStrictChecking);
}

EDT_ClipboardResult CEditBuffer::CanCut(CEditSelection& selection, XP_Bool bStrictChecking)
{
    if( IsTableOrCellSelected() )
    {
        //TODO: MODIFY TO CHECK FOR CONTENTS TO COPY?
        return EDT_COP_OK;
    }
    EDT_ClipboardResult result = selection.IsInsertPoint() ? EDT_COP_SELECTION_EMPTY : EDT_COP_OK;
    if ( bStrictChecking && result == EDT_COP_OK
        && selection.CrossesSubDocBoundary() ) {
        result = EDT_COP_SELECTION_CROSSES_TABLE_DATA_CELL;
    }
    return result;
}

EDT_ClipboardResult CEditBuffer::CanCopy(XP_Bool bStrictChecking){
    return CanCut(bStrictChecking); /* In the future we may do something for read-only docs. */
}

EDT_ClipboardResult CEditBuffer::CanCopy(CEditSelection& selection, XP_Bool bStrictChecking){
    return CanCut(selection, bStrictChecking); /* In the future we may do something for read-only docs. */
}

EDT_ClipboardResult CEditBuffer::CanPaste(XP_Bool bStrictChecking){
    CEditSelection selection;
    GetSelection(selection);
    return CanPaste(selection, bStrictChecking);
}

EDT_ClipboardResult CEditBuffer::CanPaste(CEditSelection& selection, XP_Bool bStrictChecking){
    EDT_ClipboardResult result = EDT_COP_OK;
    if ( bStrictChecking
        && selection.CrossesSubDocBoundary() ) {
        result = EDT_COP_SELECTION_CROSSES_TABLE_DATA_CELL;
    }
    return result;
}

XP_Bool CEditBuffer::CanSetHREF(){
    if( IsSelected()
            || ( m_pCurrent && m_pCurrent->GetHREF() != ED_LINK_ID_NONE ) ){
        return TRUE;
    }
    else {
        return FALSE;
    }
}

//  This needs to be written to:
//    Return data in EDT_HREFData struct containing:
//    pTitle:   ALL the text of elements with same HREF
//              or the image name if only an image is selected
//    pHREF     what we are getting now
//    pTarget   what we are ignoring now!
//    pMocha    "

char *CEditBuffer::GetHREF(){
    ED_LinkId id = GetHREFLinkID();
    if( id != ED_LINK_ID_NONE ){
        return linkManager.GetHREF(id) ;
    }
    else {
        return 0;
    }
}

ED_LinkId CEditBuffer::GetHREFLinkID(){
    CEditLeafElement *pElement = m_pCurrent;

    if( IsSelecting() ){
        return ED_LINK_ID_NONE;
    }

    if( IsSelected() ){
        ElementOffset i,i1;
        CEditLeafElement *pEnd;
        XP_Bool bFromStart;
        GetSelection( pElement, i, pEnd, i1, bFromStart );
    }
    if( pElement ){
        return pElement->Leaf()->GetHREF();
    }
    return ED_LINK_ID_NONE;
}

char* CEditBuffer::GetHREFText(){
    CEditLeafElement *pElement = m_pCurrent;
    ED_LinkId id;
    char* pBuf = 0;

    if( pElement && pElement->IsText() &&
        (id = pElement->GetHREF()) != ED_LINK_ID_NONE ){

        // Move back to find start of contiguous elements
        //  with same HREF
        CEditLeafElement *pStartElement = pElement;
        while ( (pElement = (CEditLeafElement*)pElement->GetPreviousSibling()) != NULL &&
                 pElement->IsText() &&
                 pElement->GetHREF() == id ) {
            pStartElement = pElement;
        }

        // We now have starting EditText element
        pElement = pStartElement;
        char* pText = pStartElement->Text()->GetText();
        pBuf = pText ? XP_STRDUP( pText ) : 0;
        if( !pBuf ){
            return 0;
        }

        // Now scan forward to accumulate text
        while ( (pElement = (CEditLeafElement*)pElement->GetNextSibling()) != NULL &&
                pElement && pElement->IsText() &&
                pElement->GetHREF() == id ){
            // Cast to access text class (we always test element first)
            // CEditTextElement *pText = (CEditTextElement*)pElement;

            pBuf = (char*)XP_REALLOC(pBuf, XP_STRLEN(pBuf) + pElement->Text()->GetSize() + 1);
            if( !pBuf ){
                return 0;
            }
            char* pText = pElement->Text()->GetText();
            if ( pText ) {
                strcat( pBuf, pText );
            }
        }
   }
   return pBuf;
}

void CEditBuffer::SetCaret(){
    InternalSetCaret(TRUE);
}

void CEditBuffer::InternalSetCaret(XP_Bool bRevealPosition){
    LO_TextStruct* pText;
    int iLayoutOffset;
    int32 winHeight, winWidth;
    int32 topX, topY;

    if( m_bNoRelayout ){
        return;
    }

    // jhp Check m_pCurrent because IsSelected won't give accurate results when
    // the document is being layed out.
    if( !IsSelected() && m_pCurrent && !m_inScroll ){

        if( m_pCurrent->IsA( P_TEXT ) ){
            if( !m_pCurrent->Text()->GetLOTextAndOffset( m_iCurrentOffset,
                    m_bCurrentStickyAfter, pText, iLayoutOffset ) ){
                return;
            }
        }

        FE_DestroyCaret(m_pContext);

        if ( bRevealPosition ) {
            RevealPosition(m_pCurrent, m_iCurrentOffset, m_bCurrentStickyAfter);
        }
        FE_GetDocAndWindowPosition( m_pContext, &topX, &topY, &winWidth, &winHeight );
        m_lastTopY = topY;

        if( m_pCurrent->IsA( P_TEXT ) ){
            if( !m_pCurrent->Text()->GetLOTextAndOffset( m_iCurrentOffset, m_bCurrentStickyAfter,
                                                    pText, iLayoutOffset ) ){
                return;
            }

            FE_DisplayTextCaret( m_pContext, FE_VIEW, pText, iLayoutOffset );
        }
        else
        {
            LO_Position effectiveCaretPosition = GetEffectiveCaretLOPosition(m_pCurrent, m_iCurrentOffset, m_bCurrentStickyAfter);

            switch ( effectiveCaretPosition.element->type )
            {
                case LO_IMAGE:
                    FE_DisplayImageCaret( m_pContext,
                                & effectiveCaretPosition.element->lo_image,
                                (ED_CaretObjectPosition)effectiveCaretPosition.position );
                    break;
                default:
                    FE_DisplayGenericCaret( m_pContext,
                                & effectiveCaretPosition.element->lo_any,
                                (ED_CaretObjectPosition)effectiveCaretPosition.position );
                    break;
            }
        }
    }
}


LO_Position CEditBuffer::GetEffectiveCaretLOPosition(CEditElement* pElement, intn iOffset, XP_Bool bCurrentStickyAfter)
{
    LO_Position position;
    int iPos;

	XP_Bool bValid = pElement->Leaf()->GetLOElementAndOffset( iOffset, bCurrentStickyAfter, position.element, iPos);
    if ( ! bValid ) {
        position.element = 0;
        position.position = 0;
        return position;
    }

    XP_ASSERT(position.element);
    position.position = iPos;

    // Linefeeds only have a single position.  If you are at the end
    //  of a linefeed, you are really at the beginning of the next line.
    if( position.element && position.element->type == LO_LINEFEED && position.position == 1 ){
        //LO_Element *pNext = LO_NextEditableElement( pLoElement );
        LO_Element *pNext= position.element->lo_any.next;

#ifdef MQUOTE
         // Skip over bullets resulting from MQUOTE.
        while (pNext && pNext->type == LO_BULLET)
        {
            pNext = pNext->lo_any.next;
        }
#endif

        if( pNext ){
            position.element = pNext;
            position.position = 0;
        }
        else {
            position.position = 0;
        }
    }

    return position;
}

// A utility function for ensuring that a range is visible. This handles one
// axis; it's called twice, once for each dimension.
//
// All variable names are given in terms of the vertical case. The horizontal
// case is the same.
//
// top - the position of the top edge of the visible portion of the document,
// in the document's coordinate system.
//
// length - the length of the window, in the document's coordinate system.
//
// targetLow - the first position we desire to be visible.
//
// targetHigh - the last position we desire to be visible.
//
// In case both positions can't be made visible, targetLow wins.
//
// upMargin - if we have to move up, where to
// position the targetLow relative to top.
// downMargin - if we have to move down, where to position targetLow relative to top.
//
// The margins should be in the range 0..length - 1
//
// Returns TRUE if top changed.

PRIVATE
XP_Bool MakeVisible(int32& top, int32 length, int32 targetLow, int32 targetHigh, int32 upMargin, int32 downMargin)
{
    int32 newTop = top;
    if ( targetLow < newTop ) {
        // scroll up
        newTop = targetLow - upMargin;
        if ( newTop < 0 ) {
            newTop = 0;
        }
    }
    else if ( targetHigh >= (newTop + length) ) {
        // scroll down
        int32 potentialTop = targetHigh - downMargin;
        if ( potentialTop < 0 ) {
            potentialTop = 0;
        }
        if ( potentialTop < targetLow && targetLow < potentialTop + length ) {
            newTop = potentialTop;
        }
        else {
            // Too narrow to show all of cursor. Show just the top part.
            newTop = targetLow - upMargin;
            if ( newTop < 0 ) {
                newTop = 0;
            }
        }
    }

    XP_Bool changed = newTop != top;
    top = newTop;
    return changed;
}

// Prevent reentrant calling of FE_SetDocPostion
static XP_Bool edt_bSettingDocPosition = FALSE;

void CEditBuffer::RevealPosition(CEditElement* pElement, int iOffset, XP_Bool bStickyAfter)
{
    int32 winHeight, winWidth;
    int32 topX, topY;
    int32 targetX, targetYLow, targetYHigh;

    if ( ! pElement ) {
       XP_ASSERT(FALSE);
       return;
    }
    FE_GetDocAndWindowPosition( m_pContext, &topX, &topY, &winWidth, &winHeight );

    CEditInsertPoint ip(pElement, iOffset, bStickyAfter);
    if (IsPhantomInsertPoint(ip) ){
        pElement = pElement->GetPreviousSibling();
        if (pElement == NULL ) {
            return; // Shouldn't happen because phantom insert points are always after something.
        }
        iOffset = pElement->Leaf()->GetLen();
    }


    if ( ! GetLOCaretPosition(pElement, iOffset, bStickyAfter, targetX, targetYLow, targetYHigh) )
        return;

    // XP_TRACE(("top %d %d target %d %d %d", topX, topY, targetX, targetYLow, targetYHigh));

    // The visual position policy is that when moving up or down we stick close to
    // the existing margin, but when moving left or right we move to 2/3rds of the
    // way across the screen. (If we ever support right-to-left text we'll
    // have to revisit this.)

    const int32 kUpMargin = ( winHeight > 30 ) ? 10 : (winHeight / 3);
    const int32 kDownMargin = winHeight - kUpMargin;
    const int32 kLeftRightMargin = winWidth * 2 / 3;
    XP_Bool updateX = MakeVisible(topX, winWidth, targetX, targetX, kLeftRightMargin, kLeftRightMargin);
    XP_Bool updateY = MakeVisible(topY, winHeight, targetYLow, targetYHigh, kUpMargin, kDownMargin);

    if ( updateX || updateY ) {
        // XP_TRACE(("new top %d %d", topX, topY));
        XP_Bool bHaveCaret = ! IsSelected();
        if ( bHaveCaret ){
            FE_DestroyCaret(m_pContext);
        }
		// Prevent reentrant calling of FE_SetDocPostion
        // Fix for bug 93341. Stack overflow crash because of infinite loop
        // (only in Win95 - weird!)
        if( !edt_bSettingDocPosition ){
			edt_bSettingDocPosition = TRUE;
			FE_SetDocPosition(m_pContext, FE_VIEW, topX, topY);
    		edt_bSettingDocPosition = FALSE;
        }
        if ( bHaveCaret ){
            InternalSetCaret(FALSE);
        }
    }
}

XP_Bool CEditBuffer::GetLOCaretPosition(CEditElement* pElement, int iOffset, XP_Bool bStickyAfter,
    int32& targetX, int32& targetYLow, int32& targetYHigh)
{
    if ( ! pElement ) {
       XP_ASSERT(FALSE);
       return FALSE;
    }
    LO_Position position = GetEffectiveCaretLOPosition(pElement, iOffset, bStickyAfter);
    if ( position.element == NULL ) return FALSE;
    FE_GetCaretPosition(m_pContext, &position, &targetX, &targetYLow, &targetYHigh);
    return TRUE;
}


/* Begin of document check preference and if move curosr is enabled, 
we will move the cursor if not, we leave the cursor alone.
REQUIRES (Boolean to select or not), Valid editbuffer
RETURNS Nothing
NOTE: uses
    NavigateChunk( bSelect, LO_NA_DOCUMENT, FALSE ); 
    if we move the cursor.
and
    FE_ScrollDocTo (MWContext *context, int iLocation, int32 x,int32 y);
    if we do not
*/
void CEditBuffer::NavigateDocument(XP_Bool bSelect, XP_Bool bForward )
{
    if (!m_bEdtBufPrefInitialized)
        CEditBuffer::InitializePrefs();//static function to initialize the prefs
    if (m_bMoveCursor==TRUE)
        NavigateChunk( bSelect, LO_NA_DOCUMENT, bForward ); 
    else
    {
        int32 winHeight, winWidth;
        int32 topX, topY;

        if( m_inScroll ){
            return;
        }
        m_inScroll = TRUE;

    
        FE_GetDocAndWindowPosition( m_pContext, &topX, &topY, &winWidth, &winHeight );
        CEditLeafElement *t_leaf;
        ElementOffset t_offset; //offset is used to reveal the position of the caret and to call GetLOElementAndOffset
        XP_Bool t_stickyafter; //I honestly dont know what this is yet, but we use it for the same reasons as t_offset
        long t_maxwidth,t_maxheight; //used for the TOTAL width and height of the document.
        if (m_pCurrent)
        {
            t_leaf= m_pCurrent;
            t_offset=m_iCurrentOffset;
            t_stickyafter=m_bCurrentStickyAfter;
        }
        else
            GetInsertPoint( &t_leaf, &t_offset, &t_stickyafter );//gets the insert point.
        CL_Layer *layer = LO_GetLayerFromId(m_pContext, LO_DOCUMENT_LAYER_ID); 
        t_maxheight = LO_GetLayerScrollHeight(layer); 
        t_maxwidth = LO_GetLayerScrollWidth(layer);         
        if (bForward)
            FE_ScrollDocTo (m_pContext, t_offset, topX, t_maxheight);
        else
            FE_ScrollDocTo (m_pContext, t_offset, topX, 0);
        m_inScroll = FALSE;
    }
}


/* PageUpDown
this function will scroll the CURSOR to the proper location depending on bForward definition
we have 2 parts.  move the cursor and make the window catch up. IFF the preference tells us
*/
void CEditBuffer::PageUpDown(XP_Bool bSelect, XP_Bool bForward )
{
    if (!m_bEdtBufPrefInitialized)
        CEditBuffer::InitializePrefs();//static function to initialize the prefs

    LO_Element* pElement;
    int bCenterWindow; //0= no need, 1= bottom, -1 = top
    int iLayoutOffset;
    int32 winHeight, winWidth;
    int32 topX, topY;
    int32 iDesiredY;
    CEditLeafElement *t_leaf;
    ElementOffset t_offset; //offset is used to reveal the position of the caret and to call GetLOElementAndOffset
    XP_Bool t_stickyafter; //I honestly dont know what this is yet, but we use it for the same reasons as t_offset

    if( m_inScroll ){
        return;
    }
    m_inScroll = TRUE;

    
    FE_GetDocAndWindowPosition( m_pContext, &topX, &topY, &winWidth, &winHeight );

    if (m_pCurrent)
    {
        t_leaf= m_pCurrent;
        t_offset=m_iCurrentOffset;
        t_stickyafter=m_bCurrentStickyAfter;
    }
    else
        GetInsertPoint( &t_leaf, &t_offset, &t_stickyafter );//gets the insert point.

    t_leaf->GetLOElementAndOffset( t_offset, t_stickyafter, pElement, iLayoutOffset );

    if (m_bMoveCursor==TRUE)
    {
        ClearPhantomInsertPoint();
        ClearMove();    /* Arrow keys clear the up/down target position */
        BeginSelection();
        //is the cursor on the screen?? if not,  center scrollview on lead leaf element/cursor
        //if it is, just move the window +- one screen height
        if ( pElement->lo_any.y < topY ) 
          bCenterWindow = -1;//move window such that the TOP of the window shows the carret
        else if ((pElement->lo_any.y+pElement->lo_any.height)>(topY+winHeight))
        {
            bCenterWindow = 1;//move window such that the BOTTOM of the window shows the carret
        }
        else
          bCenterWindow = 0;
        // m_iDesiredX is where we would move if we could.  This keeps the
        //  cursor moving, basically, straight up and down, even if there is
        //  no text or gaps
        if( m_iDesiredX == -1 ){
          m_iDesiredX = pElement->lo_any.x +
                  (t_leaf->IsA(P_TEXT)
                      ? LO_TextElementWidth( m_pContext,
                              (LO_TextStruct*) pElement, iLayoutOffset)
                      : 0 );
        }

        if (bForward)
            iDesiredY = pElement->lo_any.y + winHeight;
        else
            iDesiredY = pElement->lo_any.y - winHeight;
        if (iDesiredY<0)
        {
            iDesiredY=0;
            m_iDesiredX=0;//x and y should go to beginnning
        }

        // what is location?? cant find it used. i will use t_offset???
        // FE_SetDocPosition(MWContext *context, int iLocation, int32 iX, int32 iY);
        if (bCenterWindow == -1)
            FE_SetDocPosition(m_pContext, t_offset, topX, iDesiredY);//keep x the same
        else if (bCenterWindow == 1)
        {
            int t_tempdesiredy=iDesiredY+pElement->lo_any.height-winHeight;
            if (t_tempdesiredy<0)
                t_tempdesiredy=0;
            FE_SetDocPosition(m_pContext, t_offset, topX, t_tempdesiredy);
        }
        else
        {
          if (bForward)
              FE_SetDocPosition(m_pContext, t_offset, topX, topY+winHeight);
          else
          {
              int t_tempdesiredy=topY-winHeight;
              if (t_tempdesiredy<0)
                  t_tempdesiredy=0;
              FE_SetDocPosition(m_pContext, t_offset, topX, t_tempdesiredy);
          }
        }

        if (bSelect)
        {
            ExtendSelection( m_iDesiredX, iDesiredY );
        }
        else
        {
    #ifdef LAYERS
            LO_PositionCaret( m_pContext, m_iDesiredX, iDesiredY, NULL );
    #else
            LO_PositionCaret( m_pContext, m_iDesiredX, iDesiredY );
    #endif
        }
        m_lastTopY = topY;
        m_inScroll = FALSE;//do not forget this!  this allows the screen to refresh properly.  used only to prevent recursion
        EndSelection();
        DoneTyping();
    }
    else
    {
        if (bForward)
            iDesiredY = topY + winHeight;
        else
            iDesiredY = topY - winHeight;
        if (iDesiredY<0)
        {
            iDesiredY=0;
        }
        m_inScroll = FALSE;//do not forget this!  this allows the screen to refresh properly.  used only to prevent recursion
        FE_SetDocPosition(m_pContext, t_offset, topX, iDesiredY);//keep x the same
    }
}


void CEditBuffer::WindowScrolled(){

    if( m_inScroll ){
        return;
    }
    m_inScroll = TRUE;
    if( !IsSelected() ){
        InternalSetCaret(FALSE);
    }

    m_inScroll = FALSE;

#if 0
//TODO: Ask mjudge if we need to keep this for anything

    // This code keeps the cursor on the screen, which is pretty weird behavior for
    // a text editor. That's not how any WYSIWYG editor works -- I'm guessing it's
    // some crufty key-based editor feature. -- jhp

    LO_Element* pElement;
    int iLayoutOffset;
    int32 winHeight, winWidth;
    int32 topX, topY;
    int32 iDesiredY;

    if( m_inScroll ){
        return;
    }
    m_inScroll = TRUE;


    FE_GetDocAndWindowPosition( m_pContext, &topX, &topY, &winWidth, &winHeight );


    // if there is a current selection, we have nothing to do.
    if( !IsSelected() ){

        m_pCurrent->GetLOElementAndOffset( m_iCurrentOffset, m_bCurrentStickyAfter,
                                                    pElement, iLayoutOffset );

        // m_iDesiredX is where we would move if we could.  This keeps the
        //  cursor moving, basically, straight up and down, even if there is
        //  no text or gaps
        if( m_iDesiredX == -1 ){
            m_iDesiredX = pElement->lo_any.x +
                    (m_pCurrent->IsA(P_TEXT)
                        ? LO_TextElementWidth( m_pContext,
                                (LO_TextStruct*) pElement, iLayoutOffset)
                        : 0 );
        }


        if( pElement->lo_any.y < topY ){
            iDesiredY = (pElement->lo_any.y - m_lastTopY) + topY;
            // caret is above the current window
#ifdef LAYERS
            LO_PositionCaret( m_pContext, m_iDesiredX, iDesiredY, NULL );
#else
            LO_PositionCaret( m_pContext, m_iDesiredX, iDesiredY );
#endif
        }
        else if( pElement->lo_any.y+pElement->lo_any.height > topY+winHeight ){
            // caret is below the current window
            iDesiredY = (pElement->lo_any.y - m_lastTopY) + topY;
#ifdef LAYERS
            LO_PositionCaret( m_pContext, m_iDesiredX, iDesiredY, NULL);
#else
            LO_PositionCaret( m_pContext, m_iDesiredX, iDesiredY);
#endif
        }
        else {
            SetCaret();
        }
    }
    m_lastTopY = topY;
    m_inScroll = FALSE;
   InternalSetCaret(FALSE);
#endif
}

void CEditBuffer::DebugPrintTree(CEditElement* pElement){
    const size_t kIndentBufSize = 256;
    char indent[kIndentBufSize];
    char *pData;
    char *pData1;
    PA_Tag *pTag;

    int indentSize = printState.m_iLevel*2;
    if ( indentSize >= kIndentBufSize ) {
        indentSize = kIndentBufSize - 1;
    }

    CEditElement *pNext = pElement->GetChild();
	int i;
    for(i = 0; i < indentSize; i++ ){
        indent[i] = ' ';
    }
    indent[i] = '\0';

    if( pElement->IsA(P_TEXT) ) {
        pData = pElement->Text()->GetText();
        pData1 = pElement->Text()->DebugFormat();
        pTag = 0;
    }
    else if( pElement->IsContainer() ){
        ED_Alignment alignment = pElement->Container()->GetAlignment();
        if ( alignment < LO_ALIGN_CENTER || alignment > ED_ALIGN_ABSTOP ) {
            pData = "Bad alignment.";
        }
        else {
            pData = lo_alignStrings[alignment];
        }
        pData1 = "";
        pTag = 0;
    }
    else {
        pData1 = "";
        pTag = pElement->TagOpen(0);
        if( pTag ){
            // no multiple tag fetches.
            //XP_ASSERT( pTag->next == 0 );
            PA_LOCK( pData, char*, pTag->data );
        }
        else {
            pData = 0;
        }
    }

    printState.m_pOut->Printf("\n");
    ElementIndex baseIndex = pElement->GetElementIndex();
    ElementIndex elementCount = pElement->GetPersistentCount();
    printState.m_pOut->Printf("0x%08x %6ld-%6ld", pElement, (long)baseIndex,
                (long)baseIndex + elementCount);
    printState.m_pOut->Printf("%s %s: %s%c",
            indent,
            EDT_TagString(pElement->GetType()),
            pData1,
            (pData ? '"': ' ') );
    if( pData ){
        printState.m_pOut->Write( pData, XP_STRLEN(pData));
        printState.m_pOut->Write( "\"", 1 );
    }

    if( pTag ){
        PA_UNLOCK( pTag->data );
        PA_FreeTag( pTag );
    }

    while( pNext ){
        printState.m_iLevel++;
        DebugPrintTree( pNext );
        printState.m_iLevel--;
        pNext = pNext->GetNextSibling();
    }
}

void CEditBuffer::CheckAndPrintComment(CEditLeafElement* pElement, CEditSelection& selection, XP_Bool bEnd){
    CEditInsertPoint ip(pElement, bEnd ? pElement->GetLen() : 0);
    CheckAndPrintComment2(ip, selection, FALSE);
    CheckAndPrintComment2(ip, selection, TRUE);
}

void CEditBuffer::CheckAndPrintComment2(const CEditInsertPoint& where, CEditSelection& selection, XP_Bool bEnd){
    CEditInsertPoint* pSelEdge = selection.GetEdge(bEnd);
    if (where.m_pElement == pSelEdge->m_pElement && where.m_iPos == pSelEdge->m_iPos){
        printState.PrintSelectionComment(bEnd, pSelEdge->m_bStickyAfter);
    }
}

void CEditBuffer::PrintTree( CEditElement* pElement ){
    CEditElement* pNext = pElement->GetChild();

    // Note the hack here: text elements are responsible for
    // printing their own selection comments because the selection
    // edge may fall inside the text element. We could make all
    // elements handle their own selection, but that would be tedious.

    XP_Bool bCheckForSelectionAsComment = printState.m_bEncodeSelectionAsComment
        && pElement->IsLeaf() && ! pElement->IsText();
    if ( bCheckForSelectionAsComment){
        CheckAndPrintComment(pElement->Leaf(), printState.m_selection, FALSE);
    }

    pElement->PrintOpen( &printState );

    while( pNext ){
        PrintTree( pNext );
        pNext = pNext->GetNextSibling();
    }
    pElement->PrintEnd( &printState );
    if ( bCheckForSelectionAsComment){
        CheckAndPrintComment(pElement->Leaf(), printState.m_selection, TRUE);
    }
}


void CEditBuffer::StreamToPositionalText( CEditElement* pElement, IStreamOut* pOut ){
    CEditElement* pNext = pElement->GetChild();

    pElement->StreamToPositionalText( pOut, FALSE );

    while( pNext ){
        StreamToPositionalText( pNext, pOut );
        pNext = pNext->GetNextSibling();
    }
    pElement->StreamToPositionalText( pOut, TRUE );
}


void CEditBuffer::AppendTitle( char *pTitle ){
    if( m_pTitle == 0 ){
        m_pTitle = XP_STRDUP( pTitle );
    }
    else {
        int32 titleLen = XP_STRLEN(m_pTitle)+XP_STRLEN(pTitle)+1;
        m_pTitle = (char*)XP_REALLOC( m_pTitle,
                 titleLen);
        strcat( m_pTitle, pTitle );
    }
}

void CEditBuffer::PrintMetaData( CPrintState *pPrintState ){
    // According to RFC 2070, print the charset http-equiv as soon as possible.
    int contentTypeID = FindMetaData(TRUE, CONTENT_TYPE);
    if ( contentTypeID >= 0 ) {
        PrintMetaData(pPrintState, contentTypeID);
    }
    for( int i = 0; i < m_metaData.Size(); i++ ){
        if ( i != contentTypeID ) {
            PrintMetaData(pPrintState, i);
        }
    }
}

void CEditBuffer::PrintMetaData( CPrintState *pPrintState, int index ){
    if ( index < 0 || index >= m_metaData.Size() ) {
        return;
    }
    EDT_MetaData *pData = m_metaData[index];
    if( pData != 0  ){
        // LTNOTE:
        // You can't merge these into a single printf.  edt_MakeParamString
        //  uses a single static buffer.
        //
        if( pData->pContent ){
            pPrintState->m_pOut->Printf( "   <META %s=%s ",
                    (pData->bHttpEquiv ? "HTTP-EQUIV" : "NAME"),
                    edt_MakeParamString( pData->pName ) );
    
            // This changes internal quotes into &quot, which breaks some metatags
            //  that use quotes around URLs, such as NetWatch (bug 111054)
            //  edt_MakeParamString( pData->pContent ) );
            // TODO: INVESTIGATE IF WE SHOULD NOT TRANSLATE " INTO &QUOT IN edt_MakeParamString

            // Copy to new buff to append quotes before and after 
            int iLen = XP_STRLEN(pData->pContent) + 2; // 2 extra for added quotes
            char *pBuf = edt_WorkBuf( iLen+1 );        // Extra for terminal \0
            if( pBuf ){
                pBuf[0] = '"';
                XP_STRCPY((pBuf+1), pData->pContent);
                pBuf[iLen-1] = '"';
                pBuf[iLen] = '\0';
                pPrintState->m_pOut->Printf( "CONTENT=%s>\n", pBuf );
            }
        }
    }
}

void CEditBuffer::PrintDocumentHead( CPrintState *pPrintState ){
    XP_Bool bPageComposer = ! IsComposeWindow();

    XP_Bool bHaveBaseTarget = m_pBaseTarget && *m_pBaseTarget;
    XP_Bool bHaveFontDefURL = m_pFontDefURL && *m_pFontDefURL;
    CParseState* pParseState = GetParseState();
    XP_Bool bHaveHeadTags = pParseState->m_pJavaScript != NULL;
    XP_Bool bNeedHead = bPageComposer
        || bHaveBaseTarget || bHaveFontDefURL
        || bHaveHeadTags;
    
    // Unfortunately, we don't conform to any standard DOCTYPE yet.
    // cmanske: TODO: NEED TO SEE IF WE CAN WRITE THIS NOW - ITS IMPORTANT!
    // TODO: Put DOCTYPE string in allxpstr.h and .rc???
    pPrintState->m_pOut->Printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\n");

    pPrintState->m_pOut->Printf("<HTML>\n");
    
    if ( bNeedHead ) {
        pPrintState->m_pOut->Printf("<HEAD>\n");

        if ( bPageComposer ) {
            // Print the MetaData first because it includes the charset, which can affect the way the
            // title is parsed. See RFC 2070.
            PrintMetaData( pPrintState );

            // Only print the title if it exists and is not empty.

            if ( m_pTitle && *m_pTitle ) {
                pPrintState->m_pOut->Printf("   <TITLE>");
                edt_PrintWithEscapes( pPrintState, m_pTitle, FALSE );
                pPrintState->m_pOut->Printf(
                              "</TITLE>\n");
            }
        }

        // Print BASE TARGET if nescessary
        if ( bHaveBaseTarget ) {
            pPrintState->m_pOut->Printf("   <BASE TARGET=%s>\n",
                edt_MakeParamString( m_pBaseTarget ));
        }

        // Print the font ref if nescessary
        if ( bHaveFontDefURL ){
            pPrintState->m_pOut->Printf("   <LINK REL=FONTDEF SRC=%s",
                edt_MakeParamString( m_pFontDefURL ));
            if ( m_bFontDefNoSave ) {
                pPrintState->m_pOut->Printf(" NOSAVE");
            }
            pPrintState->m_pOut->Printf(">\n");
        }
        if( bHaveHeadTags ){
            Write(pParseState->m_pJavaScript, pPrintState->m_pOut);
        }
        pPrintState->m_pOut->Printf("</HEAD>\n");
    }

    if ( IsBodyTagRequired() ) {
        // print the contents of the body tag.
        pPrintState->m_pOut->Printf("<BODY");
        if( m_colorText.IsDefined() ) {
            pPrintState->m_pOut->Printf(" TEXT=\"#%06lX\"", m_colorText.GetAsLong() );
        }
        if( m_colorBackground.IsDefined() ) {
            pPrintState->m_pOut->Printf(" BGCOLOR=\"#%06lX\"", m_colorBackground.GetAsLong() );
        }
        if( m_colorLink.IsDefined() ) {
            pPrintState->m_pOut->Printf(" LINK=\"#%06lX\"", m_colorLink.GetAsLong() );
        }
        if( m_colorFollowedLink.IsDefined() ) {
            pPrintState->m_pOut->Printf(" VLINK=\"#%06lX\"", m_colorFollowedLink.GetAsLong() );
        }
        if( m_colorActiveLink.IsDefined() ) {
            pPrintState->m_pOut->Printf(" ALINK=\"#%06lX\"", m_colorActiveLink.GetAsLong() );
        }
        if( m_pBackgroundImage && *m_pBackgroundImage) {
            pPrintState->m_pOut->Printf(" BACKGROUND=%s", edt_MakeParamString(m_pBackgroundImage) );
        }
        if ( m_bBackgroundNoSave ) {
            pPrintState->m_pOut->Printf(" NOSAVE");
        }

        if( m_pBodyExtra && *m_pBodyExtra ) {
            pPrintState->m_pOut->Printf(" %s", m_pBodyExtra );
        }

        pPrintState->m_pOut->Printf(">\n");
    }
}

XP_Bool CEditBuffer::IsBodyTagRequired(){
    return (! IsComposeWindow() )
        || m_colorText.IsDefined() 
        || m_colorBackground.IsDefined()
        || m_colorLink.IsDefined()
        || m_colorFollowedLink.IsDefined() 
        || m_colorActiveLink.IsDefined()
        || ( m_pBackgroundImage && *m_pBackgroundImage ) 
        || ( m_pBodyExtra && *m_pBodyExtra );
}

void CEditBuffer::PrintDocumentEnd( CPrintState *pPrintState ){
    if ( IsBodyTagRequired() ){
        pPrintState->m_pOut->Printf( "\n"
                "</BODY>\n");
    }
    if( GetParseState()->m_pPostBody ){
        Write(GetParseState()->m_pPostBody, pPrintState->m_pOut);
    }
    pPrintState->m_pOut->Printf("</HTML>\n");
}

void CEditBuffer::Write(CStreamOutMemory *pSource, IStreamOut* pDest){
    // Copies entire contents of pSource to pDest.
    int32 iLen = pSource->GetLen();
    XP_HUGE_CHAR_PTR pScript = pSource->GetText();
    while( iLen ){
        int16 iToWrite = (int16) min( iLen, 0x4000 );
        pDest->Write( pScript, iToWrite );
        iLen -= iToWrite;
        pScript += iToWrite;
    }
}


class CEditSaveData {
public:
  CEditSaveData() {ppIncludedFiles = NULL;pSourceURL = NULL;pSaveToTempData = NULL;}
  ~CEditSaveData(); 

// Don't save CEditBuffer because it might have changed from undo/redo logic
  MWContext *pContext; 

  ED_SaveFinishedOption finishedOpt;
  char* pSourceURL;
  ITapeFileSystem *tapeFS;
  XP_Bool bSaveAs;
  XP_Bool bKeepImagesWithDoc;
  XP_Bool bAutoAdjustLinks;
  XP_Bool bAutoSave;
  char **ppIncludedFiles;
  CEditSaveToTempData *pSaveToTempData;
};

CEditSaveData::~CEditSaveData() {
  XP_FREEIF(pSourceURL);
  
  // In the success case, ppIncludedFiles and pSaveToTempData will
  // have been set to NULL in CEditBuffer::SaveFileReal(), so they don't
  // get freed here.
  CEditSaveObject::FreeList(ppIncludedFiles);
  if (pSaveToTempData) {
    // failure.
    (*pSaveToTempData->doneFn)(NULL,pSaveToTempData->hook);
    delete pSaveToTempData;
  }
}

// Callback from plugins to start saving process.
PRIVATE void edt_SaveFilePluginCB(EDT_ImageEncoderStatus status, void* pArg) {
  CEditSaveData *pData = (CEditSaveData *)pArg;
  XP_ASSERT(pData);
  XP_Bool bFileSaved = FALSE;

  if (status != ED_IMAGE_ENCODER_USER_CANCELED) {
    // OK or exception.  Note that we still do the save if an exception.

    CEditBuffer *pBuffer = LO_GetEDBuffer(pData->pContext);
    if (pBuffer) {
      pBuffer->SaveFileReal(pData);
      bFileSaved = TRUE;
      // ppIncludedFiles, and tapeFS will be deleted by CEditBuffer::SaveFileReal().
    }
    else {
      XP_ASSERT(0);
    }
  }
  else {
    // user canceled.
    pData->tapeFS->Complete( FALSE, NULL, NULL ); // Tell tapeFS to kill itself.
  }
  if( !bFileSaved )
  {
      //cmanske EXPERIMENTAL - This was set at start of file save
      // and must be cleared now if we really didn't do the save
      pData->pContext->edit_saving_url = FALSE;
  }
  
  delete pData;
}

// Setup to save file
// !!!!! All of the saving functions ultimately go through this one !!!!!
ED_FileError CEditBuffer::SaveFile( ED_SaveFinishedOption finishedOpt,
                                    char* pSourceURL,
                                    ITapeFileSystem *tapeFS,
                                    XP_Bool bSaveAs,
                                    XP_Bool bKeepImagesWithDoc,
                                    XP_Bool bAutoAdjustLinks,
                                    XP_Bool bAutoSave,
                                    char **ppIncludedFiles,
                                    CEditSaveToTempData *pSaveToTempData){
  // Create argument for asynchoronous call to plugin code.
  CEditSaveData *pData = new CEditSaveData;
  if (!pData) {
    XP_ASSERT(0);
    tapeFS->Complete( FALSE, NULL, NULL ); // Tell tapeFS to kill itself.
    CEditSaveObject::FreeList(ppIncludedFiles);
    if (pSaveToTempData) {
      // failure.
      (*pSaveToTempData->doneFn)(NULL,pSaveToTempData->hook);
      delete pSaveToTempData;
    }
    return ED_ERROR_BLOCKED;
  }
  // fill pData.
  pData->pContext = m_pContext;
  pData->finishedOpt = finishedOpt;
  pData->pSourceURL = XP_STRDUP(pSourceURL);
  pData->tapeFS = tapeFS;
  pData->bSaveAs = bSaveAs;
  pData->bKeepImagesWithDoc = bKeepImagesWithDoc;
  pData->bAutoAdjustLinks = bAutoAdjustLinks;
  pData->bAutoSave = bAutoSave;
  pData->ppIncludedFiles = ppIncludedFiles;
  pData->pSaveToTempData = pSaveToTempData;

  //cmanske - This is also set in constructor of CFileSave,
  //  but we must set it earlier because of asynchronous plugin --
  //  the front ends should stay in a wait loop while this is TRUE 
  //  so they do not continue until saving is completed
  m_pContext->edit_saving_url = TRUE;

  if (pSaveToTempData) {
    // Don't send event when saving temp file.
    // Call callback directly.
    edt_SaveFilePluginCB(ED_IMAGE_ENCODER_OK,(void *)pData);
  }
  else {
    // Build event name to pass to plugin code.
    char *pDestURL = tapeFS->GetDestAbsURL();
    switch (tapeFS->GetType()) {
      case ITapeFileSystem::File:
        if (bAutoSave) {
          EDT_PerformEvent(m_pContext,"autosave",pDestURL,TRUE,TRUE,
                           edt_SaveFilePluginCB,(void *)pData);    
        }
        else {
          EDT_PerformEvent(m_pContext,"save",pDestURL,TRUE,TRUE,
                           edt_SaveFilePluginCB,(void *)pData);    
        }
        break;
      case ITapeFileSystem::Publish:
        EDT_PerformEvent(m_pContext,"publish",pDestURL,TRUE,TRUE,
                         edt_SaveFilePluginCB,(void *)pData);     
        break;
      case ITapeFileSystem::MailSend:
        EDT_PerformEvent(m_pContext,"send",NULL,TRUE,TRUE,
                         edt_SaveFilePluginCB,(void *)pData);     
        // Don't give a URL.
        break;
      default:
        XP_ASSERT(0);
    }
    XP_FREEIF(pDestURL);
  }

  // I'm sick of loosing prefs, so save them as part of 
  //  file saving, since that will take some time anyway
  PREF_SavePrefFile();
  
  return ED_ERROR_NONE;
}


void CEditBuffer::SaveFileReal(CEditSaveData *pData) {
    if (pData->pSaveToTempData) {
      // Don't want to edit temp file after call to EDT_SaveToTempFile().
      pData->finishedOpt = ED_FINISHED_REVERT_BUFFER;
    }
    else {
      // We are going to ignore the finishedOpt flag and do the right thing
      // in the back end for saving and publishing.  Not for mailing.
      if (pData->tapeFS->GetType() == ITapeFileSystem::File ||
          pData->tapeFS->GetType() == ITapeFileSystem::Publish) {
        if (EDT_IS_NEW_DOCUMENT(m_pContext)) {
          // Source is a new document.
          pData->finishedOpt = ED_FINISHED_GOTO_NEW;
        }
        else {
          if (NET_URL_Type(pData->pSourceURL) != FILE_TYPE_URL) {
            // Source is a remote doc.
            pData->finishedOpt = ED_FINISHED_GOTO_NEW;
          }
          else {
            if (pData->tapeFS->GetType() == ITapeFileSystem::File) {
              // Source is a local file, dest is local file.
              pData->finishedOpt = ED_FINISHED_GOTO_NEW;
            }
            else {  
              XP_ASSERT(pData->tapeFS->GetType() == ITapeFileSystem::Publish);
              // Source is a local file, dest is remote file.
              pData->finishedOpt = ED_FINISHED_REVERT_BUFFER;
            }
          }
        }
      }
    }

    DoneTyping();
    CEditSaveObject *pSaveObject;
    if( m_pSaveObject != 0 ){
        // This could happen if user is in the middle of saving a file
        //  (such as at an overwrite or error dialog when inserting image)
        //  and then AutoSave triggers saving the file. So don't ASSERT
        // XP_ASSERT(0);           // shouldn't be returning a value...
        
        pData->tapeFS->Complete( FALSE, NULL, NULL ); // Tell tapeFS to kill itself.
        return;
    }
    // Set write time to 0 so we don't trigger reload before file is saved
    // (doesn't completely solve this problem)
    if( pData->bSaveAs) {
        m_iFileWriteTime = 0;
    }

    // Stamp the document character set into the document.
    SetEncoding(GetDocCharSetID());

    // tapeFS freed by CEditSaveObject (actually CFileSaveObject::~CFileSaveObject).
    m_pSaveObject = 
         pSaveObject = new CEditSaveObject( this, pData->finishedOpt, pData->pSourceURL, pData->tapeFS, 
                   pData->bSaveAs, pData->bKeepImagesWithDoc, pData->bAutoAdjustLinks, pData->bAutoSave,
                   pData->pSaveToTempData);
    pData->pSaveToTempData = NULL; // memory is now the responsibility of CEditSaveObject.

    pSaveObject->AddAllFiles(pData->ppIncludedFiles);
    pData->ppIncludedFiles = NULL;  // freed in AddAllFiles().
        
    // This may save 1 or more files and delete itself before
    //  returning here. Error status (m_status) is set to pSaveObject->m_status
    //  via CEditBuffer::FileFetchComplete(m_status)
    m_status = ED_ERROR_NONE;
    pSaveObject->SaveFiles();
}

int16 CEditBuffer::GetRAMCharSetID() {
    return m_originalWinCSID & (~CS_AUTO);
}

int16 CEditBuffer::GetDocCharSetID() {
    // Copied from layform.c
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(m_pContext);
    int16 doc_csid = INTL_GetCSIDocCSID(csi);
    if ( m_bForceDocCSID ) {
        // Used when changing a document's character set.
        doc_csid = m_forceDocCSID;
    }
    else if (doc_csid == CS_DEFAULT)
		doc_csid = INTL_DefaultDocCharSetID(m_pContext);
	doc_csid &= (~CS_AUTO);
    return doc_csid;
}

void CEditBuffer::ForceDocCharSetID(int16 csid){
    m_bForceDocCSID = TRUE;
    m_forceDocCSID = csid;
}

ED_FileError CEditBuffer::PublishFile( ED_SaveFinishedOption finishedOpt,
                           char *pSourceURL,
                           char **ppIncludedFiles,
                           char *pDestURL, /* Directories must have trailing slash, ie after HEAD call */
                           char *pUsername,
                           char *pPassword,
                           XP_Bool   bKeepImagesWithDoc,
                           XP_Bool   bAutoAdjustLinks ) {
    char *pDocTempDir = GetCommandLog()->GetDocTempDir();
    if (!pDocTempDir) {
      char *tmplate = XP_GetString(XP_EDT_ERR_PUBLISH_PREPARING_ROOT);
      if (tmplate) {
        char *msg = PR_smprintf(tmplate,pSourceURL);
        if (msg) {
          FE_Alert(m_pContext,msg);
          XP_FREE(msg);
        }
      }
      return ED_ERROR_FILE_OPEN;
    }

    // Create Abstract file system to write to remote server.
    ITapeFileSystem *tapeFS = 
      new CTapeFSPublish(m_pContext,pDestURL,pUsername,pPassword,pDocTempDir);
    XP_ASSERT(tapeFS);
    // tapeFS freed by CEditSaveObject.

    // saveAs set to TRUE, why not?
    return SaveFile( finishedOpt, pSourceURL, tapeFS, TRUE,
                     bKeepImagesWithDoc, bAutoAdjustLinks, 
                     FALSE,  // not auto-saving
                     ppIncludedFiles ); // explicitly specify which files to send with doc.
}

void CEditBuffer::InitEscapes(){
    // We escape character in the RAM character set, but we decide to escape depending upon the
    // document character set. So, for example if we're writing to SJIS, we don't escape,
    // even if we're originally in LATIN-1

    // Our strategy is:
    // For MacRoman or ISO_Latin_1, use entites for hi-bit characters.
    // For other character sets, minimize the number of entities we put out.
    // The minimum is:
    // &amp; for ampersand
    // &lt; for less-than

    int16 win_csid = GetRAMCharSetID();
    int16 doc_csid = GetDocCharSetID();

	XP_Bool bWinSimple = 
        win_csid == CS_MAC_ROMAN
        || win_csid == CS_LATIN1
        || win_csid == CS_ASCII;

	XP_Bool bDocSimple = 
        doc_csid == CS_MAC_ROMAN
        || doc_csid == CS_LATIN1
        || doc_csid == CS_ASCII;

    // Bug to fix after 4.0B3 -- can't quote if original csid is not 8-bit either.
    // The fix is to quote after transcoding, rather than before transcoding.
    XP_Bool bQuoteHiBits = bWinSimple && bDocSimple;

    edt_InitEscapes(win_csid, bQuoteHiBits);
}

// CM: Return 0 if OK, or -1 if device is full.
int CEditBuffer::WriteToFile( XP_File hFile ){ //char *pFileName ){
    int iResult = 0;
    CStreamOutFile *pOutFile;
    pOutFile = new CStreamOutFile(hFile, FALSE);
    CConvertCSIDStreamOut* pConverter = new CConvertCSIDStreamOut( GetRAMCharSetID(), GetDocCharSetID(), pOutFile);
    InitEscapes();
    printState.Reset( pConverter, this );
    Reduce( m_pRoot );
    PrintTree( m_pRoot );
    printState.Reset( 0, this );
    if( pOutFile->Status() != IStreamOut::EOS_NoError ){
        iResult = -1;
    }
    else {
        iResult = 0;
    }
    delete pConverter;
    // To Jack: this is a problem
    // When saving to a new file, this does nothing
    // 7/2/96
    //   edit_saving_url flag is set/cleared in CEditSaveObject
    //   and GetFileWriteTime() is done in that object's destructor
    // GetFileWriteTime();

    ////m_autoSaveTimer.Restart();  hardts. Moved to CEditBuffer::FileFetchComplete
    return iResult;
}

CEditDocState *CEditBuffer::RecordState() {
  CEditDocState *ret = new CEditDocState();
  if (!ret) {
    XP_ASSERT(0);
    return NULL;
  }
  
  WriteToBuffer(&ret->m_pBuffer,TRUE);
  ret->m_version = GetCommandLog()->GetVersion();
  return ret;
}

int32 CEditBuffer::WriteToBuffer( XP_HUGE_CHAR_PTR* ppBuffer, XP_Bool bEncodeSelectionAsComment ){
    CStreamOutMemory *pOutMemory = new CStreamOutMemory();
    CConvertCSIDStreamOut* pConverter = new CConvertCSIDStreamOut( GetRAMCharSetID(), GetDocCharSetID(), pOutMemory);

    InitEscapes();
    printState.Reset( pConverter, this );
    Reduce( m_pRoot );
    if ( m_bLayoutBackpointersDirty ) {
        bEncodeSelectionAsComment = FALSE;
    }
    printState.m_bEncodeSelectionAsComment = bEncodeSelectionAsComment;
    if ( printState.m_bEncodeSelectionAsComment ){
        GetSelection(printState.m_selection);
    }
    PrintTree( m_pRoot );

    printState.Reset( 0, this );
    pConverter->ForgetStream();
    delete pConverter;

    *ppBuffer = pOutMemory->GetText();
    int32 result = pOutMemory->GetLen();
#ifdef DEBUG_WRITETOBUFFER
    // Use XP_TRACE to print out the document one line at a time.
    // XP_TRACE has an internal limit of around 512 characters on
    // the ammount of characters it can print in one call.
    char* pBuffer = *ppBuffer;
    int32 bufLen = result;
    int32 lineStart = 0;
    const int MAXLINELENGTH = 130;
    for(int32 i = 0; i < bufLen; i++){
        char c = pBuffer[i];
        if ( c == '\n' || c == '\r' || i - lineStart > MAXLINELENGTH ) {
            pBuffer[i] = '\0';
            XP_TRACE(("%s", pBuffer + lineStart));
            pBuffer[i] = c;
            lineStart = i;
            if ( c == '\n' || c == '\r' ){
                lineStart++;
                if ( c == '\r' && pBuffer[lineStart] == '\n') {
                    i++;
                    lineStart++;
                }
            }
        }
    }
#endif
    delete pOutMemory;
    return result;
}

XP_HUGE_CHAR_PTR CEditBuffer::GetPositionalText( ){
    CStreamOutMemory *pOutMemory = new CStreamOutMemory();
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_pContext);
    Reduce( m_pRoot );
    StreamToPositionalText( m_pRoot, pOutMemory );
    XP_HUGE_CHAR_PTR pRet = pOutMemory->GetText();

    // If the buffer is too large, display alert message and return NULL.
    // In win16 edt_RemoveNBSP() would loop for larger buffers.
    if (pOutMemory->GetLen() > MAX_POSITIONAL_TEXT) {
        char* tmplate = XP_GetString(XP_EDT_MSG_TEXT_BUFFER_TOO_LARGE);
        if (tmplate) {
            char* msg = PR_smprintf(tmplate, pOutMemory->GetLen(), MAX_POSITIONAL_TEXT);
            if (msg) {
                FE_Alert(m_pContext, msg);
                XP_FREE(msg);
            }
        }
        else
            XP_ASSERT(FALSE);

        XP_HUGE_FREE(pRet);
        pRet = NULL;
    }
    else
        edt_RemoveNBSP( INTL_GetCSIWinCSID(c), (char *)pRet );

    delete pOutMemory;
    return pRet;
}


void CEditBuffer::RestoreState(CEditDocState *pState) {
  if (!pState) return;

  // Maybe we should check that ReadFromBuffer() succeeded before setting the
  // version.
  GetCommandLog()->SetVersion(pState->m_version);
  ReadFromBuffer(pState->m_pBuffer);
}


void CEditBuffer::ReadFromBuffer(XP_HUGE_CHAR_PTR pBuffer){
    /* This starts an asynchronous process that causes the current buffer to be deleted,
     * and then a new CEditBuffer instance to be created for the same MWContext.
     */
    CGlobalHistoryGroup::GetGlobalHistoryGroup()->IgnoreNextDeleteOf(this);
    GetCommandLog()->SetInReload(TRUE);

    char *pDocURL = LO_GetBaseURL(m_pContext);
    URL_Struct *url_s = NET_CreateURLStruct(pDocURL, NET_DONT_RELOAD);

    if(!url_s){
        XP_TRACE(("Couldn't create the url struct."));
        return;
    }

    StrAllocCopy(url_s->content_type, TEXT_HTML);
    NET_StreamClass *stream =
        NET_StreamBuilder(FO_PRESENT, url_s, m_pContext);
    if(stream) {
        delete m_pRoot;
        m_pRoot = 0;
        m_pCurrent = 0;
        delete this; // See ya!

        // XP_STRCPY(buffer, XP_GetString(XP_HTML_MISSING_REPLYDATA));
        int32 length = XP_STRLEN(pBuffer);
        if ( length <= 0 ) {
            pBuffer = EDT_GetEmptyDocumentString();
            length = XP_STRLEN(pBuffer);
        }

        // Output buffer in smaller chunks. put_clock does not take XP_HUGE_CHAR_PTR.
        const int CHUNK_SIZE = 32000;
        char *pChunk = (char *)XP_ALLOC(CHUNK_SIZE);
        if (pChunk != NULL) {
            XP_HUGE_CHAR_PTR pSrc = pBuffer;
            char *pDest = pChunk;
            int count = 0;
            for (;;) {
                if ((*pDest++ = *pSrc++) == 0) {
                    (*stream->put_block)(stream, pChunk, count);
                    break;
                }                    
                else if (++count == CHUNK_SIZE) {
                    (*stream->put_block)(stream, pChunk, count);
                    pDest = pChunk;
                    count = 0;
                }
            }
        }
        else
            XP_ASSERT(FALSE);

        (*stream->complete)(stream);
        XP_FREE(stream);
        XP_FREEIF(pChunk);
    }
    NET_FreeURLStruct(url_s);
}

void CEditBuffer::WriteToStream( IStreamOut *pOut ) {
    InitEscapes();
    CConvertCSIDStreamOut converter( GetRAMCharSetID(), GetDocCharSetID(), pOut);

    printState.Reset( &converter, this );
    Reduce( m_pRoot );
    PrintTree( m_pRoot );
    printState.Reset( 0, this );
    converter.ForgetStream();
}

#ifdef XP_WIN16
// code segment is full, switch to a new segment
#pragma code_seg("EDTBUF2_TEXT","CODE")
#endif

#ifdef DEBUG
void CEditBuffer::DebugPrintState(IStreamOut& stream) {
    {
        // Print the document as HTML
        XP_HUGE_CHAR_PTR pData;
        WriteToBuffer(&pData, TRUE);
        XP_HUGE_CHAR_PTR b = pData;
        int lineLimit = 200;
        char* pLine = (char*) XP_ALLOC(lineLimit);
        while ( *b != '\0' ) {
            int i = 0;
            while ( *b != '\0' && *b != '\n'){
                if ( i < lineLimit-1) {
                    pLine[i++] = *b;
                }
                b++;
            }
            pLine[i] = '\0';
            stream.Printf("%s\n", pLine);
            if ( *b == '\0') break;
            b++;
        }
        XP_FREE(pLine);
        XP_HUGE_FREE(pData);
    }
    {
        CEditSelection selection;
        GetSelection(selection);
        stream.Printf("\nCurrent selection: ");
        selection.Print(stream);
        stream.Printf("\n");
    }
    {
        CPersistentEditSelection selection;
        GetSelection(selection);
        stream.Printf("\nCurrent selection: ");
        selection.Print(stream);
        stream.Printf("\n");
    }
    if ( IsPhantomInsertPoint() ) {
        stream.Printf("   IsPhantomInsertPoint() = TRUE\n");
    }
    stream.Printf("Current typing command: ");
    stream.Printf("%s ", m_bTyping ? "true" : "false");
    stream.Printf("\n");
    stream.Printf("Document version: %d. Stored version %d.", GetCommandLog()->GetVersion(),
        GetCommandLog()->GetStoredVersion());
    stream.Printf("\n");
    GetCommandLog()->Print(stream);
    {
        stream.Printf("\n\nFiles asociated with the current Buffer:");
        char*p = GetAllDocumentFiles(NULL,TRUE);
        char *p1 = p;
        while( p && *p ){
            stream.Printf("\n  %s", p );
            p += XP_STRLEN( p );
        }
        if( p1) XP_FREE( p1 );
    }

    {
        stream.Printf("\n\nNamed Anchors in the current Buffer:");
        char*p = GetAllDocumentTargets();
        char *p1 = p;
        while( p && *p ){
            stream.Printf("\n  %s", p );
            p += XP_STRLEN( p );
        }
        if( p1 ) XP_FREE( p1 );
    }
    // lo_PrintLayout(m_pContext);
}
#endif

#ifdef DEBUG
void CEditBuffer::ValidateTree(){
    if ( m_bSkipValidation ) return;
    // Have the tree validate itself.
    m_pRoot->ValidateTree();
    if ( ! m_iSuppressPhantomInsertPointCheck ) {
        // Check that there are no phantom insert points other than at m_pCurrent
        CEditLeafElement* pElement;
        for ( pElement = m_pRoot->GetFirstMostChild()->Leaf();
            pElement;
            pElement = pElement->NextLeafAll() ){
            CEditInsertPoint ip(pElement, 0);
            if ( IsPhantomInsertPoint(ip) ){
                if ( pElement != m_pCurrent ) {
                    XP_ASSERT(FALSE);
                }
            }
        }
    }
}

void CEditBuffer::SuppressPhantomInsertPointCheck(XP_Bool bSuppress){
    if ( bSuppress )
        m_iSuppressPhantomInsertPointCheck++;
    else
        m_iSuppressPhantomInsertPointCheck--;
}
#endif

void CEditBuffer::DisplaySource( ){
    CStreamOutNet *pOutNet = new CStreamOutNet( m_pContext );
    CConvertCSIDStreamOut* pConverter = new CConvertCSIDStreamOut( GetRAMCharSetID(), GetDocCharSetID(), pOutNet);
    InitEscapes();
    printState.Reset( pConverter, this );
    PrintTree( m_pRoot );
#ifdef DEBUG
    DebugPrintTree( m_pRoot );
    DebugPrintState(*pConverter);
#endif
    printState.Reset( 0, this );
    delete pConverter;
}


//mjudge, this method will leave spaces instead of "\n"s unfortunately, 
//they wont go away.  I will look for another place to remove them nicely....
char* CEditBuffer::NormalizeText( char* pSrc ){
    char *pStr = pSrc;
    char *pDest;
    char lastChar = 1;
    pDest = pStr;

    if( GetParseState()->bLastWasSpace ){
        while( *pStr != '\0' && XP_IS_SPACE( *pStr ) ){
            lastChar = ' ';
            pStr++;
        }
    }

    //
    // Loop through convert all white space to a single white space.
    // leave \r\n and \n and \r and convert them to \n's to later parse out in edtele.cpp finishload
    while( *pStr != '\0'){

        if (  ( !*(pStr+1) && (*pStr== '\n' || *pStr== '\r') ) ||
                ( *(pStr+1) && !*(pStr+2) && *pStr== '\r' && *pStr== '\n' )  ){
            lastChar = *pDest++= '\n';
            break;//out of while loop
            }
        else
            if( XP_IS_SPACE( *pStr ) ){
                lastChar = *pDest++ = ' ';
                while( *pStr != '\0' && XP_IS_SPACE( *pStr )){
                    pStr++;
                }
        }
        else {
            lastChar = *pDest++ = *pStr++;
        }
    }
    *pDest = '\0';

    // if we actually coppied something, check the last character. happens when there is ANY text
    if( pSrc != pDest ){
        GetParseState()->bLastWasSpace = XP_IS_SPACE( lastChar );
    }
    return pSrc;
}



void CEditBuffer::RepairAndSet(CEditSelection& selection)
{
    // force layout stop displaying the current selection.
#ifdef LAYERS
    LO_StartSelectionFromElement( m_pContext, 0, 0, NULL );
#else
    LO_StartSelectionFromElement( m_pContext, 0, 0);
#endif
    CPersistentEditSelection persel = EphemeralToPersistent(selection);
    Reduce(selection.GetCommonAncestor());
    selection = PersistentToEphemeral(persel);
    Relayout( selection.m_start.m_pElement, 0, selection.m_end.m_pElement, RELAYOUT_NOCARET );
    // Need to force selection.
    SetSelection(selection);
}

XP_Bool CEditBuffer::Reduce( CEditElement *pRoot ){
    // work backwards so when children go away, we don't have
    //  lossage.
    CEditElement *pChild, *pNext;
    pChild = pRoot->GetChild();

    while( pChild ) {
        pNext = pChild->GetNextSibling();
        if( Reduce( pChild )){
            pChild->Unlink();       // may already be unlinked
            delete pChild;
        }
        pChild = pNext;
    }
    return pRoot->Reduce( this );
}

//
//
//
void CEditBuffer::NormalizeTree(){
    CEditElement* pElement;

    // Make sure all text is in a container
    pElement = m_pRoot;
    while( (pElement = pElement->FindNextElement( &CEditElement::FindTextAll,0 ))
            != 0 ) {
        // this block of text does not have a container.
        if( pElement->FindContainer() == 0 ){
        }
    }

}

// Finish load timer.

CFinishLoadTimer::CFinishLoadTimer(){
}

void CFinishLoadTimer::OnCallback() {
    m_pBuffer->FinishedLoad2();
}

void CFinishLoadTimer::FinishedLoad(CEditBuffer* pBuffer)
{
    m_pBuffer = pBuffer;
	const uint32 kRelayoutDelay = 1;
    SetTimeout(kRelayoutDelay);
}

void CEditBuffer::FinishedLoad(){
    // To avoid race conditions, set up a timer.
    // An example of a race condition is when the
    // save dialog box goes up when the load is
    // finished, but the image
    // attributes haven't been set yet.
    m_finishLoadTimer.FinishedLoad(this);
}


#ifdef DEBUG_ltabb
int bNoAdjust = 0;
#endif


// Callback from plugins to start saving process.
PRIVATE void edt_OpenDoneCB(EDT_ImageEncoderStatus /*status*/, void* pArg) {
    MWContext *pContext = (MWContext *)pArg;
    // Notify front end that user interaction can resume
    FE_EditorDocumentLoaded( pContext );

    // Make current doc the most-recently-edited in prefs history list
    EDT_SyncEditHistory( pContext );
}

// Relayout the whole document.
void CEditBuffer::FinishedLoad2()
{
    // In case this is a reload, restore the state of the wait flag.
    if ( EDT_IsPluginActive(m_pContext) ) {
        m_pContext->waitingMode = 1;
    }

    // Are there any extra levels on the Parse state?
    while ( m_parseStateStack.StackSize() > 1 ) {
        if ( GetParseState()->m_inJavaScript ) {
            WriteClosingScriptTag();
        }
        else {
            // XP_ASSERT(FALSE); // How did this happen?
        }
        PopParseState();
    }
    // And make sure the head has been finished.
    WriteClosingScriptTag();

    m_pCreationCursor = NULL;

    // Get rid of empty items.
    Reduce( m_pRoot );

    // Give everyone a chance to clean themselves up

    m_pRoot->FinishedLoad(this);

#ifdef EDT_DDT
#ifdef DEBUG_ltabb
    if( ! bNoAdjust ){
        m_pRoot->AdjustContainers(this);
    }
#else
    m_pRoot->AdjustContainers(this);
#endif
    // The AdjustContainer process may have produced empty paragraphs.
    // Calling FinishedLoad a second time cleans these up.
    m_pRoot->FinishedLoad(this);
#endif

#ifdef DEBUG
    m_pRoot->ValidateTree();
#endif

    // May have cleared out some more items.
    Reduce( m_pRoot );

#ifdef DEBUG
    m_pRoot->ValidateTree();
#endif

    // Temporarily set a legal selection.
    m_pCurrent = m_pRoot->NextLeafAll();
    while ( m_pCurrent && m_pCurrent->GetElementType() == eInternalAnchorElement ) {
        m_pCurrent = m_pCurrent->NextLeaf();
    }
    m_iCurrentOffset = 0;

    XP_Bool bShouldSendOpenEvent = !GetCommandLog()->InReload();

    // Set page properties (color, background, etc.)
    //  for a new document:
    XP_Bool bIsNewDocument = EDT_IS_NEW_DOCUMENT(m_pContext)
                                    && !GetCommandLog()->InReload();

    if( bIsNewDocument ){
        // This will call EDT_SetPageData(),
        // which calls RefreshLayout()
        FE_SetNewDocumentProperties(m_pContext);
        m_bDummyCharacterAddedDuringLoad = TRUE; /* Sometimes it's a no-break space. */
        // Get rid of extra characters.
    }
    RefreshLayout();

    // Add GENERATOR meta-data. This tells us who most recently edited the doucment.
    {
 	    char generatorValue[300];
	    XP_SPRINTF(generatorValue, "%.90s/%.100s [%.100s]", XP_AppCodeName, XP_AppVersion, XP_AppName);

        EDT_MetaData *pData = MakeMetaData( FALSE, "GENERATOR", generatorValue);

        SetMetaData( pData );
        FreeMetaData( pData );
    }

    SetSelectionInNewDocument();

    m_bBlocked = FALSE;
	m_bReady = TRUE;

    if ( GetCommandLog()->InReload() ){
        GetCommandLog()->SetInReload(FALSE);
    }
    else {
        DocumentStored();
        GetCommandLog()->Trim();
    }

#ifdef DEBUG
    m_pRoot->ValidateTree();
#endif

    // Initialize correct witdth data for all tables and cells in document
    FixupTableData();
    
    if ( bShouldSendOpenEvent ) {
		History_entry * hist_entry = SHIST_GetCurrent(&(m_pContext->hist));
        char* pURL = hist_entry ? hist_entry->address : 0;
        EDT_PerformEvent(m_pContext, "open", pURL, TRUE, FALSE, edt_OpenDoneCB, m_pContext);
    }
    else {
        // Notify front end that user interaction can resume
        FE_EditorDocumentLoaded( m_pContext );

        // Make current doc the most-recently-edited in prefs history list
        EDT_SyncEditHistory( m_pContext );
    }
    // Flag set during buffer creation to tell 
    //  if we imported a text file
    if( m_bImportText )
        ConvertCurrentDocToNewDoc();
}

void CEditBuffer::ConvertCurrentDocToNewDoc()
{
    char * pUntitled = XP_GetString(XP_EDIT_NEW_DOC_NAME);

    // Traverse all links and images and change URLs to absolute
    //   since we will be destroying our current base doc URL
    EDT_ImageData *pImageData;
    char *pAbsolute = NULL;
    EDT_PageData *pPageData = GetPageData();
    if( !pPageData){
        return;
    }
    
    // Should be the same as pEntry->Address???
    char *pBaseURL = LO_GetBaseURL(m_pContext);

    // Call Java Plugin hook for pages to be closed,
    //  BUT only if it really was an lockable source
    int iType = NET_URL_Type(pBaseURL);
    if( iType == FTP_TYPE_URL ||
        iType == HTTP_TYPE_URL ||
        iType == SECURE_HTTP_TYPE_URL ||
        iType == FILE_TYPE_URL ){
        EDT_PreClose(m_pContext, pBaseURL, NULL, NULL);
    }

    // Walk the tree and find all HREFs.
    CEditElement *pLeaf = m_pRoot->FindNextElement( 
                                  &CEditElement::FindLeafAll,0 );
    // First sweep, mark all HREFs as not adjusted.
    while (pLeaf) {
        linkManager.SetAdjusted(pLeaf->Leaf()->GetHREF(),FALSE);
        pLeaf = pLeaf->FindNextElement(&CEditElement::FindLeafAll,0 );
    }
    // Second sweep, actually adjust the HREFs.
    pLeaf = m_pRoot->FindNextElement( 
            &CEditElement::FindLeafAll,0 );
    while (pLeaf) {
        ED_LinkId linkId = pLeaf->Leaf()->GetHREF();
        if (linkId && !linkManager.GetAdjusted(linkId)) {
            linkManager.AdjustLink(linkId, pBaseURL, NULL, NULL);          
            linkManager.SetAdjusted(linkId,TRUE);
        }
        pLeaf = pLeaf->FindNextElement(&CEditElement::FindLeafAll,0 );
    }

    // Regular images.
    CEditElement *pImage = m_pRoot->FindNextElement( 
                                   &CEditElement::FindImage, 0 );
    while( pImage ){
        pImageData = pImage->Image()->GetImageData();
        if( pImageData ){
            if( pImageData->pSrc && *pImageData->pSrc ){
                char * pOld = XP_STRDUP(pImageData->pSrc);
                pAbsolute = NET_MakeAbsoluteURL( pBaseURL, pImageData->pSrc );
                if( pAbsolute ){
                    XP_FREE(pImageData->pSrc);
                    pImageData->pSrc = pAbsolute;
                }
             }
             if( pImageData->pLowSrc && *pImageData->pLowSrc){
                pAbsolute = NET_MakeAbsoluteURL( pBaseURL, pImageData->pLowSrc );
                if( pAbsolute ){
                    XP_FREE(pImageData->pLowSrc);
                    pImageData->pLowSrc = pAbsolute;
                }
            }    
            pImage->Image()->SetImageData( pImageData );
            edt_FreeImageData( pImageData );
        }
        pImage = pImage->FindNextElement( &CEditElement::FindImage, 0 );
    }

    // If there is a background Image, make it absolute also
    if( pPageData->pBackgroundImage && *pPageData->pBackgroundImage){
        pAbsolute = NET_MakeAbsoluteURL( pBaseURL, pPageData->pBackgroundImage );
        if( pAbsolute ){
            XP_FREE(pPageData->pBackgroundImage);
            pPageData->pBackgroundImage = pAbsolute;
        }
    }

    // Change context's "title" string
    XP_FREEIF(m_pContext->title);
    m_pContext->title = NULL;
    m_pContext->is_new_document = TRUE;

    // Change the history entry data
    History_entry * pEntry = SHIST_GetCurrent(&(m_pContext->hist));
	if(pEntry ){
        XP_FREEIF(pEntry->address);
        pEntry->address = XP_STRDUP(pUntitled);
        XP_FREEIF(pEntry->title);
    }
    // Layout uses this as the base URL for all links and images
    LO_SetBaseURL( m_pContext, pUntitled );

    // Cleat the old title in page data
    XP_FREEIF(pPageData->pTitle); 

    // This will set new background image,
    //   call FE_SetDocTitle() with new "file://Untitled" string,
    //   and refresh the layout of entire doc
    SetPageData(pPageData);

    FreePageData(pPageData);
    
    // This marks the page as "dirty" so we ask user to save
    //  the page even if they didn't type anything
    StartTyping(TRUE);
}
  
void CEditBuffer::SetSelectionInNewDocument(){
    VALIDATE_TREE(this);
    if ( ! m_pRoot )
        return;

    CEditInternalAnchorElement* pBeginSelection = m_pStartSelectionAnchor;
    CEditInternalAnchorElement* pEndSelection = m_pEndSelectionAnchor;
    XP_Bool bBeginStickyAfter = m_bStartSelectionStickyAfter;
    XP_Bool bEndStickyAfter = m_bEndSelectionStickyAfter;

    // At this point there is at most one selection comment of each type in
    // the document.

    // Error recovery - if we have either a begin selection or an end selection,
    // use it for the other edge.
    if ( pBeginSelection && ! pEndSelection){
        pEndSelection = pBeginSelection;
        bEndStickyAfter = bBeginStickyAfter;
    }
    else if ( ! pBeginSelection && pEndSelection){
        pBeginSelection = pEndSelection;
        bBeginStickyAfter = bEndStickyAfter;
    }

    if ( pBeginSelection && pEndSelection ){
        // We have an existing selection.
        // Move from the selection markers to the nearby leaf.
        CEditLeafElement* pB = (CEditLeafElement*) pBeginSelection->NextLeafAll(bBeginStickyAfter);
        if ( pB == NULL ){
            // At the edge of the document
            bBeginStickyAfter = ! bBeginStickyAfter;
            pB = (CEditLeafElement*) pBeginSelection->NextLeafAll(bBeginStickyAfter);
        }
        CEditLeafElement* pE = (CEditLeafElement*) pEndSelection->NextLeafAll(bEndStickyAfter);
        if ( pE == NULL ){
            // At the edge of the document
            bEndStickyAfter = ! bEndStickyAfter;
            pE = (CEditLeafElement*) pEndSelection->NextLeafAll(bEndStickyAfter);
        }
        if ( pB == pEndSelection ){ // The two comments are next to each other.
            pB = pE;
        }
        else if ( pE == pBeginSelection ){ // The two comments are reversed and next to each other.
            pE = pB;
        }

        if ( pB == pBeginSelection || pB == pEndSelection
            || pE == pBeginSelection || pE == pEndSelection ){
            // Degenerate case -- no other elements in the document.
            delete pBeginSelection;
            if ( pBeginSelection != pEndSelection ){
                delete pEndSelection;
            }
            m_pRoot->FinishedLoad(this); // Fixes up container.
            m_pCurrent = m_pRoot->NextLeafAll();
            m_iCurrentOffset = 0;
            Relayout(m_pRoot, 0, m_pRoot->GetLastMostChild() );
       }
        else if ( pB != NULL && pE != NULL ){
            // Get rid of the comments
            delete pBeginSelection;
            if ( pBeginSelection != pEndSelection ){
                delete pEndSelection;
            }

            // Swap if start happens to come after end.
            CEditInsertPoint start(pB, bBeginStickyAfter ? 0 : pB->GetLen(), bBeginStickyAfter);
            CEditInsertPoint end(pE, bEndStickyAfter ? 0 : pE->GetLen(), bEndStickyAfter);
            XP_Bool bFromStart = FALSE;
            if ( start > end ){
                CEditInsertPoint temp = start;
                start = end;
                end = temp;
                bFromStart = TRUE;
            }
            if ( start.m_pElement->IsEndOfDocument() ){
                // Selection at the end of the document.
                // So select the end of the previous element.
                XP_Bool bInsertPoint = (pB == pE);
                start.m_pElement = start.m_pElement->PreviousLeaf();
                if ( start.m_pElement != NULL ) {
                    start.m_iPos = start.m_pElement->GetLen();
                }
                if ( bInsertPoint ) {
                    end = start;
                }
            }
            if ( start.m_pElement != NULL && end.m_pElement != NULL ) {
                CEditSelection selection(start, end, bFromStart);
                SetSelection(selection);
                // FinishedLoad can delete the selected items.
                // Using a persistent selection is safe, because
                // the worst that can happen is that the resulting
                // selection is several characters off.
                // (The right thing to do is make FinishedLoad
                // preserve the selection. But this would be
                // expensive to implement.)
                CPersistentEditSelection persel = EphemeralToPersistent(selection);
                LO_ClearSelection(m_pContext);
                m_pRoot->FinishedLoad(this);
                Reduce( m_pRoot );
                Relayout(m_pRoot, 0, m_pRoot->GetLastMostChild(), RELAYOUT_NOCARET );
                SetSelection(persel);
                return;
            }
            // Otherwise fall through to default behavior
        }
    }
    //if ( pRelayoutFrom != NULL ) Relayout(pRelayoutFrom,0);

    // Default behavior
    m_pCurrent = m_pRoot->NextLeafAll();
    m_iCurrentOffset = 0;
}

void CEditBuffer::DummyCharacterAddedDuringLoad(){
    m_bDummyCharacterAddedDuringLoad = TRUE;
}

XP_Bool CEditBuffer::IsFileModified(){
    int32 last_write_time = m_iFileWriteTime;

    //Get current time and save in m_iFileWriteTime,
    // but NOT if in the process of saving a file
    if( m_pContext->edit_saving_url ){
        return FALSE;
    }

    GetFileWriteTime();
    // Skip if current_time is 0 -- first time through
    return( last_write_time != 0 && last_write_time != m_iFileWriteTime );
}

void CEditBuffer::GetFileWriteTime(){
    char *pDocURL = LO_GetBaseURL(m_pContext);
    char *pFilename = NULL;

    // Don't set the time if we are in the process of saving the file
    if( m_pContext->edit_saving_url ){
        return;
    }

    // Must be a local file type
    if( NET_IsLocalFileURL(pDocURL) &&
    // No longer uses XP_ConvertURLToLocalFile
		( XP_STRCMP( pDocURL, XP_GetString(XP_EDIT_NEW_DOC_NAME) ) != 0 ) &&
		((pFilename = NET_ParseURL(pDocURL, GET_PATH_PART)) != NULL ) ) {
        XP_StatStruct statinfo;
        if( -1 != XP_Stat(pFilename, &statinfo, xpURL) &&
            statinfo.st_mode & S_IFREG ){
            m_iFileWriteTime = statinfo.st_mtime;
        }
    }
    // pFilename is allocated even if XP_ConvertUrlToLocalFile returns FALSE
    if( pFilename ){
        XP_FREE(pFilename);
    }
}

static void SetDocColor(MWContext* pContext, int type, ED_Color& color)
{
    LO_Color *pColor = edt_MakeLoColor(color);
    // If pColor is NULL, this will use the "MASTER",
    //  (default Browser) color, and that's OK!
    LO_SetDocumentColor( pContext, type, pColor );
    if( pColor) XP_FREE(pColor);
}

void CEditBuffer::RefreshLayout(){
    VALIDATE_TREE(this);

    SetDocColor(m_pContext, LO_COLOR_BG, m_colorBackground);
    SetDocColor(m_pContext, LO_COLOR_FG, m_colorText);
    SetDocColor(m_pContext, LO_COLOR_LINK, m_colorLink);
    SetDocColor(m_pContext, LO_COLOR_VLINK, m_colorFollowedLink);
    SetDocColor(m_pContext, LO_COLOR_ALINK, m_colorActiveLink);

    // Fix for bug 42390.
    // Implementation of LO_SetBackgroundImage has changed, it now requires
    // an absolute URL.  Maybe we should require that m_pBackgroundImage is 
    // always absolute.
    if (m_pBackgroundImage && *m_pBackgroundImage) {
      char *pAbsolute = NET_MakeAbsoluteURL( LO_GetBaseURL(m_pContext ), m_pBackgroundImage );
      if (pAbsolute) {
        LO_SetBackgroundImage( m_pContext, pAbsolute );
        XP_FREE(pAbsolute);
      }
    }
    else {
      LO_SetBackgroundImage( m_pContext, m_pBackgroundImage );
    }

    Relayout(m_pRoot, 0, m_pRoot->GetLastMostChild() );
}

void CEditBuffer::SetDisplayParagraphMarks(XP_Bool bDisplay) {
    m_pContext->display_paragraph_marks = bDisplay;
    RefreshLayout();
}

XP_Bool CEditBuffer::GetDisplayParagraphMarks() {
    return m_pContext->display_paragraph_marks;
}


void CEditBuffer::SetDisplayTables(XP_Bool bDisplay) {
    m_pContext->display_table_borders = bDisplay;
    RefreshLayout();
}

XP_Bool CEditBuffer::GetDisplayTables() {
    return m_pContext->display_table_borders;
}

ED_TextFormat CEditBuffer::GetCharacterFormatting( ){
    CEditLeafElement *pElement = m_pCurrent;

    //
    // While selecting, we may not have a valid region
    //
    if( IsSelecting() ){
        return TF_NONE;
    }

    if( IsSelected() ){
        ElementOffset i,i1;
        CEditLeafElement *pEnd;
        XP_Bool bFromStart;
        GetSelection( pElement, i, pEnd, i1, bFromStart );
    }

    if( pElement && pElement->IsA(P_TEXT )){
        return pElement->Text()->m_tf;
    }

    return TF_NONE;
}

int CEditBuffer::GetFontSize( ){
    CEditLeafElement *pElement = m_pCurrent;

    if( IsSelecting() ){
        return 0;
    }

    if( IsSelected() ){
        ElementOffset i,i1;
        CEditLeafElement *pEnd;
        XP_Bool bFromStart;
        GetSelection( pElement, i, pEnd, i1, bFromStart );
    }
    if( pElement && pElement->IsA(P_TEXT )){
        return pElement->Text()->GetFontSize();
    }

	//
	// The following logic is similar to CEditBuffer::GetCharacterData().
	//

	// Try to find the previous text element and use its font size.
	if (pElement) {
		CEditTextElement *tElement = pElement->PreviousTextInContainer();
		if ( tElement){
			return tElement->GetFontSize();
		}

		// Get font size from parent of pElement.
		return pElement->GetDefaultFontSize();
	}

    return 0;
}

int CEditBuffer::GetFontPointSize( ){
    CEditLeafElement *pElement = m_pCurrent;

    if( IsSelecting() ){
        return 0;
    }

    if( IsSelected() ){
        ElementOffset i,i1;
        CEditLeafElement *pEnd;
        XP_Bool bFromStart;
        GetSelection( pElement, i, pEnd, i1, bFromStart );
    }
    if( pElement && pElement->IsA(P_TEXT )){
        return pElement->Text()->GetFontPointSize();
    }

	//
	// The following logic is similar to CEditBuffer::GetCharacterData().
	//

	// Try to find the previous text element and use its font size.
	if (pElement) {
		CEditTextElement *tElement = pElement->PreviousTextInContainer();
		if ( tElement){
			return tElement->GetFontPointSize();
		}

		// Get font size from parent of pElement.
        // There is only one default for all containers, unlike relative font size
		// return pElement->GetDefaultFontSize();
	}

    return ED_FONT_POINT_SIZE_DEFAULT;
}

int CEditBuffer::GetFontFaceIndex()
{
    int iReturn = -1;   // Default is "unknown"

    EDT_CharacterData * pData = GetCharacterData();
    if( pData ){
        if( pData->mask & TF_FONT_FACE ){
            // We are sure of the font state - see if it is set to something
            if( (pData->values & TF_FONT_FACE) && pData->pFontFace ){
                iReturn = ED_FONT_LOCAL;
            } else if( pData->mask & TF_FIXED ){
                // We don't have a NS Font - we have a default HTML font
                //   and we are sure of the Fixed Width state
                if( pData->values & TF_FIXED ){
                    iReturn = 1;   // We have default fixed-with font
                } else {
                    iReturn = 0;   // We have default proportional font
                }
            }
        }
        XP_FREE(pData);
    }
    return iReturn;
}

static char pFontFace[256] = "";

// Use this to search font faces in menu or dropdown list
char * CEditBuffer::GetFontFace()
{
    char * pFontFaces = EDT_GetFontFaces();
    if( !pFontFaces ){
        return NULL;
    }
    EDT_CharacterData * pData = GetCharacterData();
    if( !pData ){
        return NULL;
    }
    
    // Truncate static string we might return;
    *pFontFace = '\0';
    
    // Most of the time we copy font from EDT_CharacterData to this static
    char * pReturn = pFontFace;

    if( pData->mask & TF_FONT_FACE ){
        // We are sure of the font state - see if it is set to something
        if( (pData->values & TF_FONT_FACE) && pData->pFontFace ){
            // We are sure we have a NS Font - get the list to scan

            char * pFontTagList = EDT_GetFontFaceTags();
            if( pFontTagList ){
                //  Start at the second string in the list ("Fixed Width")
                char *pFontTag = pFontTagList + XP_STRLEN(pFontTagList) + 1;
                char *pFace = pFontFaces + XP_STRLEN(pFontFaces) + 1;
                if( *pFontTag != '\0' ) {
                    int iLen;
                    // Skip over fixed-width string for 1st real NS Font
                    pFontTag += XP_STRLEN(pFontTag) + 1;
                    pFace += XP_STRLEN(pFace) + 1;
                    int index = 2;
    	            while( (iLen = XP_STRLEN(pFontTag)) > 0 ) {
                        if( 0 == XP_STRCMP( pFontTag, pData->pFontFace ) ){
                            // We found a "translated" font group,
                            //   return local font that corresponds
                            pReturn = pFace;
                            break;
                        }
                        // Next string
                        pFontTag += iLen+1;
                        pFace += XP_STRLEN(pFace) + 1; 
                        index++;
                    }
                }
            }
            // If we didn't find it in the XP list, it must be
            //   a local system font, so just copy what's in character data
            if( !*pReturn ){
                XP_STRCPY(pFontFace, pData->pFontFace);
            }
        } else if( pData->mask & TF_FIXED ){
            // We don't have a font face -- must be a default HTML font
            //   and we are sure of the Fixed Width state
            if( pData->values & TF_FIXED ){
                // we are "Fixed Width" - its the second string
                pReturn = pFontFaces + XP_STRLEN(pFontFaces) + 1;
            } else {
                // we are "Variable Width"
                pReturn = pFontFaces;
            }
        }
    }

    XP_FREE(pData);

    if(pReturn && !*pReturn){
        // Empty string - return NULL instead
        return NULL;
    }
    return pReturn;
}

ED_Color CEditBuffer::GetFontColor( ){
    CEditLeafElement *pElement = m_pCurrent;

    if( IsSelecting() ){
        return ED_Color::GetUndefined();
    }

    if( IsSelected() ){
        ElementOffset i,i1;
        CEditLeafElement *pEnd;
        XP_Bool bFromStart;
        GetSelection( pElement, i, pEnd, i1, bFromStart );
    }
    if( pElement && pElement->IsA(P_TEXT )){
        return pElement->Text()->GetColor();
    }
	return ED_Color::GetUndefined();
}

TagType CEditBuffer::GetParagraphFormattingSelection(CEditSelection& selection)
{
    TagType type = P_UNKNOWN;

    // Grab the current selection.
//    CEditSelection selection;
    if( selection.IsEmpty() )
        GetSelection(selection);
    CEditContainerElement* pStart = selection.m_start.m_pElement->FindContainer();
    CEditContainerElement* pEnd = selection.m_end.m_pElement->FindContainer();
    XP_Bool bUseEndContainer = !selection.EndsAtStartOfContainer();

    // Scan for all text elements and save first container type found
    // return that type only if all selected text elements have the same type
    while( pStart ){
        if( type == P_UNKNOWN ){
            // First time through
            type = pStart->GetType();
        } else if( type != pStart->GetType() ) {
            // Type is different - get out now
            return P_UNKNOWN;
        }
        // Start = end, we're done
        if( pStart == pEnd ){
            break;
        }
        CEditElement* pLastChild = pStart->GetLastMostChild();
        if ( ! pLastChild ) break;
        CEditElement* pNextChild = pLastChild->NextLeaf();
        if ( ! pNextChild ) break;
        pStart = pNextChild->FindContainer();
        if ( pStart == pEnd && ! bUseEndContainer ) break;
    }
    return type;
}

TagType CEditBuffer::GetParagraphFormatting()
{
    CEditContainerElement *pCont;
    // Empty selection -- will get current selection if exists
    CEditSelection selection;
    TagType type = P_UNKNOWN;

    if( IsSelected() ){
        return GetParagraphFormattingSelection(selection);
    }
    if( IsTableOrCellSelected() )
    {
        // Get each cell contents to examine - as if it were selected
        if( GetFirstCellSelection(selection) )
        {
            // Get format from first selected cell
            type = GetParagraphFormattingSelection(selection);
            if( type != P_UNKNOWN )
            {
                while( GetNextCellSelection(selection) )
                {
                    // All cells must have the same type, else stop now
                    if( type != GetParagraphFormattingSelection(selection) )
                    {
                        type = P_UNKNOWN;
                        break;
                    }
                }
            }
        }
        return type;
    } 
    else {
        if( m_pCurrent && (pCont = m_pCurrent->FindContainer()) != 0){
            type = pCont->GetType();
        }
        else {
            type = P_UNKNOWN;
        }
    }
    return type;
}

void CEditBuffer::PositionCaret(int32 x, int32 y) {
    VALIDATE_TREE(this);
    ClearPhantomInsertPoint();
    DoneTyping();
    ClearMove();
#ifdef LAYERS
    LO_PositionCaret( m_pContext, x, y, NULL );
#else
    LO_PositionCaret( m_pContext, x, y );
#endif
}

// This is static so all windows may share current drag table data
EDT_DragTableData* CEditBuffer::m_pDragTableData = NULL;

XP_Bool CEditBuffer::StartDragTable(int32 /*x*/, int32 /*y*/)
{
    if( !m_pDragTableData )
    {
        m_pDragTableData = XP_NEW( EDT_DragTableData );
    }
    if( m_pDragTableData )
    {
        // Initialize data for possible dragging
        // This is used by PositionDropCaret()
        m_pDragTableData->iDropType = ED_DROP_NORMAL;
        m_pDragTableData->pDragOverCell = NULL;
        // Source that we are dragging
        m_pDragTableData->iSourceType = GetTableSelectionType(); //m_TableHitType;

        // Get first cell of selection - we ignore a drop on that cell
        if( m_pSelectedLoTable )
        {
            // Get first cell of entire table
            LO_Element *pLoCell = m_pSelectedLoTable->next;
            while( pLoCell && pLoCell->type != LO_CELL ) { pLoCell = pLoCell->lo_any.next; }
            m_pDragTableData->pFirstSelectedCell = pLoCell;
        }
        else {
            // Get first cell in selected LO_CELL list
            m_pDragTableData->pFirstSelectedCell = (LO_Element*)m_SelectedLoCells[0];
        }
        return TRUE;
    }
    return FALSE;
}

void CEditBuffer::StopDragTable()
{ 
    // Free global struct and set to NULL
    XP_FREEIF(m_pDragTableData);
}

/* Converts ("snaps") input X, Y (doc coordinates) to X, Y needed for drop caret 
 *  and calls appropriate front-end FE_Display<Text|Generic|Image>Caret to use show where
 *  a drop would occur. It does NOT change current selection or internal caret position
 * Also handles table selection
*/
XP_Bool CEditBuffer::PositionDropCaret(int32 x, int32 y)
{
#ifdef XP_WIN
    // If we are dragging a table selection,
    //  get feedback data, except when entire table selected
    //   (use normal caret placement for that)
    if( m_pDragTableData && m_pDragTableData->iSourceType > ED_HIT_SEL_TABLE )
    {
        // Get where we currently are
        LO_Element *pLoCell = NULL;
        int32 iWidth, iHeight;

        // Get where user is currently dragging over
        ED_DropType iDropType = GetTableDropRegion(&x, &y, &iWidth, &iHeight, &pLoCell);

        // No point in dropping exactly on top of selection being dragged
        if( iDropType == ED_DROP_NONE )
        {
            FE_DestroyCaret(m_pContext);
            return FALSE;
        }
        
        if( iDropType != ED_DROP_NORMAL )
        {
            // Call Front end to display drop feedback only if
            //  different from previous condition
            if( pLoCell != m_pDragTableData->pDragOverCell ||
                iDropType != m_pDragTableData->iDropType )
            {
                m_pDragTableData->iDropType = iDropType;
                m_pDragTableData->pDragOverCell = pLoCell;
                m_pDragTableData->X = x;
                m_pDragTableData->Y = y;
                m_pDragTableData->iWidth = iWidth;
                m_pDragTableData->iHeight = iHeight;

                FE_DestroyCaret(m_pContext);
                if( m_pDragTableData->iDropType == ED_DROP_REPLACE_CELL )
                {
                    // TODO: SET "REPLACE" BIT FOR ALL CELLS TO BE REPLACED
                    //   AND CALL FEs TO REDRAW EACH CELL
                    // TEMP: Just show replace cell as a regular drop caret
                    lo_PositionDropCaret(m_pContext, x, y, NULL);
                } else {
                    FE_DisplayDropTableFeedback(m_pContext, m_pDragTableData);
                }
            }
            return TRUE;
        }
    }
#endif
    FE_DestroyCaret(m_pContext);
    lo_PositionDropCaret(m_pContext, x, y, NULL);
    return TRUE;
}

void CEditBuffer::DeleteSelectionAndPositionCaret( int32 x, int32 y )
{
    XP_Bool bTableSelected = IsTableOrCellSelected();
    if( bTableSelected && !IsSelected() )
    {
    // Nothing selected - nothing to delete
        PositionCaret(x,y);    
        return;
    }

    // Get where we would drop if not deleting anything
    int32 position;
    LO_Element * pLoElement = lo_PositionDropCaret(m_pContext, x, y, &position );
    if( pLoElement )
    {
        // Get the CEditObject at drop point and use it to create new insert point
        CPersistentEditSelection PSel;
        GetSelection(PSel);
        CEditElement * pEdElement = pLoElement->lo_any.edit_element;
	    // Create an "ephemeral" insert point from the EditLeafObject
	    CEditInsertPoint IP(pEdElement, pLoElement->lo_any.edit_offset + position /*???? offset */);
        CPersistentEditInsertPoint Insert = EphemeralToPersistent(IP);

        // Check if new insert point will be after the selection
        //  we will delete and adjust the index by the amount deleted
        // (Table elements are handled below)
        if( !bTableSelected && Insert.m_index > PSel.m_start.m_index )
		    Insert.m_index -= PSel.m_end.m_index - PSel.m_start.m_index;
#if 0
//TODO: This needs more work
        if( bTableSelected )
        {
            CEditTableElement *pTable = NULL;
            if( m_pSelectedEdTable )
                pTable = m_pSelectedEdTable;
            else
                pTable = m_SelectedEdCells[0]->GetParentTable();

            if( pTable )
            {
                if( m_pSelectedEdTable )
                {
            	    SetInsertPoint(Insert);
                    CEditElement *pStart = m_pRoot;
                    CEditElement *pPrev = pTable->GetPreviousSibling();
                    if( pPrev )
                        pStart = pPrev->FindNextElement(&CEditElement::FindLeafAll,0 );
                    CEditElement *pEnd = pTable->GetLastMostChild()->NextLeaf();
                    // First unselect the table
                    ClearTableAndCellSelection();
                    // Delete the entire table
                    // Assumes current insert point is NOT inside the selected table
    	            pTable->Unlink();
                    delete pTable;
                    //Relayout(pStart, 0,  pEnd);
                } else
                {
                    // Delete all selected cells (this will call Relayout())
                    DeleteSelectedCells();
            	    SetInsertPoint(Insert);
                }
            }
        }
        else
#endif
        {
    	    // Delete the selection and set new insert position
    	    DeleteSelection();
            SetInsertPoint(Insert);
        }
    }
}
 
 // this routine is called when the mouse goes down.
//
void CEditBuffer::StartSelection( int32 x, int32 y, XP_Bool doubleClick ){
    VALIDATE_TREE(this);
    ClearPhantomInsertPoint();
    {
        // This is a hack to avoid auto-scrolling to the old selection.
        XP_Bool scrolling = m_inScroll;
        m_inScroll = TRUE;
        ClearSelection();
        m_inScroll = scrolling;
    }

    // Remove any existing table or cell selection
    // This is the best place to put this so all "normal" selection-setting
    //  clears the table selection, but it prevents moving caret without clearing table
    ClearTableAndCellSelection();

    FE_DestroyCaret(m_pContext);
    ClearMove(); // do this when moving caret and not up or down arrow
    DoneTyping();
    if ( doubleClick ) {
#ifdef LAYERS
        LO_DoubleClick( m_pContext, x, y, NULL );
#else
        LO_DoubleClick( m_pContext, x, y );
#endif
    }
    else {
#ifdef LAYERS
        LO_Click( m_pContext, x, y, FALSE, NULL );
#else
        LO_Click( m_pContext, x, y, FALSE );
#endif /* LAYERS */
    }
}

// Note: By using MoveAndHideCaretInTable,
//  we do NOT expose caret, scroll window etc.
void CEditBuffer::MoveToFirstSelectedCell()
{
    int iSize = m_SelectedLoCells.Size();
    if( iSize )
        MoveAndHideCaretInTable((LO_Element*)m_SelectedLoCells[0]);
}

void CEditBuffer::MoveToLastSelectedCell()
{
    int iSize = m_SelectedLoCells.Size();
    if( iSize )
        MoveAndHideCaretInTable((LO_Element*)m_SelectedLoCells[iSize-1]);
}

void CEditBuffer::MoveAndHideCaretInTable(LO_Element *pLoElement)
{
    if(!pLoElement)
        return;
    // From StartSelection()
    ClearPhantomInsertPoint();
    FE_DestroyCaret(m_pContext);
    // What do these do?
    ClearMove();
    DoneTyping();
    CEditElement *pLastElement = NULL;

    if( pLoElement->type == LO_TABLE )
    {
        // Get first cell
        while( pLoElement->type != LO_CELL )
            pLoElement = pLoElement->lo_any.next;
    }

    if( pLoElement->type != LO_CELL )
        return;

    // Search for the last element in the cell that 
    //   has edit pointer, and thus is a leaf
    LO_Element * pCellElement = pLoElement->lo_cell.cell_list;
    while( pCellElement )
    {
        if( pCellElement->lo_any.edit_element )
            pLastElement = pCellElement->lo_any.edit_element;

		pCellElement = pCellElement->lo_any.next;
    }

    if( pLastElement )
    {    
        // From SetInsertPoint()
    #ifdef LAYERS
        LO_StartSelectionFromElement( m_pContext, 0, 0, NULL );
    #else
        LO_StartSelectionFromElement( m_pContext, 0, 0);
    #endif
        m_pCurrent = pLastElement->Leaf();
        if( pLastElement->IsText() )
        {
            char *pText = pLastElement->Text()->m_pText;
            m_iCurrentOffset = pText ? XP_STRLEN(pText) : 0;
        } else {
            m_iCurrentOffset = 1;   //TODO: ASK JACK IF THIS IS OK FOR ALL NON-TEXT
        }
        m_bCurrentStickyAfter = FALSE;
    }
}

// Use only after deleting a cell, and before Relayout(),
//  so later doesn't crash because current element is deleted
void CEditBuffer::MoveToExistingCell(CEditTableElement *pTable, int32 X, int32 Y)
{
    if( !pTable )
        return;
    LO_Element *pLoElement = NULL;

    // Get the cell under the supplied coordinates
    GetTableHitRegion(X, Y, &pLoElement);
    CEditTableCellElement *pCell = NULL;
    if( pLoElement && pLoElement->type == LO_CELL )
    {
        // Try to get cell at
        //   Current cursor location...
        intn i;
        intn iRowCount = pTable->m_RowLayoutData.Size();
        intn iColCount = pTable->m_ColumnLayoutData.Size(); 
        for( i = 0; i < iRowCount; i++ )
        {
            // Find the row spanning the Y value
            if( Y >= pTable->m_RowLayoutData[i]->Y )
            {
                if( i+1 == iRowCount || Y < pTable->m_RowLayoutData[i+1]->Y )
                    pCell = pTable->m_RowLayoutData[i]->pEdCell;
                break;
            }
        }
        if( pCell )
        {
            // Scan through cells in the row found
            //  to get cell spanning the X value
            while( pCell )
            {
                int32 iCellX = pCell->GetX();
                if( X >= iCellX && X < (iCellX + pCell->GetFullWidth()) )
                    break;

                CEditTableCellElement *pNextCell = pTable->GetNextCellInRow(pCell);
                if( pNextCell == NULL )
                    break;

                pCell = pNextCell;
            }
        }
        
        // Unlikely, but defer to 1st cell in table if not found above
        if( !pCell )
            pCell = pTable->GetFirstCell();
    }
    if( pCell )
    {
        SetTableInsertPoint(pCell);
        return;
    }
    // Set insert point to beginning of page if no cell found
//    CEditInsertPoint insertPoint(m_pRoot->GetFirstMostChild(), 0);
//    CEditSelection selection = CEditSelection(insertPoint, insertPoint);
    CEditElement *pFirstElement = m_pRoot->GetFirstMostChild();
    CEditSelection selection(pFirstElement, 0, pFirstElement, 0);
    SetSelection(selection);
}

// this routine is called when the right mouse goes down.
//
void CEditBuffer::SelectObject( int32 x, int32 y ){
    PositionCaret(x,y);
    ClearSelection();
    FE_DestroyCaret(m_pContext);
#ifdef LAYERS
    LO_SelectObject(m_pContext, x, y, NULL);
#else
    LO_SelectObject(m_pContext, x, y);
#endif
}

//
//
// this routine is called when the mouse is moved while down.
//  at this point we actually decide that we are inside a selection.
//
void CEditBuffer::ExtendSelection( int32 x, int32 y ){
    BeginSelection();
    LO_ExtendSelection( m_pContext, x, y );

    // may not result in an actual selection.
    //  if this is the case, just position the caret.
    if( ! IsSelected() ) {
        // If there's no selection, it is still possible that a
        // selection has been started.
        if ( ! LO_IsSelectionStarted( m_pContext ) ) {
            StartSelection( x, y, FALSE );
        }
        else
        {
            // The selection is the empty selection.
            //ClearSelection();
            //FE_DestroyCaret(m_pContext);
            //ClearMove();
        }
    }
    else {
        m_pCurrent = 0;
        m_iCurrentOffset = 0;
        // We really did extend selection - 
        // Remove any existing table or cell selection
        ClearTableAndCellSelection();
     }
}

void CEditBuffer::ExtendSelectionElement( CEditLeafElement *pEle, int iOffset, XP_Bool bStickyAfter ){
    int iLayoutOffset;
    LO_Element* pLayoutElement;
    BeginSelection();

    if( IsSelected() ){
        LO_Element *pStart, *pEnd;
        intn iStartPos, iEndPos;
        XP_Bool bFromStart;
        LO_GetSelectionEndPoints( m_pContext,
                    &pStart, &iStartPos,
                    &pEnd, &iEndPos,
                    &bFromStart, 0);
        if( !(pStart == pEnd && iStartPos == iEndPos) ){
            if( iOffset ) iOffset--;
        }
    }
    else {
        if( iOffset ) iOffset--;
    }


    // we need to convert a selection edit iOffset to a Selection
    //  offset.  This will only work when selecting to the right...

    pEle->GetLOElementAndOffset( iOffset, bStickyAfter,
               pLayoutElement, iLayoutOffset );

    LO_ExtendSelectionFromElement( m_pContext, pLayoutElement,
                iLayoutOffset, FALSE );
}

void CEditBuffer::SelectAll(){
    VALIDATE_TREE(this);
    // Remove any existing table or cell selection
    ClearTableAndCellSelection();
    ClearPhantomInsertPoint();
    ClearMove();
    DoneTyping();
	// Clear selection (don't resync insertion point to avoid scrolling.)
	ClearSelection(FALSE);
    // Destroy the cursor. Perhaps this is something ClearSelection should do.
    FE_DestroyCaret(m_pContext);
	// Delegate actual selection to the layout engine
    if( LO_SelectAll(m_pContext) ) {
	    // Clear insertion point
	    m_pCurrent = 0;
	    m_iCurrentOffset = 0;
	}
    RevealSelection();
}


// This is the old version 
void CEditBuffer::SelectTable(){
    VALIDATE_TREE(this);
    ClearPhantomInsertPoint();
    ClearMove();
    DoneTyping();
    CEditSelection selection;
    GetSelection(selection);
    CEditTableElement* pStartTable = selection.m_start.m_pElement->GetTableIgnoreSubdoc();
    CEditSelection startAll;
    if ( pStartTable ) {
        pStartTable->GetAll(startAll);
    }
    CEditTableElement* pEndTable = selection.m_end.m_pElement->GetTableIgnoreSubdoc();
    CEditSelection endAll;
    if ( pEndTable ) {
        pEndTable->GetAll(endAll);
    }
    if ( pStartTable ) {
        if ( pEndTable ) {
            // Both a start table and an end table.
            selection.m_start = startAll.m_start;
            selection.m_end = endAll.m_end;
        }
        else {
            // Just a start table.
            selection = startAll;
        }
    } else {
        if ( pEndTable ) {
            // Just an end table.
            selection = endAll;
        }
        else {
            // No table. Don't change selection.
            return;
        }
    }

    SetSelection(selection);
}

// Select the cell boundary of cell containing the current edit element
void CEditBuffer::SelectTableCell()
{
    CEditTableCellElement* pEdCell = m_pCurrent->GetTableCellIgnoreSubdoc();
    if( pEdCell )
    {
        LO_CellStruct *pLoCell = pEdCell->GetLoCell();
        if( pLoCell )
        {
            SelectCell(TRUE, pLoCell, pEdCell);
        }
    }
}


void CEditBuffer::BeginSelection( XP_Bool bExtend, XP_Bool bFromStart ){
    // Remove any existing table or cell selection
    ClearTableAndCellSelection();

    ClearMove(); // For side effect of flushing table formatting timer.
    if( !IsSelected() ){
        int iLayoutOffset;
        CEditLeafElement *pNext = 0;
        LO_Element* pLayoutElement;

        m_bSelecting = TRUE;

        FE_DestroyCaret( m_pContext );

        ClearPhantomInsertPoint();

        // If we're at the end of an element, and the next element isn't a break,
        // then move the cursor to the start of the next element.
        if ( m_pCurrent->GetLen() == m_iCurrentOffset
                && (pNext = m_pCurrent->LeafInContainerAfter()) != 0
                && ! m_pCurrent->CausesBreakAfter() && ! pNext->CausesBreakBefore()){
            XP_Bool good = pNext->GetLOElementAndOffset( 0, FALSE,
                    pLayoutElement, iLayoutOffset );
            if ( ! good ) {
                XP_ASSERT(FALSE);
                return;
            }
       }
        else {
            XP_Bool good = m_pCurrent->GetLOElementAndOffset( m_iCurrentOffset, m_bCurrentStickyAfter,
                        pLayoutElement, iLayoutOffset );
            if ( ! good ) {
                XP_ASSERT(FALSE);
                return;
            }
        }

        // if we're selecting forward, and
        // if we are before the space that cause the wrap, we should be
        //  starting selection from the line feed.
        if( ! bFromStart && pLayoutElement->type == LO_TEXT
                    && iLayoutOffset == pLayoutElement->lo_text.text_len )
        {
            if( pLayoutElement->lo_any.next &&
                pLayoutElement->lo_any.next->type == LO_LINEFEED )
            {
                pLayoutElement = pLayoutElement->lo_any.next;
                iLayoutOffset = 0;
            }
            else
            {
                // The layout element is shorter than we think it should
                //  be.
                // This appears to be an OK situation, now that we don't merge
                // text elements. See bug 58763.
                // XP_ASSERT( FALSE );
            }
        }

        XP_ASSERT( pLayoutElement );

        if ( bExtend ) {
            if ( ! LO_SelectElement( m_pContext, pLayoutElement,
                    iLayoutOffset, bFromStart ) ) {
                SetCaret(); // If the selection moved off the end of the document, set the caret again.
            }
        }
        else {
#ifdef LAYERS
            LO_StartSelectionFromElement( m_pContext, pLayoutElement,
                    iLayoutOffset, NULL );
#else
            LO_StartSelectionFromElement( m_pContext, pLayoutElement,
                    iLayoutOffset);
#endif
        }
    }
}

void CEditBuffer::EndSelection(int32 /* x */, int32 /* y */){
    EndSelection();
}

void CEditBuffer::EndSelection( )
{
    // Clear the move just in case EndSelection is called out of order
    // (this seems to be happening in the WinFE when you click a lot.)
    // ClearMove has a side-effect of flushing the relayout timer.
    ClearMove();

    // Fix for the mouse move that does not move enough
    //   to show selection (kills caret)

    if( LO_IsSelectionStarted(m_pContext) ){
        SetCaret();
    }
    m_bSelecting = FALSE;

    // Make the active end visible
    RevealSelection();
}

void CEditBuffer::RevealSelection()
{
    if ( IsSelected() ) {
        CEditLeafElement* pStartElement;
        ElementOffset iStartOffset;
        CEditLeafElement* pEndElement;
        ElementOffset iEndOffset;
        XP_Bool bFromStart;
        GetSelection( pStartElement, iStartOffset, pEndElement, iEndOffset, bFromStart );
        if ( bFromStart ) {
            RevealPosition(pStartElement, iStartOffset, FALSE);
        }
        else {
            RevealPosition(pEndElement, iEndOffset, FALSE);
        }
    }
    else
    {
        if(m_pCurrent != NULL){
            RevealPosition(m_pCurrent, m_iCurrentOffset, m_bCurrentStickyAfter);
        }
    }
}

void CEditBuffer::SelectCurrentElement(){
    FE_DestroyCaret(m_pContext);
    SelectRegion( m_pCurrent, 0, m_pCurrent, 1,
        TRUE, TRUE );
    m_pCurrent = 0;
    m_iCurrentOffset = 0;
}


void CEditBuffer::ClearSelection( XP_Bool bResyncInsertPoint, XP_Bool bKeepLeft ){

    if( !bResyncInsertPoint ){
#ifdef LAYERS
       LO_StartSelectionFromElement( m_pContext, 0, 0, NULL);
#else
       LO_StartSelectionFromElement( m_pContext, 0, 0);
#endif
       return;
    }

    CEditSelection selection;
    GetSelection(selection);
    selection.ExcludeLastDocumentContainerEnd();
    SetInsertPoint(*selection.GetEdge( ! bKeepLeft ));
}


void CEditBuffer::GetInsertPoint( CEditLeafElement** ppEle, ElementOffset *pOffset, XP_Bool *pbStickyAfter){
    LO_Element *pStart, *pEnd;
    intn iStartPos, iEndPos;
    XP_Bool bFromStart;

    if( IsSelected() ){
        //
        // Grab the current selection.
        //
        LO_GetSelectionEndPoints( m_pContext,
                    &pStart, &iStartPos,
                    &pEnd, &iEndPos,
                    &bFromStart, 0 );

        XP_ASSERT( pStart );
        XP_ASSERT( pEnd );

        if( pStart == pEnd && iEndPos == iStartPos ){
            bFromStart = TRUE;
        }

        if( bFromStart ){
            *pbStickyAfter = iStartPos != 0;
            while( pStart && pStart->lo_any.edit_element == 0 ){
                pStart = pStart->lo_any.next;
                if( pStart &&pStart->type == LO_TEXT ){
                    iStartPos = pStart->lo_text.text_len;
                    if( iStartPos ) iStartPos--;
                }
                else {
                    iStartPos = 0;
                }
            }
            if( pStart ){
                *ppEle = pStart->lo_any.edit_element->Leaf(),
                *pOffset = pStart->lo_any.edit_offset+iStartPos;
            }
            else{
                // LTNOTE: NEED to implement end of document:
                //Insert point is at end of document
                //EndOfDocument( FALSE );
                XP_ASSERT(FALSE);
            }
       }
        else {
            *pbStickyAfter = iEndPos != 0;
            while( pEnd && pEnd->lo_any.edit_element == 0 ){
                pEnd = pEnd->lo_any.prev;
                if( pEnd && pEnd->type == LO_TEXT ){
                    iEndPos = pEnd->lo_text.text_len;
                    if( iEndPos ) iEndPos--;
                }
                else {
                    iEndPos = 0;
                }
            }
            if( pEnd ){
                *ppEle = pEnd->lo_any.edit_element->Leaf(),
                *pOffset = pEnd->lo_any.edit_offset+iEndPos;
            }
            else {
                // LTNOTE: NEED to implement end of document:
                //Insert point is at end of document.
                //EndOfDocument( FALSE );
                XP_ASSERT(FALSE);
            }
        }

    }
    else {
        *ppEle = m_pCurrent;
        *pOffset = m_iCurrentOffset;
        *pbStickyAfter = m_bCurrentStickyAfter;
    }
    if (ppEle && *ppEle && (*ppEle)->IsEndOfDocument()) {
        // End of document. Move back one edit position
        CEditInsertPoint ip(*ppEle, *pOffset, *pbStickyAfter);
        ip = ip.PreviousPosition();
        *ppEle = ip.m_pElement;
        *pOffset = ip.m_iPos;
        *pbStickyAfter = ip.m_bStickyAfter;
    }
}


//
// The 'PropertyPoint' is the beginning of a selection or current insert
//  point.
//
// Returns
//  TRUE if a single element is selected like an image or hrule
//  FALSE no selection or an extended selection.
//
XP_Bool CEditBuffer::GetPropertyPoint( CEditLeafElement**ppElement,
            ElementOffset* pOffset ){
    LO_Element *pStart, *pEnd;
    intn iStartPos, iEndPos;
    XP_Bool bFromStart;
    XP_Bool bSingleElementSelection;


    if( IsSelected() ){
        //
        // Grab the current selection.
        //
        LO_GetSelectionEndPoints( m_pContext,
                    &pStart, &iStartPos,
                    &pEnd, &iEndPos,
                    &bFromStart, &bSingleElementSelection );

        XP_ASSERT( pStart->lo_any.edit_element );

        //------------------------- Begin Removable code ------------------------
        //
        // LTNOTE:  We are doing this in GetSelectionEndpoints, but I
        //  don't think we need to be doing it.  If the above assert never
        //  happens remove this code
        //
        while( pStart && pStart->lo_any.edit_element == 0 ){
            pStart = pStart->lo_any.next;
            if( pStart &&pStart->type == LO_TEXT ){
                iStartPos = pStart->lo_text.text_len;
                if( iStartPos ) iStartPos--;
            }
            else {
                iStartPos = 0;
            }
        }
        //------------------------- End Removable code ------------------------

        *ppElement = pStart->lo_any.edit_element->Leaf();
        *pOffset = pStart->lo_any.edit_offset+iStartPos;
        XP_ASSERT( *ppElement );
    }
    else {
        *ppElement = m_pCurrent;
        *pOffset = m_iCurrentOffset;
        bSingleElementSelection = FALSE;
    }
    return bSingleElementSelection;
}

void CEditBuffer::SelectRegion(CEditLeafElement *pBegin, intn iBeginPos,
            CEditLeafElement* pEnd, intn iEndPos, XP_Bool bFromStart, XP_Bool bForward  ){
    int iBeginLayoutOffset;
    LO_Element* pBeginLayoutElement;
    int iEndLayoutOffset;
    LO_Element* pEndLayoutElement;

    // Remove any existing table or cell selection???
    //ClearTableAndCellSelection();

    // If the start is an empty element, grab previous item.
    if( iBeginPos == 0 && pBegin->Leaf()->GetLen() == iBeginPos ){
        CEditLeafElement *pPrev = pBegin->PreviousLeafInContainer();
        if( pPrev ){
            pBegin = pPrev;
            iBeginPos = pPrev->GetLen();
        }
    }

    // if the end is a phantom insert point, grab the previous
    // item.

    if( iEndPos == 0 ){
        CEditLeafElement* pPrev = pEnd->PreviousLeafInContainer();
        if( pPrev ){
            pEnd = pPrev;
            iEndPos = pPrev->GetLen();
        }
    }

    XP_Bool startOK = pBegin->GetLOElementAndOffset( iBeginPos, FALSE,
            pBeginLayoutElement, iBeginLayoutOffset );
    XP_Bool endOK = pEnd->GetLOElementAndOffset( iEndPos, FALSE,
               pEndLayoutElement, iEndLayoutOffset );

    XP_ASSERT(startOK);
    XP_ASSERT(endOK);
    if (startOK && endOK ) {
        if ( ! IsSelected() ) {
            FE_DestroyCaret( m_pContext );
        }
        // Hack for end-of-document
        if ( iEndLayoutOffset < 0 ) iEndLayoutOffset = 0;

        /* LO_SelectRegion can call back to relayout the document under these circumstances:
         * If we've changed the size of an image, and we select or unselect the image, then
         * lo_HilightSelect calls CL_CompositeNow, which decides that the document bounds have
         * changed, and calls the front end to change the document dimensions.
         * This will eventually call EDT_RefreshLayout, which will
         * get confused and think that the document is not currently selected.
         */
        XP_Bool save_NoRelayout = m_bNoRelayout;
        m_bNoRelayout = TRUE;

        LO_SelectRegion( m_pContext, pBeginLayoutElement, iBeginLayoutOffset,
            pEndLayoutElement, iEndLayoutOffset, bFromStart, bForward );

        m_bNoRelayout = save_NoRelayout;

#ifdef DEBUG_EDITOR_LAYOUT
        // Verify that the selection is reversible.
        CEditSelection selection;
        GetSelection( selection );
        CEditInsertPoint a(pBegin, iBeginPos);
        CEditInsertPoint b(pEnd, iEndPos);
        CEditSelection original(a, b, bFromStart);
        XP_ASSERT(original == selection);
#endif
    }

//    EndSelection();
}

void CEditBuffer::SetSelection(CEditSelection& selection)
{
    if ( selection.IsInsertPoint() ) {
        SetInsertPoint(selection.m_start.m_pElement, selection.m_start.m_iPos, selection.m_start.m_bStickyAfter);
    }
    else {
        SelectRegion(selection.m_start.m_pElement, selection.m_start.m_iPos,
            selection.m_end.m_pElement, selection.m_end.m_iPos,
            selection.m_bFromStart);
    }
}

void CEditBuffer::SetInsertPoint(CEditInsertPoint& insertPoint) {
    CEditSelection selection(insertPoint, insertPoint, FALSE);
    SetSelection(selection);
}

void CEditBuffer::SetInsertPoint(CPersistentEditInsertPoint& insertPoint) {
    CEditInsertPoint p = PersistentToEphemeral(insertPoint);
    SetInsertPoint(p);
}

// Use data from supplied selection, or get from current selection if supplied is empty
void CEditBuffer::GetSelection( CEditSelection& selection, CEditLeafElement*& pStartElement, ElementOffset& iStartOffset,
                CEditLeafElement*& pEndElement, ElementOffset& iEndOffset, XP_Bool& bFromStart )
{
    if( selection.m_start.m_pElement == 0 || selection.m_end.m_pElement == 0 )
    {
        // This version will get existing selection first
        GetSelection( pStartElement, iStartOffset, pEndElement, iEndOffset, bFromStart );
        // Fill in values for caller's selection object as well
        selection.m_start.m_pElement = pStartElement;
        selection.m_start.m_iPos = iStartOffset;
        selection.m_end.m_pElement = pEndElement;
        selection.m_end.m_iPos = iEndOffset;
        selection.m_bFromStart = bFromStart;
    }
    else
    {
        // Get data from supplied selection
        pStartElement = selection.m_start.m_pElement;
        iStartOffset = selection.m_start.m_iPos;
        pEndElement = selection.m_end.m_pElement;
        iEndOffset = selection.m_end.m_iPos;
        bFromStart = selection.m_bFromStart;
    }
}

void CEditBuffer::GetSelection( CEditLeafElement*& pStartElement, ElementOffset& iStartOffset,
                CEditLeafElement*& pEndElement, ElementOffset& iEndOffset, XP_Bool& bFromStart ){
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_pContext);
    int16 win_csid = INTL_GetCSIWinCSID(c);

    //cmanske: IS THIS RISKY? WE ASSERT HERE IF WE DON'T RELAYOUT
    if( /*!m_bNoRelayout && */ m_bLayoutBackpointersDirty ) {
        XP_ASSERT(FALSE);
        return;
    }
    LO_Element *pStart, *pEnd;
    intn iStartPos, iEndPos;


    XP_ASSERT( IsSelected());

    //
    // Grab the current selection.
    //
    LO_GetSelectionEndPoints( m_pContext,
                &pStart, &iStartPos,
                &pEnd, &iEndPos,
                &bFromStart, 0 );

     if ( ! pStart->lo_any.edit_element ) {
        XP_ASSERT(FALSE);
        // We should always be handed back an editable element.
        // LTNOTE: if we blow up here it is because we have searched to the end
        //  of document.
        while( pStart->lo_any.edit_element == 0 ){
            pStart = pStart->lo_any.next;
            iStartPos = 0;
        }
    }

    pStartElement = pStart->lo_any.edit_element->Leaf();
    iStartOffset = pStart->lo_any.edit_offset+iStartPos;

    if ( ! pEnd->lo_any.edit_element ) {
        XP_ASSERT(FALSE); // We should always be handed back an editable element.
        while( pEnd->lo_any.edit_element == 0 ){
            pEnd = pEnd->lo_any.prev;
            if( pEnd->type == LO_TEXT ){
                iEndPos = pEnd->lo_text.text_len;
                if( iEndPos ) iEndPos--;
            }
            else {
                iEndPos = 0;
            }
        }
    }

    pEndElement = pEnd->lo_any.edit_element->Leaf();
    // jhp The simple way seems to work better.
    iEndOffset = pEnd->lo_any.edit_offset+iEndPos;

    if ( iEndOffset < 0 ) {
        iEndOffset = 0; // End-of-document flummery
    }


	// Normalize multibyte position,
	// make it always start from beginning of char, end of the beginning of next char
	if (pStart->type == LO_TEXT && pStart->lo_text.text
	    && INTL_NthByteOfChar(win_csid, (char *) pStart->lo_text.text, iStartOffset+1) > 1) {
		XP_TRACE(("iStartOffset = %d which is wrong for multibyte", (int)iStartOffset));
		iStartOffset = INTL_NextCharIdx(win_csid, (unsigned char *) pStart->lo_text.text,iStartOffset);
	}
	if (pEnd->type == LO_TEXT && pEnd->lo_text.text
	    && INTL_NthByteOfChar(win_csid, (char *) pEnd->lo_text.text, iEndOffset+1) > 1) {
		XP_TRACE(("iEndOffset = %d which is wrong for multibyte", (int)iEndOffset));
		iEndOffset = INTL_NextCharIdx(win_csid, (unsigned char *) pStart->lo_text.text,iEndOffset);
	}
}

void CEditBuffer::MakeSelectionEndPoints( CEditSelection& selection,
    CEditLeafElement*& pBegin, CEditLeafElement*& pEnd ){
#ifdef DEBUG
    // If the selection is busted, we're in big trouble.
    CPersistentEditSelection persel = EphemeralToPersistent(selection);
    XP_ASSERT ( persel.m_start.m_index <=  persel.m_end.m_index);
#endif
    pEnd = selection.m_end.m_pElement->Divide( selection.m_end.m_iPos )->Leaf();
    pBegin = selection.m_start.m_pElement->Divide( selection.m_start.m_iPos )->Leaf();
    
    //cmanske: Don't set this flag unless we really did divide into new elements
    if( pEnd != selection.m_end.m_pElement ||
        pBegin != selection.m_start.m_pElement )
    {
        m_bLayoutBackpointersDirty = TRUE;
    }
}

void CEditBuffer::MakeSelectionEndPoints( CEditLeafElement*& pBegin,
        CEditLeafElement*& pEnd ){
    CEditSelection selection;
    GetSelection(selection);
    MakeSelectionEndPoints(selection, pBegin, pEnd);
}

int CEditBuffer::Compare( CEditElement *p1, int i1Offset,
                           CEditElement *p2, int i2Offset ) {

    CEditPositionComparable *pPos1, *pPos2;
    int iRetVal;

    pPos1 = new CEditPositionComparable( p1, i1Offset);
    pPos2 = new CEditPositionComparable( p2, i2Offset);

    // find which one is really the beginning.
    iRetVal = pPos1->Compare( pPos2 );
    delete pPos1;
    delete pPos2;
    return iRetVal;
}

// Prevent recursive calls for autosaving
XP_Bool CEditBuffer::m_bAutoSaving = FALSE;

void CEditBuffer::AutoSaveCallback() {
    if ( m_bAutoSaving || IsBlocked() ) {
        XP_TRACE(("Auto Save ignored -- busy."));
        return;
    }
    if ( ! GetCommandLog()->IsDirty() ) {
        XP_TRACE(("Skipping AutoSave because we're not dirty."));
        return;
    }
    // Skip trying to save if we are already in the process of saving a file
    if( m_pContext->edit_saving_url ) {
        XP_TRACE(("Skipping AutoSave because we're not already saving a file."));
        return;
    }
    // This unnecessary on WinFE as FE_CheckAndAutoSaveDocument()
    // catches it.  But, avoids having to make the other FEs deal with it.
    if (m_pContext->type == MWContextMessageComposition) {
        XP_TRACE(("No auto-save for message composition"));
        return;
    }
    XP_TRACE(("Auto Save...."));

    m_bAutoSaving = TRUE;
    if( EDT_IS_NEW_DOCUMENT(m_pContext) ){
        // New document -- prompt front end to get filename and "save file as"
         if( !FE_CheckAndAutoSaveDocument(m_pContext) ){
            // User canceled the Autosave prompt, turn off autosave
            SetAutoSavePeriod(0);
         }
    } else {
        History_entry * hist_entry = SHIST_GetCurrent(&(m_pContext->hist));

	    if(hist_entry && hist_entry->address)
	    {

          if (NET_URL_Type(hist_entry->address) != FILE_TYPE_URL) {
            // Remote document -- prompt front end to get local filename and "save file as"
             if( !FE_CheckAndAutoSaveDocument(m_pContext) ){
                // User canceled the Autosave prompt, turn off autosave
                SetAutoSavePeriod(0);
             }
          }
          else {      
            // A local file, just save it.
            char *szLocalFile = NULL;
            if ( XP_ConvertUrlToLocalFile( hist_entry->address, &szLocalFile ) )
            {
                char buf[300];

	            PR_snprintf(buf, sizeof(buf)-1, XP_GetString(XP_EDITOR_AUTO_SAVE), szLocalFile);
	            FE_Progress(m_pContext, buf);


              /////// ! Duplicating some code from EDT_SaveFile, but I didn't want to change
              /////// ! the interface for EDT_SaveFile(), but we still need to specify that we
              /////// ! are auto-saving.
              // Create Abstract file system to write to disk.
              char *pDestPathURL = edt_GetPathURL(hist_entry->address);
              XP_ASSERT(pDestPathURL);
              ITapeFileSystem *tapeFS = new CTapeFSFile(pDestPathURL,hist_entry->address);
              XP_ASSERT(tapeFS);
              XP_FREEIF(pDestPathURL);
              // tapeFS freed by CEditSaveObject.

              char **ppEmptyList = (char **)XP_ALLOC(sizeof(char *));
              XP_ASSERT(ppEmptyList);
              ppEmptyList[0] = NULL;
  
              // go to the newly saved document.
              SaveFile( ED_FINISHED_GOTO_NEW, hist_entry->address, tapeFS, 
                              FALSE, // bSaveAs 
                              FALSE, FALSE, // don't need to move images or adjust
                                            // links because document isn't moving.
                              TRUE, // is AutoSave
                              ppEmptyList); // Don't send along any files, even LOCALDATA.
            }
            if (szLocalFile){
                XP_FREE(szLocalFile);
            }
          } // local file
        }
    } // not a new document

    m_bAutoSaving = FALSE;
}

void CEditBuffer::FileFetchComplete(ED_FileError status) {
    m_status = status;
    FE_Progress(m_pContext, NULL);

    // hardts.  Moved from CEditBuffer::WriteToFile()
    if (status == ED_ERROR_NONE) {
        m_autoSaveTimer.Restart();
    }
}

void CEditBuffer::SetAutoSavePeriod(int32 minutes) {
    m_autoSaveTimer.SetPeriod(minutes);
}

int32 CEditBuffer::GetAutoSavePeriod() {
    return m_autoSaveTimer.GetPeriod();
}

void CEditBuffer::SuspendAutoSave(){
    m_autoSaveTimer.Suspend();
}

void CEditBuffer::ResumeAutoSave(){
    m_autoSaveTimer.Resume();
}

void CEditBuffer::ClearMove(XP_Bool bFlushRelayout){
    if ( bFlushRelayout ) {
        m_relayoutTimer.Flush();
    }
    m_iDesiredX = -1;
}


void CEditBuffer::SetHREFSelection( ED_LinkId id ){
    VALIDATE_TREE(this);
    CEditLeafElement *pCurrent, *pEnd, *pBegin, *pNext;

    //
    // Guarantee that the text blocks of the beginning and end of selection
    //  are atomic.
    //
    MakeSelectionEndPoints( pBegin, pEnd );
    pCurrent = pBegin;

    while( pCurrent != pEnd ){
        pNext = pCurrent->NextLeafAll();
        pCurrent->Leaf()->SetHREF( id );
        pCurrent = pNext;
    }

	CEditSelection tmp(pBegin, 0, pEnd, 0);
    RepairAndSet(tmp);
}

EDT_ClipboardResult CEditBuffer::DeleteSelection(CEditSelection& selection, XP_Bool bCopyAppendAttributes){
    SetSelection(selection);
    return DeleteSelection(bCopyAppendAttributes);
}

EDT_ClipboardResult CEditBuffer::DeleteSelection(XP_Bool bCopyAppendAttributes){
    VALIDATE_TREE(this);
    EDT_ClipboardResult result = CanCut(TRUE);
    if ( result == EDT_COP_OK )
    {
        if ( IsSelected() )
        {
            CEditLeafElement *pBegin;
            CEditLeafElement *pEnd;

            CEditSelection selection;
            GetSelection(selection);
            if ( selection.ContainsLastDocumentContainerEnd() )
            {
                selection.ExcludeLastDocumentContainerEnd();
                SetSelection(selection);
            }
            if ( selection.IsInsertPoint() )
            {
                return result;
            }
            MakeSelectionEndPoints( selection, pBegin, pEnd );
            DeleteBetweenPoints( pBegin, pEnd, bCopyAppendAttributes );
        }
    }
    return result;
}

CPersistentEditSelection CEditBuffer::GetEffectiveDeleteSelection(){
    // If the selection includes the end of the document, then the effective selection is
    // less than that.
    CEditSelection selection;
    GetSelection(selection);
    selection.ExcludeLastDocumentContainerEnd();
    selection.ExpandToIncludeFragileSpaces();
    return EphemeralToPersistent(selection);
}

void CEditBuffer::DeleteBetweenPoints( CEditLeafElement* pBegin, CEditLeafElement* pEnd,
     XP_Bool bCopyAppendAttributes){
    CEditLeafElement *pCurrent;
    CEditLeafElement *pNext;

    pCurrent = pBegin;

    XP_ASSERT( pEnd != 0 );

    CEditLeafElement* pPrev = pCurrent->PreviousLeafInContainer();
    CEditElement* pCommonAncestor = pBegin->GetCommonAncestor(pEnd);
    CEditElement* pContainer = pCurrent->FindContainer();
    CEditElement* pCurrentContainer;
    while( pCurrent != pEnd ){
        pNext = pCurrent->NextLeafAll();

        // if we've entered a new container, merge it.
        pCurrentContainer = pCurrent->FindContainer();

        // DeleteElement removes empty paragraphs so we don't have to do the merges.
        //
        //if( pContainer != pCurrentContainer ){
        //    pContainer->Merge( pCurrentContainer );
        //}

#ifdef DEBUG
         m_pRoot->ValidateTree();
#endif

        if( pCurrent->DeleteElement( pContainer )) {
            pContainer = pNext->FindContainer();
        }
#ifdef DEBUG
        m_pRoot->ValidateTree();
#endif
        pCurrent = pNext;
    }

    //
    // if the selection spans partial paragraphs, merge the remaining paragraphs
    //
    if( pPrev && pPrev->FindContainer() != pEnd->FindContainer() ){
        pPrev->FindContainer()->Merge( pEnd->FindContainer(), bCopyAppendAttributes );
    }

    m_pCurrent = pEnd->PreviousLeafInContainer();
    if( m_pCurrent != 0 ){
        m_iCurrentOffset = m_pCurrent->GetLen();
    }
    else {
        m_pCurrent = pEnd;
        m_iCurrentOffset = 0;
    }

    //FixupSpace();

    // probably needs to be the common ancestor.
    ClearSelection(FALSE);
    CEditInsertPoint tmp(pEnd,0);
    CPersistentEditInsertPoint end2 = EphemeralToPersistent(tmp);
    Reduce( pCommonAncestor );
    SetCaret();
    if ( pPrev && pPrev->FindContainer() != pContainer ) {
        Reduce( pPrev->FindContainer() );
    }
    CEditInsertPoint end3 = PersistentToEphemeral(end2);
    Relayout( pContainer, 0,  end3.m_pElement);
}

// Deletes cells -- if all cells in a col or row are selected,
//  this removes the row or col. Otherwise, just cell contents
//  are deleted to minimize disturbing table structure
// Caller must manage UNDO (BatchChanges) process
void CEditBuffer::DeleteSelectedCells()
{
    CEditTableElement *pTable = NULL;
    XP_Bool bRelayout;
    XP_Bool bNeedRelayout = FALSE;

    do {
        bRelayout = FALSE;

        // We are done if no cells are selected
        intn iCount = m_SelectedEdCells.Size();
        CEditTableCellElement *pFirstSelectedCell = m_SelectedEdCells[0];

        if( iCount > 0 && pFirstSelectedCell )
        {
            if( !pTable )
                pTable = pFirstSelectedCell->GetParentTable();

            // Be sure insert point is in the table
            SetTableInsertPoint(pFirstSelectedCell, TRUE);

            CEditTableCellElement *pCell;
            intn i, number;

            for( i = 0; i < iCount; i++ )
            {    
                pCell = m_SelectedEdCells[i];
                // Skip over cells where we already deleted their contents
                if( pCell->IsDeleted() )
                    continue;
                
                // Are we deleting entire column(s)?
                if( pCell->AllCellsInColumnAreSelected() )
                {
                    // Move to the first column being deleted
                    SetTableInsertPoint(pCell, TRUE);

                    // For efficiency, find contiguous selected columns
                    //  so we can delete them all at once
                    number = 1;
                    // We don't want to use GetNextCellInRow() because of 
                    //  COLSPAN effect. This gets the first cell in the
                    //  next geometric row using the table's layout data
                    pCell = pTable->GetFirstCellInNextColumn(pCell->GetX());
                    while( pCell )
                    {
                        if( pCell->AllCellsInColumnAreSelected() )
                            number++;
                        else
                            break;

                        pCell = pTable->GetFirstCellInNextColumn(pCell->GetX());
                    }

                    // Delete the column(s) -- this will relayout the table
            		AdoptAndDo(new CDeleteTableColumnCommand(this, number));
                    bRelayout = TRUE;
                    // We don't need to do relayout if
                    // this is the last thing we do
                    bNeedRelayout = FALSE;
                    break;
                }
                // Are we deleting entire rows(s)?
                else if( pCell->AllCellsInRowAreSelected() )
                {
                    // Move to the first row being deleted
                    SetTableInsertPoint(pCell, TRUE);

                    // For efficiency, find contiguous selected rows
                    //  so we can delete them all at once
                    number = 1;
                    // We don't want to use GetNextCellInColumn() because of 
                    //  ROWSPAN effect. This gets the first cell in the
                    //  next geometric row using the table's layout data
                    pCell = pTable->GetFirstCellInNextRow(pCell->GetX());
                    while( pCell )
                    {
                        if( pCell->AllCellsInRowAreSelected() )
                            number++;
                        else
                            break;

                        pCell = pTable->GetFirstCellInNextRow(pCell->GetX());
                    }
            		AdoptAndDo(new CDeleteTableRowCommand(this, number));
                    bRelayout = TRUE;
                    bNeedRelayout = FALSE;
                    break;
                }
                else 
                {
                    // Clear contents only
                    // TRUE means mark cell as deleted
                    pCell->DeleteContents(TRUE);
                    // We need to force table relayout if
                    //  this is the last thing we do
                    bNeedRelayout = TRUE;

                    // If we cleared contents of cell containing caret
                    // we must reset the insert point
                    if( pCell == pFirstSelectedCell )
                        SetTableInsertPoint(pFirstSelectedCell, TRUE);
                }
            }
        }
    }
    // Repeat entire process if we actually deleted anything
    //   since we have a new m_SelectedEdCell array after relayout
    while( bRelayout );


    if( pTable )
    {
        // We need to relayout only if deleting cell contents
        //  was the last thing we did
        if( bNeedRelayout )
            Relayout(pTable, 0);

        // Clear the m_bDeleted flags for all cells in the table
        CEditTableCellElement *pCell = pTable->GetFirstCell();
        while( pCell )
        {
            pCell->SetDeleted(FALSE);
            pCell = pTable->GetNextCellInTable();
        }    
    }
}

// Currently (4/22/98) not used. But we may want a FE call 
//   in the future to clear cells contents
EDT_ClipboardResult CEditBuffer::ClearSelectedCells()
{
    CEditTableCellElement *pFirstCell = GetFirstSelectedCell();
    if( pFirstCell )
    {
        
        BeginBatchChanges(kGroupOfChangesCommandID);

        CEditTableCellElement *pCell = pFirstCell;
        while(pCell)
        {
            pCell->DeleteContents();
            pCell = GetNextSelectedCell();
        }
        // Reset the insert point in the first cell cleared
        SetTableInsertPoint(pFirstCell);

        Relayout(pFirstCell->GetParentTable(), 0);

        EndBatchChanges();
    }
    return EDT_COP_OK;
}

EDT_ClipboardResult CEditBuffer::PasteQuoteBegin( XP_Bool bHTML ){
    if ( m_bPasteQuoteMode != FALSE ) {
        XP_ASSERT(FALSE);
    }

    EDT_ClipboardResult result = CanPaste(TRUE);
    if ( result != EDT_COP_OK ) return result;

    m_bPasteQuoteMode = TRUE;
    BeginBatchChanges(kPasteTextCommandID);
    DeleteSelection();
    m_bPasteHTML = bHTML;
    if ( ! m_pPasteHTMLModeText ) {
        m_pPasteHTMLModeText = new CStreamOutMemory();
        m_pPasteTranscoder = NULL;
    }
    m_bAbortPasteQuote = FALSE;
    return result;
}

EDT_ClipboardResult CEditBuffer::PasteQuote(char* pText){
    // XP_ASSERT(FALSE); // Should convert to new code.
    return PasteQuoteINTL(pText, GetRAMCharSetID());
}

EDT_ClipboardResult CEditBuffer::PasteQuoteINTL(char* pText, int16 csid){
    EDT_ClipboardResult result = EDT_COP_OK;
    XP_Bool bBackwardsCompatible = FALSE;
    if ( m_bPasteQuoteMode != TRUE ) {
        //  XP_ASSERT(FALSE); // Mail guys forgot to call PasteQuoteBegin()
        bBackwardsCompatible = TRUE;
        result = PasteQuoteBegin(FALSE);
        if ( result != EDT_COP_OK ) {
            return result;
        }
    }

    // Stop if we are in "abort paste quote" mode
    if (m_bAbortPasteQuote)
        return result;

    // If the text is too long, abort paste operation.
    // An error message will be displayed in PasteQuoteEnd().
    if (XP_STRLEN(pText) >= MAX_PASTE_SIZE) {
        m_bAbortPasteQuote = TRUE;
    }
    else if ( m_bPasteHTML ) {
        // Check if we need to set up a new transcoder.
        if ( ! m_pPasteTranscoder || csid != m_pPasteTranscoder->GetOldCSID() ) {
            if ( m_pPasteTranscoder ) {
                m_pPasteTranscoder->ForgetStream();
                delete m_pPasteTranscoder;
            }
            m_pPasteTranscoder = new CConvertCSIDStreamOut( csid, GetRAMCharSetID(), m_pPasteHTMLModeText);
        }
        m_pPasteTranscoder->Write(pText, XP_STRLEN(pText));
    }
    else {
        result = PasteText(pText, TRUE, FALSE, csid, TRUE,TRUE);
    }

    if ( bBackwardsCompatible ) {
        result = PasteQuoteEnd();
    }

    return result;
}

EDT_ClipboardResult CEditBuffer::PasteQuoteEnd(){
    EDT_ClipboardResult result = EDT_COP_OK;
	XP_Bool bThisIsDeleted = FALSE;
	CEditCommandLog* pCommandLog = GetCommandLog();
    if ( m_bPasteQuoteMode != TRUE ) {
        XP_ASSERT(FALSE);
        return result;
    }
    if ( m_bPasteHTML && m_pPasteHTMLModeText ) {
        // Paste as HTML. The other 1/2 of this logic
        // is in CheckAndPrintComment2
        result = CanPaste(TRUE);
		// Save to delete after we've deleted "this".
		CStreamOutMemory* pPasteText = m_pPasteHTMLModeText;
        CConvertCSIDStreamOut* pPasteTextTranscoder = m_pPasteTranscoder;

        // Display error message if aborting paste operation.
        if (m_bAbortPasteQuote) {
            char* msg = XP_GetString(XP_EDT_MSG_CANNOT_PASTE);
            FE_Alert(m_pContext, msg);
        }

        if ( result == EDT_COP_OK && !m_bAbortPasteQuote) {
            if( IsSelected() ){
                DeleteSelection();
            }
            m_bPasteHTMLWhenSavingDocument = TRUE;
            XP_HUGE_CHAR_PTR pData;
            WriteToBuffer(&pData, TRUE);
            m_bPasteHTMLWhenSavingDocument = FALSE;
			m_pPasteHTMLModeText = 0;  // so our destructor doesn't try to clean this up.
            m_pPasteTranscoder = 0;
            ReadFromBuffer(pData); // This deletes "this".
			bThisIsDeleted = TRUE;
            XP_HUGE_FREE(pData);
        }
        XP_HUGE_FREE(pPasteText->GetText());
        delete pPasteTextTranscoder; // Automaticly deletes pPasteText
        if ( ! bThisIsDeleted){
			m_pPasteHTMLModeText = 0;
            m_pPasteTranscoder = 0;
		}
    }
	pCommandLog->EndBatchChanges();
    if ( ! bThisIsDeleted ) {
		m_bPasteQuoteMode = FALSE;
	}
    return result;
}

void CEditBuffer::PasteHTMLHook(CPrintState* pPrintState){
    if (m_bPasteHTMLWhenSavingDocument ) {
        // Insert the paste text in-line, as we're writing out the document.
        XP_HUGE_CHAR_PTR pSource = m_pPasteHTMLModeText->GetText();
        int32 len = m_pPasteHTMLModeText->GetLen();
        const int32 kChunkSize = 4096; // Must be smaller than 64K for Win16.
        char* buf = (char*) XP_ALLOC(kChunkSize);
        while ( len > 0 ) {
            int32 chunk = len;
            if ( chunk > kChunkSize ){
                chunk = kChunkSize;
            }
            len -= chunk;
            // Copy from the huge pointer to the regular pointer
            char* p = buf;
            for(int32 i = 0; i < chunk; i++ ){
                *p++ = *pSource++;
            }
            pPrintState->m_pOut->Write(buf, chunk);
        }
        XP_FREE(buf);
    }
}

EDT_ClipboardResult CEditBuffer::PasteText( char *pText, XP_Bool bMailQuote, XP_Bool bIsContinueTyping, int16 csid, XP_Bool bRelayout , XP_Bool bReduce){
    int16 newcsid = GetRAMCharSetID();
    if ( csid != newcsid) {
        // Need to transcode pText to the current
        CStreamOutMemory memory;
        CConvertCSIDStreamOut transcoder( csid, newcsid, &memory);
        transcoder.Write(pText, XP_STRLEN(pText));
        XP_HUGE_CHAR_PTR pTranscoded = memory.GetText();
        EDT_ClipboardResult result = PasteText((char*) pTranscoded, bMailQuote, bIsContinueTyping, bRelayout, bReduce);
        XP_HUGE_FREE(pTranscoded);
        transcoder.ForgetStream();
        return result;
    }
    else {
        return PasteText(pText, bMailQuote, bIsContinueTyping, bRelayout, bReduce);
    }
}

//
// LTNOTE: this routine needs to be broken into PasteText and PasteFormattedText
//  It didn't happen because we were too close to ship.
//
EDT_ClipboardResult CEditBuffer::PasteText( char *pText, XP_Bool bMailQuote, XP_Bool bIsContinueTyping, XP_Bool bRelayout, XP_Bool bReduce)
{
    // If the text is too long, display alert message and return.
    if (XP_STRLEN(pText) >= MAX_PASTE_SIZE) {
        char* msg = XP_GetString(XP_EDT_MSG_CANNOT_PASTE);
        FE_Alert(m_pContext, msg);
        return EDT_COP_OK;
    }

    VALIDATE_TREE(this);
    SUPPRESS_PHANTOMINSERTPOINTCHECK(this);

    if ( !bIsContinueTyping ) {
        EDT_ClipboardResult result = CanPaste(TRUE);
        if ( result != EDT_COP_OK ) return result;
    }

    //
    // No one likes the other way of quoting, always use mailquote
    //
    bMailQuote = TRUE;

    m_bNoRelayout = TRUE;
    int iCharsOnLine = 0;

    if( IsSelected() ){
        DeleteSelection();
    }
    FixupInsertPoint();

    // If the pasted text starts with returns, the start element can end up
    // being moved down the document as paragraphs are inserted above it.
    // So we hold onto the start as a persistent insert point.

    CPersistentEditInsertPoint persistentStart;
    GetInsertPoint(persistentStart);

    // If we're in preformatted text, every return is a break.
    // If we're in normal text, single returns are spaces,
    // double returns are paragraph marks.

    //XP_Bool bInFormattedText = m_pCurrent->FindContainer()->GetType() == P_PREFORMAT;
    XP_Bool bInFormattedText = m_pCurrent->InFormattedText();

    XP_Bool bLastCharWasReturn = FALSE;

    while( *pText ){
        XP_Bool bReturn = FALSE;
        if( *pText == 0x0d ){
            if( *(pText+1) == 0xa ){
                pText++;
            }
            bReturn = TRUE;
        }
        else if ( *pText == 0xa ){
            bReturn = TRUE;
        }

        if( bReturn ){
            if ( bInFormattedText ) {
                InsertBreak( ED_BREAK_NORMAL, bIsContinueTyping );
                iCharsOnLine = 0;
            } 
            else if ( bMailQuote ){
                ReturnKey( bIsContinueTyping );
#ifdef EDT_DDT
                MorphContainer( P_NSDT);
#else
                MorphContainer( P_DESC_TITLE );
#endif
                iCharsOnLine = 0;
            }
            else {
                if ( bLastCharWasReturn ){
                    ReturnKey( bIsContinueTyping );
                    iCharsOnLine = 0;
                    bLastCharWasReturn = FALSE;
                }
                else {
                    // Remember this return for later. It will
                    // become either a space or a return.
                    bLastCharWasReturn = TRUE;
                }
            }
        }
        else {
            if ( bLastCharWasReturn ) {
                InsertChar( ' ', bIsContinueTyping );
                bLastCharWasReturn = FALSE;
            }

            if ( *pText == '\t' ){
                if ( bInFormattedText ) {
                    do {
                        InsertChar( ' ', bIsContinueTyping );
                        iCharsOnLine++;
                    } while( iCharsOnLine % DEF_TAB_WIDTH != 0 );
                }
                else {
                    InsertChar( ' ', bIsContinueTyping );
                    iCharsOnLine++;
                }
            }
            else {
                // Insert all the characters up to the next return or tab
                int32 value = 0;
                char old = 0;
                while((old = pText[value]) != '\0'){
                    if ( old == '\t' || old == '\015' || old == '\012' ){
                        break;
                    }
                    value++;
                }
                if ( value > 0 ) {
                    pText[value] = '\0';
                    InsertChars( pText, bIsContinueTyping, bReduce); //adding this flag to stop the insertchars from reducing the tree. 
                    pText[value] = old;
                    pText += value - 1; /* -1 Because pText is incremented by one at the end of the loop. */
                    iCharsOnLine += value;
                }
            }
            bLastCharWasReturn = FALSE;
        }
        pText++;
    }

    if ( bLastCharWasReturn ) {
        InsertChar( ' ', bIsContinueTyping );
    }


    if (bReduce)//dont reduce if bReduce is false
        Reduce(m_pRoot);

    CEditInsertPoint start = PersistentToEphemeral(persistentStart);

    m_bNoRelayout = FALSE;

    // We now suppress layout when pasting into tables:
    //  entire table will be layed out after all cell pasting is finished
    if( bRelayout )
    {
        // At one time pEnd was managed so that it pointed beyond the pasted text.
        // Aparently that code was lost in the mists of time. Now we just hope that
        // the next element is far enough. (Lloyd and I think it has to be.)
        CEditElement* pEnd = m_pCurrent->NextLeaf();
        // if the end of the paragraph ends in a break, we need to relayout further.
        if( pEnd && pEnd->IsBreak()
            || ( pEnd->GetNextSibling() && pEnd->GetNextSibling()->IsBreak() ) ){
            pEnd = m_pRoot->GetLastMostChild(); // This is almost always too far, but it is safe.
        }

        Reflow( start.m_pElement, start.m_iPos, pEnd );
        //
        if( bMailQuote ){
            CEditInsertPoint start = PersistentToEphemeral(persistentStart);
            //SetInsertPoint( start );
        }
    }

    return EDT_COP_OK;
}

EDT_ClipboardResult CEditBuffer::PasteHTML( char *pBuffer, XP_Bool bUndoRedo ){

    CStreamInMemory stream(pBuffer);
    return PasteHTML(stream, bUndoRedo);
}

EDT_ClipboardResult CEditBuffer::PasteHTML( IStreamIn& stream, XP_Bool /* bUndoRedo */ ){
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_pContext);
    VALIDATE_TREE(this);
    EDT_ClipboardResult result = CanPaste(TRUE);
    if ( result != EDT_COP_OK ) return result;

    // First thing in the buffer is some version info
    
    int32 signature = stream.ReadInt();
    if ( signature != GetClipboardSignature() ) {
        FE_Alert(m_pContext,XP_GetString(XP_EDT_BAD_CLIPBOARD_VERSION));
        return EDT_COP_CLIPBOARD_BAD;
    }
    int32 version = stream.ReadInt();
    if ( version != GetClipboardVersion() ) {
        FE_Alert(m_pContext,XP_GetString(XP_EDT_BAD_CLIPBOARD_VERSION));
        return EDT_COP_CLIPBOARD_BAD;
    }
    int32 clipcsid = stream.ReadInt();
    if ( clipcsid != INTL_GetCSIWinCSID(c) ) {
        /* In the future we could transcode. */
        FE_Alert(m_pContext,XP_GetString(XP_EDT_BAD_CLIPBOARD_ENCODING));
        return EDT_COP_CLIPBOARD_BAD;
    }
    // This is non-zero when stream contains just a table
    // It tells us whether it is an entire table or row(s), 
    //      column(s), or "loose cells" copying
    EEditCopyType iCopyType = (EEditCopyType)stream.ReadInt();

    // Pasting cells into existing table is entirely different
    //  if pasting rows, columns, or cells
    // Note: type = eEditCopyTable used for entire table,
    //  allowing an embeded table in existing table
    if( iCopyType > eCopyTable && IsInsertPointInTableCell() )
    {
        return PasteCellsIntoTable(stream, iCopyType);
    }
    
    // Don't relayout until all is pasted
    m_bNoRelayout = TRUE;

    // "Normal" HTML pasting at current caret location
    if( IsSelected() )
    {
        DeleteSelection();
    }
#ifdef DEBUG
    m_pRoot->ValidateTree();
#endif
    ClearPhantomInsertPoint();
    FixupInsertPoint();

    CEditElement* pStart = m_pCurrent;
    int iStartOffset = m_iCurrentOffset;
    CEditLeafElement* pRight = m_pCurrent->Divide(m_iCurrentOffset)->Leaf();
    XP_ASSERT(pRight->FindContainer());
    XP_Bool bAtStartOfParagraph = pRight->PreviousLeafInContainer() == NULL;
    CEditLeafElement* pLeft = pRight->PreviousLeaf(); // Will be NULL at start of document

    // The first thing in the buffer is a flag that tells us if we need to merge the end.
    int32 bMergeEnd = stream.ReadInt();
    // The buffer has zero or more elements. (The case where there's more than one is
    // the one where there are multiple paragraphs.)
    CEditElement* pFirstInserted = NULL;
    CEditElement* pLastInserted = NULL;

    {
        CEditElement* pElement;
        while ( NULL != (pElement = CEditElement::StreamCtor(&stream, this)) )
        {
            // We should always begin with a container
            if ( pElement->IsLeaf() )
            {
                XP_ASSERT(FALSE);
                delete pElement;
                continue;
            }
#ifdef DEBUG
            pElement->ValidateTree();
#endif
            if ( ! pFirstInserted ) 
            {
                pFirstInserted = pElement;
                if ( ! bAtStartOfParagraph )
                {
    XP_ASSERT(pRight->FindContainer());
                    InternalReturnKey(FALSE);
    XP_ASSERT(pRight->FindContainer());
               }
            }
            pLastInserted = pElement;
            pElement->InsertBefore(pRight->FindContainer());

            // Table seems to be a special case
            // If it is inserted NOT at beginning of existing paragraph, contents of
            //  first cell are moved into that paragraph (see below)
            //cmanske: KLUDGE - THIS WORKS, BUT IS IT REALLY OK???
            // (This BUG  exists in 4.0x)
            if( pElement->IsTable() )
            {
                bAtStartOfParagraph = TRUE;
            }

#ifdef DEBUG
            m_pRoot->ValidateTree();
#endif
        }
    }

#ifdef DEBUG
    m_pRoot->ValidateTree();
#endif

    if ( pFirstInserted )
    {
        CEditElement* pLast = pLastInserted->GetLastMostChild()->NextLeafAll();
        // Get the children early, because if we paste a single container,
        // it will be deleted when we merge the left edge
        CEditLeafElement* pFirstMostNewChild = pFirstInserted->GetFirstMostChild()->Leaf();
        CEditLeafElement* pLastMostNewChild = pLastInserted->GetLastMostChild()->Leaf();
        m_pCurrent = pLastMostNewChild;
        m_iCurrentOffset = pLastMostNewChild->GetLen();
        ClearPhantomInsertPoint();
        if ( ! bAtStartOfParagraph )
        {
            if ( pLeft )
            {
                CEditContainerElement* pLeftContainer = pLeft->FindContainer();
                CEditContainerElement* pFirstNewContainer =
                    pFirstMostNewChild->FindContainer();
                if ( pLeftContainer != pFirstNewContainer )
                {
                    pLeftContainer->Merge(pFirstNewContainer);
                    // Now deleted in Merge delete pFirstNewContainer;
                    FixupInsertPoint();
                }
            }
        }
        else {
            // The insert went into the container before us. Adjust the start.
            pStart = pFirstInserted->GetFirstMostChild();
            iStartOffset = 0;
        }

        if ( bMergeEnd )
        {
            CEditContainerElement* pRightContainer = pRight->FindContainer();
            CEditContainerElement* pLastNewContainer = pLastMostNewChild->FindContainer();
            if ( pRightContainer != pLastNewContainer )
            {
                pLastNewContainer->Merge(pRightContainer);
                // Now deleted in Merge delete pRightContainer;
                FixupInsertPoint();
            }
        }
        else {
            // Move to beginning of next container
            m_pCurrent = pLastMostNewChild->NextContainer()->GetFirstMostChild()->Leaf();
            m_iCurrentOffset = 0;
        }

        // Have to FinishedLoad after merging because FinishedLoad is eager to
        // remove spaces from text element at the end of containers. If we let
        // FinishedLoad run earlier, we couldn't paste text that ended in white space.

        m_pRoot->FinishedLoad(this);

#ifdef DEBUG
        m_pRoot->ValidateTree();
#endif

        m_bNoRelayout = FALSE;
        //FixupSpace( );
        // We need to reduce before we relayout because otherwise
        // null insert points mess up the setting of end-of-paragraph
        // marks.
		CEditInsertPoint tmp(pLast,0);
		CPersistentEditInsertPoint end2 = EphemeralToPersistent(tmp);
        Reduce( m_pRoot );
        CEditInsertPoint end3 = PersistentToEphemeral(end2);
        Relayout( pStart, iStartOffset, end3.m_pElement );
    }
    else {
        m_bNoRelayout = FALSE;
    }

   return result;
}

EDT_ClipboardResult CEditBuffer::PasteCellsIntoTable( IStreamIn& stream, EEditCopyType iCopyType )
{
    m_bNoRelayout = FALSE;
    CEditTableCellElement* pTableCell = NULL;
    // Default for pasting is BEFORE
    // TODO: Should we ask user to insert AFTER if currently in last cell in row or column?
    XP_Bool bAfterCurrentCell = FALSE;

    // Shouldn't be here if this is "normal" or entire table
    XP_ASSERT(iCopyType > eCopyTable);
    
    // Move caret into target cell
    if( m_pDragTableData )
    {
        // We are dragging source cells
        XP_ASSERT(m_pDragTableData->pDragOverCell);

        // Dragging has more flexibility for where to paste
        bAfterCurrentCell = (m_pDragTableData->iDropType == ED_DROP_INSERT_AFTER || 
                             m_pDragTableData->iDropType == ED_DROP_INSERT_BELOW );

        CEditElement *pElement = edt_GetTableElementFromLO_Element(m_pDragTableData->pDragOverCell, LO_CELL);
        if( pElement )
        {
            CEditElement *pChild = pElement->GetFirstMostChild();
            if( pChild )
            {
                CEditInsertPoint ip(pChild, 0);
                SetInsertPoint(ip);
            }
        }
    } else {
        // Pasting from clipboard
        // Get current insert point
        CEditInsertPoint ip;
        GetTableInsertPoint(ip);
        pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    }

    //Get the table from the stream
    int32 bMergeEnd = stream.ReadInt();
    CEditTableElement* pSourceTable =(CEditTableElement*)CEditElement::StreamCtor(&stream, this);

    XP_ASSERT(pSourceTable->IsTable());

    if( pTableCell && pSourceTable )
    {
        CEditTableElement* pTable = pTableCell->GetTable();
        if ( pTable )
        {
            // Note: This value counts COLSPAN,
            //       so this is number of "virtual" columns
            //       Be sure pasting routines can handle missing cells
            intn iSourceCols = pSourceTable->CountColumns();
            int32 X = pTableCell->GetX();
            int32 Y = pTableCell->GetY();
            int32 iNewX = X + (bAfterCurrentCell ? pTableCell->GetWidth() : 0);
            // Try to place cursor in inserted cell
            int32 iCaretX = iNewX + (bAfterCurrentCell ? pTable->GetCellSpacing() : 0);

            CEditTableRowElement* pCurrentRow = (CEditTableRowElement*)pTableCell->GetParent();
            XP_ASSERT(pCurrentRow->IsTableRow());
            
            intn iCurrentRows = pTable->GetRows(); // CountRows();
            // Number of rows we may insert
            intn iTotalRows = pSourceTable->CountRows();
            // Current row index
            intn iRow = 0;

            CEditTableRowElement* pFirstRow = pTable->GetFirstRow();
            if( pFirstRow != pCurrentRow )
            {
                //Insert empty cells for rows above target row
                CEditTableRowElement* pTempRow = pFirstRow;
                while( pTempRow != pCurrentRow )
                {
                    pTempRow->InsertCells(X, iNewX, iSourceCols);
                    pTempRow = pTempRow->GetNextRow();
                    // Increment row where we will start to insert source cells
                    //   and also the total rows we may insert
                    iRow++;
                    iTotalRows++;
                }
            }
            
            CEditTableRowElement* pSourceRow = NULL;
            CEditTableRowElement *pNextRow = NULL;
            // The number of columns we will have to insert if we
            //   need to add rows at bottom of table
            //   so overflow from inserted columns line up
            intn iColumnsBefore = pTable->GetColumnsSpanned(pTable->GetColumnX(0), X);
            for( ; iRow < iCurrentRows; iRow++ )
            {
                if( !pCurrentRow )
                    break;

                pSourceRow = pSourceTable->GetFirstRow();
                // pSourceRow may be 0 if there are less rows in source
                //   than in target, so this will just insert blank cell in that case
                pCurrentRow->InsertCells(X, iNewX, iSourceCols, pSourceRow);
                // Delete the row just used
                if( pSourceRow )
                    delete pSourceRow;

                // Next row to insert into
                if( (pNextRow = pCurrentRow->GetNextRow()) != NULL )
                    pCurrentRow = pNextRow;
            }
            // There shouldn't be any rows left now
            //   (this is a check for CountRows();
            XP_ASSERT(pNextRow == NULL);
            
            // Let's be sure!
            pNextRow = NULL;
            
            // If needed, continue to insert more source rows,
            //  but we need to add new rows to current table first            
            // TODO: ASK USER IF THEY WANT TO APPEND EXTRA ROWS OR JUST SKIP THEM
            for( ; iRow < iTotalRows; iRow++ )
            {
                pSourceRow = pSourceTable->GetFirstRow();
                if( !pCurrentRow || !pSourceRow )
                {
                    // We should always have source row,
                    //  else iSourceRows is not accurate
                    //  and we should always have a current row
                    XP_ASSERT(TRUE);
                    break;
                }
                if( iColumnsBefore )
                {
                    // We need cells before the insert column,
                    //  so make a new row with blank cells
                    pNextRow = new CEditTableRowElement(iColumnsBefore);
                    if( pNextRow )
                    {
                        pNextRow->InsertAfter(pCurrentRow);
                        // Append the source cells to the the new row
                        pNextRow->AppendRow(pSourceRow);
                    }
                } else {
                    // No extra columns needed,
                    //   just append source row
                    pSourceRow->Unlink();
                    pSourceRow->InsertAfter(pCurrentRow);
                    pNextRow = pSourceRow;
                }

                XP_ASSERT(pNextRow);
                if( pNextRow )
                {
                    // Fill in rest of row with empty cells
                    pNextRow->PadRowWithEmptyCells(pTable->GetColumns() + iSourceCols);
                    // Be sure any empty cells have required empty text elements
                    pNextRow->FinishedLoad(this);
                }
                pCurrentRow = pNextRow;
            }

            // Delete whats left of the source table
            delete pSourceTable;

            Relayout(pTable, 0);
            MoveToExistingCell(pTable, iCaretX, Y);
        }
#ifdef DEBUG
        m_pRoot->ValidateTree();
#endif
    }

    //TODO: FINISH PASTECELLSINTOTABLE
    return EDT_COP_OK;
}


EDT_ClipboardResult CEditBuffer::PasteHREF( char **ppHref, char **ppTitle, int iCount){
    VALIDATE_TREE(this);
    SUPPRESS_PHANTOMINSERTPOINTCHECK(this);
    EDT_ClipboardResult result = CanPaste(TRUE);
    if ( result != EDT_COP_OK ) return result;
    m_bNoRelayout = TRUE;
    XP_Bool bFirst = TRUE;

    if( IsSelected() ){
        DeleteSelection();
    }
    FixupInsertPoint();

    FormatCharacter( TF_NONE );
    InsertChar(' ', FALSE);

    CEditElement *pStart = m_pCurrent;
    int iStartOffset = m_iCurrentOffset;

    m_pCurrent->Divide( m_iCurrentOffset );

    int i = 0;
    while( i < iCount ){
        char *pTitle = ppTitle[i];

        if( pTitle == 0 ){
            pTitle = ppHref[i];
        }
        else {
            // LTNOTE:
            // probably shouldn't be doing this to a buffer we were passed
            //  but what the hell.
            NormalizeText( pTitle );

            // kill any trailing spaces..
            int iLen = XP_STRLEN( pTitle )-1;
            while( iLen >= 0 && pTitle[iLen] == ' ' ){
                pTitle[iLen] = 0;
                iLen--;
            }
            if( *pTitle == 0 ){
                pTitle = ppHref[i];
            }
        }

        CEditTextElement *pElement = new CEditTextElement( 0, pTitle );
        pElement->SetHREF( linkManager.Add( ppHref[i], 0 ));

        if( bFirst && m_iCurrentOffset == 0 ){
            pElement->InsertBefore( m_pCurrent );
            pStart = pElement;
        }
        else {
            pElement->InsertAfter( m_pCurrent );
        }
        if( bFirst ){
            //FixupSpace();
            bFirst = FALSE;
        }
        m_pCurrent = pElement;
        m_iCurrentOffset = pElement->Text()->GetLen();
        i++;
    }

    m_bNoRelayout = FALSE;
    //FixupSpace( );
    // Fixes bug 21920 - Crash when dropping link into blank document.
    // We might have an empty text container just after m_pCurrent.
    Reduce(m_pRoot);
    FormatCharacter( TF_NONE );
    InsertChar(' ', FALSE);
    Relayout( pStart, iStartOffset, m_pCurrent );
    Reduce( m_pRoot );
    return result;
}

EDT_ClipboardResult CEditBuffer::PasteTextAsNewTable(char *pText, intn iRows, intn iCols)
{
    if( iRows == 0 || iCols == 0 )
       CountRowsAndColsInPasteText(pText, &iRows, &iCols); 

    EDT_TableData *pData = EDT_NewTableData();
    if( pData )
    {
        pData->iRows = iRows;
        pData->iColumns = iCols;
        InsertTable(pData);
        EDT_FreeTableData(pData);
        intn iRow = 0;
        // We assume InsertTable will put caret in first cell of table
        CEditInsertPoint ip;
        GetTableInsertPoint(ip);
        CEditTableElement *pTable = ip.m_pElement->GetTableIgnoreSubdoc();
        if( pTable )
        {
            CEditTableCellElement* pNextCell = pTable->GetFirstCell();
            
            char *pCellText = pText;
            XP_Bool bDone = FALSE;
            XP_Bool bEndOfRow = FALSE;

            while(!bDone)
            {            
                do {
                    char current = *pText;
                    if( current == 9)
                    {
                        // We found the end of the cell's text
                        *pText = '\0';
                        pText++;
                        break;
                    }
                    if( current == 13 || current == 10 || current == '\0' )
                    {
                        // We found the end of the cell's text
                        if( current != '\0' )
                        {
                            // Terminate text for this cell
                            *pText = '\0';

                            // Skip over other end-of-line characters
                            char next = *(pText+1);
                            if( (current == 13 && next == 10) || 
                                (current == 10 && next == 13) )
                            {
                                pText++;
                            }                    
                            pText++;
                        }
                        bEndOfRow = TRUE;
                        break;
                    }
                    pText++;
                } while( pText );

                if( pCellText && *pCellText )
                    PasteText(pCellText, FALSE, FALSE, FALSE, TRUE); //Last 2param = don't relayout but we want to reduce

                intn iNextRow = iRow;
                
                // Move insert point to next cell
                // If its in next row, iNextRow will be incremented
                if( NextTableCell(FALSE, TRUE, &iNextRow) )
                {
                    if(bEndOfRow)
                    {
                        // Next item should go into next row
                        iRow++;
                        // If we have a short row (fewer tabs than maximum),
                        //   skip to first cell of next row
                        XP_Bool bGetNext = TRUE;
                        while( bGetNext && iRow != iNextRow )
                        {
                            bGetNext = NextTableCell(FALSE, TRUE, &iNextRow);
                        }
                        bEndOfRow = FALSE;
                    } 
                    else if( iRow != iNextRow )
                    {
                        // Should never happen - we are in next row of new table,
                        //   but we're not at end of text "row"
                        XP_ASSERT(FALSE);
                    }

                    // The next cell item starts at next character
                    pCellText = pText;

                    if( *pText == '\0' )
                        bDone = TRUE;
                }
                else
                {
                    //TODO: We should probably test if any text is left
                    //      here - there shouldn't be any
                    bDone = TRUE;
                }
            } //while(!bDone)

            // Return insert point to the first cell
            SetInsertPoint(ip);
            // Relayout the entire table
            Relayout(pTable, 0);
        }
    }

    return EDT_COP_OK;
}

EDT_ClipboardResult CEditBuffer::PasteTextIntoTable(char *pText, XP_Bool /*bReplace*/, intn iRows, intn iCols)
{
    if( iRows == 0 || iCols == 0 )
       CountRowsAndColsInPasteText(pText, &iRows, &iCols); 
    //TODO: FINISH PASTETEXTINTOTABLE()
    return EDT_COP_OK;
}


//
// Make sure that there are not spaces next to each other after a delete,
//  or paste
//
void CEditBuffer::FixupSpace( XP_Bool /*bTyping*/){
    if( m_pCurrent->InFormattedText() ){
        return;
    }

    if( m_pCurrent->IsBreak() && m_iCurrentOffset == 1 ){
        // Can't have a space after a break.
        CEditLeafElement *pNext = m_pCurrent->TextInContainerAfter();
        if( pNext
                && pNext->IsA(P_TEXT)
                && pNext->Text()->GetLen() != 0
                && pNext->Text()->GetText()[0] == ' '){
            pNext->Text()->DeleteChar(m_pContext, 0);
            return;
        }
    }

    if( !m_pCurrent->IsA(P_TEXT) ){
        return;
    }

    CEditTextElement *pText = m_pCurrent->Text();

    if( pText->GetLen() == 0 ){
        return;
    }
    if( m_iCurrentOffset > 0 && pText->GetText()[ m_iCurrentOffset-1 ] == ' ' ){
        if( m_iCurrentOffset == pText->GetLen() ){
            CEditLeafElement *pNext = pText->TextInContainerAfter();
            if( pNext
                    && pNext->IsA(P_TEXT)
                    && pNext->Text()->GetLen() != 0
                    && pNext->Text()->GetText()[0] == ' '){
                pNext->Text()->DeleteChar(m_pContext, 0);
                //m_iCurrentOffset--;  // WHy???
                return;
            }
        }
        else if( m_iCurrentOffset < pText->GetLen() && pText->GetText()[m_iCurrentOffset] == ' ' ){
            pText->DeleteChar(m_pContext, m_iCurrentOffset);
            return;
        }
    }
    // check for beginning of paragraph with a space.
    else if( m_iCurrentOffset == 0 && pText->GetText()[0] == ' ' ){
        pText->DeleteChar(m_pContext, 0);
        CEditLeafElement *pNext = m_pCurrent->TextInContainerAfter();
        if( m_pCurrent->Text()->GetLen() == 0 && pNext ){
            m_pCurrent = pNext;
        }
    }
    // Can't reduce here because it might smash objects pointers we know about
}

EDT_ClipboardResult CEditBuffer::CutSelection( char **ppText, int32* pTextLen,
                    char **ppHtml, int32* pHtmlLen){
    VALIDATE_TREE(this);
    EDT_ClipboardResult result = CanCut(TRUE);
    if ( result != EDT_COP_OK ) return result;

    result = CopySelection( ppText, pTextLen, ppHtml, pHtmlLen );
    if ( result != EDT_COP_OK ) return result;

    CPersistentEditSelection selection = GetEffectiveDeleteSelection();
    BeginBatchChanges(kCutCommandID);
    result = DeleteSelection();
    EndBatchChanges();

    // Check to see if we trashed the document
    XP_ASSERT ( m_pCurrent && m_pCurrent->GetElementType() != eEndElement );
    return result;
}

XP_Bool CEditBuffer::CutSelectionContents( CEditSelection& selection,
                    char **ppHtml, int32* pHtmlLen ){
    XP_Bool result = CopySelectionContents( selection, ppHtml, pHtmlLen );
    if ( result ) {
        CEditLeafElement *pBegin;
        CEditLeafElement *pEnd;
        MakeSelectionEndPoints( selection, pBegin, pEnd );

        DeleteBetweenPoints( pBegin, pEnd );
    }
    return TRUE;
}


EDT_ClipboardResult CEditBuffer::CopySelection( char **ppText, int32* pTextLen,
                    char **ppHtml, int32* pHtmlLen )
{
    EDT_ClipboardResult result = CanCopy(TRUE);
    if ( result != EDT_COP_OK ) return result;

    //cmanske: Added table selection handling
    if ( ppText )
    {
        if( IsTableOrCellSelected() )
        {
            // Collect text from all cells in
            //  format ready to paste into spreadsheets
            *ppText = GetTabDelimitedTextFromSelectedCells();
        }
        else 
        {
            // This adds hard CR/LF at the end of each row
            // TODO: Shouldn't we "unwrap" paragraphs?
            *ppText = (char*) LO_GetSelectionText( m_pContext );
        }
    }
    if ( pTextLen ) *pTextLen = XP_STRLEN( *ppText );
    
    CEditSelection selection;
    EEditCopyType iCopyType;
    CEditTableElement *pTempTable = NULL;
    CEditRootDocElement *pTempRoot = NULL;


    CEditTableElement *pTable = NULL;    

    if( IsTableOrCellSelected() )
    {
        if( m_pSelectedEdTable )
        {
            // Make selection from the entire table
            m_pSelectedEdTable->GetAll(selection);
            iCopyType = eCopyTable;
            pTable = m_pSelectedEdTable;
        } else {
            // Create a new empty table
            pTempTable = new CEditTableElement(0,0);

            if(!pTempTable)
                return EDT_COP_SELECTION_EMPTY;

            CEditTableRowElement *pRow = new CEditTableRowElement();
            if(!pRow)
            {
                delete pTempTable;
                return EDT_COP_SELECTION_EMPTY;
            }
            pRow->InsertAsFirstChild(pTempTable);

            intn iCurrentRow = 1;
            intn iRow = 1;
            intn iMaxCols = 1;
            intn iCols = 0;

            CEditTableCellElement *pCell = GetFirstSelectedCell();
            
            if(!pCell)
            {
            ABORT_COPY:
                delete pTempTable; // This will delete row as well
                return EDT_COP_SELECTION_EMPTY;
            }
            
            pTable = pCell->GetParentTable();
            
            // We need to make a temporary root to our table, 
            //    primarily to supply the current buffer
            //    needed by GetWinCSID() during tag parsing from stream
            pTempRoot = new CEditRootDocElement(this);
            if(!pTempRoot)
                goto ABORT_COPY;

            pTempTable->InsertAsFirstChild(pTempRoot);
                
            // Just in case something tries to relayout
            //    during this. We will be temporarily 
            //    mangling cell pointers, which will 
            //    definitely crash if layout occurs before 
            //    we are fully restored
            m_bNoRelayout = TRUE;

            while( pRow && pCell )
            {
                if( iRow > iCurrentRow )
                {
                    // We are on next row
                    
                    // Save maximum number of columns
                    if( iCols > iMaxCols )
                        iMaxCols = iCols;
                    
                    iCurrentRow = iRow;

                    // Add new row to table
                    pRow = new CEditTableRowElement();
                    if(pRow)
                        pRow->InsertAsLastChild(pTempTable);

                } else {
                    iCols++;
                }
                // Switch cell to current row
                if( pRow )
                    pCell->SwitchLinkage(pRow);

                // Get next selected cell
                pCell = GetNextSelectedCell(&iRow);
            }
            // cmanske: There is a problem with streaming out a table whose last element 
            //   is an empty cell - it fails "local.Intersects(selection)"
            //   in CEditElement::PartialStreamOut
            // I tried adding an extra container+text element after the table, but this didn't work

            // Get selection type -- may be different from m_TableHitType
            //   when all cells in a row or col are selected
            ED_HitType iSelectionType = GetTableSelectionType();
            switch( iSelectionType /*m_TableHitType*/ )
            {
                case ED_HIT_SEL_ROW:
                    iCopyType = eCopyRows;
                    break;
                case ED_HIT_SEL_COL:
                    iCopyType = eCopyColumns;
                    break;
                default:
                    iCopyType = eCopyCells;
                    break;
            }
            // Set the number of columns and rows in table data
            // Note that we must supply csid:
            //   temp table can't get it 'cause its not part of doc.
            EDT_TableData *pData = pTempTable->GetData();
            if( pData )
            {
                pData->iColumns = iMaxCols;
                pData->iRows = iRow;
                EDT_TableData *pOldData = pTable->GetData();
                if( pOldData )
                {
                    pData->bBorderWidthDefined = pOldData->bBorderWidthDefined;
                    pData->iBorderWidth = pOldData->iBorderWidth;
                    pData->iCellSpacing = pOldData->iCellSpacing;
                    pData->iCellPadding = pOldData->iCellPadding;
                }
                pTempTable->SetData(pData);
                EDT_FreeTableData(pData);
            }
            // Make the selection from the temporary table
            pTempTable->GetAll(selection);
        }
    } else {
        // Get normal selection
        GetSelection(selection);
        iCopyType = eCopyNormal;
    }
    // Build the HTML stream from the selection
    if( !selection.IsEmpty() )
        CopySelectionContents( selection, ppHtml, pHtmlLen, iCopyType );

    if( pTempRoot )
    {
        // Restore original linkage for all selected cells
        CEditTableCellElement *pCell = GetFirstSelectedCell();
        while( pCell )
        {
            pCell->RestoreLinkage();
            pCell = GetNextSelectedCell();
        }
        // Deletes root and table under it
        delete pTempRoot;

        // Restore layout
        m_bNoRelayout = FALSE;
    }
    return result;
}

XP_Bool CEditBuffer::CopySelectionContents( CEditSelection& selection,
                                            char **ppHtml, int32* pHtmlLen, 
                                            EEditCopyType iCopyType )
{
    CStreamOutMemory stream;

    XP_Bool result = CopySelectionContents(selection, stream, iCopyType);

    *ppHtml = stream.GetText();
    *pHtmlLen = stream.GetLen();

    return result;
}

XP_Bool CEditBuffer::CopySelectionContents( CEditSelection& selection,
                                            IStreamOut& stream, EEditCopyType iCopyType )
{
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_pContext);
    stream.WriteInt(GetClipboardSignature());
    stream.WriteInt(GetClipboardVersion());
    stream.WriteInt(INTL_GetCSIWinCSID(c));
    //cmanske: New - contains info about what we are copying,
    //  used for tables
    int32 iTemp = iCopyType; // Lets be sure it ends up as an int32!
    stream.WriteInt(iTemp);

    // Never merge at the end of table
    //TODO: Problem: This will be TRUE if we have a "normal" selection
    //   that happens to end in a table, thus the last cell will be merged with
    //   next conainter after insert point
    int32 bMergeEnd = ( iCopyType == eCopyNormal ) ? !selection.EndsAtStartOfContainer() : 0;
    stream.WriteInt(bMergeEnd);

    // If streaming out "temporary" table for cell copying,
    //    we must use it as the root of stream creation since that is 
    //    where elements are examined if they contain the selection elements
    if( iCopyType > eCopyNormal )
    {
        CEditTableElement *pTable = selection.m_start.m_pElement->GetParentTable();
        if( pTable )
            pTable->PartialStreamOut(&stream, selection);
#ifdef DEBUG
        else XP_TRACE(("CEditBuffer::CopySelectionContents:  Failed to find parent table"));
#endif            
    } else {
        m_pRoot->PartialStreamOut(&stream, selection);
    }
    stream.WriteInt( (int32)eElementNone );
    return TRUE;
}

XP_Bool CEditBuffer::CopyBetweenPoints( CEditElement *pBegin,
                    CEditElement *pEnd, char **ppText, int32* pTextLen,
                    char **ppHtml, int32* pHtmlLen ){
    if ( ppText ) *ppText = (char*) LO_GetSelectionText( m_pContext );
    if ( pTextLen ) *pTextLen = XP_STRLEN( *ppText );
    CEditInsertPoint a(pBegin, 0);
    CEditInsertPoint b(pEnd, 0);
    CEditSelection s(a,b);
    return CopySelectionContents(s, ppHtml, pHtmlLen);
}

int32 CEditBuffer::GetClipboardSignature(){
    return 0xc43954af; /* Doesn't mean anything special, but must stay the same. */
}

int32 CEditBuffer::GetClipboardVersion(){
    return 0x040000; /* Should roughly match product version in binary-coded-decimal */
}



// New Table Cut/Paste routines
XP_Bool CEditBuffer::CountRowsAndColsInPasteText(char *pText, intn* pRows, intn* pCols)
{
    intn iRows = 1;
    intn iCols = 1;
    intn iMaxCols = 0;
    XP_Bool bColsOK = TRUE;
    while( *pText )
    {
        char current = *pText;
        char next = *(pText+1);

        if( current == 13 || current == 10 )
        {
            // We have CR            
            // Check if number of cells in this row is same as 
            //   previous. Return this to user
            if(iMaxCols > 0 && iMaxCols != iCols )
                bColsOK = FALSE;

            // Save maximumn columns per row and setup for next row
            if( iCols > iMaxCols )
                iMaxCols = iCols;
            iCols = 1;
            
            // Skip over other next-line chars
            if( (current == 13 && next == 10) || 
                (current == 10 && next == 13) )
            {
                pText++;
            }                    
            pText++;

            // Check if there's anything else after this,
            // Increment rows only there's something else in next row
            if(*pText)
                iRows++;
        } else {
            if( current == 9)
                iCols++;

            pText++;
        }
    }
    if( pRows )
        *pRows = iRows;
    if( pCols )
        *pCols = iMaxCols;

    return bColsOK;
}



//
// Used during parse phase.
//
ED_Alignment CEditBuffer::GetCurrentAlignment(){
    if( GetParseState()->m_formatAlignStack.IsEmpty()){
        return m_pCreationCursor->GetDefaultAlignment();
    }
    else {
        return GetParseState()->m_formatAlignStack.Top();
    }
}

void CEditBuffer::GetSelection( CEditSelection& selection ){
    if ( IsSelected() ) {
        GetSelection( selection.m_start.m_pElement, selection.m_start.m_iPos,
            selection.m_end.m_pElement, selection.m_end.m_iPos,
            selection.m_bFromStart);
    }
    else {
        GetInsertPoint( &selection.m_start.m_pElement, &selection.m_start.m_iPos, &selection.m_start.m_bStickyAfter);
        selection.m_end = selection.m_start;
        selection.m_bFromStart = FALSE;
    }
}

void CEditBuffer::GetSelection( CPersistentEditSelection& persistentSelection ){
    CEditSelection selection;
    GetSelection(selection);
    persistentSelection = EphemeralToPersistent(selection);
}

void CEditBuffer::GetInsertPoint(CEditInsertPoint& insertPoint){
    if ( ! IsSelected() )
    {
        GetInsertPoint( & insertPoint.m_pElement, & insertPoint.m_iPos, & insertPoint.m_bStickyAfter );
    }
    else
    {
        CEditSelection selection;
        GetSelection(selection);
        selection.ExcludeLastDocumentContainerEnd();
        insertPoint = *selection.GetActiveEdge();
    }
}

void CEditBuffer::GetInsertPoint(CPersistentEditInsertPoint& insertPoint){
    CEditInsertPoint pt;
    GetInsertPoint(pt);
    insertPoint = EphemeralToPersistent(pt);
}

void CEditBuffer::SetSelection(CPersistentEditSelection& persistentSelection){
    CEditSelection selection = PersistentToEphemeral(persistentSelection);
    SetSelection(selection);
}

void CEditBuffer::CopyEditText(CEditText& text){
    text.Clear();
    if ( IsSelected() ) {
        XP_Bool copyOK = CopySelection(NULL, NULL, text.GetPChars(), text.GetPLength());
        XP_ASSERT(copyOK);
    }
}

void CEditBuffer::CopyEditText(CPersistentEditSelection& persistentSelection, CEditText& text){
    text.Clear();
    if ( ! persistentSelection.IsInsertPoint() ) {
        CEditSelection selection = PersistentToEphemeral(persistentSelection);
        CopySelectionContents(selection, text.GetPChars(), text.GetPLength());
    }
}

void CEditBuffer::CutEditText(CEditText& text){
    text.Clear();
    CEditSelection selection;
    GetSelection(selection);
    if ( ! selection.IsInsertPoint() ) {
        CutSelectionContents(selection, text.GetPChars(), text.GetPLength());
    }
}

void CEditBuffer::PasteEditText(CEditText& text){
    if ( text.Length() > 0 )
        PasteHTML( text.GetChars(), TRUE);
}

// Persistent to regular selection conversion routines
CEditInsertPoint CEditBuffer::PersistentToEphemeral(CPersistentEditInsertPoint& persistentInsertPoint){
    CEditInsertPoint result = m_pRoot->IndexToInsertPoint(
        persistentInsertPoint.m_index, persistentInsertPoint.m_bStickyAfter);
#ifdef DEBUG_EDITOR_LAYOUT
    // Check for reversability
    CPersistentEditInsertPoint p2 = result.m_pElement->GetPersistentInsertPoint(result.m_iPos);
    XP_ASSERT(persistentInsertPoint == p2);
    // Check for legality.
    if ( result.m_pElement->IsEndOfDocument() &&
        result.m_iPos != 0){
        XP_ASSERT(FALSE);
        result.m_iPos = 0;
    }

#endif
    return result;
}

CPersistentEditInsertPoint CEditBuffer::EphemeralToPersistent(CEditInsertPoint& insertPoint){
    CPersistentEditInsertPoint result = insertPoint.m_pElement->GetPersistentInsertPoint(insertPoint.m_iPos);
#ifdef DEBUG_EDITOR_LAYOUT
    // Check for reversability
    CEditInsertPoint p2 = m_pRoot->IndexToInsertPoint(result.m_index);
    if ( ! ( p2 == insertPoint ||
        insertPoint.IsDenormalizedVersionOf(p2) ) ) {
        // One or the other is empty.
        if ( insertPoint.m_pElement->Leaf()->GetLen() == 0 ||
            p2.m_pElement->Leaf()->GetLen() == 0 ) {
            // OK, do they at least have the same persistent point?
            CPersistentEditInsertPoint pp2 = p2.m_pElement->GetPersistentInsertPoint(p2.m_iPos);
            XP_ASSERT(result == pp2);
        }
        else
        {
            // Just wrong.
            XP_ASSERT(FALSE);
        }
    }
#endif
    return result;
}

CEditSelection CEditBuffer::PersistentToEphemeral(CPersistentEditSelection& persistentSelection){
    CEditSelection selection(PersistentToEphemeral(persistentSelection.m_start),
        PersistentToEphemeral(persistentSelection.m_end),
        persistentSelection.m_bFromStart);
    XP_ASSERT(!selection.m_start.IsEndOfDocument());
    return selection;
}

CPersistentEditSelection CEditBuffer::EphemeralToPersistent(CEditSelection& selection){
    CPersistentEditSelection persistentSelection;
    XP_ASSERT(!selection.m_start.IsEndOfDocument());
    persistentSelection.m_start = EphemeralToPersistent(selection.m_start);
    persistentSelection.m_end = EphemeralToPersistent(selection.m_end);
    persistentSelection.m_bFromStart = selection.m_bFromStart;
    return persistentSelection;
}

void CEditBuffer::AdoptAndDo(CEditCommand* command) {
    DoneTyping();
    GetCommandLog()->AdoptAndDo(command);
}

void CEditBuffer::Undo() {
    DoneTyping();
    GetCommandLog()->Undo();
}

void CEditBuffer::Redo() {
    DoneTyping();
    GetCommandLog()->Redo();
}

void CEditBuffer::Trim() {
    DoneTyping();
    GetCommandLog()->Trim();
}

intn CEditBuffer::GetCommandHistoryLimit() {
    return GetCommandLog()->GetCommandHistoryLimit();
}

void CEditBuffer::SetCommandHistoryLimit(intn newLimit) {
    GetCommandLog()->SetCommandHistoryLimit(newLimit);
}

// Returns NULL if out of range
intn CEditBuffer::GetUndoCommand(intn n) {
    return GetCommandLog()->GetUndoCommand(n);
}

intn CEditBuffer::GetRedoCommand(intn n) {
    return GetCommandLog()->GetRedoCommand(n);
}

void CEditBuffer::BeginBatchChanges(intn id) {
    DoneTyping();
    GetCommandLog()->BeginBatchChanges(id);
}

void CEditBuffer::EndBatchChanges() {
    GetCommandLog()->EndBatchChanges();
}

XP_Bool CEditBuffer::IsWritable(){
#ifdef EDITOR_JAVA
	return ! ( EditorPluginManager_PluginsExist() 
        && EditorPluginManager_IsPluginActive(GetPlugins()));
#else
    return TRUE;
#endif
}

void CEditBuffer::StartTyping(XP_Bool bTyping){
    if ( bTyping && ! m_bTyping ) {
        GetCommandLog()->StartTyping(kTypingCommandID);
        m_bTyping = TRUE;
    }
}

void CEditBuffer::DoneTyping() {
    if ( m_bTyping ) {
#ifdef DEBUG_TYPING
        XP_TRACE(("Done typing."));
#endif
        GetCommandLog()->EndTyping();
    }
    m_bTyping = FALSE;
}

#if 0 
// NOT USED But may be useful?
// This must be fairly efficient, so minimal checking
// Find the cell whose X value is within a cell's horizontal location
// (IGNORE the Y value -- assumes we are already near top of table)
LO_Element* FindClosestCellToTableTop(LO_Element *pLoTable, LO_Element *pEndLoElement, int32 x)
{
    LO_Element *pLoCell = NULL;
    if( pLoTable && pEndLoElement)
    {
        LO_Element *pLoElement = pLoTable->lo_any.next;
        while(pLoElement != pEndLoElement)
        {
            if( pLoElement->lo_any.type == LO_CELL &&
                x >= pLoElement->lo_any.x && x <= (pLoElement->lo_any.x + pLoElement->lo_any.width) )
            {
                return pLoElement;
            }
            pLoElement = pLoElement->lo_any.next;
        }
    }
    return pLoCell;
}

// Find the cell whose Y value is within a cell's vertical location
// (IGNORE the X value -- assumes we are already near left edge of table)
PRIVATE
LO_Element* FindClosestCellToTableLeft(LO_Element *pLoTable, LO_Element *pEndLoElement, int32 y)
{
    LO_Element *pLoCell = NULL;
    if( pLoTable && pEndLoElement)
    {
        LO_Element *pLoElement = pLoTable->lo_any.next;
        while(pLoElement != pEndLoElement)
        {
            if( pLoElement->lo_any.type == LO_CELL &&
                y >= pLoElement->lo_any.y && y <= (pLoElement->lo_any.y + pLoElement->lo_any.height) )
            {
                return pLoElement;
            }
            pLoElement = pLoElement->lo_any.next;
        }
    }
    return pLoCell;
}
#endif

CEditTableCellElement* CEditBuffer::GetFirstCellInCurrentColumn()
{
//    if( !IsInsertPointInTable() )
//        return NULL;

    // Get current leaf element at caret or selection
    // Leaf elements are our only connection to coresponding LO_Elements
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if( pTableCell )
    {
        // Returns cell with EXACT X value (don't include spanned cells)
        return pTableCell->GetFirstCellInColumn(pTableCell->GetX());
    }
    return NULL;
}

CEditTableCellElement* CEditBuffer::GetFirstCellInCurrentRow()
{
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if( pTableCell )
    {
        // Returns cell with EXACT Y value (don't include spanned cells)
        return pTableCell->GetFirstCellInRow(pTableCell->GetY());
    }
    return NULL;
}

PRIVATE
LO_Element *edt_XYToNestedTableAndCell(LO_Element *pStartCell, int32 x, int32 y, LO_Element **ppCellElement)
{
    if( pStartCell == NULL || pStartCell->type != LO_CELL )
        return NULL;

    // Check cell contents list for a nested table
	LO_Element *pTableElement = NULL;
	LO_Element *pCellElement = NULL;
	LO_Element *pCellList = pStartCell->lo_cell.cell_list;
    while( pCellList )
    {
        if( pCellList->type == LO_TABLE &&
		    ((y >= pCellList->lo_any.y) &&
            (y < pCellList->lo_any.y + pCellList->lo_any.y_offset + pCellList->lo_any.height) &&
			(x >= pCellList->lo_any.x) &&
			(x < (pCellList->lo_any.x + pCellList->lo_any.x_offset + pCellList->lo_any.width))) )
        {
            // We are inside a nested table
            pTableElement = pCellList;
            
            // Get first cell in table and search through that cell list
            //   to find deepest nested table and cell enclosing cursor
            LO_Element *pNestedCell = pCellList->lo_any.next;
            XP_ASSERT(pNestedCell->type == LO_CELL);
            while( pNestedCell )
            {
		        if( (y >= pNestedCell->lo_any.y) &&
                    (y < pNestedCell->lo_any.y + pNestedCell->lo_any.y_offset + pNestedCell->lo_any.height) &&
			        (x >= pNestedCell->lo_any.x) &&
			        (x < (pNestedCell->lo_any.x + pNestedCell->lo_any.x_offset + pNestedCell->lo_any.width)) )
                {
                    // We are inside a cell in a nested table
                    // We may stop here, but look recursively into its cell list first
                    pCellElement = pNestedCell;

                    LO_Element *pNestedCell2 = NULL;
                    LO_Element *pNestedTable2 = edt_XYToNestedTableAndCell(pNestedCell, x, y, &pNestedCell2);
                    if( pNestedTable2 )
                    {
                        // We found a deeper nested table
                        pTableElement = pNestedTable2;
                        // This may be NULL if we are in table and not inside a cell in that table
                        pCellElement = pNestedCell2;
                    }
                    // We found the enclosing cell, so stop searching nested table
                    break;
                }
                // Check all cells in the nested table
                pNestedCell = pNestedCell->lo_any.next;
            }
            // Found an enclosing table - done scaning original cell list
            break;
        }
        // Next cell in list
        pCellList = pCellList->lo_any.next;
    }
    // Return deepest table and/or cell found
    if( ppCellElement )
        *ppCellElement = pCellElement;
    
    return pTableElement;
}

static void edt_SetHitLimits(LO_Element *pLoElement,
                             int32 *pLeftLimit, int32 *pTopLimit, int32 *pRightLimit, int32 *pBottomLimit,
                             int32 *pRight, int32 *pBottom, XP_Bool /*bDropLimits*/)
{
    int32 left = pLoElement->lo_any.x;
    int32 top = pLoElement->lo_any.y;
    int32 right = left + pLoElement->lo_any.width;
    int32 bottom = top + pLoElement->lo_any.height;

    // We use this for cell sizing detection in inter_cell_space
    if( pRight )
        *pRight = right;

    if( pBottom )
        *pBottom = bottom;

    if( pLoElement->type == LO_TABLE )
    {
        // We assume that we don't need to worry about too-small tables
        // Include the entire beveled border as hit region
        *pLeftLimit = left + max(ED_SIZING_BORDER, pLoElement->lo_table.border_left_width);
        *pRightLimit = right - max(ED_SIZING_BORDER, pLoElement->lo_table.border_left_width);
        
        *pTopLimit = top + max(ED_SIZING_BORDER, pLoElement->lo_table.border_top_width);
        *pBottomLimit = bottom - max(ED_SIZING_BORDER, pLoElement->lo_table.border_bottom_width);
    } else {
        // Figure sizing regions but reduce if cell is too small
        //  so side and top hit regions are at least ED_SIZING_BORDER wide
        //  This may eliminate corner hit regions if element rect is too small
    
        int32 border;
        if( pLoElement->lo_any.width < 3*ED_SIZING_BORDER )
        {
            border = pLoElement->lo_any.width > ED_SIZING_BORDER ? 
                                                  ((pLoElement->lo_any.width-ED_SIZING_BORDER)/2) : 0;
        } else {
        
            border = ED_SIZING_BORDER;
        }
        *pLeftLimit =  left + border;

        // This reduces the hit region within the cell if the inter-cell space is large enough
        //   to serve as the column sizing hit region
        *pRightLimit =  right - min(border, max(0, ED_SIZING_BORDER - pLoElement->lo_cell.inter_cell_space));

        if( pLoElement->lo_any.width < 3*ED_SIZING_BORDER )
        {
            border = pLoElement->lo_any.height > ED_SIZING_BORDER ? 
                                                  ((pLoElement->lo_any.width-ED_SIZING_BORDER)/2) : 0;
        } else {
            border = ED_SIZING_BORDER;
        }

        *pTopLimit = top + border;
        *pBottomLimit = bottom - border;
    }
}

// Find Table or cell element and specific mouse hit regions,
//  do not look into cells for other element types except another table
ED_HitType CEditBuffer::GetTableHitRegion(int32 x, int32 y, LO_Element **ppElement, XP_Bool bModifierKeyPressed)
{
    // Signal to use last-selected element and hit type
    if( x <= 0 )
    {
        if( ppElement )
            *ppElement = m_pSelectedTableElement;
        return m_TableHitType;
    }

    // Initialize in case we don't find any table element
    if( ppElement)
        *ppElement = NULL;

    // From LO_XYToElement
	int32 doc_id = XP_DOCID(m_pContext);
	lo_TopState *top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
		return ED_HIT_NONE;

	lo_DocState *state = top_state->doc_state;

	LO_Element *pLoElement = NULL;
	LO_Element *pEndLoElement = NULL;
	LO_Element *pTableElement = NULL;
	LO_Element *pCellElement = NULL;
    int32 left_limit, right_limit, top_limit, bottom_limit, right, bottom;
    int32 xPlus = x + ED_SIZING_BORDER;
    XP_Bool     bAboveOrBelowTable = FALSE;

	int32 line = lo_PointToLine(m_pContext, state, x, y);
	int32 lineBelow = lo_PointToLine(m_pContext, state, x, y + ED_SIZING_BORDER);
	int32 lineAbove = lo_PointToLine(m_pContext, state, x, y - ED_SIZING_BORDER);
  
    if( lineBelow != line || lineAbove != line )
    {
        // We are near boundary between two lines,
        //   check if we are above or below a table
        // THIS ASSUMES TABLE IS FIRST ELEMENT ON lineBelow or lineAbove
        //   AND IS TALLEST ELEMENT IN THE LINE
        if( lineBelow != line )
            lo_GetLineEnds(m_pContext, state, lineBelow, &pLoElement, &pEndLoElement);
        
        if( lineAbove != line )
            lo_GetLineEnds(m_pContext, state, lineAbove, &pLoElement, &pEndLoElement);
         
        if( pLoElement && pLoElement->type == LO_TABLE &&
            (xPlus >= pLoElement->lo_any.x) &&
			(x < (pLoElement->lo_any.x + pLoElement->lo_any.x_offset 
                    + pLoElement->lo_any.width + ED_SIZING_BORDER)) )
        {
            pTableElement = pLoElement;
            bAboveOrBelowTable = TRUE;
        }
    }

    if( !bAboveOrBelowTable )
    {
        lo_GetLineEnds(m_pContext, state, line, &pLoElement, &pEndLoElement);

	    while (pLoElement && pLoElement != pEndLoElement)
	    {
			if (pLoElement->type == LO_TABLE)
			{
                if ((y >= pLoElement->lo_any.y) &&
                    (y <= pLoElement->lo_any.y + pLoElement->lo_any.y_offset + pLoElement->lo_any.height) &&
			        (xPlus >= pLoElement->lo_any.x) &&
			        (x <= (pLoElement->lo_any.x + pLoElement->lo_any.x_offset + pLoElement->lo_any.width + ED_SIZING_BORDER)))
                {
                    // Save the table element
                    pTableElement = pLoElement;
                }
			}
		    else if( pLoElement->type == LO_CELL )
            {
                // Note that we include the inter-cell space when testing if inside a cell
                if ((y >= pLoElement->lo_cell.y) &&
                    (y <= pLoElement->lo_cell.y + pLoElement->lo_cell.y_offset + pLoElement->lo_any.height +
                            pLoElement->lo_cell.inter_cell_space ) &&
			        (x >= pLoElement->lo_cell.x) &&
			        (x <= (pLoElement->lo_cell.x + pLoElement->lo_cell.x_offset + pLoElement->lo_any.width +
                            pLoElement->lo_cell.inter_cell_space )))
		        {

                    pCellElement = pLoElement;

                    // Search its contents for nested table and cell
                    LO_Element *pNestedCell = NULL;
                    LO_Element *pNestedTable = edt_XYToNestedTableAndCell(pLoElement, x, y, &pNestedCell);
                    if( pNestedTable )
                    {
                        pTableElement = pNestedTable;
                        pCellElement = pNestedCell;
                    }
                    // We're done once we find the enclosing cell
                    break;
		        }

            }
		    pLoElement = pLoElement->lo_any.next;
	    }
    }

    // Now check where in Table and Cell the cursor is over
    if (pTableElement)
    {
        XP_Bool bCanDragTable = (!bModifierKeyPressed && (pTableElement->lo_table.ele_attrmask & LO_ELE_SELECTED));

        // First check for cell hit regions
        if(pCellElement)
        {
            XP_Bool bCanDragCells = (!bModifierKeyPressed && (pCellElement->lo_cell.ele_attrmask & LO_ELE_SELECTED));

            // Return the cell found, even if result is ED_HIT_NONE
            if(ppElement) *ppElement = pCellElement;
            
            // Set test limits for Cell element
            edt_SetHitLimits(pCellElement, &left_limit, &top_limit, &right_limit, &bottom_limit, &right, &bottom, FALSE );
                        
            if( x >= right_limit )
            {
                if( x >= right_limit ||  // Anywhere in inter-cell space to the right of the cell
                   (y >= top_limit && y <= bottom_limit) ) // Inside cell, near right edge, excluding corners
                {
                    // If a selected cell - drag selection,
                    //  except if modifier key is pressed for sizing
                    if( bCanDragCells )
                        return ED_HIT_DRAG_TABLE;
                    
                    // Modifier key pressed an near right edge, excluding corners
                    return ED_HIT_SIZE_COL;
                }
                
                if( y >= bottom_limit )
                    // Lower right corner - leave for sizing object in cell
                    return ED_HIT_NONE;
            }

            // We are in multiple cell selection mode,
            //  so select the cell automatically,
            //  but be sure we are not in inter-cell area (too confusing)
            //  Also leave area near bottom for drag selection
            if( m_SelectedLoCells.Size() && bModifierKeyPressed &&
                x <= right && y <= bottom_limit )
            {
                return ED_HIT_SEL_CELL;
            }
            
            if( x <= left_limit )
            {
                // Leave left edge for placing cursor
                return ED_HIT_NONE;
            }
            if( y <= top_limit )
            {
                // Along top edge
                if( x <= right_limit )
                {
                    // Near top border of cell, excluding corners

                    // If a selected cell - drag selection,
                    //  except if modifier key is pressed for sizing
                    if( bCanDragCells )
                        return ED_HIT_DRAG_TABLE;
                
                    return ED_HIT_SEL_CELL;
                }
                // Upper right corner - leave for sizing image flush against borders
                return ED_HIT_NONE;
            }

            if( y >= bottom_limit )  // Along bottom edge (including intercell region)
            {
                // If a selected cell - drag selection,
                //  except if modifier key is pressed for sizing
                if( bCanDragCells )
                    return ED_HIT_DRAG_TABLE;
                
                // Size the row
                return ED_HIT_SIZE_ROW;
            }


            // Inside of a cell and no hit regions
            return ED_HIT_NONE;
        }

        // Return the table element
        if( ppElement) *ppElement = pTableElement;

        // Set test limits for Table element
        edt_SetHitLimits(pTableElement, &left_limit, &top_limit, &right_limit, &bottom_limit, 0, 0, FALSE );

        if( x <= left_limit )
        {
            // Within left border of table
            if( y <= top_limit )
            {
                // Upper left corner

                // If a selected table - drag selection,
                //  except if modifier key is pressed for sizing
                if( bCanDragTable )
                    return ED_HIT_DRAG_TABLE;

                if( bModifierKeyPressed )
                    return ED_HIT_SEL_ALL_CELLS;

                return ED_HIT_SEL_TABLE; 
            }
            if( y >= bottom_limit )
                // Lower left corner
                return ED_HIT_ADD_ROWS;
            
            // Along left edge of table - select row
            if( ppElement )
            { 
                *ppElement = lo_GetFirstCellInColumnOrRow(m_pContext, pTableElement, x, y, GET_ROW, NULL);
            }
            return ED_HIT_SEL_ROW;
        } 
        else if( x >= right_limit )
        {
            if( y >= bottom_limit )
                // Lower right corner
                return ED_HIT_ADD_COLS;

            // If a selected table - drag selection,
            //  except if modifier key is pressed for sizing
            if( bCanDragTable )
                return ED_HIT_DRAG_TABLE;

            // Along right edge
            return ED_HIT_SIZE_TABLE_WIDTH;
        }
        else if( y >= bottom_limit )
        {
            // If a selected table - drag selection,
            //  except if modifier key is pressed for sizing
            if( bCanDragTable )
                return ED_HIT_DRAG_TABLE;

            // Below bottom, excluding corners
            return ED_HIT_SIZE_TABLE_HEIGHT;
        }

        if( y <= top_limit )
        {
            // If a selected table - drag selection,
            //  except if modifier key is pressed for sizing
            if( bCanDragTable )
                return ED_HIT_DRAG_TABLE;

            // Along top border - select column
            if( ppElement)
            { 
                *ppElement = lo_GetFirstCellInColumnOrRow(m_pContext, pTableElement, x, y, GET_COL, NULL);
            }
            return ED_HIT_SEL_COL;
        }
    }
    return ED_HIT_NONE;
}

ED_DropType CEditBuffer::GetTableDropRegion(int32 *pX, int32 *pY, int32 *pWidth, int32 *pHeight, LO_Element **ppElement)
{
    if( !m_pDragTableData || !pX || !pY || !pWidth || !pHeight )
        return ED_DROP_NORMAL;

    ED_HitType  iSourceType = m_pDragTableData->iSourceType;
    ED_DropType iDropType = ED_DROP_NORMAL;

    // Initialize in case we don't find a table element
    if( ppElement)
        *ppElement = NULL;

    // Filter out types that are not applicable
    if( ! (iSourceType == ED_HIT_SEL_COL  ||
           iSourceType == ED_HIT_SEL_ROW  || 
           iSourceType == ED_HIT_SEL_CELL ||
           iSourceType == ED_HIT_SEL_ALL_CELLS) )
    {
        return ED_DROP_NORMAL;
    }


    // From LO_XYToElement
	int32 doc_id = XP_DOCID(m_pContext);
	lo_TopState *top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
		return ED_DROP_NORMAL;

	lo_DocState *state = top_state->doc_state;

	LO_Element *pLoElement = NULL;
	LO_Element *pEndLoElement;
	LO_Element *pTableElement = NULL;
	LO_Element *pCellElement = NULL;
    LO_Element *pFirstCell = NULL;
    XP_Bool     bLastCellInRow = FALSE;
    int32 x = *pX;
    int32 y = *pY;
	int32 line = lo_PointToLine(m_pContext, state, x, y);

    int32 iLeft, iTop, iRight, iBottom;
    int32 left_limit, right_limit, top_limit, bottom_limit, right, bottom;
  
    lo_GetLineEnds(m_pContext, state, line, &pLoElement, &pEndLoElement);

	while (pLoElement != pEndLoElement)
	{
		if (pLoElement->type == LO_TABLE)
		{
            // Save table if Y value is ANYWHERE inside the table
            // (if outside this, then there can't be a table on the line)
            if( (y >= pLoElement->lo_any.y) &&
                (y <= pLoElement->lo_any.y + pLoElement->lo_any.y_offset + pLoElement->lo_any.height) )
            {
                pTableElement = pLoElement;
                LO_TableStruct *pTable = (LO_TableStruct*)pTableElement;

                // Snap cursor coordinates to bounding rect of all cells in the table
                //  so we find correct cell in first pass when cursor is to the left
                //  or right of the this rect (in table borders or margins before and after table)
                // (We will not find a cell only when there are ragged-right rows in the table)

                iLeft = pTable->x + pTable->x_offset + pTable->border_left_width + pTable->inter_cell_space;
                iRight = pTable->x + pTable->x_offset + pTable->width;
                iTop = pTable->y + pTable->y_offset + pTable->border_top_width + pTable->inter_cell_space;
                iBottom = pTable->y + pTable->y_offset + pTable->height;

                x = min(iRight, max(x, iLeft));
                y = min(iBottom, max(y, iTop));
            }
		}
		else if( pLoElement->type == LO_CELL )
        {
            // Save the first cell
            if( !pFirstCell )
                pFirstCell = pLoElement;

            int32 iCellRight;
            LO_Element *pNextCell = pLoElement->lo_any.next;
            while( pNextCell && pNextCell->type != LO_CELL ) pNextCell = pNextCell->lo_any.next;
            
            if( pNextCell && pNextCell->lo_cell.x < pLoElement->lo_cell.x )
            {
                // This is last cell in current row, so we can consider inserting at AFTER
                bLastCellInRow = TRUE;
                // Extend hit region to rightmost edge of cell area
                iCellRight = iRight;
            }
            else 
            {
                iCellRight = pLoElement->lo_cell.x + pLoElement->lo_cell.x_offset + pLoElement->lo_any.width;
            }   

            // Note that we include the inter-cell space BEFORE the cell when testing if inside a cell
            if ((y >= pLoElement->lo_cell.y - pLoElement->lo_cell.inter_cell_space) &&
                (y <= pLoElement->lo_cell.y + pLoElement->lo_cell.y_offset + pLoElement->lo_any.height) &&
			    (x >= pLoElement->lo_cell.x - pLoElement->lo_cell.inter_cell_space) &&
			    (x <= iCellRight))
		    {

                pCellElement = pLoElement;

                // Search its contents for nested table and cell
                LO_Element *pNestedCell = NULL;
                LO_Element *pNestedTable = edt_XYToNestedTableAndCell(pLoElement, x, y, &pNestedCell);
                if( pNestedTable )
                {
                    pTableElement = pNestedTable;
                    pCellElement = pNestedCell;
                }
                // We're done once we find the enclosing cell
                break;
		    }
        }
		pLoElement = pLoElement->lo_any.next;
	}

    if( pTableElement && pFirstCell )
    {
#if 0
// TODO: FINISH THIS!
        if( !pCellElement )
        {
            // Get bounding sides of all cells in table
            LO_Element *pLastCellInRow = NULL;
            LO_Element *pLastCellInCol = NULL;
            lo_GetFirstAndLastCellsInColumnOrRow(pCellElement, &pLastCellInRow, FALSE );
            lo_GetFirstAndLastCellsInColumnOrRow(pCellElement, &pLastCellInCol, TRUE );
            if( !pLastCellInRow || !pLastCellInCol )
                return ED_DROP_NORMAL; 
            int32 iLeft = pFirstCell->lo_any.x;
            int32 iTop =  pFirstCell->lo_any.y;
            int32 iRight = pLastCellInRow->lo_any.x + pLastCellInRow->lo_any.width;
            int32 iBottom = pLastCellInRow->lo_any.y + pLastCellInRow->lo_any.height;
            
            XP_Bool bFindBefore = FALSE;
            XP_Bool bFindAfter = FALSE;
            XP_Bool bFindAbove = FALSE;
            XP_Bool bFindBelow = FALSE;
            if( iSourceType != ED_HIT_SEL_COL )
            {
                if( y < pFirstCell->lo_any.y )
                    bFindAbove = TRUE;
                else {
                    bFindBelow = TRUE;
            }
            if( iSourceType != ED_HIT_SEL_ROW )
            {
                if( x < pFirstCell->lo_any.x )
                    bFindBefore = TRUE;
                else
                    bFindAfter = TRUE;
            }
            if(! bFindBefore || !bFindAbove || !bFindAfter || !bFindBelow )
            {
                if( 
            }

            LO_Element *pLoElement = pLoTable->lo_any.next;
            int32 iMaxX = 0;
            int32 iMaxY = 0;

            // Find cell closest to table edge we figured out above
            while( pLoElement && pLoElement->type != LO_LINEFEED)
            {
                
                if( pLoElement->lo_any.type == LO_CELL )
                {
                    if( y >= pLoElement->lo_any.y && y <= (pLoElement->lo_any.y + pLoElement->lo_any.height) )
                    {
                        if( bFindBefore )
                        {
                            pCellElement = pLoElement;
                            break;
                        }
                        if( 
                    }
                }
                pLoElement = pLoElement->lo_any.next;
            }
            if( !pCellElement )
            {
            }
        }
#endif
        if(pCellElement)
        {
            if(ppElement) *ppElement = pCellElement;

            // Return signal that we shouldn't drop here
            //   if we are exactly over the source selection
            if( pCellElement == m_pDragTableData->pFirstSelectedCell )
                return ED_DROP_NONE;
            
            LO_Element *pBefore = pCellElement->lo_any.prev;
            while( pBefore->type != LO_CELL && pBefore->type != LO_TABLE )
                pBefore = pBefore->lo_any.prev;

            // Save cell before only if it is first cell of selection being dragged
            if( pBefore->type == LO_TABLE || 
                (pBefore->type == LO_CELL && pBefore->lo_cell.x >= pCellElement->lo_cell.x) )
            {
                pBefore = NULL;
            }

            // Also don't allow dropping under other circumstances
            if( iSourceType == ED_HIT_SEL_COL && 
                (pBefore != NULL && pBefore == m_pDragTableData->pFirstSelectedCell) )
            {
                return ED_DROP_NONE;        
            }
            
            // Set test limits for Cell element: TRUE = get limits for Drag/Drop
            edt_SetHitLimits(pCellElement, &left_limit, &top_limit, &right_limit, &bottom_limit, &right, &bottom, TRUE );
                        
            *pX = pCellElement->lo_cell.x;
            *pY = pCellElement->lo_cell.y;
            int32 iExtra = pCellElement->lo_cell.inter_cell_space - ED_SIZING_BORDER;

            if( iSourceType != ED_HIT_SEL_ROW && (x <= left_limit || x > right) )
            {
                // Position the feedback in the center of intercell region
                //   or just before cell if intercell is too narrow
                *pWidth = ED_SIZING_BORDER;
                *pHeight = pCellElement->lo_cell.height;
                if( x > right )
                {
                    iDropType = ED_DROP_INSERT_AFTER;
                    *pX += pCellElement->lo_cell.width;
                    if( iExtra > 1 )
                        *pX += iExtra/2;
                } else {
                    iDropType = ED_DROP_INSERT_BEFORE;
                    if( iExtra > 1 )
                        *pX -= (iExtra/2 + ED_SIZING_BORDER);
                    else
                        *pX -= ED_SIZING_BORDER;
                }
            }
            if( iDropType == ED_DROP_NORMAL && iSourceType != ED_HIT_SEL_COL && (y <= top_limit || y > bottom) )
            {
                *pWidth = pCellElement->lo_cell.width;
                *pHeight = ED_SIZING_BORDER;;
                if( y > bottom )
                {
                    iDropType = ED_DROP_INSERT_BELOW;
                    *pY += pCellElement->lo_cell.height;
                    if( iExtra > 1 )
                        *pY += iExtra/2;
                } else {
                    iDropType = ED_DROP_INSERT_ABOVE;
                    if( iExtra > 1 )
                        *pY -= (iExtra/2 + ED_SIZING_BORDER);
                    else
                        *pY -= ED_SIZING_BORDER;
                }
            }
            if( iDropType == ED_DROP_NORMAL )
            {
                // Inside of a cell and no hit regions
                iDropType = ED_DROP_REPLACE_CELL;
                *pWidth = pCellElement->lo_cell.width;
                *pHeight = pCellElement->lo_cell.height;
            }
        }
    }
    return iDropType;
}

//
// Table and Cell selection 
//

// Internal version to select table cell
XP_Bool CEditBuffer::SelectTableElement(CEditTableCellElement *pCell, ED_HitType iHitType)
{
    if( pCell )
    {
        return SelectTableElement( pCell->GetX(), pCell->GetY(), 
                                   (LO_Element*)GetLoCell((CEditElement*)pCell),
                                   iHitType, FALSE, FALSE );
    }
    return FALSE;
}

// The only table and cell selection method exposed to FEs
XP_Bool CEditBuffer::SelectTableElement( int32 x, int32 y, LO_Element *pLoElement, ED_HitType iHitType,
                                         XP_Bool bModifierKeyPressed, XP_Bool bExtendSelection )
{
    if( iHitType == ED_HIT_NONE )
        return FALSE;

    // We will drag existing selection - so don't change selection
    if( iHitType == ED_HIT_DRAG_TABLE )
        return TRUE;

    // If not supplied, try to get appropriate element 
    //   and coordinates from current edit element
    if( !pLoElement )
        pLoElement = GetCurrentLoTableElement( iHitType, &x, &y );

    if( !pLoElement )
    {
        return FALSE;
    }

    // Properly end current non-cell selection if in progress
    if( IsSelecting() )
        EndSelection();

    // Extend selection from previous cell element
    //  to new element if SHIFT key is pressed
    if( bExtendSelection &&
        (iHitType == ED_HIT_SEL_COL ||
         iHitType == ED_HIT_SEL_ROW ||
         iHitType == ED_HIT_SEL_CELL) )
    {
        return ED_HIT_NONE != ExtendTableCellSelection(x,y);
    }
    m_pSelectedTableElement = NULL;
    m_TableHitType = ED_HIT_NONE;
    m_pPrevExtendSelectionCell = NULL;
    
    if( pLoElement->type == LO_TABLE &&
        (iHitType == ED_HIT_SEL_TABLE  ||
         iHitType == ED_HIT_SIZE_TABLE_WIDTH ||
         iHitType == ED_HIT_ADD_ROWS   ||
         iHitType == ED_HIT_ADD_COLS) )
    {
        LO_TableStruct *pLoTable =  &pLoElement->lo_table;
        // Remove any existing table or cell selection
        ClearTableAndCellSelection();

        CEditTableElement *pEdTable = SelectTable(TRUE, pLoTable);
        if( pEdTable )
        {
            if( IsSelected() )
                ClearSelection();

            // Move the caret to the first cell in table being selected
            CEditElement *pLeaf = pEdTable->FindNextElement(&CEditElement::FindLeafAll,0 );
            if( pLeaf )
            {
                ClearPhantomInsertPoint();
                ClearMove();
                FE_DestroyCaret(m_pContext);
                SetInsertPoint( pLeaf->Leaf(), 0, FALSE );
            }

            // Save selected element data
            m_pSelectedTableElement = pLoElement;
            m_TableHitType = ED_HIT_SEL_TABLE;
            return TRUE;
        } else {
            return FALSE;
        }
    }
    
    if( bModifierKeyPressed )
    {
        // Remove just the selection of a table
        //  since that can never be appended
        SelectTable(FALSE);
    } else {
        ClearTableAndCellSelection();
    }

    if( iHitType == ED_HIT_SEL_CELL )
    {
        if( pLoElement->type != LO_CELL )
            return FALSE;

        // We need to set this so mouse-move event
        //  doesn't wipe out appended cell selections
        m_pPrevExtendSelectionCell = pLoElement;

        // If cell was already selected and "Append" key is pressed,
        //  UNSELECT that cell (First param = FALSE)
        XP_Bool bSelect = !(bModifierKeyPressed && (pLoElement->lo_cell.ele_attrmask & LO_ELE_SELECTED));
        CEditTableCellElement *pEdCell = SelectCell( bSelect, &pLoElement->lo_cell, NULL );
        if( pEdCell )
        {
            if( IsSelected() )
                ClearSelection();

            // Move the caret to the cell being selected
            SetTableInsertPoint(pEdCell);
            if( bSelect )
            {
                // Save selected element data if selecting cell
                m_pSelectedTableElement = pLoElement;
            }
            // Set selection type
            // Note: Unselecting a cell will discontinue existing row or column selection
            // Also: Don't try to figure out if extending cell selection
            //   now results in a full row or column (via GetTableSelectionType())
            //   because that makes extending selections to complicated
            // Do only when needed, such as GetTableCellData() and when copying and dragNdrop
            m_TableHitType = ED_HIT_SEL_CELL;
            
            SortSelectedCells();
            return TRUE;
        } else {
            return FALSE;
        }
    }

    XP_Bool bSelRow = iHitType == ED_HIT_SEL_ROW;
    XP_Bool bSelCol = iHitType == ED_HIT_SEL_COL;

    CEditTableCellElement * pEdCell = NULL;

    if( bSelRow || bSelCol )
    {
        // Find cell closest to the given X or Y so that column or row
        //   doesn't include ROWSPAN or COLSPAN "spillover" cells
        LO_Element *pLastCellElement = NULL;
        LO_Element *pElement = lo_GetFirstCellInColumnOrRow(m_pContext, pLoElement, x, y, bSelCol, &pLastCellElement );

        int32 iColLeft, iRowTop;
        if( !pElement )
            return FALSE;
        if( bSelCol )
            // Left side defines the column
            iColLeft = pElement->lo_any.x;
        else
            // Top defines the row
            iRowTop = pElement->lo_any.y;

        XP_Bool bDone = FALSE;
        do {             
            if( pElement && pElement->lo_any.type == LO_CELL )
            {
                // Select cells in a column (cells that share left border value)
                //  or a row (all cells sharing top border value)
                if( (bSelCol && iColLeft == pElement->lo_any.x) ||
                    (bSelRow && iRowTop == pElement->lo_any.y) )
                {
                    // Select the cell - returns the corresponding Edit Cell object
                    CEditTableCellElement * pSelectedCell = SelectCell(TRUE, &pElement->lo_cell, NULL);

                    // Save the edit cell and other data 
                    if( pElement == pLoElement )
                    {
                        pEdCell = pSelectedCell;
                        m_pSelectedTableElement = pElement;
                        m_pPrevExtendSelectionCell = pElement;
                    }
                }
            }
            bDone = (pElement == NULL) || (pElement == pLastCellElement);
            pElement = pElement->lo_any.next;
        } 
        while( !bDone );

        if( IsSelected() )
            ClearSelection();
        
        if( pEdCell )
        {
            // Move caret to edit cell 
            SetTableInsertPoint(pEdCell);

            // Decide new selection type
            // Note: Don't use GetTableSelectionType() here
            if( bModifierKeyPressed && m_TableHitType != ED_HIT_NONE && 
                m_TableHitType != iHitType )
            {
                // We are appending to a previous selection
                //  that is NOT of the same type,
                //  then we can't assume total selection is all rows or all columns
                // But leave "All cells" type as is
                if( !(m_TableHitType == ED_HIT_SEL_ALL_CELLS &&
                     iHitType == ED_HIT_SEL_ROW ) )
                {
                    m_TableHitType = ED_HIT_SEL_CELL;
                }
            } else {
                m_TableHitType = iHitType;
            }

            // Be sure selected cell array is in same order
            //   as cells in table
            SortSelectedCells();

            return TRUE;
        }
#ifdef DEBUG_akkana
        printf("CEditBuffer::SelectTableElement(): Ugh -- falling through!\n");
#endif /* DEBUG_akkana */
    }

    if( iHitType == ED_HIT_SEL_ALL_CELLS )
    {
        LO_Element *pLastCellElement = NULL;
        LO_Element *pElement = lo_GetFirstAndLastCellsInTable(m_pContext, pLoElement, &pLastCellElement );
        
        XP_Bool bDone = FALSE;
        do {             
            if( pElement && pElement->lo_any.type == LO_CELL )
            {
                // Select the cell - returns the corresponding Edit Cell object
                CEditTableCellElement * pSelectedCell = SelectCell(TRUE, &pElement->lo_cell, NULL);

                // Save the edit cell and other data 
                if( pElement == pLoElement )
                {
                    pEdCell = pSelectedCell;
                    m_pSelectedTableElement = pElement;
                    m_pPrevExtendSelectionCell = pElement;
                }
            }
            bDone = (pElement == pLastCellElement);
            pElement = pElement->lo_any.next;
        } while( pElement && !bDone );
        
        if( pEdCell )
        {
            // Move caret to edit cell 
            SetTableInsertPoint(pEdCell);
            // Assume we are selected as "rows"
            m_TableHitType = ED_HIT_SEL_ALL_CELLS;
            SortSelectedCells();
            return TRUE;
        }
    }

    // Not likely - no hit types found
    return FALSE;
}

void CEditBuffer::SelectBlockOfCells(LO_CellStruct *pLastCell)
{
    if( !pLastCell )
        return;

    // Clear any existing selected cells
    for( int i = m_SelectedLoCells.Size()-1; i >= 0; i-- )
    {
        SelectCell(FALSE, m_SelectedLoCells[i], m_SelectedEdCells[i]);
    }

    if( m_pSelectedTableElement && m_pSelectedTableElement->type == LO_CELL )
    {
        LO_CellStruct *pFirstCell = (LO_CellStruct*)m_pSelectedTableElement;
        int32 xMin = min(pFirstCell->x, pLastCell->x);
        int32 yMin = min(pFirstCell->y, pLastCell->y);
        int32 xMax = max(pFirstCell->x + pFirstCell->width, pLastCell->x + pLastCell->width);
        int32 yMax = max(pFirstCell->y + pFirstCell->height, pLastCell->y + pLastCell->height);
    
        // Scan all cells in table and select those within our range
        LO_Element *pLastCell;
        LO_Element *pElement = lo_GetFirstAndLastCellsInTable(m_pContext, m_pSelectedTableElement, &pLastCell);
        while( pElement )
        {
            if( pElement->type == LO_CELL )
            {
                LO_CellStruct *pLoCell = (LO_CellStruct*)pElement;

                if( pLoCell->x >= xMin && (pLoCell->x + pLoCell->width) <= xMax &&
                    pLoCell->y >= yMin && (pLoCell->y + pLoCell->height) <= yMax )
                {
                    // Select the cell
                    SelectCell(TRUE, pLoCell);
                }
            }
            // We're done when we hit last cell
            if( pElement == pLastCell )
                break;
            pElement = pElement->lo_any.next;
        }
    }
    else 
    {
        // No previous selected cell - select just given cell
        SelectCell(TRUE, pLastCell, NULL);
    }
}

ED_HitType CEditBuffer::ExtendTableCellSelection(int32 x, int32 y)
{
    LO_Element *pLoElement = NULL;
    // Get the cell we are currently over
    ED_HitType iHitType = GetTableHitRegion(x, y, &pLoElement, TRUE);

    // Eliminate incompatable conditions to extend the selection
    if( !pLoElement ||
        !(iHitType == ED_HIT_SEL_ROW ||
          iHitType == ED_HIT_SEL_COL ||
          iHitType == ED_HIT_SEL_CELL ) ||
        !(m_TableHitType == ED_HIT_SEL_ROW ||
          m_TableHitType == ED_HIT_SEL_COL ||
          m_TableHitType == ED_HIT_SEL_CELL ) ||
        (m_TableHitType == ED_HIT_SEL_ROW &&
         iHitType == ED_HIT_SEL_COL) ||
        (m_TableHitType == ED_HIT_SEL_COL &&
         iHitType == ED_HIT_SEL_ROW) ||
        (m_TableHitType == ED_HIT_SEL_CELL &&
         iHitType != ED_HIT_SEL_CELL) )
    {
        return ED_HIT_NONE;
    }

    // Range of selected cells
    LO_Element *pFirstLoCell = NULL;
    LO_Element *pLastLoCell = NULL;

    if( m_TableHitType == ED_HIT_SEL_CELL )
    {
        // We are extending a previous cell selection
        pFirstLoCell = pLastLoCell = pLoElement;
    }
    else
    {
        // Get the last cell of row or column
        pFirstLoCell = lo_GetFirstAndLastCellsInColumnOrRow(pLoElement, &pLastLoCell, m_TableHitType == ED_HIT_SEL_COL);
    }

    if( pFirstLoCell && pFirstLoCell != m_pPrevExtendSelectionCell )
    {
        // Reselect block starting from previous start to end of current addition
        SelectBlockOfCells( (LO_CellStruct*)pLastLoCell );
        
        // Remember the first cell
        // We must use first instead of last so SelectTableElement
        //   can use the result of GetTableHitRegion (which is first cell of col or row)
        //   to suppress conflicts between setting first selection and extending selections 
        m_pPrevExtendSelectionCell = pFirstLoCell;
    }
    // Note: Don't call GetTableSelectionType() here,
    //  it complicates things to much.
    return m_TableHitType;
}

LO_Element* CEditBuffer::GetCurrentLoTableElement(ED_HitType iHitType, int32 *pX, int32 *pY)
{
    // Get current leaf element at caret or selection
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    if( ip.m_pElement && ip.m_pElement->GetLayoutElement() )
    {
        // Table select hit areas
        if( iHitType == ED_HIT_SEL_TABLE  ||
            iHitType == ED_HIT_SIZE_TABLE_WIDTH ||
            iHitType == ED_HIT_ADD_ROWS   ||
            iHitType == ED_HIT_ADD_COLS )
        {
            LO_TableStruct *pLoTable = lo_GetParentTable(m_pContext, ip.m_pElement->GetLayoutElement());
            if( pLoTable )
            {
                *pX = pLoTable->x+1;
                *pY = pLoTable->y+1;
            }
            return (LO_Element*)pLoTable;
        }

        LO_CellStruct *pLoCell = lo_GetParentCell(m_pContext, ip.m_pElement->GetLayoutElement());
        if( pLoCell )
        {
            if( iHitType == ED_HIT_SEL_CELL || iHitType == ED_HIT_SEL_ALL_CELLS )
            {
                *pX = pLoCell->x+1;
                *pY = pLoCell->y+1;
                return (LO_Element*)pLoCell;
            }
            if( iHitType == ED_HIT_SEL_ROW || iHitType == ED_HIT_SEL_COL )
            {
                // Find the first cell of the row or column
                *pX = pLoCell->x+1;
                *pY = pLoCell->y+1;
                return (LO_Element*)pLoCell;
            }
        }
    }
    return NULL;
}

// Get the corresponding CEditTableElement if LoElementType == LO_TABLE,
//   or CEditTableCellElement if LoElementType = LO_CELL
CEditElement* edt_GetTableElementFromLO_Element(LO_Element *pLoElement, int16 LoElementType)
{
    if( pLoElement &&
        (LoElementType == LO_TABLE ||
         LoElementType == LO_CELL) )
    {
        LO_Element *pTemp = pLoElement;
#if 0
// This code is much more efficient but works ONLY if 
//   we store the CEditElement pointer in the corresponding
//    LO_Element->lo_any.edit_element during table layout
        if( LoElementType == LO_TABLE )
        {
            // We want the table pointer
            if( pLoElement->type == LO_TABLE &&
                pLoElement->lo_any.edit_element )
            {
                return pLoElement->lo_any.edit_element;
            }
            else if( pLoElement->type == LO_CELL )
            {
                // We are a cell but want the parent table,
                //  so backup to find it
                while( pTemp->type != LO_TABLE )
                    pTemp = pTemp->lo_any.prev;
                if( pTemp->lo_any.edit_element )
                    return pTemp->lo_any.edit_element;
            }
        }
        else if( LoElementType == LO_CELL &&
                 pLoElement->type == LO_CELL &&
                 pLoElement->lo_any.edit_element )
        {
            // We are a cell and want the edit cell element
            return pLoElement->lo_any.edit_element;
        }
        // Fall through with "leaf searching" strategy 
        //   if LO_Element passed in is not a table or cell
#endif
        // The editor assures that every cell has an editable element in it
        // so find the first CEditElement and get CEditTable or CEditTableCell
        //   object containing it
        while ( pTemp != NULL )
        {
            // Find first cell (should hit immediately if pLoElement is a cell,
            //   else looks for first cell if pLoElement is a table
            if( pTemp->lo_any.type == LO_CELL )
            {
                LO_Element *pCellElement  = pTemp->lo_cell.cell_list;

	            // Find first editable element inside the cell
                while( pCellElement )
	            {
                    if( pCellElement->lo_any.edit_element )
                        break;
                    // TODO: CHECK IF CELL CONTAINS ANOTHER TABLE HERE?

		            pCellElement = pCellElement->lo_any.next;
	            }
                if(!pCellElement )
                {
                    // None found - should not happen
                    XP_ASSERT(FALSE);
                    return NULL;
                }
            
                if( LoElementType == LO_TABLE )
                    return pCellElement->lo_any.edit_element->GetTableIgnoreSubdoc();
                else
                    return pCellElement->lo_any.edit_element->GetTableCellIgnoreSubdoc();
            }
            
            pTemp = pTemp->lo_any.next;
        }
    }
    // We should always find an Edit element
    XP_ASSERT(FALSE);
    return NULL;
}

LO_CellStruct* CEditBuffer::GetLoCell(CEditElement *pEdElement)
{
    // Get closest leaf element so we can find the corresponding layout element
    if( !pEdElement || !pEdElement->IsInTableCell() )
        return NULL;
    if( !pEdElement->IsLeaf() )
    {
        pEdElement = pEdElement->FindNextElement(&CEditElement::FindLeafAll,0 );
    }
    if( pEdElement && pEdElement->Leaf()->GetLayoutElement() )
    {    
        return lo_GetParentCell(m_pContext, pEdElement->Leaf()->GetLayoutElement());
    }
    return NULL;
}

CEditTableElement* CEditBuffer::SelectTable(XP_Bool bSelect, LO_TableStruct *pLoTable,
                              CEditTableElement *pEdTable)
{
    // We can allow NULL pointer for table if we are just unselecting existing table
    if( bSelect && !pLoTable )
    {
        XP_ASSERT(FALSE);
        return NULL;
    }
    // Assume we are deselecting current table if no pointers supplied
    if( !bSelect && !pLoTable && !pEdTable )
    {
        pLoTable = m_pSelectedLoTable;
        pEdTable = m_pSelectedEdTable;
    }
    if( !pLoTable )
        return NULL;

    //XP_Bool bWasSelected = bSelect ? pLoTable->ele_attrmask & LO_ELE_SELECTED : FALSE;
    XP_Bool bWasSelected = pLoTable->ele_attrmask & LO_ELE_SELECTED;

    // Clear existing selected table if different
    if( m_pSelectedLoTable && pLoTable != m_pSelectedLoTable )
    {
        m_pSelectedLoTable->ele_attrmask &= ~LO_ELE_SELECTED;

        FE_DisplayEntireTableOrCell(m_pContext, (LO_Element*)m_pSelectedLoTable);
        m_pSelectedLoTable = NULL;

        // We should always have the matching CEditTableElement
        XP_ASSERT(m_pSelectedEdTable);

        m_pSelectedEdTable = NULL;
    }
    if( !pLoTable )
        return NULL;

//    XP_TRACE(("Table width = %d", pLoTable->width));

    // Get Edit table element if not supplied
    if( !pEdTable )
        pEdTable = (CEditTableElement*)edt_GetTableElementFromLO_Element( (LO_Element*)pLoTable, LO_TABLE );

    XP_ASSERT(pEdTable);
    if( !pEdTable )
        return NULL;

    XP_Bool bChanged;

    if( bSelect )
    {
        // Select the table and save pointers
        pLoTable->ele_attrmask |= LO_ELE_SELECTED;
        m_pSelectedEdTable = pEdTable;
        m_pSelectedLoTable = pLoTable;
        bChanged = !bWasSelected;
    } 
    else 
    {
        // Unselect the table
        pLoTable->ele_attrmask &= ~LO_ELE_SELECTED;
        bChanged = bWasSelected;
        m_pSelectedLoTable = NULL;
        m_pSelectedEdTable = NULL;
    }        

    // Redisplay only if changed from previous state
    if( bChanged )
        FE_DisplayEntireTableOrCell(m_pContext, (LO_Element*)pLoTable);
    return pEdTable;
}

CEditTableCellElement* CEditBuffer::SelectCell(XP_Bool bSelect, LO_CellStruct *pLoCell, CEditTableCellElement *pEdCell)
{
    // We must have one type of cell or the other
    XP_ASSERT(pLoCell || pEdCell);
    if( !pLoCell )
    {
        pLoCell = GetLoCell((CEditElement*)pEdCell);
        if( !pLoCell )
            return NULL;
    }

    XP_Bool bWasSelected = (XP_Bool)pLoCell->ele_attrmask & LO_ELE_SELECTED;

    // Get corresponding Edit cell if not supplied
    if( !pEdCell )
        pEdCell = (CEditTableCellElement*)edt_GetTableElementFromLO_Element( (LO_Element*)pLoCell, LO_CELL );

    if(bSelect && !pEdCell)
        return NULL;


    pEdCell->SetSelected(bSelect);

    XP_Bool bChanged;
    int iSize = m_SelectedEdCells.Size();

    if( bSelect )
    {
        // Select the cell
        pLoCell->ele_attrmask |= LO_ELE_SELECTED;

        // Add pointers to lists if not already in the respective list
        int i;
        CEditTableCellElement *pNewEdCell = pEdCell;
        LO_CellStruct *pNewLoCell = pLoCell;
        for( i = 0; i < iSize; i++ )
        {
            if( m_SelectedEdCells[i] == pEdCell )
            {
                pNewEdCell = NULL;
                // Be sure the Layout array matches the EditArray in size
                // A bit risky, but most efficient for relayout usage
                m_SelectedLoCells.SetSize(iSize);
                m_SelectedLoCells[i] = pLoCell;
                break;
            }
        }
        for( i = 0; i < m_SelectedLoCells.Size(); i++ )
        {
            if( m_SelectedLoCells[i] == pLoCell )
            {
                pNewLoCell = NULL;
                break;
            }
        }
        if( pNewEdCell && pNewLoCell )
        {
            // Returns index to last element, add 1 for size
            iSize = m_SelectedEdCells.Add(pNewEdCell) + 1;
            m_SelectedLoCells.Add(pNewLoCell);
        }

#ifdef DEBUG
        // TRACE CELL
//        if( bSelect )
//            XP_TRACE(("Cell: Row= %d, X= %d, Y= %d, width= %d", pEdCell->GetRow(), pEdCell->GetX(), pEdCell->GetY(), pLoCell->width));
#endif
        bChanged = !bWasSelected;
    } 
    else 
    {
        // Delete existing cell from list
        iSize = m_SelectedEdCells.Delete(pEdCell) + 1;
        m_SelectedLoCells.Delete(pLoCell);

        // Unselect the cell, including "special" selection style
        pLoCell->ele_attrmask &= ~(LO_ELE_SELECTED | LO_ELE_SELECTED_SPECIAL);
        bChanged = bWasSelected;
    }        

#ifdef DEBUG
    if( iSize != m_SelectedLoCells.Size() )
        XP_TRACE(("Edit Array Size = %d, LO Array Size = %d",iSize, m_SelectedLoCells.Size()));
#endif

    // Redisplay only if changed from previous state
    //XP_TRACE(("Display Cell - Select=%d, Changed=%d",bSelect,bChanged));
    if( bChanged )
        FE_DisplayEntireTableOrCell(m_pContext, (LO_Element*)pLoCell);
    return pEdCell;    
}

void CEditBuffer::DisplaySpecialCellSelection( CEditTableCellElement *pFocusCell,  EDT_TableCellData *pCellData )
{
    XP_Bool bCellFound = FALSE;
    intn iSelectedCount = m_SelectedLoCells.Size(); 

    // PROBLEM: If we don't clear table selection, we end up with table and cell
    //  selected when examining table properties. Simplest solution is to 
    //  clear table selection
    if( m_pSelectedLoTable )
        SelectTable(FALSE);        

    for( intn i = 0; i < iSelectedCount; i++ )
    {
        LO_CellStruct *pLoCell = m_SelectedLoCells[i];
        CEditTableCellElement *pEdCell = m_SelectedEdCells[i];
        XP_ASSERT(pEdCell);
        XP_Bool bWasSpecial = (XP_Bool)pLoCell->ele_attrmask & LO_ELE_SELECTED_SPECIAL;

        if( pEdCell == pFocusCell )
        {
            // Remove the special selection from the focus cell
            if( bWasSpecial )
            {
                pLoCell->ele_attrmask &= ~LO_ELE_SELECTED_SPECIAL;
                FE_DisplayEntireTableOrCell(m_pContext, (LO_Element*)pLoCell);
            }
            bCellFound = TRUE;
        }
        else if( !bWasSpecial )
        {
            // Set special selection attribute in addition to existing "normal" selection
            pLoCell->ele_attrmask |= LO_ELE_SELECTED_SPECIAL;
            FE_DisplayEntireTableOrCell(m_pContext, (LO_Element*)pLoCell);
        }
    }
    // Select the focus cell if it was not found to be
    //  selected above
    if( pFocusCell && !bCellFound )
    {
        // If no other cells were selected,
        //   save single-cell selection type
        if( iSelectedCount == 0 )
            m_TableHitType = ED_HIT_SEL_CELL;
        SelectCell(TRUE, NULL, pFocusCell);
    }

    // Update cell data. Used primarily when EDT_StartSpecialCellSelection()
    //   is called when no cells were previously selected
    if( pCellData )
    {
        pCellData->iSelectionType = m_TableHitType;
        pCellData->iSelectedCount = m_SelectedLoCells.Size();
    }
}

// Start multi-cell selection highlighting with cell containing the caret
void CEditBuffer::StartSpecialCellSelection(EDT_TableCellData *pCellData)
{
    CEditTableCellElement* pEdCell = m_pCurrent->GetTableCellIgnoreSubdoc();
    if( pEdCell )
        DisplaySpecialCellSelection( pEdCell, pCellData );
}

static void edt_ClearSpecialCellSelection(MWContext *pContext, LO_CellStruct *pLoCell)
{
    if( pContext && pLoCell && (pLoCell->ele_attrmask & LO_ELE_SELECTED_SPECIAL) )
    {
        pLoCell->ele_attrmask &= ~LO_ELE_SELECTED_SPECIAL;
        FE_DisplayEntireTableOrCell(pContext, (LO_Element*)pLoCell);
    }
}

void CEditBuffer::ClearSpecialCellSelection()
{
    for( intn i = 0; i < m_SelectedLoCells.Size(); i++ )
    {
        edt_ClearSpecialCellSelection(m_pContext, m_SelectedLoCells[i]);
    }
}

// Sort cell lists so simple traversal through array 
//  moves row-wise from upper left corner to lower right corner of table
void CEditBuffer::SortSelectedCells()
{
    int iCount = m_SelectedLoCells.Size();
    if( iCount <= 1 )
        return;
    
    // Optimization for only two cells selected
    if( iCount == 2 )
    {
        if(m_SelectedLoCells[1]->x < m_SelectedLoCells[0]->x &&
           m_SelectedLoCells[1]->y <= m_SelectedLoCells[0]->y )
        {
            LO_CellStruct *pLoCell = m_SelectedLoCells[0];
            m_SelectedLoCells[0] = m_SelectedLoCells[1];
            m_SelectedLoCells[1] = pLoCell;   

            CEditTableCellElement *pEdElement = m_SelectedEdCells[0];
            m_SelectedEdCells[0] = m_SelectedEdCells[1];
            m_SelectedEdCells[1] = pEdElement;   
        }
        return;
    }

    XP_ASSERT(iCount == m_SelectedEdCells.Size());

    LO_CellStruct **pNewLoCells = (LO_CellStruct**)XP_ALLOC(iCount*sizeof(LO_CellStruct*));
    if( !pNewLoCells )
        return;

    CEditTableCellElement **pNewEdCells = (CEditTableCellElement**)XP_ALLOC(iCount*sizeof(CEditTableCellElement*));
    if( !pNewEdCells )
    {
        XP_FREE(pNewLoCells);
        return;
    }

    LO_Element *pLoElement = (LO_Element*)m_SelectedLoCells[0];
    LO_Element *pFirstSelected = NULL;

    // Scan backwards until we hit the table element to find-selected cell
    do {
        if( pLoElement->type == LO_CELL && (pLoElement->lo_cell.ele_attrmask & LO_ELE_SELECTED) )
            pFirstSelected = pLoElement;

        pLoElement = pLoElement->lo_any.prev;
    } while (pLoElement->type != LO_TABLE ); 
    
    
    if( pFirstSelected ) // Should never fail
    {
        pLoElement = pFirstSelected;
        int i = 0;
        do {
            if( pLoElement->lo_cell.ele_attrmask & LO_ELE_SELECTED )
            {
                // Build new lists of both LO and EDIT elements
                pNewLoCells[i] = (LO_CellStruct*)pLoElement;
                
                // Find the corresponding Edit element from the old lists
                int iIndex = m_SelectedLoCells.Find(pLoElement);
                //XP_ASSERT(iIndex >= 0);
                if( iIndex >= 0 )
                    pNewEdCells[i] = m_SelectedEdCells[iIndex];
                else
                    // THIS IS A PROBLEM -- WE SHOULD NEVER GET HERE
                    pNewEdCells[i] = (CEditTableCellElement*)edt_GetTableElementFromLO_Element(pLoElement, LO_CELL);
                i++;
            }
            pLoElement = pLoElement->lo_any.next;
        } while (pLoElement && pLoElement->type == LO_CELL ); 
    
        // Replace old lists with new sorted lists
        for( i = 0; i < iCount; i++ )
        {
            m_SelectedLoCells[i] = pNewLoCells[i];
            m_SelectedEdCells[i] = pNewEdCells[i];
        }
    }
    XP_FREE(pNewLoCells);
    XP_FREE(pNewEdCells);
}

void CEditBuffer::ClearTableAndCellSelection()
{
    m_pSelectedTableElement = NULL;
    m_pPrevExtendSelectionCell = NULL;
    m_TableHitType = ED_HIT_NONE;

    int iEdSize = m_SelectedEdCells.Size();
    int iLoSize = m_SelectedLoCells.Size();
    if( m_pSelectedEdTable == 0 &&
        iEdSize == 0 )
        // Nothing to do
        return;

    SelectTable(FALSE);

    // These arrays should always be in synch
    //XP_ASSERT( iEdSize == iLoSize );
    if( iEdSize != iLoSize )
        XP_TRACE(("EdSize=%d, LoSize=%d", iEdSize, iLoSize));

    // Remove selection for all existing cells
    //  (From the end is quicker - no pointer copying needed)
    for( int i = iEdSize-1; i >= 0; i-- )
    {
        SelectCell(FALSE, m_SelectedLoCells[i], m_SelectedEdCells[i]);
    }
    
    // Array of pointers should now be empty
    XP_ASSERT(m_SelectedLoCells.Size() == 0);
    XP_ASSERT(m_SelectedEdCells.Size() == 0);
}

#if 0   
// NOT USED???
void CEditBuffer::ClearCellAttrmask( LO_Element *pLoElement, uint16 attrmask )
{
    if( pLoElement )
    {
        while( pLoElement && pLoElement->type != LO_LINEFEED )
        {
            if( pLoElement->type == LO_CELL )
            {
                XP_Bool bWasSet = pLoElement->lo_cell.ele_attrmask & attrmask;
                // Clear the mask bit(s)
                pLoElement->lo_cell.ele_attrmask &= ~attrmask;
                // Tell front end to invalidate cell for redraw
                if( bWasSet )
                    FE_DisplayEntireTableOrCell(m_pContext, pLoElement);
            }
            pLoElement = pLoElement->lo_any.next;
        }
    }
}
#endif

// Should be called after positioning caret on object
//   clicked on for context menu (right button on Windows)
void CEditBuffer::ClearCellSelectionIfNotInside()
{
    // Not in a table - clear table selection
    if( !IsInsertPointInTable() )
    {
        ClearTableAndCellSelection();
        return;
    }
    // Get current leaf element at caret or selection
    CEditInsertPoint ip;
    GetTableInsertPoint(ip);
    if( ip.m_pElement )
    {
        // Find the LO cell element and check if it is selected
        //  (No cell found if not within a table)
        LO_CellStruct *pLoCell = GetLoCell(ip.m_pElement);
        if( pLoCell && !(pLoCell->ele_attrmask & LO_ELE_SELECTED) )
        {
            ClearTableAndCellSelection();
        }
    }
}

void CEditBuffer::ClearTableIfContainsElement(CEditElement *pElement)
{
    if( pElement )
    {
        // Depend on member flag being set correctly
        if( pElement->IsSelected() )
        {
            if( pElement->IsTable() )
            {
                SelectTable(FALSE, NULL, (CEditTableElement*)pElement);
                return;
            }
            else if( pElement->IsTableCell() )
            {
                SelectCell(FALSE, NULL, (CEditTableCellElement*)pElement);
                return;
            }
        }
        // We are not a cell or table - check if containing layout cell or table is selected
        LO_CellStruct *pLoCell = GetLoCell(pElement);
        if( pLoCell )
        {
            // If selected, unselect
            if( pLoCell->ele_attrmask & LO_ELE_SELECTED )
            {
                SelectCell(FALSE, pLoCell);
            } else {
                // Cell not selected - check for table
                LO_TableStruct *pLoTable = lo_GetParentTable(m_pContext, (LO_Element*)pLoCell);
                if( pLoTable && pLoTable->ele_attrmask & LO_ELE_SELECTED )
                {
                    SelectTable(FALSE, pLoTable);
                }
            }
        }
    }
}

// Call this the first time, if result is TRUE then call 
//   GetNextCellSelection() and repeat formating or other action 
//    until it returns null. 
// Current insert point is saved during first time,
//   and restored after no more cells are left to process
XP_Bool CEditBuffer::GetFirstCellSelection(CEditSelection& selection)
{
    CEditTableCellElement *pCell = NULL;

    m_iNextSelectedCell = 0;

    if( m_pSelectedEdTable )
    {
        pCell = m_pSelectedEdTable->GetFirstCell();
    }
    else if( m_SelectedEdCells.Size() )
    {
        // Get first "selected" cell
        pCell = m_SelectedEdCells[0];

        // Set for next selected item
        m_iNextSelectedCell = 1;
    }
    if( pCell )
    {
        // Just return pointers and offsets of the cell contents as a selection
        pCell->GetAll(selection);
        selection.m_bFromStart = TRUE;
        EphemeralToPersistent(selection);
        return TRUE;
    }
    return FALSE;
}

XP_Bool CEditBuffer::GetNextCellSelection(CEditSelection& selection)
{
    CEditTableCellElement *pCell = NULL;

    if( m_pSelectedEdTable )
    {
        // Get next cell in entire table
        pCell = m_pSelectedEdTable->GetNextCellInTable();
    } 
    else if( m_iNextSelectedCell && m_iNextSelectedCell < m_SelectedEdCells.Size() )
    {
        // Get next "selected" cell
        pCell = m_SelectedEdCells[m_iNextSelectedCell++];
    }
    if( pCell )
    {
        pCell->GetAll(selection);
        selection.m_bFromStart = TRUE;
        EphemeralToPersistent(selection);
        return TRUE;
    }
    return FALSE;
}

// Extracted from old version of SelectTableCell
void CEditBuffer::SelectCellContents(CEditTableCellElement *pCell)
{

    CEditSelection selection;
    if ( pCell ) 
    {
        pCell->GetAll(selection);
        // KLUDGE ALERT: Without this, attempts to set cell data
        //  go to next cell. As it is, there is a bug in which
        //  caret is in next cell after unselecting this cell
        selection.m_bFromStart = TRUE;
        EphemeralToPersistent(selection);
        SetSelection(selection);
    }
}


CEditTableCellElement *CEditBuffer::GetFirstSelectedCell()
{
    CEditTableCellElement *pCell = NULL;

    m_iNextSelectedCell = 0;

    if( m_pSelectedEdTable )
    {
        pCell = m_pSelectedEdTable->GetFirstCell();
    }
    else if( m_SelectedEdCells.Size() )
    {
        // Get first "selected" cell
        pCell = m_SelectedEdCells[0];

        // Set for next selected item
        m_iNextSelectedCell = 1;
    }
    return pCell;
}

CEditTableCellElement *CEditBuffer::GetNextSelectedCell(intn* pRowCounter)
{
    CEditTableCellElement *pCell = NULL;

    if( m_pSelectedEdTable )
    {
        // Get next cell in entire table
        pCell = m_pSelectedEdTable->GetNextCellInTable(pRowCounter);
    } 
    else if( m_iNextSelectedCell && m_iNextSelectedCell < m_SelectedEdCells.Size() )
    {
        if( m_iNextSelectedCell < 1 )
            return GetFirstSelectedCell(); 

        // Get next "selected" cell
        pCell = m_SelectedEdCells[m_iNextSelectedCell];
        if( pRowCounter )
        {
            intn iPrevRow = m_SelectedEdCells[m_iNextSelectedCell - 1]->GetRow();
            // Increment the row counter if supplied
            intn iRow =  pCell->GetRow();
            if( iRow != iPrevRow )
                (*pRowCounter)++;
        }
        m_iNextSelectedCell++;
    }
    return pCell;
}

// Dynamic object sizing

ED_SizeStyle CEditBuffer::CanSizeObject(LO_Element *pLoElement, int32 xVal, int32 yVal)
{
    // Table and Cells are special case - use more complicated hit testing
    if( pLoElement && (pLoElement->type == LO_TABLE || pLoElement->type == LO_CELL) )
    {
        ED_HitType iHitType = GetTableHitRegion(xVal, yVal, &pLoElement, FALSE);
        switch( iHitType )
        {
            case ED_HIT_SIZE_COL:
            case ED_HIT_SIZE_TABLE_WIDTH:
                return ED_SIZE_RIGHT;
            case ED_HIT_SIZE_ROW:
            case ED_HIT_SIZE_TABLE_HEIGHT:
                return ED_SIZE_BOTTOM;
            case ED_HIT_ADD_COLS:
                return ED_SIZE_ADD_COLS;
            case ED_HIT_ADD_ROWS:
                return ED_SIZE_ADD_ROWS;
            default:
                return 0;
        }
    } 

    if( !pLoElement )
    {
#ifdef LAYERS
		pLoElement = LO_XYToElement(m_pContext, xVal, yVal, 0);
#else
		pLoElement = LO_XYToElement(m_pContext, xVal, yVal);
#endif
    }
    if( !pLoElement || pLoElement->lo_any.edit_offset < 0)
        return 0;

    XP_Bool bCanSizeWidth = FALSE;
    XP_Bool bCanSizeHeight = FALSE;
    XP_Bool bInHRule = pLoElement->type == LO_HRULE;
    XP_Bool bIsIcon = FALSE;

    LO_Any_struct * pAny = (LO_Any_struct*) pLoElement;

    // NOTE: NO CONVERSION DONE -- ASSUMES FE IS IN PIXELS
    int32 iXoffset = xVal - (pAny->x + pAny->x_offset);
    int32 iYoffset = yVal - (pAny->y + pAny->y_offset);
    int32 bottom_limit = pAny->height - ED_SIZING_BORDER;
    int32 right_limit =  pAny->width - ED_SIZING_BORDER;

    // Look for the elements that can be sized:
    switch ( pLoElement->type )
    {
        case LO_IMAGE:       // 4
        {
      		CEditElement * pElement = pLoElement->lo_any.edit_element;
            if( !pElement ){
                return 0;
            }
            if( pElement->IsIcon() ){
                 //  Only allow sizing width or height
                 //  if it already has the respective attribute set
                bCanSizeWidth = pElement->GetWidth();
                bCanSizeHeight = pElement->GetHeight();
                bIsIcon = TRUE;
                break;
            }
            if( !pElement->IsImage() )
                return 0;
            // Adjust for image border
            iXoffset -= ((LO_ImageStruct*)pAny)->border_width;
            iYoffset -= ((LO_ImageStruct*)pAny)->border_width;
            // Fall through to allow sizing both width and height
        }
        case LO_HRULE:       // 3
            bCanSizeWidth = TRUE;
            bCanSizeHeight = TRUE;
            break;
        default:
            return 0;
    }

    // Can't size anything - get out now!
    if( !bCanSizeWidth && !bCanSizeHeight )
        return 0;

    if( iXoffset < 0 || iYoffset < 0 )
        return 0;

    // Note that TOP RIGHT is prefered in the case
    //   of small object (overlapping hits regions),
    //   and left/right sides are favored over bottom corners
    if( iXoffset >= right_limit )
    {
        if( !bCanSizeHeight || bInHRule )
        {
            return ED_SIZE_RIGHT;
        }
        if( iYoffset <= ED_SIZING_BORDER )
            return ED_SIZE_TOP_RIGHT;
        if( iYoffset < bottom_limit )
            return ED_SIZE_RIGHT;

        return ED_SIZE_BOTTOM_RIGHT;

    } else if( iXoffset <= ED_SIZING_BORDER )
    {
        if( !bCanSizeHeight || bInHRule )
            return ED_SIZE_LEFT;

        if( iYoffset <= ED_SIZING_BORDER )
            return ED_SIZE_TOP_LEFT;

        if( iYoffset < bottom_limit )
            return ED_SIZE_LEFT;

        return ED_SIZE_BOTTOM_LEFT;

    } else if( bCanSizeHeight  )
    {
        if( iYoffset >= bottom_limit )
            return ED_SIZE_BOTTOM;

        if( iYoffset <= ED_SIZING_BORDER )
            return ED_SIZE_TOP;
    }
    return 0;
}

ED_SizeStyle CEditBuffer::StartSizing(LO_Element *pLoElement, int32 xVal, int32 yVal,
                                      XP_Bool bLockAspect, XP_Rect *pRect){
    XP_ASSERT(pRect);

    if( m_pSizingObject ){
        // Improbable, but what the heck
        delete m_pSizingObject;
    }
    // Unselect table or cell(s)
    ClearTableAndCellSelection();

    // Shouldn't be here without this, but check anyway
    int iStyle = CanSizeObject(pLoElement, xVal, yVal);

    if( iStyle ){
        m_pSizingObject = new CSizingObject();
        if( m_pSizingObject ){
            if( m_pSizingObject->Create(this, pLoElement, iStyle,
                                         xVal, yVal, bLockAspect, pRect) ){
                return iStyle;
            } else {
                delete m_pSizingObject;
                m_pSizingObject = NULL;
            } 
        }
    }
    return 0;
}

XP_Bool CEditBuffer::GetSizingRect(int32 xVal, int32 yVal,
                                   XP_Bool bLockAspect, XP_Rect *pRect){
    XP_ASSERT(pRect);
    if( m_pSizingObject && pRect ){
        return m_pSizingObject->GetSizingRect(xVal, yVal, bLockAspect, pRect);
    }
    return FALSE;
}

void CEditBuffer::EndSizing(){
    if( ! m_pSizingObject ){
        return;
    }
    m_pSizingObject->ResizeObject();
    
    delete m_pSizingObject;
    m_pSizingObject = NULL;

    ResumeAutoSave();
}

void CEditBuffer::CancelSizing(){
    if( m_pSizingObject )
    {
        // Erase the drawing of added rows or columns
        m_pSizingObject->EraseAddRowsOrCols();
        delete m_pSizingObject;
        m_pSizingObject = NULL;
    }
    ResumeAutoSave();
}

#ifdef EDITOR_JAVA
EditorPluginManager CEditBuffer::GetPlugins(){
    return GetCommandLog()->GetPlugins();
}
#endif


//-----------------------------------------------------------------------------
// Spellcheck stuff
//-----------------------------------------------------------------------------

XP_Bool CEditBuffer::FindNextMisspelledWord( XP_Bool bFirst, 
        XP_Bool bSelect, CEditLeafElement** ppWordStart ){
    CEditLeafElement *pInsertPoint;
    XP_Bool bSingleItem;
    ElementOffset iOffset;

    if( ppWordStart && *ppWordStart ){
        pInsertPoint = *ppWordStart;
        iOffset = 0;
    }
    else {
        bSingleItem = GetPropertyPoint( &pInsertPoint, &iOffset );
    }
    while( pInsertPoint->IsMisspelled() && ! bFirst ){
        pInsertPoint = pInsertPoint->NextLeaf();
        if ( pInsertPoint && pInsertPoint->IsFirstInContainer() ) {
            break;
        }
    }
    while( pInsertPoint && !pInsertPoint->IsMisspelled() ){
        pInsertPoint = pInsertPoint->NextLeaf();
    }

    if( pInsertPoint ){
        CEditLeafElement *pEndInsertPoint = pInsertPoint;
        while( pEndInsertPoint && pEndInsertPoint->IsMisspelled() ){
            CEditLeafElement *pNext = pEndInsertPoint->LeafInContainerAfter();
            if ( pNext != NULL )
                pEndInsertPoint = pNext;
            else
                break;
        }

        if (pEndInsertPoint && pEndInsertPoint->IsMisspelled()){
            iOffset = pEndInsertPoint->GetLen();
        }
        else {
            iOffset = 0;
        }

        if( bSelect ){
            // force the window to scroll to the beginning of the selection.
            SetInsertPoint( pInsertPoint, 0, FALSE );
            SelectRegion( pInsertPoint, 0, pEndInsertPoint, iOffset, TRUE, TRUE );
        }
    }

    if( ppWordStart != 0 ) *ppWordStart = pInsertPoint;

    return pInsertPoint != 0;
}

static XP_HUGE_CHAR_PTR GetMisspelledWordText( CEditLeafElement *pLeaf ){
    CStreamOutMemory m;
    while( pLeaf && pLeaf->IsMisspelled() && pLeaf->IsText() ){
        m.Write( pLeaf->Text()->GetText(), pLeaf->Text()->GetLen() );
        pLeaf = pLeaf->LeafInContainerAfter();
    }
    // zero terminate the buffer
    m.Write( "", 1 );
    return m.GetText();
}

static void IgnoreMisspelledWord( CEditLeafElement *pLeaf ){
    while( pLeaf && pLeaf->IsMisspelled() && pLeaf->IsText() ){
        pLeaf->Text()->m_tf &= ~TF_SPELL;
        pLeaf = pLeaf->LeafInContainerAfter();
    }
}

static void ReplaceMisspelledWord( CEditLeafElement *pStart, char *pNewWord ){
    CEditLeafElement *pPrev;

    pStart->Text()->m_tf &= ~TF_SPELL;

    // hack to keep us from deleting the entire word
    if( pNewWord && *pNewWord ){
        pStart->Text()->SetText( pNewWord );
    }
    else {
        pStart->Text()->SetText(" ");
    }

    pStart = pStart->LeafInContainerAfter();
    while( pStart && pStart->IsMisspelled() ){
        pPrev = pStart;
        pStart = pStart->LeafInContainerAfter();
        pPrev->Unlink();
        delete pPrev;
    }
}

void CEditBuffer::IterateMisspelledWords( EMSW_FUNC eFunc, char* pOldWord, 
        char* pNewWord, XP_Bool bAll ){

    CEditLeafElement *pWordStart, *pBegin, *pEnd;
    XP_Bool bFirst = TRUE;

    // 
    // Find the next word (including the one we might be on), but don't select
    //  it and return the word
    //

    
    pBegin = 0;
    pEnd = 0;
    pWordStart = 0;
    do {
        FindNextMisspelledWord( bFirst, FALSE, &pWordStart );
        if( bFirst ){
            bFirst = FALSE;
            ClearSelection(TRUE,TRUE);       
        }
        if ( pWordStart ) {
            XP_HUGE_CHAR_PTR pThisWord = GetMisspelledWordText( pWordStart );

            // Ignore all words if no specific word is specified.
            if( (eFunc == EMSW_IGNORE && pOldWord == NULL) ||
                XP_STRCMP( (char*) pThisWord, pOldWord ) == 0 ){
                // do the actual replacing here
                if( pBegin == 0 ) pBegin = pWordStart;
                pEnd = pWordStart;

                switch( eFunc ){
                case EMSW_IGNORE:
                    IgnoreMisspelledWord( pWordStart );
                    break;

                case EMSW_REPLACE:
                    ReplaceMisspelledWord( pWordStart, pNewWord );
                    break;
                }
            }

            XP_HUGE_FREE( pThisWord );
        }
    } while( bAll && pWordStart );

    // if we actually did some work
    if( pBegin && pEnd ) {
        Relayout( pBegin, 0, pEnd );
    }
}

void CEditBuffer::SetBaseTarget(char* pTarget){
    XP_FREEIF(m_pBaseTarget);
    m_pBaseTarget = pTarget ? XP_STRDUP(pTarget) : 0;
}

char* CEditBuffer::GetBaseTarget(){
    return m_pBaseTarget;
}




XP_Bool
CEditBuffer::ReplaceOnce( char *pReplaceText, XP_Bool bRelayout, XP_Bool bReduce)
{/* This utility function assumes that the text 
    you want replaced has been selected already. */

#ifdef FORMATING_FIXED
    CEditSelection selection;
    EDT_CharacterData  *formatting;  /* This will hold the formatting of the selected text */
	int i;

    /* Get the formatting of the selected text */
    formatting = GetCharacterDataSelection(0, selection);

    /* Paste the new text over the selected text */
    PasteText( pReplaceText, FALSE, FALSE, bRelayout , bReduce);

    /* select the new text */
    for (i = XP_STRLEN( pReplaceText ); i > 0; i-- )
    	SelectPreviousChar();

    /* apply the old formatting */
    SetCharacterData( formatting );
#else
    /* Paste the new text over the selected text */
    PasteText( pReplaceText, FALSE, FALSE, bRelayout , bReduce);
#endif

    return TRUE;
}



void
CEditBuffer::ReplaceLoop(char *pReplaceText, XP_Bool bReplaceAll,
                         char *pTextToLookFor, XP_Bool bCaseless,
                         XP_Bool bBackward, XP_Bool /* bDoWrap */ )
{
    LO_Element *start_ele_loc, *end_ele_loc,
               *original_start_ele_loc, *original_end_ele_loc;
    int32       start_pos, end_pos,
                original_start_pos, original_end_pos,
                tlx, tly;
    CL_Layer*   layer;/* this will be ignored */
    CEditLeafElement* origEditElement;//used for replace all to move insertion point back to the proper location

    BeginBatchChanges(kGroupOfChangesCommandID);


    /* Get the original starting and finising elements */
    LO_GetSelectionEndpoints( m_pContext,
							  &original_start_ele_loc, /* The element the selection begins in */
							  &original_end_ele_loc,   /* The element the selection ended in */
							  &original_start_pos,     /* Index into the starting element */
							  &original_end_pos,    /* Index into the ending element */
							  &layer);                 /* this will be ignored */

    if ( bReplaceAll )
    {
        origEditElement = this->m_pCurrent;

        NavigateDocument( FALSE, FALSE );
        LO_GetSelectionEndpoints( m_pContext,
							  &start_ele_loc, /* The element the selection begins in */
							  &end_ele_loc,   /* The element the selection ended in */
							  &start_pos,     /* Index into the starting element */
							  &end_pos,       /* Index into the ending element */
							  &layer);                 /* this will be ignored */
    }
    else
    {
        start_ele_loc = original_start_ele_loc;
        end_ele_loc = original_end_ele_loc;
        start_pos = original_start_pos;
        end_pos = original_end_pos;
    }

    XP_Bool     done = FALSE, Wrapped = FALSE;
    //if we are replacing all, start at top of doc. remember original insertion point.
    while ( !done )
    {
        XP_Bool found = LO_FindText(m_pContext, pTextToLookFor, &start_ele_loc,
                            &start_pos, &end_ele_loc, &end_pos, !bCaseless, !bBackward);
        if ( found )
        {
			LO_SelectText( m_pContext, start_ele_loc, start_pos,
				           end_ele_loc, end_pos, &tlx, &tly);

            ReplaceOnce( pReplaceText , !bReplaceAll, !bReplaceAll); //do not relayout if we are replacing all!
            //ReplaceOnce( pReplaceText , TRUE, TRUE); 
            if ( bReplaceAll )
            {/* We need to reset our starting position to our previous ending position */
                //do not care to refresh anything now!!
                m_bLayoutBackpointersDirty = FALSE;//trust me
                start_ele_loc =  end_ele_loc;
                start_pos = end_pos;
            }
            else
                done=TRUE;
        }
        else if (!bReplaceAll) /* found == FALSE  and we are not replacing all.*/
        {/* If found == FALSE, and there isn't a selection, 
            then we need to wrap to check the portion before the insertion point. */
            if ( !Wrapped )/* We only want to Wrap ONCE */
            {
                start_pos = 0;
                end_pos = original_start_pos;
                start_ele_loc = NULL;
                end_ele_loc = original_start_ele_loc;
                Wrapped = TRUE;
            }
            else
                done = TRUE;
        }
        else
        {
            done = TRUE;
        }
    }
    if (bReplaceAll) //need to relayout now that we are done.
    {
        //set insertion point back to its old location, valid, or not.
        /*call Reflow( CEditElement* pStartElement,
                            int iEditOffset,
                            CEditElement *pEndElement,
                            intn relayoutFlags ){*/
        m_bLayoutBackpointersDirty = TRUE;//trust me

        SetInsertPoint(origEditElement, original_start_pos, FALSE);
        Reduce(this->m_pRoot->GetFirstMostChild());//we must finish what we have begun...
        // relayout the stream
	    Relayout(this->m_pRoot->GetFirstMostChild(), 0);
    }
}




void CEditBuffer::ReplaceText( char *pReplaceText, XP_Bool bReplaceAll,
								char *pTextToLookFor, XP_Bool bCaseless,
								XP_Bool bBackward, XP_Bool bDoWrap )
{
	EDT_CharacterData *formatting;
    CEditSelection selection;
	int i;

    BeginBatchChanges(kGroupOfChangesCommandID);
    // This replaces the selected "find" text just fine,
    //  but it ignores character attributes of replaced text
    if( IsSelected() )
        formatting = GetCharacterDataSelection(0, selection);
    else
    	formatting = NULL;

    PasteText(pReplaceText, FALSE, FALSE, TRUE, TRUE); 
	for (i = XP_STRLEN( pReplaceText ); i > 0; i-- )
	    SelectPreviousChar();
    SetCharacterData( formatting );
    
    if ( bReplaceAll )
    {
    	/* normal local variables */
		int32 		start_position, end_position;
		LO_Element	*start_ele_loc = NULL, *end_ele_loc = NULL, 
					*final_done_loc, *current_done_location;
    	MWContext	*tempContext;
    	CL_Layer	*layer;	// this will be ignored
     	XP_Bool		found;

   		/* use non-editor LO_* call */
		LO_GetSelectionEndpoints( m_pContext,
								&start_ele_loc,
								&end_ele_loc,
								&start_position,
								&end_position,
								&layer);
		final_done_loc = start_ele_loc;
		current_done_location = NULL;
		
		/* brade: as of today; the bDoWrap is not working correct so force it off for now */
		bDoWrap = FALSE;
		
   		do {
			start_ele_loc = end_ele_loc;
			end_ele_loc = current_done_location;
    		tempContext = m_pContext;
	    	found = LO_FindGridText( m_pContext,
								&tempContext,
								pTextToLookFor,
								&start_ele_loc,
								&start_position,
								&end_ele_loc,
								&end_position,
								!bCaseless,
								!bBackward);
			if ( found )
			{
				int32		tlx, tly;
				
				LO_SelectText( m_pContext,
					start_ele_loc,
					start_position,
					end_ele_loc,
					end_position,
					&tlx,
					&tly);
				
			    if( IsSelected() )
        			formatting = GetCharacterDataSelection(0, selection);
			    PasteText( pReplaceText, FALSE, FALSE, TRUE, TRUE);
				for (i = XP_STRLEN( pReplaceText ); i > 0; i-- )
				    SelectPreviousChar();
			    SetCharacterData( formatting );

   				/* use non-editor LO_* call */
				LO_GetSelectionEndpoints( m_pContext,
								&start_ele_loc,
								&end_ele_loc,
								&start_position,
								&end_position,
								&layer);
			}
			else if ( bDoWrap )
			{
				/* not found; but we need to wrap */
				bDoWrap = FALSE;	/* only wrap this once */
				found = TRUE;		/* reset our flag so we keep looping! */

				/* Try again from the beginning.  These are the values
				   LO_GetSelectionEndpoints returns if there is no selection */
				end_ele_loc = NULL;		/* this will reset start_ele_loc above */
				start_position = 0;
				end_position = 0;
				
				current_done_location = final_done_loc;
			}
			
		} while ( found );
    }
    
    EndBatchChanges();
}

//-----------------------------------------------------------------------------
// CEditTagCursor
//-----------------------------------------------------------------------------
CEditTagCursor::CEditTagCursor( CEditBuffer* pEditBuffer,
            CEditElement *pElement, int iEditOffset, CEditElement* pEndElement ):
            m_pEditBuffer(pEditBuffer),
            m_pCurrentElement(pElement),
            m_endPos(pEndElement,0),
            m_tagPosition(tagOpen),
            m_stateDepth(0),
            m_currentStateDepth(0),
            m_iEditOffset( iEditOffset ),
            m_bClearRelayoutState(0),
            m_pContext( pEditBuffer->GetContext() ),
            m_pStateTags(0)
            {

    CEditElement *pContainer = m_pCurrentElement->FindContainer();
    if( (m_pCurrentElement->PreviousLeafInContainer() == 0 )
            && iEditOffset == 0
            && pContainer
            && pContainer->IsA( P_LIST_ITEM ) ){
        m_pCurrentElement = pContainer;
    }

    //m_bClearRelayoutState = (m_pCurrentElement->PreviousLeafInContainer() != 0 );

    //
    // If the element has a parent, setup the depth
    //
    pElement = m_pCurrentElement->GetParent();

    while( pElement ){
        PA_Tag *pTag = pElement->TagOpen( 0 );
        PA_Tag *pTagEnd = pTag;

        while( pTagEnd->next != 0 ) pTagEnd = pTag->next;

        pTagEnd->next = m_pStateTags;
        m_pStateTags = pTag;
        pElement = pElement->GetParent();
    }
}

CEditTagCursor::~CEditTagCursor(){
    EDT_DeleteTagChain( m_pStateTags );
}

CEditTagCursor* CEditTagCursor::Clone(){
    CEditTagCursor *pCursor = new CEditTagCursor( m_pEditBuffer,
                                                  m_pCurrentElement,
                                                  m_iEditOffset,
                                                  m_endPos.Element());
    return pCursor;
}

//
// We iterate through the edit tree generating tags.  The goto code here
//  insures that after we deliver a tag, the m_pCurrentElement is always pointing
//  to the next element to be output
//
PA_Tag* CEditTagCursor::GetNextTag(){
    XP_Bool bDone = FALSE;
    PA_Tag* pTag = 0;

    // Check for end
    if( m_pCurrentElement == 0 ){
        return 0;
    }

    while( !bDone && m_pCurrentElement != 0 ){
        // we are either generating the beginning tag, content or end tags
        switch( m_tagPosition ){

        case tagOpen:        	
            pTag = m_pCurrentElement->TagOpen( m_iEditOffset );
            bDone = (pTag != 0);
            m_iEditOffset = 0;  // only counts on the first tag.
            if( m_pCurrentElement->GetChild() ){
                m_pCurrentElement = m_pCurrentElement->GetChild();
                m_tagPosition = tagOpen;
            }
            else {
                m_tagPosition = tagEnd;
                if( !m_pCurrentElement->IsContainer() &&
                        !WriteTagClose(m_pCurrentElement->GetType()) 
                         ){
                    goto seek_next;
                }
            }
            break;

        case tagEnd:
            pTag = m_pCurrentElement->TagEnd();
            bDone = (pTag != 0);
        seek_next:
            if( m_pCurrentElement->GetNextSibling()){
                m_pCurrentElement = m_pCurrentElement->GetNextSibling();
                m_tagPosition = tagOpen;
            }
            else {
                //
                // We've exausted all the elements at this level. Set the
                //  current element to the end of our parent.  m_tagPosition is
                //  already set to tagEnd, but lets be explicit
                //
                m_pCurrentElement = m_pCurrentElement->GetParent();
                m_tagPosition = tagEnd;
                if( m_pCurrentElement 
                        && !m_pCurrentElement->IsContainer() 
                        && !WriteTagClose(m_pCurrentElement->GetType()) 
                        ){
                    goto seek_next;
                }
            }
            break;
        }
    }

    if( bDone ){
        return pTag;
    }
    else {
        XP_DELETE(pTag);
        return 0;
    }
}

PA_Tag* CEditTagCursor::GetNextTagState(){
    PA_Tag *pRet = m_pStateTags;
    if( m_pStateTags ){
        m_pStateTags = 0;
    }
    return pRet;
}

XP_Bool CEditTagCursor::AtBreak( XP_Bool* pEndTag ){
    XP_Bool bAtPara;
    XP_Bool bEndTag;

    if( m_pCurrentElement == 0 ){
        return FALSE;
    }

    if(  m_tagPosition == tagEnd ){
        bEndTag = TRUE;
        bAtPara = BitSet( edt_setTextContainer,  m_pCurrentElement->GetType()  );
    }
    else {
        bEndTag = FALSE;
        bAtPara = BitSet( edt_setParagraphBreak,  m_pCurrentElement->GetType()  );
    }

    if( bAtPara ){
        // force the layout engine to process this tag before computing line
        //  position.
        //if( m_pCurrentElement->IsA( P_LINEBREAK ){
        //    bEndTag = TRUE;
        //}

        // if there is an end position and the current position is before it
        //  ignore this break.
        CEditPosition p(m_pCurrentElement);

        if( m_endPos.IsPositioned() &&
                 m_endPos.Compare(&p) <= 0  ){
            return FALSE;
        }
        if( CurrentLine() != -1 ){
            *pEndTag = bEndTag;
            return TRUE;
        }
        else {
            //XP_ASSERT(FALSE)
        }
    }
    return FALSE;
}


int32 CEditTagCursor::CurrentLine(){
    CEditElement *pElement;
    LO_Element * pLayoutElement, *pLoStartLine;
    int32 iLineNum;

    //
    // Pop to the proper state
    //
    CEditElement *pSave = m_pCurrentElement;
    while( m_pCurrentElement && m_tagPosition == tagEnd ){
        PA_Tag* pTag = GetNextTag();
        EDT_DeleteTagChain( pTag );
        m_tagPosition = tagEnd;     // restore this.
    }


    // if we fell of the end of the document, then we are done.
    if( m_pCurrentElement == 0 ){
        m_pCurrentElement = pSave;
        return -1;
    }

    // LTNOTE: need to check to see if we are currently at a TextElement..
    pElement = m_pCurrentElement->NextLeaf();
    m_pCurrentElement = pSave;

    if( pElement == 0 ){
        return -1;
    }

    XP_ASSERT( pElement->IsLeaf() );
    pLayoutElement = pElement->Leaf()->GetLayoutElement();
    if( pLayoutElement == 0 ){
        return -1;
    }

    // Find the first element on this line.
    pLoStartLine = m_pEditBuffer->FirstElementOnLine( pLayoutElement, &iLineNum );

    // LTNOTE: this isn't true in delete cases.
    //XP_ASSERT( pLoStartLine == pLayoutElement );

    return iLineNum;
}

LO_Element* CEditBuffer::FirstElementOnLine(LO_Element* pTarget, int32* pLineNum){
    // LO_FirstElementOnLine does a binary search in Y to find the first element on a line for
    // a given (x,y) position. If the y coordinate of the position is right at the
    // top of a given line, and the previous line has an image, it is possible for
    // the search to return the previous line rather than the line than the
    // correct line. See bug 76945. A work-around is to start the search in the center of
    // the current element. (We don't calculate the exact center, since we don't
    // take the margin of an image into account, but it's close enough. Actually, probably just
    // adding 1 to the Y coordinate would be enough, but this way seems safer.
    //
    // I don't know why we don't just start at the current element and work
    // backwards through the linked list to find the first element on the line. That should
    // be faster and more accurate. ltabb might know why we do it this way instead.

    int32 x = pTarget->lo_any.x + pTarget->lo_any.width / 2;
    int32 y = pTarget->lo_any.y + pTarget->lo_any.height / 2;
    return LO_FirstElementOnLine(m_pContext, x, y, pLineNum);
}

void CEditBuffer::ChangeEncoding(int16 csid) {
    // NOTE: We no longer need to check and warn user if doc is
    // dirty because we are not reloading buffer from a URL.
    // Causes doc to be reread from temporary internal buffer.

    ForceDocCharSetID(csid); // Will be translated to this id when saved
    // Note: Why doesn't this allow Undo to work?
    BeginBatchChanges(0); // Marks document as dirty.
    SetEncoding(csid); // Will claim to be this id when saved.
    EndBatchChanges();
    CEditDocState *pState = RecordState(); // Actually translates.
    if (pState) {
        RestoreState(pState);
        // At this point this is deleted
        delete pState;
    }
    else {
        XP_ASSERT(0);
    }
}

// Add content-type meta-data. This tells the reader which character set was used to create the document.
// See RFC 2070, "Internationalization of the Hypertext Markup Language"
// http://ds.internic.net/rfc/rfc2070.txt
// <META HTTP-EQUIV="Content-Type" CONTENT="text/html; charset=ISO-2022-JP">
void CEditBuffer::SetEncoding(int16 csid) {
 	char contentValue[200];
    char charSet[100];
    int16 plainCSID = csid & ~CS_AUTO;
    INTL_CharSetIDToName(plainCSID, charSet);
    XP_SPRINTF(contentValue, "text/html; charset=%.100s", charSet);

    EDT_MetaData *pData = MakeMetaData( TRUE, CONTENT_TYPE, contentValue);

    SetMetaData( pData );
    FreeMetaData( pData );
}

XP_Bool CEditBuffer::HasEncoding() {
    return FindMetaData(TRUE, CONTENT_TYPE) >= 0;
}

// Used for QA only - Ctrl+Alt+Shift+N accelerator for automated testing
void EDT_SelectNextNonTextObject(MWContext *pContext)
{
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SelectNextNonTextObject();
}

void CEditBuffer::SelectNextNonTextObject()
{
    CEditLeafElement *pElement;
    ElementOffset iOffset;
    XP_Bool bStickyAfter;

    GetInsertPoint( &pElement, &iOffset, &bStickyAfter );
    XP_Bool bSelectObject = FALSE;
    XP_Bool bDone = FALSE;

    while( !bDone )
    {
        if( m_pSelectedEdTable )
        {
            // Last item selected was a table - clear it
            ClearTableAndCellSelection();
        } else {
            pElement = (CEditLeafElement*)pElement->FindNextElement(&CEditElement::FindLeafAll,0);
        }

        // No more to find
        if( ! pElement )
            return;

        // Test if we are inside a table
        CEditTableElement *pTable = pElement->GetParentTable();

        if( pTable && pTable != m_pNonTextSelectedTable )
        {
            // We found a table -- select it
            LO_TableStruct *pLoTable = pTable->GetLoTable();
            if( pLoTable )
            {
                SelectTable(TRUE, pLoTable, pTable);
            }
            // That's all we need to do
            bDone = TRUE;
        }
        m_pNonTextSelectedTable = pTable;    

        if( !pTable )
        {
            EEditElementType eType = pElement->GetElementType();
            if( eType == eImageElement ||
                eType == eHorizRuleElement ||
                eType == eTargetElement )
            {
                bSelectObject = TRUE;
                bDone = TRUE;
            }
        }
    }
    
    if( pElement )
    {
        CEditInsertPoint ip(pElement, 0);
        SetInsertPoint(ip);
        if( bSelectObject )
        {
            LO_Element *pLoElement = pElement->GetLayoutElement();
            if( pLoElement )
            {
                StartSelection( pLoElement->lo_any.x+5, pLoElement->lo_any.y+1 );
            }
        }
    }
    
}

#ifdef XP_WIN16
// code segment is full, switch to a new segment
#pragma code_seg("EDTSAVE3_TEXT","CODE")
#endif

#endif
