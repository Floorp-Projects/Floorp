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


//
// Public interface and shared subsystem data.
//

#ifdef EDITOR

#include "editor.h"
#include "libimg.h"
#include "intl_csi.h"
#include "xp_str.h"

#ifdef USE_SCRIPT
#define  EDT_IS_SCRIPT(tf) (0 != (tf & (TF_SERVER | TF_SCRIPT | TF_STYLE)))
#else
#define EDT_IS_SCRIPT(tf) (FALSE)
#endif

//-----------------------------------------------------------------------------
// CEditElement
//-----------------------------------------------------------------------------

//
// This version of the constructor is used to create a child element.
//
CEditElement::CEditElement(CEditElement *pParent, TagType tagType, char* pData): 
        m_tagType(tagType), 
        m_pParent(pParent), 
        m_pNext(0), 
        m_pChild(0),
        m_pTagData(0)
{
    if ( pData ) {
        SetTagData(pData);
    }
    CommonConstructor();
}

CEditElement::CEditElement(CEditElement *pParent, PA_Tag *pTag, int16 /*csid*/) :
        m_tagType(pTag->type), 
        m_pParent(pParent), 
        m_pNext(0), 
        m_pChild(0),
        m_pTagData(0)
{
    char *locked_buff;

    PA_LOCK(locked_buff, char *, pTag->data);
    if( locked_buff && *locked_buff != '>'){
        SetTagData( locked_buff );
    }
    PA_UNLOCK(pTag->data);
    CommonConstructor();
}

void CEditElement::CommonConstructor(){
    if( m_pParent ){
        CEditElement* pE = m_pParent->GetChild();

        //
        // there already is a child, add this child to the end of the list.
        //
        if( pE ){
            while( pE->m_pNext != 0 ){
                pE = pE->m_pNext;
            }
            pE->RawSetNextSibling(this);
        }
        else {
            // make this the first child.
            m_pParent->RawSetChild(this);
        }
    }
}


CEditElement::CEditElement( IStreamIn *pIn, CEditBuffer* /* pBuffer */ ):
        m_pParent(0), 
        m_pNext(0), 
        m_pChild(0),
        m_pTagData(0)
{
    m_tagType = (TagType) pIn->ReadInt();
    m_pTagData = pIn->ReadZString();
}

CEditElement::~CEditElement(){
    Finalize();
}

void CEditElement::Finalize(){
    Unlink();
    DeleteChildren();
    if( m_pTagData ) {
        XP_FREE(m_pTagData);
        m_pTagData = NULL;
    }
}

CEditTextElement* CEditElement::Text(){
    XP_ASSERT(IsText());
    return (CEditTextElement*)this;
}

XP_Bool CEditElement::IsLeaf() { return FALSE; }
CEditLeafElement* CEditElement::Leaf(){ XP_ASSERT(IsLeaf());
    return (CEditLeafElement*)this;
}

XP_Bool CEditElement::IsRoot() { return FALSE; }
CEditRootDocElement* CEditElement::Root(){ XP_ASSERT(IsRoot());
    return (CEditRootDocElement*)this;
}

XP_Bool CEditElement::IsContainer() { return FALSE; }
CEditContainerElement* CEditElement::Container(){ XP_ASSERT(IsContainer());
    return (CEditContainerElement*)this;
}

XP_Bool CEditElement::IsList() { return FALSE; }
CEditListElement* CEditElement::List(){ XP_ASSERT(IsList());
    return (CEditListElement*)this;
}

XP_Bool CEditElement::IsBreak() { return FALSE; }
CEditBreakElement* CEditElement::Break(){ XP_ASSERT(IsBreak());
    return (CEditBreakElement*)this;
}

XP_Bool CEditElement::CausesBreakBefore() { return FALSE;}
XP_Bool CEditElement::CausesBreakAfter() { return FALSE;}
XP_Bool CEditElement::AllowBothSidesOfGap() { return FALSE; }

XP_Bool CEditElement::IsImage() { return FALSE; }
CEditImageElement* CEditElement::Image(){
    // Not all P_IMAGE elements are images.  CEditIconElements have P_IMAGE
    //  as their tagType but are not CEditImageElements.
    XP_ASSERT(m_tagType==P_IMAGE
        && GetElementType() == eImageElement
        && IsImage() ) ;
    return (CEditImageElement*)this;
}

XP_Bool CEditElement::IsIcon() { return FALSE; }
CEditIconElement* CEditElement::Icon(){ XP_ASSERT(IsIcon());
    return (CEditIconElement*)this;
}

CEditTargetElement* CEditElement::Target(){ XP_ASSERT(GetElementType() == eTargetElement);
    return (CEditTargetElement*)this;
}

CEditHorizRuleElement* CEditElement::HorizRule(){ XP_ASSERT(m_tagType==P_HRULE);
    return (CEditHorizRuleElement*)this;
}

XP_Bool CEditElement::IsRootDoc() { return FALSE; }
CEditRootDocElement* CEditElement::RootDoc() { XP_ASSERT(IsRootDoc()); return (CEditRootDocElement*) this; }
XP_Bool CEditElement::IsSubDoc() { return FALSE; }
CEditSubDocElement* CEditElement::SubDoc() { XP_ASSERT(IsSubDoc()); return (CEditSubDocElement*) this; }
XP_Bool CEditElement::IsTable() { return FALSE; }
CEditTableElement* CEditElement::Table() { XP_ASSERT(IsTable()); return (CEditTableElement*) this; }
XP_Bool CEditElement::IsTableRow() { return FALSE; }
CEditTableRowElement* CEditElement::TableRow() { XP_ASSERT(IsTableRow()); return (CEditTableRowElement*) this; }
XP_Bool CEditElement::IsTableCell() { return FALSE; }
CEditTableCellElement* CEditElement::TableCell() { XP_ASSERT(IsTableCell()); return (CEditTableCellElement*) this; }
XP_Bool CEditElement::IsCaption() { return FALSE; }
CEditCaptionElement* CEditElement::Caption() { XP_ASSERT(IsCaption()); return (CEditCaptionElement*) this; }
XP_Bool CEditElement::IsText() { return FALSE; }
XP_Bool CEditElement::IsLayer() { return FALSE; }
CEditLayerElement* CEditElement::Layer() { XP_ASSERT(IsLayer()); return (CEditLayerElement*) this; }
XP_Bool CEditElement::IsDivision() { return FALSE; }
CEditDivisionElement* CEditElement::Division() { XP_ASSERT(IsDivision()); return (CEditDivisionElement*) this; }

XP_Bool CEditElement::IsEndOfDocument() { return GetElementType() == eEndElement; }
XP_Bool CEditElement::IsEndContainer() { return FALSE; }


void
CEditElement::SetTagData(char* tagData)
{
    if( m_pTagData ) {
        XP_FREE(m_pTagData);
    }
    if( tagData ){
        m_pTagData = XP_STRDUP(tagData);
    }
    else {
        m_pTagData = tagData;
    }
}

void CEditElement::StreamOut( IStreamOut *pOut ){
    pOut->WriteInt( GetElementType() );
    pOut->WriteInt( m_tagType );
    pOut->WriteZString( m_pTagData );
}

XP_Bool CEditElement::ShouldStreamSelf( CEditSelection& local, CEditSelection& selection)
{
    return ( local.EqualRange( selection ) || ! local.Contains(selection));
}

// Partially stream out this element and each child.except if we are told to stream all
void CEditElement::PartialStreamOut( IStreamOut* pOut, CEditSelection& selection) 
{
    CEditSelection local;
    GetAll(local);
#ifdef DEBUG
    TagType type = GetType();
#endif

    if ( local.Intersects(selection) )
    {
        XP_Bool bWriteSelf = ShouldStreamSelf(local, selection);
        if( bWriteSelf )
        {
            StreamOut(pOut);
        }
        CEditElement* pChild;
        for ( pChild = GetChild(); pChild; pChild = pChild->GetNextSibling() )
        {
            pChild->PartialStreamOut(pOut, selection);
        }
        if ( bWriteSelf )
        {
            pOut->WriteInt((int32)eElementNone);
        }
    }
}

XP_Bool CEditElement::ClipToMe(CEditSelection& selection, CEditSelection& local) {
    // Returns TRUE if selection intersects with "this".
    GetAll(local);
    return local.ClipTo(selection);
}

void CEditElement::GetAll(CEditSelection& selection) {
    CEditLeafElement* pFirstMostChild = CEditLeafElement::Cast(GetFirstMostChild());
    if ( ! pFirstMostChild ) {
        XP_ASSERT(FALSE);
        return;
    }
    selection.m_start.m_pElement = GetFirstMostChild()->Leaf();
    selection.m_start.m_iPos = 0;
    CEditLeafElement* pLast = GetLastMostChild()->Leaf();
    CEditLeafElement* pNext = pLast->NextLeaf();
    if ( pNext ) {
        selection.m_end.m_pElement = pNext;
        selection.m_end.m_iPos = 0;
    }
    else {
     // edge of document. Can't select any further.
        selection.m_end.m_pElement = pLast;
        selection.m_end.m_iPos = pLast->GetLen();
    }
    selection.m_bFromStart = FALSE;
}

EEditElementType CEditElement::GetElementType()
{
    return eElement;
}

// Get parent table of the element
CEditTableElement* CEditElement::GetParentTable()
{
    CEditElement *pElement = this;
    do { pElement = pElement->GetParent(); }
    while( pElement && !pElement->IsTable() );
    return (CEditTableElement*)pElement;
}

// Get parent table cell of the element
CEditTableCellElement* CEditElement::GetParentTableCell()
{
    CEditElement *pElement = this;
    do { pElement = pElement->GetParent(); }
    while( pElement && !pElement->IsTableCell() );
    return (CEditTableCellElement*)pElement;
}

// 
// static function calls the appropriate stream constructor
//

CEditElement* CEditElement::StreamCtor( IStreamIn *pIn, CEditBuffer *pBuffer ){
    CEditElement* pResult = StreamCtorNoChildren(pIn, pBuffer);
    if ( pResult ) {
        pResult->StreamInChildren(pIn, pBuffer);
    }
    return pResult;
}

void CEditElement::StreamInChildren(IStreamIn* pIn, CEditBuffer* pBuffer){
    CEditElement* pChild;
    while ( (pChild = CEditElement::StreamCtor(pIn, pBuffer)) != NULL ) {
        pChild->InsertAsLastChild(this);
    }
}

CEditElement* CEditElement::StreamCtorNoChildren( IStreamIn *pIn, CEditBuffer *pBuffer ){

    EEditElementType eType = (EEditElementType) pIn->ReadInt();
    switch( eType ){
        case eElementNone:
            return 0;

        case eElement:
            return new CEditElement( pIn, pBuffer );

        case eTextElement:
            return new CEditTextElement( pIn, pBuffer  );

        case eImageElement:
            return new CEditImageElement( pIn, pBuffer  );

        case eHorizRuleElement:
            return new CEditHorizRuleElement( pIn, pBuffer  );

        case eBreakElement:
            return new CEditBreakElement( pIn, pBuffer  );

        case eContainerElement:
            return new CEditContainerElement( pIn, pBuffer  );

        case eListElement:
            return new CEditListElement( pIn, pBuffer  );

        case eIconElement:
            return new CEditIconElement( pIn, pBuffer  );

        case eTargetElement:
            return new CEditTargetElement( pIn, pBuffer  );

        case eTableElement:
             return new CEditTableElement( pIn, pBuffer  );

        case eCaptionElement:
             return new CEditCaptionElement( pIn, pBuffer  );

        case eTableRowElement:
             return new CEditTableRowElement( pIn, pBuffer  );

        case eTableCellElement:
             return new CEditTableCellElement( pIn, pBuffer  );

        case eLayerElement:
             return new CEditLayerElement( pIn, pBuffer  );

        case eDivisionElement:
            return new CEditDivisionElement( pIn, pBuffer );

       default:
            XP_ASSERT(0);
    }
    return 0;
}


//
// Scan up the tree looking to see if we are within 'tagType'.  If we stop and
//  we are not at the top of the tree, we found the tag we are looking for.
//
XP_Bool CEditElement::Within( int tagType ){
    CEditElement* pParent = GetParent();
    while( pParent && pParent->GetType() != tagType ){
        pParent = pParent->GetParent();
    }
    return (pParent != 0);
}

CEditBuffer* CEditElement::GetEditBuffer(){
    CEditRootDocElement *pE = GetRootDoc();
    if( pE ){
        return pE->GetBuffer();
    }
    else {
        return 0;
    }
}

XP_Bool CEditElement::InFormattedText(){
    CEditElement* pParent = GetParent();

#ifdef USE_SCRIPT
    if( IsA( P_TEXT) && (Text()->m_tf & (TF_SERVER|TF_SCRIPT|TF_STYLE)) != 0 ){
        return TRUE;
    }
#endif

    while( pParent && BitSet( edt_setFormattedText, pParent->GetType() ) == 0 ){
        pParent = pParent->GetParent();
    }
    return (pParent != 0);
}


//
// Fills in the data member of the tag, as well as the type informaiton.
//
void CEditElement::SetTagData( PA_Tag* pTag, char* pTagData){
    PA_Block buff;
    char *locked_buff;
    int iLen;

    if ( NULL == pTagData ) {
        XP_ASSERT(FALSE);
        return;
    }

    pTag->type = m_tagType;
    pTag->edit_element = this;

    iLen = XP_STRLEN(pTagData);
    buff = (PA_Block)PA_ALLOC((iLen+1) * sizeof(char));
    if (buff != NULL)
    {
        PA_LOCK(locked_buff, char *, buff);
        XP_BCOPY(pTagData, locked_buff, iLen);
        locked_buff[iLen] = '\0';
        PA_UNLOCK(buff);
    }
    else { 
        // LTNOTE: out of memory, should throw an exception
        return;
    }

    pTag->data = buff;
    pTag->data_len = iLen;
    pTag->next = NULL;
    return;
}

PA_Tag* CEditElement::TagOpen( int /* iEditOffset */ ){
    PA_Tag *pTag = XP_NEW( PA_Tag );
    XP_BZERO( pTag, sizeof( PA_Tag ) );
    if( GetTagData() ){
        SetTagData( pTag, GetTagData() );
    }
    else {
        SetTagData( pTag, ">" );
    }
    return pTag;
}

PA_Tag* CEditElement::TagEnd( ){
    if( TagHasClose( m_tagType ) || BitSet( edt_setWriteEndTag, m_tagType ) ){
        PA_Tag *pTag = XP_NEW( PA_Tag );
        XP_BZERO( pTag, sizeof( PA_Tag ) );
        pTag->type = m_tagType;
        pTag->is_end = TRUE;
        pTag->edit_element = this;
        return pTag;
    }
    return 0;
}


XP_Bool CEditElement::Reduce( CEditBuffer* /* pBuffer */ ){
    if( !BitSet( edt_setSoloTags,  GetType()  ) &&  GetChild() == 0 ){
        return TRUE;
    }
    else if( BitSet( edt_setCharFormat,  GetType()  ) ){
        CEditElement *pNext = GetNextSibling();
        if( pNext && pNext->GetType() == GetType() ){
            // FONTs and Anchors need better compares than this.
            Merge(pNext);

            // make sure it stays in the tree so it dies a natural death (because
            //  it has no children)
            pNext->InsertAfter(this);
            return FALSE;
        }
    }
    return FALSE;
}

int CEditElement::GetDefaultFontSize(){
    TagType t = GetType();
    if( !BitSet( edt_setTextContainer,  t  ) ){
        CEditElement* pCont = FindContainer();
        if( pCont ){
            t = pCont->GetType();
        }
        else {
            return 0;       // no default font size
        }
    }

    switch( t ){
        case P_HEADER_1:
            return 6;
        case P_HEADER_2:
            return 5;
        case P_HEADER_3:
            return 4;
        case P_HEADER_4:
            return 3;
        case P_HEADER_5:
            return 2; 
        case P_HEADER_6:
            return 1;
        default:
            return 3;
    }
}

CEditInsertPoint CEditElement::IndexToInsertPoint(ElementIndex index, XP_Bool bStickyAfter) {
    if ( index < 0 ) {
        XP_ASSERT(FALSE);
        index = 0;
    }
    CEditElement* pChild;
    CEditElement* pLastChild = NULL;
    ElementIndex childCount = 0;
    // XP_TRACE(("IndexToInsertPoint: 0x%08x (%d) index = %d", this, GetElementIndex(), index));
    for ( pChild = GetChild();
        pChild;
        pChild = pChild->GetNextSibling()) {
        pLastChild = pChild;
        childCount = pChild->GetPersistentCount();
        if ( index < childCount
            || (index == childCount && ! bStickyAfter) ){
            return pChild->IndexToInsertPoint(index, bStickyAfter);
        }
            
        index -= childCount;
    }
    if ( ! pLastChild ) { // No children at all
        childCount = GetPersistentCount();
        if ( index > childCount ){
            XP_ASSERT(FALSE);
            index = childCount;
        }
        return CEditInsertPoint(this, index);
    }
    // Ran off end of children
    return pLastChild->IndexToInsertPoint(childCount, bStickyAfter);
}

CPersistentEditInsertPoint CEditElement::GetPersistentInsertPoint(ElementOffset offset){
    XP_ASSERT(FALSE);   // This method should never be called
    return CPersistentEditInsertPoint(GetElementIndex() + offset);
}

ElementIndex CEditElement::GetPersistentCount()
{
    ElementIndex count = 0;
    for ( CEditElement* c = GetChild();
        c;
        c = c->GetNextSibling() ) {
        count += c->GetPersistentCount();
    }
    return count;
}

ElementIndex CEditElement::GetElementIndex()
{
    CEditElement* parent = GetParent();
    if ( parent )
        return parent->GetElementIndexOf(this);
    else
        return 0;
}

ElementIndex CEditElement::GetElementIndexOf(CEditElement* child)
{
    ElementIndex index = GetElementIndex();
    for ( CEditElement* c = GetChild();
        c;
        c = c->GetNextSibling() ) {
        if ( child == c )
        {
            return index;
        }
        index += c->GetPersistentCount();
    }
    XP_ASSERT(FALSE); // Couldn't find this child.
    return index;
}

void CEditElement::FinishedLoad( CEditBuffer* pBuffer ){
    CEditElement* pNext = 0;
    for ( CEditElement* pChild = GetChild();
        pChild;
        pChild = pNext ) {
        pNext = pChild->GetNextSibling();
        if ( IsAcceptableChild(*pChild) ){
            pChild->FinishedLoad(pBuffer);
        }
        else {
#ifdef DEBUG
            XP_TRACE(("Removing an unacceptable child. Parent type %d child type %d.\n",
                GetElementType(), pChild->GetElementType()));
#endif
            delete pChild;
        }
    }
}

//
// Containers can't be deleted during adjustment or we will blow up.  We need
//  While adjusting containers, new containers are inserted.
//
void CEditElement::AdjustContainers( CEditBuffer* pBuffer ){
    for ( CEditElement* pChild = GetChild();
        pChild;
        pChild = pChild->GetNextSibling() ) {

        pChild->AdjustContainers(pBuffer);
    }
}



int16 CEditElement::GetWinCSID(){
	// International Character set ID
	CEditRootDocElement* pRoot = GetRootDoc();
	if ( pRoot ) {
		CEditBuffer* pBuffer = pRoot->GetEditBuffer();
		if ( pBuffer ) {
			return pBuffer->GetRAMCharSetID();
		}
	}
    XP_ASSERT(FALSE);
	return  CS_FE_ASCII;
}

void CEditElement::EnsureSelectableSiblings(CEditBuffer* pBuffer)
{
    // To ensure that we can edit before or after the table,
    // make sure that there is a container before and after the table.

    CEditElement* pParent = GetParent();
    if ( ! pParent ) {
        return;
    }
    {
        // Make sure the previous sibling exists and is a container
        CEditElement* pPrevious = GetPreviousSibling();
        // EXPERIMENTAL: Don't force another container before inserted table
#if 0
        if ( ! pPrevious ) {
#else
        if ( ! pPrevious || !pPrevious->IsContainer() ) {
#endif
            pPrevious = CEditContainerElement::NewDefaultContainer( NULL,
                            pParent->GetDefaultAlignment() );
            pPrevious->InsertBefore(this);
			pPrevious->FinishedLoad(pBuffer);
        }
    }
    {
        // Make sure the next sibling exists and is container
        CEditElement* pNext = GetNextSibling();
#if 0
        if ( ! pNext || pNext->IsEndContainer() ) {
#else
        if ( ! pNext || pNext->IsEndContainer() || !pNext->IsContainer()) {
#endif
            pNext = CEditContainerElement::NewDefaultContainer( NULL,
                            pParent->GetDefaultAlignment() );
            pNext->InsertAfter(this);
			pNext->FinishedLoad(pBuffer);
        }
    }
}

//-----------------------------------------------------------------------------
//  Reverse navagation (left)
//-----------------------------------------------------------------------------

// these routines are a little expensive.  If we need to, we can make the linked
//  lists of elements, doubly linked.
//
CEditElement* CEditElement::GetPreviousSibling(){
    if( GetParent() == 0 ){
        return 0;
    }

    // point to our first sibling.
    CEditElement *pSibling = GetParent()->GetChild();

    // if we are the first sibling, then there is no previous sibling.
    if ( pSibling == this ){
        return 0;
    }

    // if we get an Exception in this loop, the tree is messed up!
    while( pSibling->GetNextSibling() != this ){
        pSibling = pSibling->GetNextSibling();
    }
    return pSibling;
    
}

void CEditElement::SetChild(CEditElement *pChild){
    RawSetChild(pChild);
}

void CEditElement::SetNextSibling(CEditElement* pNext){
    RawSetNextSibling(pNext);
}

CEditElement* CEditElement::GetLastChild(){
    CEditElement* pChild;
    if( (pChild = GetChild()) == 0 ){
        return 0;
    }
    while( pChild->GetNextSibling() ){
        pChild = pChild->GetNextSibling();
    }
    return pChild;
}

CEditElement* CEditElement::GetFirstMostChild(){
    CEditElement* pChild = this;
    CEditElement* pPrev = pChild;
    while( pPrev ){
        pChild = pPrev;
        pPrev = pPrev->GetChild();
    }
    return pChild;
}

CEditElement* CEditElement::GetLastMostChild(){
    CEditElement* pChild = this;
    CEditElement* pNext = pChild;
    while( pNext ){
        pChild = pNext;
        pNext = pNext->GetLastChild();
    }
    return pChild;
}

CEditContainerElement*  CEditElement::GetPreviousNonEmptyContainer()
{
    CEditElement *pPrev = GetPreviousSibling();
    CEditContainerElement* pPrevContainer;
    while (pPrev && pPrev->IsContainer())
    {
        pPrevContainer=pPrev->Container();
        if (!pPrevContainer->IsEmpty())
            return pPrevContainer;
        pPrev=pPrev->GetPreviousSibling();
    }
    return NULL;
}

CEditContainerElement*  CEditElement::GetNextNonEmptyContainer()
{
    CEditElement *pNext = GetNextSibling();
    CEditContainerElement* pNextContainer;
    while (pNext&& pNext->IsContainer())
    {
        pNextContainer=pNext->Container();
        if (!pNextContainer->IsEmpty())
            return pNextContainer;
        pNext=pNext->GetNextSibling();
    }
    return NULL;
}

///////////////////////////////////////////////
/////END CEDITELEMENT IMPLEMENTATION///////////
///////////////////////////////////////////////


CEditTableCellElement* CEditElement::GetTableCell() {
    // Returns containing cell, or NULL if none.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsTableCell() ) {
            break;
        }
        if ( pElement->IsSubDoc() && pElement != this ) {
            return NULL;
        }
        pElement = pElement->GetParent();
    }
    return (CEditTableCellElement*) pElement;
}

CEditTableCellElement* CEditElement::GetTableCellIgnoreSubdoc() {
    // Returns containing cell, or NULL if none.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsTableCell() ) {
            break;
        }
        pElement = pElement->GetParent();
    }
    return (CEditTableCellElement*) pElement;
}

CEditTableRowElement* CEditElement::GetTableRow() {
    // Returns containing cell, or NULL if none.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsTableRow() ) {
            break;
        }
        if ( pElement->IsSubDoc() && pElement != this ) {
            return NULL;
        }
        pElement = pElement->GetParent();
    }
    return (CEditTableRowElement*) pElement;
}

CEditTableRowElement* CEditElement::GetTableRowIgnoreSubdoc() {
    // Returns containing cell, or NULL if none.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsTableRow() ) {
            break;
        }
        pElement = pElement->GetParent();
    }
    return (CEditTableRowElement*) pElement;
}

CEditCaptionElement* CEditElement::GetCaption() {
    // Returns containing tavle, or NULL if none.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsCaption() ) {
            break;
        }
        if ( pElement->IsSubDoc() && pElement != this ) {
            return NULL;
        }
        pElement = pElement->GetParent();
    }
    return (CEditCaptionElement*) pElement;
}

CEditCaptionElement* CEditElement::GetCaptionIgnoreSubdoc() {
    // Returns containing table, or NULL if none.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsCaption() ) {
            break;
        }
        pElement = pElement->GetParent();
    }
    return (CEditCaptionElement*) pElement;
}

CEditTableElement* CEditElement::GetTable() {
    // Returns containing table, or NULL if none.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsTable() ) {
            break;
        }
        if ( pElement->IsSubDoc() && pElement != this ) {
            return NULL;
        }
        pElement = pElement->GetParent();
    }
    return (CEditTableElement*) pElement;
}

CEditTableElement* CEditElement::GetTableIgnoreSubdoc() {
    // Returns containing table, or NULL if none.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsTable() ) {
            break;
        }
        pElement = pElement->GetParent();
    }
    return (CEditTableElement*) pElement;
}

CEditElement* CEditElement::GetTopmostTableOrLayer() {
    // Returns containing table, or NULL if none.
    CEditElement* pResult = NULL;
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsTable() || pElement->IsLayer()) {
            pResult = pElement;
        }
        pElement = pElement->GetParent();
    }
    return pResult;
}

CEditElement* CEditElement::GetTableOrLayerIgnoreSubdoc() {
    // Returns containing table, or NULL if none.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsTable() || pElement->IsLayer() ) {
            break;
        }
        pElement = pElement->GetParent();
    }
    return pElement;
}

CEditElement* CEditElement::GetSubDocOrLayerSkipRoot() {
    // Returns containing sub-doc, or NULL if none.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsLayer() ||
            (pElement->IsSubDoc() && !pElement->IsRoot() ) ) {
            break;
        }
        pElement = pElement->GetParent();
    }
    return pElement;
}

CEditLayerElement* CEditElement::GetLayer() {
    // Returns containing Layer, or NULL if none.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsLayer() ) {
            break;
        }
        if ( pElement->IsSubDoc() && pElement != this ) {
            return NULL;
        }
        pElement = pElement->GetParent();
    }
    return (CEditLayerElement*) pElement;
}

CEditLayerElement* CEditElement::GetLayerIgnoreSubdoc() {
    // Returns containing Layer, or NULL if none.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsLayer() ) {
            break;
        }
        pElement = pElement->GetParent();
    }
    return (CEditLayerElement*) pElement;
}

CEditSubDocElement* CEditElement::GetSubDoc() {
    // Returns containing sub-doc, or NULL if none.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsSubDoc() ) {
            break;
        }
        pElement = pElement->GetParent();
    }
    return (CEditSubDocElement*) pElement;
}

CEditSubDocElement* CEditElement::GetSubDocSkipRoot() {
    // Returns containing sub-doc, or NULL if none.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsSubDoc() && !pElement->IsRoot() ) {
            break;
        }
        pElement = pElement->GetParent();
    }
    return (CEditSubDocElement*) pElement;
}


CEditRootDocElement* CEditElement::GetRootDoc(){
    // Returns containing root.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsRoot() ) {
            break;
        }
        pElement = pElement->GetParent();
    }
    return (CEditRootDocElement*) pElement;
}

XP_Bool CEditElement::InMungableMailQuote(){
    // Returns true if this element is within a mungable mail quote.
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsSubDoc() ) {
            return FALSE;
        }
        else if ( pElement->IsList() ) {
            CEditListElement* pList = pElement->List();
            if ( pList->IsMailQuote() ) {
                return TRUE;
            }
        }
        pElement = pElement->GetParent();
    }
    return FALSE;
}

XP_Bool CEditElement::InMailQuote(){
  // Returns true if this element is within a mail quote.
  return (GetMailQuote() != NULL);
}

CEditListElement* CEditElement::GetMailQuote() {
    CEditElement* pElement = this;
    while ( pElement ) {
        if ( pElement->IsList() ) {
            CEditListElement* pList = pElement->List();
            if ( pList->IsMailQuote() ) {
                return pList;
            }
        }
        pElement = pElement->GetParent();
    }
    return NULL;
}

ED_Alignment CEditElement::GetDefaultAlignment(){
    if ( m_pParent )
        return m_pParent->GetDefaultAlignment();

    return ED_ALIGN_DEFAULT;
}

ED_Alignment CEditElement::GetDefaultVAlignment(){
    if ( m_pParent )
        return m_pParent->GetDefaultVAlignment();
    return ED_ALIGN_TOP;
}

CEditElement* CEditElement::UpLeft( PMF_EditElementTest pmf, void *pTestData ){
    CEditElement *pPrev = this;
    CEditElement* pRet;

    while( (pPrev = pPrev->GetPreviousSibling()) != NULL ){
        pRet = pPrev->DownLeft( pmf, pTestData );
        if( pRet ){
            return pRet;
        }
    }
    if( GetParent() ){
        return GetParent()->UpLeft( pmf, pTestData );
    }
    else{
        return 0;
    }
}

//
// All the children come before the node.
//
CEditElement* CEditElement::DownLeft( PMF_EditElementTest pmf, void *pTestData, 
            XP_Bool /* bIgnoreThis */ ){
    CEditElement *pChild;
    CEditElement *pRet;

    pChild = GetLastChild();
    while( pChild != NULL ){
        if( (pRet = pChild->DownLeft( pmf, pTestData )) != NULL ){
            return pRet;
        }
        pChild = pChild->GetPreviousSibling();
    }
    if( TestIsTrue( pmf, pTestData ) ){ 
        return this;
    }
    return 0;
}


CEditElement* CEditElement::FindPreviousElement( PMF_EditElementTest pmf, 
        void *pTestData ){
    return UpLeft( pmf, pTestData );
}

//-----------------------------------------------------------------------------
// forward navagation (right)
//-----------------------------------------------------------------------------
CEditElement* CEditElement::UpRight( PMF_EditElementTest pmf, void *pTestData ){
    CEditElement *pNext = this;
    CEditElement* pRet;

    while( (pNext = pNext->GetNextSibling()) != NULL ){
        pRet = pNext->DownRight( pmf, pTestData );
        if( pRet ){
            return pRet;
        }
    }
    if( GetParent() ){
        return GetParent()->UpRight( pmf, pTestData );
    }
    else{
        return 0;
    }
}


CEditElement* CEditElement::DownRight( PMF_EditElementTest pmf, void *pTestData, 
            XP_Bool bIgnoreThis ){

    CEditElement *pChild;
    CEditElement *pRet;

    if( !bIgnoreThis && TestIsTrue( pmf, pTestData ) ){ 
        return this;
    }
    pChild = GetChild();
    while( pChild != NULL ){
        if( (pRet = pChild->DownRight( pmf, pTestData )) != NULL ){
            return pRet;
        }
        pChild = pChild->GetNextSibling();
    }
    return 0;
}

CEditElement* CEditElement::FindNextElement( PMF_EditElementTest pmf, 
        void *pTestData ){
    CEditElement *pRet;

    pRet = DownRight( pmf, pTestData, TRUE );
    if( pRet ){
        return pRet;
    }
    return UpRight( pmf, pTestData );
}


// 
// routine looks for a valid text block for positioning during editing.
//
XP_Bool CEditElement::FindText( void* /*pVoid*/ ){ 
    CEditElement *pPrev;

    //
    // if this is a text block and the layout element actually points to 
    //  something, return it.
    //
    if( GetType() == P_TEXT ){
        CEditTextElement *pText = Text();
        if( pText->GetLen() == 0 ){
            //
            // Find only empty text blocks that occur at the beginning of
            //  a paragraph (as a paragraph place holder)
            //
            pPrev = FindPreviousElement( &CEditElement::FindTextAll, 0 );
            if( pPrev && pPrev->FindContainer() == FindContainer() ){
                return FALSE;
            }
        }
        return TRUE;
    }
    return FALSE;
}

XP_Bool CEditElement::FindImage( void* /*pVoid*/ ){ 
    return IsImage() ;
}

XP_Bool CEditElement::FindTarget( void* /*pVoid*/ ){ 
    return GetElementType() == eTargetElement ;
}

XP_Bool CEditElement::FindUnknownHTML( void* /*pVoid*/ ){ 
  return IsLeaf() && Leaf()->IsUnknownHTML();
}

// 
// routine looks for a valid text block for positioning during editing.
//
XP_Bool CEditElement::FindLeaf( void* pVoid ){ 
    if( !IsLeaf() ){
        return FALSE;
    }
    if( IsA(P_TEXT) ){
        return FindText( pVoid );
    }
    else {
        return TRUE;
    }
}


XP_Bool CEditElement::FindTextAll( void* /*pVoid*/ ){ 

    //
    // if this is a text block and the layout element actually points to 
    //  something, return it.
    //
    if( GetType() == P_TEXT ){
        return TRUE;
    }
    return FALSE;
}

XP_Bool CEditElement::FindLeafAll( void* /*pVoid*/ ){ 

    //
    // if this is a text block and the layout element actually points to 
    //  something, return it.
    //
    if( IsLeaf() ){
        return TRUE;
    }
    return FALSE;
}

XP_Bool CEditElement::FindTable( void* /*pVoid*/ ){ 
    return IsTable();
}

XP_Bool CEditElement::FindTableRow( void* /*pVoid*/ ){ 
    return IsTableRow();
}

XP_Bool CEditElement::FindTableCell( void* /*pVoid*/ ){ 
    return IsTableCell();
}

XP_Bool CEditElement::SplitContainerTest( void* /*pVoid*/ ){ 
    return BitSet( edt_setTextContainer,  GetType()  );
}

XP_Bool CEditElement::SplitFormattingTest( void* pVoid ){ 
    return (void*)GetType() == pVoid;
}

XP_Bool CEditElement::FindContainer( void* /*pVoid*/ ){
    return IsContainer();
}

XP_Bool CEditElement::GetWidth(XP_Bool * pPercent, int32 * pWidth) {
    PA_Tag *pTag = TagOpen(0);
    if( !pTag ){
        return FALSE;
    }
    XP_Bool bPercent;
    int32   iWidth;
    XP_Bool bDefined = edt_FetchDimension( pTag, 
                           PARAM_WIDTH, &iWidth, &bPercent, 0, FALSE, GetWinCSID() );
    if( bDefined ){
        if( pPercent ){
            *pPercent = bPercent;
        }
        if( pWidth ){
            *pWidth = iWidth;
        }
    }
    return bDefined;
}

XP_Bool CEditElement::GetHeight(XP_Bool * pPercent, int32 * pHeight) {
    PA_Tag *pTag = TagOpen(0);
    if( !pTag ){
        return FALSE;
    }
    XP_Bool bPercent;
    int32   iHeight;
    XP_Bool bDefined = edt_FetchDimension( pTag,
                           PARAM_HEIGHT, &iHeight, &bPercent, 0, FALSE, GetWinCSID() );
    if( bDefined ){
        if( pPercent ){
            *pPercent = bPercent;
        }
        if( pHeight ){
            *pHeight = iHeight;
        }
    }
    return bDefined;
}

void CEditElement::SetSize(XP_Bool /* bWidthPercent */, int32 /* iWidth */,
    XP_Bool /* bHeightPercent */, int32 /* iHeight */){
}

XP_Bool CEditElement::CanReflow() {
	return TRUE;
}

//-----------------------------------------------------------------------------
//  Default printing routines.
//-----------------------------------------------------------------------------

void CEditElement::PrintOpen( CPrintState *pPrintState ){
    InternalPrintOpen(pPrintState, m_pTagData);
}

void CEditElement::InternalPrintOpen( CPrintState *pPrintState, char* pTagData ){
    if( !(BitSet( edt_setCharFormat, GetType())
        || BitSet( edt_setSuppressNewlineBefore, GetType())) ){
        pPrintState->m_pOut->Write( "\n", 1 );
        pPrintState->m_iCharPos = 0;
    }

    if( pTagData && *pTagData != '>' ){
        char *pStr = pTagData;
        while( *pStr == ' ' ) pStr++;
        // Trim trailing white-space in-place
        {
            intn len = XP_STRLEN(pStr);
            while ( len > 1 && pStr[len-2] == ' ') {
                len--;
            }
            if ( len > 1 ) {
                pStr[len-1] = '>';
                pStr[len] = '\0';
            }
        }
            
        pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "<%s %s", 
                EDT_TagString(GetType()),pStr);
    }
    else {
        pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "<%s>", 
                EDT_TagString(GetType()) );
    }

    if ( BitSet( edt_setRequireNewlineAfter, GetType()) ){
        pPrintState->m_pOut->Write( "\n", 1 );
        pPrintState->m_iCharPos = 0;
    }
}

void CEditElement::PrintEnd( CPrintState *pPrintState ){
    if( TagHasClose( GetType() ) || BitSet( edt_setWriteEndTag,  GetType()  ) ){
        pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "</%s>", 
                EDT_TagString(GetType()) );
        if( !BitSet( edt_setCharFormat, GetType() ) ){
            pPrintState->m_pOut->Write( "\n", 1 );
            pPrintState->m_iCharPos = 0;
        }
    }
}


//-----------------------------------------------------------------------------
// Insertion routines
//-----------------------------------------------------------------------------
CEditElement* CEditElement::InsertAfter( CEditElement *pPrev ){
    XP_ASSERT(m_pParent == NULL);
    m_pParent = pPrev->GetParent();
    SetNextSibling(pPrev->GetNextSibling());
    pPrev->SetNextSibling(this);
    return m_pParent;
}

CEditElement* CEditElement::InsertBefore( CEditElement *pNext ){
    XP_ASSERT(m_pParent == NULL);
    XP_ASSERT(pNext != NULL);
    CEditElement *pPrev = pNext->GetPreviousSibling();
    if( pPrev == 0 ){
        InsertAsFirstChild( pNext->GetParent() );
    }
    else {
        m_pParent = pNext->GetParent();
        SetNextSibling(pPrev->m_pNext);
        pPrev->SetNextSibling(this);
    }
    return m_pParent;
}


void CEditElement::InsertAsFirstChild( CEditElement *pParent ){
    XP_ASSERT(m_pParent == NULL);
    m_pParent = pParent;
    SetNextSibling(pParent->GetChild());
    pParent->SetChild(this);
}

void CEditElement::InsertAsLastChild( CEditElement *pParent ){
    XP_ASSERT(m_pParent == NULL);
    m_pParent = pParent;
    SetNextSibling( 0 );
    CEditElement *pPrev = pParent->GetLastChild();
    if( pPrev == 0 ){
        pParent->SetChild(this);
    }
    else {
        pPrev->SetNextSibling(this);
    }
}



CEditElement* CEditElement::Split( CEditElement *pLastChild, 
            CEditElement* pCloneTree,
            PMF_EditElementTest pmf,
            void *pData ){

    CEditElement *pClone = Clone();
    pClone->SetChild(pCloneTree);

    if( pLastChild->m_pNext ){
        if( pCloneTree != 0 ){
            pCloneTree->SetNextSibling(pLastChild->m_pNext);
        }
        else {
            pClone->SetChild(pLastChild->m_pNext);
        }
        pLastChild->SetNextSibling( 0 );    
    }

    //
    // Reparent all the children that have been moved to the clone.
    //
    CEditElement* pNext = pClone->m_pChild;
    while( pNext ){
        pNext->m_pParent = pClone;
        pNext = pNext->GetNextSibling();
    }


    //
    // If we are at the container point
    //
    if( TestIsTrue( pmf, pData ) ){
        return pClone->InsertAfter( this );
    }
    else {
        return GetParent()->Split( this, pClone, pmf, pData );
    }

}

CEditElement* CEditElement::Clone( CEditElement *pParent ){
    return new CEditElement(pParent, GetType(), GetTagData());
}


//
// Copied formatting, returns the bottom of the chain (Child most formatting
//  element)
//
CEditElement* CEditElement::CloneFormatting( TagType endType ){
    if( GetType() == endType ){
        return 0;
    }
    else {
        return Clone( GetParent()->CloneFormatting(endType) );
    }
}

XP_Bool CEditElement::IsFirstInContainer(){
    CEditElement *pPrevious, *pContainer, *pPreviousContainer;

    pPrevious = PreviousLeaf();
    if( pPrevious ){
        pContainer = FindContainer();
        pPreviousContainer = pPrevious->FindContainer();
        if( pContainer != pPreviousContainer ){
            return TRUE;       
        }
        else {
            return FALSE;
        }
    }
    else {
        return TRUE;
    }
}

CEditTextElement* CEditElement::PreviousTextInContainer(){
    CEditElement *pPrevious, *pContainer, *pPreviousContainer;
    pPrevious = FindPreviousElement( &CEditElement::FindText, 0 );
    if( pPrevious ){
        pContainer = FindContainer();
        pPreviousContainer = pPrevious->FindContainer();
        if( pContainer != pPreviousContainer ){
            return 0;
        }
        else {
            return pPrevious->Text();
        }
    }
    else {
        return 0;
    }
}


CEditTextElement* CEditElement::TextInContainerAfter(){
    CEditElement *pNext, *pContainer, *pNextContainer;

    pNext = FindNextElement( &CEditElement::FindText, 0 );
    if( pNext ){
        pContainer = FindContainer();
        pNextContainer = pNext->FindContainer();
        if( pContainer != pNextContainer ){
            return 0;       
        }
        else {
            return pNext->Text();
        }
    }
    else {
        return 0;
    }
}

CEditLeafElement* CEditElement::PreviousLeafInContainer(){
    CEditElement *pPrevious, *pContainer, *pPreviousContainer;
    pPrevious = PreviousLeaf();
    if( pPrevious ){
        pContainer = FindContainer();
        pPreviousContainer = pPrevious->FindContainer();
        if( pContainer != pPreviousContainer ){
            return 0;
        }
        else {
            return pPrevious->Leaf();
        }
    }
    else {
        return 0;
    }
}


CEditLeafElement* CEditElement::LeafInContainerAfter(){
    CEditElement *pNext, *pContainer, *pNextContainer;

    pNext = NextLeaf();
    if( pNext ){
        pContainer = FindContainer();
        pNextContainer = pNext->FindContainer();
        if( pContainer != pNextContainer ){
            return 0;       
        }
        else {
            return pNext->Leaf();
        }
    }
    else {
        return 0;
    }
}

CEditLeafElement* CEditElement::NextLeafAll(XP_Bool bForward){
    if ( bForward ) {
        return (CEditLeafElement*) FindNextElement(&CEditElement::FindLeafAll, 0 );
    }
    else {
        return (CEditLeafElement*) FindPreviousElement(&CEditElement::FindLeafAll, 0 );
    }
}

CEditElement* CEditElement::GetRoot(){
    CEditElement* pRoot = this;
    while ( pRoot ) {
        CEditElement* pParent = pRoot->GetParent();
        if ( ! pParent ) break;
        pRoot = pParent;
    }
    return pRoot;
}

CEditElement* CEditElement::GetCommonAncestor(CEditElement* pOther){
    if ( this == pOther ) return this;

    // Start at root, and trickle down to find where they diverge

    CEditElement* pRoot = GetRoot();
    CEditElement* pRootOther = pOther->GetRoot();
    if ( pRoot != pRootOther ) return NULL;

    CEditElement* pCommon = pRoot;

    while ( pCommon ){
        CEditElement* pAncestor = pCommon->GetChildContaining(this);
        CEditElement* pAncestorOther = pCommon->GetChildContaining(pOther);
        if ( pAncestor != pAncestorOther ) break;
        pCommon = pAncestor;
    }
    return pCommon;
}

CEditElement* CEditElement::GetChildContaining(CEditElement* pDescendant){
    CEditElement* pParent = pDescendant;
    while ( pParent ) {
        CEditElement* pTemp = pParent->GetParent();
        if ( pTemp == this ) break; // Our direct child
        pParent = pTemp;
    }
    return pParent;
}


XP_Bool CEditElement::IsAcceptableChild(CEditElement& /* pChild */) {return TRUE;}

//
// Unlink from parent, but keep children
//
void CEditElement::Unlink(){
    CEditElement* pParent = GetParent();
    if( pParent ){
        CEditElement *pPrev = GetPreviousSibling();
        if( pPrev ){
            pPrev->SetNextSibling( GetNextSibling() );
        }
        else {
            pParent->SetChild(GetNextSibling());
        }
        m_pParent = 0;
        SetNextSibling( 0 );
    }

}

// LTNOTE: 01/01/96
//  We take take the characteristics of the thing that follows this paragraph
//  instead of keeping this paragraphs characteristics.
//
// jhp -- paste takes characteristic of second, but backspace takes
// characteristic of first, so we need a flag to control it.
// bCopyAppendAttributes == TRUE for the second (cut/paste)
// bCopyAppendAttributes == FALSE for the first (backspace/delete)

void CEditElement::Merge( CEditElement *pAppendBlock, XP_Bool bCopyAppendAttributes ){
    CEditElement* pChild = GetLastChild();
    CEditElement* pAppendChild = pAppendBlock->GetChild();

    // LTNOTE: 01/01/96 - I don't think this should be happening anymore.
    //   The way we deal with leaves and containers should keep this from 
    //   occuring...
    //
    // Check to see if pAppendBlock is a child of this.
    //    
    // LTNOTE: this is a case where we have
    // UL: 
    //   text: some text 
    //   LI: 
    //      text: XXX some More text
    //   LI:
    //      text: and some more.
    //
    // Merge occurs before xxx
    //  
    CEditElement *pAppendParent = pAppendBlock->GetParent();
    CEditElement *pAppendBlock2 = pAppendBlock;
    XP_Bool bDone = FALSE;
    while( !bDone && pAppendParent ){
        if( pAppendParent == this ){
            pChild = pAppendBlock2->GetPreviousSibling();            
            bDone = TRUE;
        }
        else {
            pAppendBlock2 = pAppendParent;
            pAppendParent = pAppendParent->GetParent();
        }
    }

    pAppendBlock->Unlink();

    if( pChild == 0 ){
        m_pChild = pAppendChild;
    }
    else {
        CEditElement *pOldNext = pChild->m_pNext;
        pChild->SetNextSibling( pAppendChild );

        // kind of sleazy.  we haven't updated the link, GetLastChild will
        //  return the end of pAppendBlock's children.
        GetLastChild()->SetNextSibling( pOldNext );
    }
    pAppendBlock->m_pChild = 0;

    while( pAppendChild ){
        pAppendChild->m_pParent = this;
        pAppendChild = pAppendChild->GetNextSibling();
    }

    //
    // LTNOTE: whack the container type to reflect the thing we just pulled 
    //  up. 
    //
    //XP_ASSERT( IsContainer() && pAppendBlock->IsContainer() );
    if( bCopyAppendAttributes && IsContainer() && pAppendBlock->IsContainer() ){
        Container()->CopyAttributes( pAppendBlock->Container() );
    }

    delete pAppendBlock;
}

// By default, yes for any non-atomic tag
XP_Bool CEditElement::IsContainerContainer(){
    return !BitSet( edt_setNoEndTag, GetType());
}

//
// Search up the tree for the element that contains us.
// - start with us.
CEditContainerElement* CEditElement::FindContainer(){
    CEditElement *pRet = this;
    while( pRet ){
        if ( pRet->IsSubDoc() && pRet != this ) break;
        if( pRet->IsContainer() ){
            return pRet->Container();
        }
        pRet = pRet->GetParent();
    }
    return 0;
}

//
// Search up the tree for the element that contains us.
// - skip us.

CEditContainerElement* CEditElement::FindEnclosingContainer(){
    CEditElement *pRet = this->GetParent();
    if ( pRet ){
        return pRet->FindContainer();
    }
    return 0;
}

void CEditElement::FindList( CEditContainerElement*& pContainer, 
            CEditListElement*& pList ) {
    
    pList = 0;
    XP_Bool bDone = FALSE;
    pContainer = FindEnclosingContainer();
    while( !bDone ){
        if( pContainer->GetParent()->IsList() ){
            pList = pContainer->GetParent()->List();
            bDone = TRUE;
        }
        else {
            CEditElement *pParentContainer = pContainer->FindEnclosingContainer();
            if( pParentContainer ){
                pContainer = pParentContainer->Container();
            }
            else {
                bDone = TRUE;
            }
        }
    }
}


CEditElement* CEditElement::FindContainerContainer(){
    CEditElement *pRet = this;
    while( pRet ){
        // Don't need explicit subdoc test because
        // sub-docs are paragraph containers.
        if ( pRet->IsContainerContainer() ){
            return pRet;
        }
        pRet = pRet->GetParent();
    }
    return 0;
}


#ifdef DEBUG
void CEditElement::ValidateTree(){
    CEditElement* pChild;
    CEditElement* pLoopFinder;
    // Make sure that all of our children point to us.
    // Makes sure all children are valid.
    // Make sure we don't have an infinite loop of children.
    // Use a second pointer that goes
    // around the loop twice as fast -- if it ever catches up with
    // pChild, there's a loop. (And yes, since you're wondering,
    // we did run into infinite loops here...)
    pChild = GetChild();
    pLoopFinder = pChild;
    while( pChild ){
        if( pChild->GetParent() != this ){
            XP_ASSERT(FALSE);
        }
        XP_ASSERT(IsAcceptableChild(*pChild));
        pChild->ValidateTree();
        for ( int i = 0; i < 2; i++ ){
            if ( pLoopFinder ) {
                pLoopFinder = pLoopFinder->GetNextSibling();
                if (pLoopFinder == pChild) {
                    XP_ASSERT(FALSE);
                    return;
                }
            }
        }
        pChild = pChild->GetNextSibling();
    }
}
#endif

CEditElement* CEditElement::Divide(int /* iOffset */){
    return this;
}

XP_Bool CEditElement::DeleteElement(CEditElement* pTellIfKilled){
    CEditElement *pKill = this;
    CEditElement *pParent;
    XP_Bool bKilled = FALSE;
    // Delete the table/cell sellection if element is in the list
    GetEditBuffer()->ClearTableIfContainsElement(pKill);
    do {
        pParent = pKill->GetParent();
        pKill->Unlink();
        if( pKill == pTellIfKilled ){
            bKilled = TRUE;
        }
        delete pKill;
        pKill = pParent;
    } while( pKill->GetChild() == 0 );
    return bKilled;
}

void CEditElement::DeleteChildren(){
    CEditElement* pNext = 0;
    for ( CEditElement* pChild = GetChild();
        pChild;
        pChild = pNext ) {
        pNext = pChild->GetNextSibling();
        pChild->Unlink();
        pChild->DeleteChildren();
        delete pChild;
    }
}

// class CEditSubDocElement

CEditSubDocElement::CEditSubDocElement(CEditElement *pParent, int tagType, char* pData)
    : CEditElement(pParent, tagType, pData)
{
}

CEditSubDocElement::CEditSubDocElement(CEditElement *pParent, PA_Tag *pTag, int16 csid)
    : CEditElement(pParent, pTag, csid)
{
}

CEditSubDocElement::CEditSubDocElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer)
    : CEditElement(pStreamIn, pBuffer)
{
}

void CEditSubDocElement::FinishedLoad( CEditBuffer* pBuffer ){
    CEditElement* pChild = GetChild();
    if ( ! pChild ) {
        // Subdocs have to have children.
        // Put an empty paragraph into any empty subdoc.
        pChild = CEditContainerElement::NewDefaultContainer( this,
                        GetDefaultAlignment() );
        // Creating it inserts it.
        (void) new CEditTextElement(pChild, 0);
    }
    // Put any leaves into a container.
    CEditContainerElement* pContainer = NULL;
    CEditElement* pNext = 0;
    for ( ;
        pChild;
        pChild = pNext) {
        pNext = pChild->GetNextSibling(); // We might unlink pChild
        if ( ! IsAcceptableChild(*pChild) ){
            if ( pChild->IsLeaf() ){
                if ( ! pContainer ){
                    pContainer = CEditContainerElement::NewDefaultContainer(NULL, GetDefaultAlignment());
                    pContainer->InsertAfter(pChild);
                }
                pChild->Unlink();
                pChild->InsertAsLastChild(pContainer);
            }
            else {
                XP_TRACE(("CEditSubDocElement: deleteing child of unknown type %d.\n",
                    pChild->GetElementType()));
                delete pChild;
                pChild = NULL;
            }
        }
        else {
            if ( pContainer ){
                pContainer->FinishedLoad(pBuffer);
                pContainer = NULL;
            }
        }
        if ( pChild ) {
            pChild->FinishedLoad(pBuffer);
        }
    }
    if ( pContainer ){
        pContainer->FinishedLoad(pBuffer);
        pContainer = NULL;
    }
}

XP_Bool CEditSubDocElement::Reduce( CEditBuffer* /* pBuffer */ ){
    return FALSE;
}

// class CEditTableElement

// Default table params for constructors and ::NewData()
//  Its alot easier to edit with > 1 pixels for these
//  Easier to select, size, and drag cells, rows, and columns
#define ED_DEFAULT_TABLE_BORDER 3
#define ED_DEFAULT_CELL_SPACING 3
// Space between edge of cell and contents
// Extra here makes it easier to place caret inside a cell
#define ED_DEFAULT_CELL_PADDING 3

CEditTableElement::CEditTableElement(intn columns, intn rows)
    : CEditElement(0, P_TABLE, 0),
      m_backgroundColor(),
      m_pBackgroundImage(0),
      m_iBackgroundSaveIndex(0),
      m_align(ED_ALIGN_LEFT),
      m_malign(ED_ALIGN_DEFAULT),
      m_iWidth(1),
      m_iWidthPixels(1),
      m_iHeight(1),
      m_iHeightPixels(1),
      m_bWidthPercent(FALSE),
      m_bHeightPercent(FALSE),
      m_iCellSpacing(ED_DEFAULT_CELL_SPACING),
      m_iCellPadding(ED_DEFAULT_CELL_PADDING),
      m_iCellBorder(0),
      m_pFirstCellInColumnOrRow(0),
      m_pNextCellInColumnOrRow(0),
      m_iGetRow(0),
      m_iRowY(0),
      m_iColX(0),
      m_bSpan(0),
      m_iRows(0),
      m_iColumns(0),
      m_pCurrentCell(0),
      m_bSaveWidthPercent(FALSE),
      m_bSaveHeightPercent(FALSE),
      m_bSaveWidthDefined(FALSE),
      m_bSaveHeightDefined(FALSE)
{
    EDT_TRY {
        for (intn row = 0; row < rows; row++ ){
            CEditTableRowElement* pRow = new CEditTableRowElement(columns);
            pRow->InsertAsFirstChild(this);
        }
    }
    EDT_CATCH_ALL {
        Finalize();
        EDT_RETHROW;
    }
}

CEditTableElement::CEditTableElement(CEditElement *pParent, PA_Tag *pTag, int16 csid, ED_Alignment align)
    : CEditElement(pParent, P_TABLE),
      m_backgroundColor(),
      m_pBackgroundImage(0),
      m_iBackgroundSaveIndex(0),
      m_align(align),
      m_malign(ED_ALIGN_DEFAULT),
      m_iWidth(1),
      m_iWidthPixels(1),
      m_iHeight(1),
      m_iHeightPixels(1),
      m_bWidthPercent(FALSE),
      m_bHeightPercent(FALSE),
      m_iCellSpacing(ED_DEFAULT_CELL_SPACING),
      m_iCellPadding(ED_DEFAULT_CELL_PADDING),
      m_iCellBorder(0),
      m_pFirstCellInColumnOrRow(0),
      m_pNextCellInColumnOrRow(0),
      m_iGetRow(0),
      m_iRowY(0),
      m_iColX(0),
      m_bSpan(0),
      m_iRows(0),
      m_iColumns(0),
      m_pCurrentCell(0),
      m_bSaveWidthPercent(FALSE),
      m_bSaveHeightPercent(FALSE),
      m_bSaveWidthDefined(FALSE),
      m_bSaveHeightDefined(FALSE)
{
    switch( m_align ) {
    case ED_ALIGN_CENTER:
        /* OK case -- 'center' tag. */
        m_align = ED_ALIGN_ABSCENTER;
        break;
    case ED_ALIGN_LEFT:
    case ED_ALIGN_ABSCENTER:
    case ED_ALIGN_RIGHT:
        break;
    default:
        XP_ASSERT(FALSE);
        /* Falls through */
    case ED_ALIGN_DEFAULT:
        m_align = ED_ALIGN_LEFT;
        break;
    }
    if( pTag ){
        EDT_TableData *pData = ParseParams(pTag, csid);
        pData->align = m_align;
        SetData(pData);
        FreeData(pData);
    }
}

CEditTableElement::CEditTableElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer)
    : CEditElement(pStreamIn, pBuffer),
      m_backgroundColor(),
      m_iBackgroundSaveIndex(0),
      m_pBackgroundImage(0),
      m_iWidth(1),
      m_iWidthPixels(1),
      m_iHeight(1),
      m_iHeightPixels(1),
      m_bWidthPercent(FALSE),
      m_bHeightPercent(FALSE),
      m_iCellSpacing(ED_DEFAULT_CELL_SPACING),
      m_iCellPadding(ED_DEFAULT_CELL_PADDING),
      m_iCellBorder(0),
      m_pFirstCellInColumnOrRow(0),
      m_pNextCellInColumnOrRow(0),
      m_iGetRow(0),
      m_iRowY(0),
      m_iColX(0),
      m_bSpan(0),
      m_iRows(0),
      m_iColumns(0),
      m_pCurrentCell(0),
      m_bSaveWidthPercent(FALSE),
      m_bSaveHeightPercent(FALSE),
      m_bSaveWidthDefined(FALSE),
      m_bSaveHeightDefined(FALSE)
{
    m_align = (ED_Alignment) pStreamIn->ReadInt();
    m_malign = (ED_Alignment) pStreamIn->ReadInt();

    // Get size data from stream -- needed for pasting tables/cells
    // We must get CSID from buffer since element is not in doc yet
    //TODO: Should we pass in CSID from paste stream instead?
    PA_Tag* pTag = CEditElement::TagOpen(0);
    EDT_TableData *pData = ParseParams( pTag, pBuffer->GetRAMCharSetID() );
    if(pData)
    {
        m_iWidth = pData->iWidth;
        m_bWidthPercent = pData->bWidthPercent;
        if( !m_bWidthPercent )
            m_iWidthPixels = m_iWidth;

        m_iHeight = pData->iHeight;
        m_bHeightPercent = pData->bHeightPercent;
        if( !m_bHeightPercent )
            m_iHeightPixels = m_iHeight;

        m_iCellSpacing = pData->iCellSpacing;
        m_iCellPadding = pData->iCellPadding;

        if( pData->bBorderWidthDefined && pData->iBorderWidth > 0 )
            m_iCellBorder = 1;
        else 
            m_iCellBorder = 0;

        EDT_FreeTableData(pData);
    }
    PA_FreeTag( pTag );
}

CEditTableElement::~CEditTableElement(){
    delete m_pBackgroundImage;
    DeleteLayoutData();
}

PA_Tag* CEditTableElement::TagOpen( int iEditOffset ){
    return InternalTagOpen(iEditOffset, FALSE);
}

PA_Tag* CEditTableElement::InternalTagOpen( int iEditOffset, XP_Bool bForPrinting){
    PA_Tag *pRet = 0;
    PA_Tag* pTag;

    // create the DIV tag if we need to.
    if( m_align == ED_ALIGN_ABSCENTER || m_align == ED_ALIGN_RIGHT ){
        pTag = XP_NEW( PA_Tag );
        XP_BZERO( pTag, sizeof( PA_Tag ) );
        if( m_align== ED_ALIGN_RIGHT ){
            SetTagData( pTag, "ALIGN=right>");
            pTag->type = P_DIVISION;
        }
        else {
            SetTagData( pTag, ">");
            pTag->type = P_CENTER;
        }
        pRet = pTag;
    }
    // create the actual table tag

    EDT_TableData* pTableData = GetData();
    char* pTagData = CreateTagData(pTableData, bForPrinting);
    if ( pTagData ) {
        pTag = XP_NEW( PA_Tag );
        XP_BZERO( pTag, sizeof( PA_Tag ) );
        SetTagData( pTag, pTagData );
        XP_FREE(pTagData);
    }
    else {
        pTag = CEditElement::TagOpen( iEditOffset );
    }
    FreeData(pTableData);

    // link the tags together.
    if( pRet == 0 ){
        pRet = pTag;
    }
    else {
        pRet->next = pTag;
    }
    return pRet;
}

PA_Tag* CEditTableElement::TagEnd( ){
    PA_Tag *pRet = CEditElement::TagEnd();
    if( m_align == ED_ALIGN_ABSCENTER || m_align == ED_ALIGN_RIGHT ){
        PA_Tag* pTag = XP_NEW( PA_Tag );
        XP_BZERO( pTag, sizeof( PA_Tag ) );
        pTag->is_end = TRUE;
        if( m_align == ED_ALIGN_RIGHT ){
            pTag->type = P_DIVISION;
        }
        else {
            pTag->type = P_CENTER;
        }

        if( pRet == 0 ){
            pRet = pTag;
        }
        else {
            pRet->next = pTag;
        }
    }
    return pRet;
}

CEditTableRowElement* CEditTableElement::GetFirstRow()
{
    CEditElement *pChild = GetChild();
    if( pChild && pChild->IsTableRow() )
    {
        return pChild->TableRow();
    }
    return NULL;
}

CEditTableCellElement* CEditTableElement::GetFirstCell()
{
    CEditElement *pChild = GetChild();
    if( pChild && pChild->IsTableRow() )
    {
        pChild = pChild->GetChild();
        if( pChild && pChild->IsTableCell() )
        {
            m_pCurrentCell = pChild->TableCell();
            // Initialize these also so we can follow
            //  this call with GetNextCellInRow() or GetNextCellInColumn()
            m_pFirstCellInColumnOrRow = m_pNextCellInColumnOrRow = m_pCurrentCell;
            return m_pCurrentCell;
        }
    }
    return NULL;
}

CEditTableCellElement* CEditTableElement::GetNextCellInTable(intn *pRowCounter) 
{ 
    if( m_pCurrentCell )
        m_pCurrentCell = m_pCurrentCell->GetNextCellInTable(pRowCounter);
    return m_pCurrentCell;
}

// The numbe of rows can simply be counted
intn CEditTableElement::CountRows()
{
    m_iRows = 0;
    for ( CEditElement* pChild = GetChild();
        pChild;
        pChild = pChild->GetNextSibling())
    {
        if ( pChild->IsTableRow() )
        {
            m_iRows++;
        }
    }
    return m_iRows;
}

// Counting columns is more difficult because ROWSPAN > 1 in a cell
//  pushes cells in following rows to the right.
//  Thus the total number of geometric columns may be greater
//  than a simple count of the number of cells in a row
// So we use a strategy similar to how we fill in m_ColumnLayoutData:
//  we count the number of unique X values for all cells
intn CEditTableElement::CountColumns()
{
    TXP_GrowableArray_int32 X_Array;
    CEditTableCellElement *pCell = GetFirstCell();

    while( pCell )
    {
        int32 iCellX = pCell->GetX();
        XP_Bool bFound = FALSE;

        // See if cell's X was already found
        intn iSize = X_Array.Size();
        for( intn i=0; i < iSize; i++ )
        {
            if( iCellX == X_Array[i] )
            {
                bFound = TRUE;
                break;
            }
        }
        if( !bFound )
            X_Array.Add(iCellX);

        pCell = GetNextCellInTable();
    }
    m_iColumns = X_Array.Size();

    return m_iColumns;
}

CEditTableRowElement* CEditTableElement::GetRow(int32 Y, intn *pRow)
{
    intn iRow = GetRowIndex(Y);
    if( iRow >= 0 )
    {
        CEditTableCellElement *pCell = GetFirstCellAtRowIndex(iRow);
        if( pRow )
            *pRow = iRow;

        if( pCell )
            return (CEditTableRowElement*)pCell->GetParent();
    }
    return NULL;

// Old method -- less efficient.
// Needed only if we must use GetRow() before table is layed out,
//  because thats when the m_RowLayoutData is built (in CEditBuffer::FixupTableData())
#if 0
    for ( CEditElement* pChild = GetChild();
            pChild;
                pChild = pChild->GetNextSibling())
    {
        if ( pChild->IsTableRow() )
        {
            // Scan row for first cell that has given Y as top
            CEditTableCellElement *pCell = (CEditTableCellElement*)(pChild->GetChild());
            while( pCell && pCell-IsTableCell() )
            {
                if( Y == pCell->GetY()  )
                {
                    if( pRow )
                        *pRow = iRow;
                    return pChild->TableRow();
                }
                pCell = (CEditTableCellElement*)(pCell->GetNextSibling());
            }
            iRow++;
        }
    }
    return NULL;
#endif
}

CEditTableCellElement* CEditTableElement::GetFirstCellInRow(CEditTableCellElement *pCell, XP_Bool bSpan)
{
    return GetFirstCellInRow(pCell->GetY(), bSpan);
}

// Get first cell of a "real" (geometric) row, based on location
// If bSpan == TRUE , this will also get a cell that spans given Y, 
//    even if top of cell != Y
CEditTableCellElement* CEditTableElement::GetFirstCellInRow(int32 Y, XP_Bool bSpan)
{

    CEditTableCellElement *pCell = GetFirstCell();
    m_iGetRow = 0;
    m_iRowY = Y;
    while( pCell )
    {
        int32 iCellY = pCell->GetY();
        if( (bSpan && (Y >= iCellY) && (Y <= (iCellY + pCell->GetHeight()))) ||
            (!bSpan && ( Y == iCellY)) )
        {
            m_pFirstCellInColumnOrRow = m_pNextCellInColumnOrRow = pCell;
            return pCell;
        }
        pCell = pCell->GetNextCellInTable(&m_iGetRow);
    }
    return NULL;
}

// Get next cell at or spaning the same geometric row as found by GetFirstCellInRow()
//  or use Y from supplied cell
CEditTableCellElement* CEditTableElement::GetNextCellInRow(CEditTableCellElement *pCell)
{
    if( pCell )
    {
        // Initialize using given cell,
        //  so we don't always have to call GetFirst...
        m_pNextCellInColumnOrRow = pCell;
        m_iRowY = pCell->GetY();
        m_bSpan = FALSE;
    }
    pCell = m_pNextCellInColumnOrRow->GetNextCellInTable(&m_iGetRow);

    while( pCell )
    {
        int32 iCellY = pCell->GetY();
        if( (m_bSpan && (m_iRowY >= iCellY) && (m_iRowY <= (iCellY + pCell->GetHeight()))) ||
            (!m_bSpan && ( m_iRowY == iCellY)) )
        {
            m_pNextCellInColumnOrRow = pCell;
            return pCell;
        }
        pCell = pCell->GetNextCellInTable(&m_iGetRow);
    }
    return NULL;
}

CEditTableCellElement* CEditTableElement::GetLastCellInRow(CEditTableCellElement *pCell)
{
    if( pCell )
    {
        // Move until next cell doesn't exist
        CEditTableCellElement *pNextCell;
        while( pCell && (pNextCell = GetNextCellInRow(pCell)) != NULL )
        {
            pCell = pNextCell;
        }
    }
    return pCell;
}

CEditTableCellElement* CEditTableElement::GetPreviousCellInRow(CEditTableCellElement *pCell)
{
    if( pCell )
    {
        // Start with the first cell..
        CEditTableCellElement *pFirstCell = GetFirstCellInRow(pCell->GetY());
        // We are the first cell - no previous exitst
        if( pFirstCell == pCell )
            return NULL;
    
        // Move until we find cell before us
        CEditTableCellElement *pPrevCell = pFirstCell;
        CEditTableCellElement *pNextCell;
        while( pPrevCell && (pNextCell = GetNextCellInRow(pPrevCell)) != pCell )
        {
            pPrevCell = pNextCell;    
        }
        return pPrevCell;
    }
    return NULL;
}

CEditTableCellElement* CEditTableElement::GetFirstCellAtColumnIndex(intn iIndex)
{
    if( iIndex >= 0 && iIndex < m_ColumnLayoutData.Size() )
        return m_ColumnLayoutData[iIndex]->pEdCell;

    return NULL;
}

CEditTableCellElement* CEditTableElement::GetFirstCellAtRowIndex(intn iIndex)
{
    if( iIndex >= 0 && iIndex < m_RowLayoutData.Size() )
        return m_RowLayoutData[iIndex]->pEdCell;

    return NULL;
}

// Get the defining left (for columns) and top (for rows) value from index
int32 CEditTableElement::GetColumnX(intn iIndex)
{
    if( iIndex < m_ColumnLayoutData.Size() )
        return m_ColumnLayoutData[iIndex]->X;

    return 0;
}

int32 CEditTableElement::GetRowY(intn iIndex)
{
    if( iIndex < m_RowLayoutData.Size() )
        return m_RowLayoutData[iIndex]->Y;

    return 0;
}

// Get grid coordinates of a cell
intn CEditTableElement::GetColumnIndex(int32 X)
{
    intn iCount = m_ColumnLayoutData.Size();
    for( intn i=0; i < iCount; i++ )
    {
        if( m_ColumnLayoutData[i]->X == X )
            return i;
    }
    return -1;
}

intn CEditTableElement::GetRowIndex(int32 Y)
{
    intn iCount = m_RowLayoutData.Size();
    for( intn i=0; i < iCount; i++ )
    {
        if( m_RowLayoutData[i]->Y == Y )
            return i;
    }
    return -1;
}


CEditTableCellElement* CEditTableElement::GetFirstCellInColumn(CEditTableCellElement *pCell, XP_Bool bSpan)
{
    return GetFirstCellInColumn(pCell->GetX(), bSpan);
}

// Get first cell of a "real" (geometric) row, based on location
// If bSpan is TRUE, this will also get a cell that spans 
//   given X, even if left edge of cell < X
CEditTableCellElement* CEditTableElement::GetFirstCellInColumn(int32 X, XP_Bool bSpan)
{
    CEditTableCellElement *pCell = GetFirstCell();
    m_iGetRow = 0;
    m_iColX = X;
    m_bSpan = bSpan;
    while( pCell )
    {
        int32 iCellX = pCell->GetX();
        // Note: Using GetWidth() is OK (and more efficient) here since all that matters is
        //  the cell's right edge spans (is > than) left-edge X value
        if( (!bSpan && X == iCellX) ||
            ( bSpan && X >= iCellX && X <= (iCellX + pCell->GetWidth()) ) )
        {
            m_pFirstCellInColumnOrRow = m_pNextCellInColumnOrRow = pCell;
            return pCell;
        }
        pCell = pCell->GetNextCellInTable(&m_iGetRow);
    }
    return NULL;
}

// Get next cell at or spaning the same geometric row as found by GetFirstCellInColumn()
//  or use X from supplied cell
CEditTableCellElement* CEditTableElement::GetNextCellInColumn(CEditTableCellElement *pCell)
{
    if( pCell )
    {
        // Initialize using given cell,
        //  so we don't always have to call GetFirst...
        m_pNextCellInColumnOrRow = pCell;
        m_iColX = pCell->GetX();
        m_bSpan = FALSE;
    }
    pCell = m_pNextCellInColumnOrRow->GetNextCellInTable(&m_iGetRow);

    while( pCell )
    {
        int32 iCellX = pCell->GetX();
        if( (!m_bSpan && m_iColX == iCellX) ||  // Must have exact match
            ( m_bSpan && m_iColX >= iCellX && m_iColX <= (iCellX + pCell->GetWidth()) ) ) //GetWidth is OK here
        {
            m_pNextCellInColumnOrRow = pCell;
            return pCell;
        }
        pCell = pCell->GetNextCellInTable(&m_iGetRow);
    }
    return NULL;
}

CEditTableCellElement* CEditTableElement::GetLastCellInColumn(CEditTableCellElement *pCell)
{
    if( pCell )
    {
        // Move until next cell doesn't exist
        CEditTableCellElement *pNextCell;
        while( pCell && (pNextCell = GetNextCellInColumn(pCell)) != NULL )
        {
            pCell = pNextCell;
        }
    }
    return pCell;
}

CEditTableCellElement* CEditTableElement::GetPreviousCellInColumn(CEditTableCellElement *pCell)
{
    if( pCell )
    {
        // Start with the first cell..
        CEditTableCellElement *pFirstCell = GetFirstCellInColumn(pCell->GetX());
        // We are the first cell - no previous exitst
        if( pFirstCell == pCell )
            return NULL;
    
        // Move until we find cell before us
        CEditTableCellElement *pPrevCell = pFirstCell;
        CEditTableCellElement *pNextCell;
        while( pPrevCell && (pNextCell = GetNextCellInColumn(pPrevCell)) != pCell )
        {
            pPrevCell = pNextCell;    
        }
        return pPrevCell;
    }
    return NULL;
}

CEditTableCellElement* CEditTableElement::GetFirstCellInNextColumn(int32 iCurrentColumnX)
{
    intn iCount = m_ColumnLayoutData.Size();
    intn i;

    // Scan data to find current cell's column in our layout data
    for( i = 0; i < iCount; i++ )
    {
        if( iCurrentColumnX == m_ColumnLayoutData[i]->X )
            break;
    }
    if( i+1 >= iCount )
        return NULL;    // No cell is after the current column

    return m_ColumnLayoutData[i+1]->pEdCell;
}

CEditTableCellElement* CEditTableElement::GetFirstCellInNextRow(int32 iCurrentRowY)
{
    intn iCount = m_RowLayoutData.Size();
    intn i;

    // Scan data to find current cell's row in our layout data
    for( i = 0; i < iCount; i++ )
    {
        if( iCurrentRowY == m_RowLayoutData[i]->Y )
            break;
    }
    if( i+1 >= iCount )
        return NULL;    // No cell is after the current row

    return m_RowLayoutData[i+1]->pEdCell;
}

// Figure out how many columns are included between given X values
intn CEditTableElement::GetColumnsSpanned(int32 iStartX, int32 iEndX)
{
    if( iStartX == iEndX )
        return 0;

    intn iColumns = 0;
    intn iTotalColumns = m_ColumnLayoutData.Size();
    for( intn i = 0; i < iTotalColumns; i++ )
    {
        if( iStartX <= m_ColumnLayoutData[i]->X &&
            iEndX > m_ColumnLayoutData[i]->X )
        {
            iColumns++;
        }
    }
    return iColumns;
}

// Test if any cell in the first row has COLSPAN > 1
XP_Bool CEditTableElement::FirstRowHasColSpan()
{
    CEditTableCellElement* pCell = GetFirstCell();
    while( pCell )
    {
        if( pCell->GetColSpan() > 1 )
            return TRUE;

        pCell = GetNextCellInRow();
    } 

    return FALSE;
}

// Currently (4/24/98) not called
// This should be done at least the before the first time a table is layed out
//   when a file is loaded. It fixes problems that cause major headaches with
//   rowspan and columnspan
// TODO: Should we also pad rows to insure that table is rectangular? (no ragged right edge)
void CEditTableElement::FixupColumnsAndRows()
{
    CEditTableCellElement *pFirstCell = GetFirstCell();
    if( !pFirstCell )
        return;

    TXP_GrowableArray_int32 X_Array;
    TXP_GrowableArray_int32 Y_Array;
    CEditTableCellElement *pCell = pFirstCell;

    while( pCell )
    {
        int32 iCellX = pCell->GetX();
        int32 iCellY = pCell->GetY();
        XP_Bool bFoundX = FALSE;
        XP_Bool bFoundY = FALSE;

        // See if cell's X was already found
        intn iSize = X_Array.Size();
        for( intn i=0; i < iSize; i++ )
        {
            if( iCellX == X_Array[i] )
            {
                bFoundX = TRUE;
                break;
            }
        }
        if( !bFoundX )
            X_Array.Add(iCellX);

        pCell = GetNextCellInTable();
    }
    m_iColumns = X_Array.Size();

    intn iColumns = CountColumns(); //m_ColumnLayoutData.Size();
    intn iRows = CountRows(); //m_RowLayoutData.Size();
    intn i, iMinSpan, iDecrease;
    // We will find the maximum number of columns in all rows
    m_iColumns = 0;

    for ( i = 0; i < iColumns; i++ )
    {
        CEditTableCellElement *pFirstCellInCol = m_ColumnLayoutData[i]->pEdCell;
        pCell = pFirstCellInCol; 
        iMinSpan = 2;

        // Scan column to check for minimum COLSPAN
        while( pCell )
        {
            if( pCell->GetColSpan() < iMinSpan )
                iMinSpan = pCell->GetColSpan();

            // Any cell with value of 1 means column is OK
            if( iMinSpan == 1 )
                break;

            pCell = GetNextCellInColumn(pCell);
        }
        if( iMinSpan > 1 )
        {
            // We have a bad column
            // Go back through to fixup COLSPAN values
            pCell = pFirstCellInCol;
            iDecrease = iMinSpan - 1;

            while( pCell )
            {
                pCell->DecreaseColSpan(iDecrease);
                pCell = GetNextCellInColumn(pCell);
            }
        }
        intn iCellsInRow = m_ColumnLayoutData[i]->iCellsInRow;
        CEditTableRowElement *pRow = (CEditTableRowElement*)pCell->GetParent();
        if( pRow )
            pRow->SetColumns(iCellsInRow);
            
        if( m_iColumns > iCellsInRow)
            m_iColumns = iCellsInRow;
    }

    // Do the same for ROWSPAN
    for ( i = 0; i < iRows; i++ )
    {
        CEditTableCellElement *pFirstCellInRow = m_RowLayoutData[i]->pEdCell;
        pCell = pFirstCellInRow; 
        iMinSpan = 2;

        // Scan Row to check for minimum ROWSPAN
        while( pCell )
        {
            if( pCell->GetRowSpan() < iMinSpan )
                iMinSpan = pCell->GetRowSpan();

            if( iMinSpan == 1 ) // Row is OK
                break;

            pCell = GetNextCellInRow(pCell);
        }
        if( iMinSpan > 1 )
        {
            // We have a bad row
            // Go back through to fixup ROWSPAN values
            pCell = pFirstCellInRow;
            iDecrease = iMinSpan - 1;

            while( pCell )
            {
                pCell->DecreaseRowSpan(iDecrease);
                pCell = GetNextCellInRow(pCell);
            }
        }
    }
}


void CEditTableElement::InsertRows(int32 Y, int32 iNewY, intn number, CEditTableElement* pSource)
{
    if ( number == 0 )
    {
        return; /* A waste of time, but legal. */
    }
    if ( number < 0 || Y < 0 )
    {
        /* Illegal. */
        XP_ASSERT(FALSE);
        return;
    }
    intn iRow;
    CEditTableRowElement* pRow = GetRow(Y, &iRow);
    if ( ! pRow ) {
        XP_ASSERT(FALSE);
        return;
    }
    // We may need this later
    //CEditTableRowElement* pNextRow = pRow->GetNextRow();

    // We must examine cells with same geometric row,
    //  but in different logical rows
    // So scan all cells near given Y and increase ROWSPAN
    //  where needed and count actual number of cells 
    //  to be inserted in new row(s)
    intn iColumns = 0;

    CEditTableCellElement *pCell = GetFirstCellInRow(Y, FALSE /*TRUE*/);
    while( pCell )
    {
        int32 iCellY = pCell->GetY();
        int32 iColSpan = pCell->GetColSpan();
        int32 iRowSpan = pCell->GetRowSpan();
        if( Y == iNewY )
        {
            // Inserting above
            
            if( iCellY == Y )
            {
                // We will be inserting cells above this one
                iColumns += iColSpan;
            } else {
                // Cell is actually in row above,
                //   so just increase its rowspan
                pCell->IncreaseRowSpan(number);
            }
        } else {
            // Inserting below current row
            if( iNewY == (iCellY + pCell->GetHeight()) )
            {
                // Bottom of this cell is where we want to insert
                iColumns += iColSpan;
            } else {
                // Cell spans the new insert Y, so just expand some more
                pCell->IncreaseRowSpan(number);
            }
        }
        pCell = GetNextCellInRow();
    }
    CEditBuffer *pBuffer = GetEditBuffer();

    // Now insert the new rows (including iColumns new cells in each)
    for ( intn row = 0; row < number; row++ )
    {
        CEditTableRowElement* pNewRow;
        if ( pSource ) {
            pNewRow = pSource->FindRow(0);
            pNewRow->Unlink();
        }
        else {
            pNewRow = new CEditTableRowElement(iColumns);
            if( !pNewRow )
                return;
            pBuffer->SetFillNewCellWithSpace();
        }
        if( Y == iNewY )
            pNewRow->InsertBefore(pRow);
        else 
            pNewRow->InsertAfter(pRow);

        pNewRow->FinishedLoad(pBuffer);
    }
    pBuffer->ClearFillNewCellWithSpace();
}

void CEditTableElement::InsertColumns(int32 X, int32 iNewX, intn number, CEditTableElement* pSource){
    if ( number == 0 )
    {
        return; /* A waste of time, but legal. */
    }
    if ( number < 0 || X < 0 )
    {
        /* Illegal. */
        XP_ASSERT(FALSE);
        return;
    }

    CEditTableRowElement* pRow = GetFirstRow();

    while( pRow )
    {
        CEditTableRowElement* pSourceRow = pSource ? pSource->FindRow(0) : 0;
        pRow->InsertCells(X, iNewX, number, pSourceRow);
        if( pSourceRow )
            delete pSourceRow;
        
        pRow = pRow->GetNextRow();
    }
}

// Note: no saving to pUndoContainer any more
void CEditTableElement::DeleteRows(int32 Y, intn number)
{
    if ( number == 0 ) {
        return; /* A waste of time, but legal. */
    }
    if ( number < 0 || Y < 0 ){
        /* Illegal. */
        XP_ASSERT(FALSE);
        return;
    }

    intn iRowIndex = GetRowIndex(Y);
    // Abort if invalid Y value
    if( iRowIndex == -1 )
        return;    

    // Restrict number of rows we can delete
    intn iLastIndex = min(iRowIndex + number - 1, m_iRows-1);

    for( ; iRowIndex <= iLastIndex; iRowIndex++ )
    {
        Y = GetRowY(iRowIndex);
        CEditTableRowElement *pRow = GetRow(Y);

        // Get first cell starting at or spanning the Y value (TRUE)
        CEditTableCellElement *pCell = GetFirstCellInRow(Y, TRUE);
        while( pCell )
        {
            // Get next cell before we delete the one we're at
            CEditTableCellElement *pNextCell = GetNextCellInRow();

            if( Y == pCell->GetY() )
            {
                // Cell is in same row as delete row
                // Unselect if selected and delete the cell
                if( pCell->IsSelected() )
                    GetEditBuffer()->SelectCell(FALSE, NULL, pCell);

                delete pCell;
                // Note: We don't worry about deleting cells with rowspan > 1
                //  even though that might upset the column layout for cells
                //  below the deleted rows. 
                // We might want to revisit this later -- we would have to 
                //  insert blank cells into rows below when rowspan > number
                //  of rows we are deleting
            }
            else if( pCell->GetRowSpan() > 1 )
            {
                // Cell spans the delete row 
                //  (its actually in a row above the delete row)
                // Reduce its rowspan instead of deleting
                pCell->DecreaseRowSpan(1);
            }
#ifdef DEBUG
            else
                XP_TRACE(("CEditTableElement::DeleteRows: Rowspan doesnt agree with rowlayout"));
#endif
            pCell = pNextCell;
        }
        // Delete the row if all cells were deleted (should always be true?)
        // VERY IMPORTANT. Never call anything to cause
        //   relayout else our m_RowLayoutData and entire coordinate
        //   system will be altered
        if( pRow && pRow->GetChild() == NULL )
            delete pRow;
    }
}

void CEditTableElement::DeleteColumns(int32 X, intn number, CEditTableElement* pUndoContainer){
    if ( number == 0 )
    {
        return; /* A waste of time, but legal. */
    }
    if ( number < 0 || X < 0 )
     {
        /* Illegal. */
        XP_ASSERT(FALSE);
        return;
    }
    CEditTableRowElement* pRow = GetFirstRow();
    
    while( pRow )
    {
        CEditTableRowElement* pUndoRow = NULL;
        if ( pUndoContainer )
        {
            pUndoRow = new CEditTableRowElement();
            pUndoRow->InsertAsLastChild(pUndoContainer);
        }
        pRow->DeleteCells(X, number, pUndoRow);
        CEditTableRowElement *pNextRow = pRow->GetNextRow();

        // Check if ALL cells were deleted - delete row if all were
        if( !pRow->GetChild() )
            delete pRow;

        pRow = pNextRow;
    }
}

// This is the old version - just finds row based on counting,
//   no ROWSPAN effect needed
CEditTableRowElement* CEditTableElement::FindRow(intn number)
{
    intn count = 0;
    if ( number < 0 )
    {
        XP_ASSERT(FALSE);
        return NULL;
    }
    for ( CEditElement* pChild = GetChild();
            pChild;
                pChild = pChild->GetNextSibling())
    {
        if ( pChild->IsTableRow() )
        {
            if ( count++ == number )
            {
                return (CEditTableRowElement*) pChild;
            }
        }
    }
    return NULL;
}

CEditCaptionElement* CEditTableElement::GetCaption(){
    // Normally first or last, but we check everybody to be robust.
    for ( CEditElement* pChild = GetChild();
        pChild;
        pChild = pChild->GetNextSibling()) {
        if ( pChild->IsCaption() ) {
            return (CEditCaptionElement*) pChild;
        }
    }
    return NULL;
}

void CEditTableElement::SetCaption(CEditCaptionElement* pCaption){
    DeleteCaption();
    if ( pCaption ){
        EDT_TableCaptionData* pData = pCaption->GetData(GetWinCSID());
        XP_Bool bAddAtTop = ! ( pData->align == ED_ALIGN_BOTTOM || pData->align == ED_ALIGN_ABSBOTTOM);
        CEditCaptionElement::FreeData(pData);
        if ( bAddAtTop ){
            pCaption->InsertAsFirstChild(this);
        }
        else {
            pCaption->InsertAsLastChild(this);
        }
    }
}

void CEditTableElement::DeleteCaption(){
    CEditCaptionElement* pOldCaption = GetCaption();
    delete pOldCaption;
}

void CEditTableElement::FinishedLoad( CEditBuffer* pBuffer ){
    // From experimentation, we know that the 2.0 navigator dumps
    // tags that aren't in td, tr, or caption cells into the doc
    // before the table.
    //
    // That's a little too radical for me. So what I do is
    // wrap tds in trs, put anything else
    // into a caption

    CEditCaptionElement* pCaption = NULL;
    CEditTableRowElement* pRow = NULL;
    CEditElement* pNext = 0;
    
    // TODO: MAKE THIS WORK BEFORE LAYOUT HAPPENS (no m_ColumnLayoutData yet)
    //FixupColumnsAndRows();
    
    // For efficiency, count rows only
    //  if we haven't layed out table
    if( m_iRows <= 0 )
        m_iRows = CountRows();

    if ( m_iRows <= 0 ) {
        // Force a row
        CEditTableRowElement* pTempRow = new CEditTableRowElement();
        pTempRow->InsertAsFirstChild(this);
    }
    
	CEditElement* pChild;
    for ( pChild = GetChild();
        pChild;
        pChild = pNext) {
        pNext = pChild->GetNextSibling(); // We might unlink pChild
        if ( ! IsAcceptableChild(*pChild) ){
            if ( pChild->IsTableCell() ){
                if ( ! pRow ){
                    pRow = new CEditTableRowElement();
                    pRow->InsertAfter(pChild);
                }
                pChild->Unlink();
                pChild->InsertAsLastChild(pRow);
                if ( pCaption ){
                    pCaption->FinishedLoad(pBuffer);
                    pCaption = NULL;
                }
            }
            else {
                // Put random children into a caption
                if ( ! pCaption ){
                    pCaption = new CEditCaptionElement();
                    pCaption->InsertAfter(pChild);
                }
                pChild->Unlink();
                pChild->InsertAsLastChild(pCaption);
                if ( pRow ){
                    pRow->FinishedLoad(pBuffer);
                    pRow = NULL;
                }
            }
        }
        else {
            if ( pRow ){
                pRow->FinishedLoad(pBuffer);
                pRow = NULL;
            }
            if ( pCaption ){
                pCaption->FinishedLoad(pBuffer);
                pCaption = NULL;
            }
        }
        pChild->FinishedLoad(pBuffer);
    }
    if ( pRow ){
        pRow->FinishedLoad(pBuffer);
        pRow = NULL;
    }
    if ( pCaption ){
        pCaption->FinishedLoad(pBuffer);
        pCaption = NULL;
    }

    // Merge all captions together.
    pCaption = NULL;
    for ( pChild = GetChild();
        pChild;
        pChild = pNext) {
        pNext = pChild->GetNextSibling(); // We might unlink pChild
        if ( pChild->IsCaption() ){
            if ( pCaption ) {
                // This is an extra caption. Merge its contents into the other caption.
                pChild->Unlink();
                CEditElement* pItem;
                while ( ( pItem = pChild->GetChild() ) != NULL ){
                    pItem->Unlink();
                    pItem->InsertAsLastChild(pCaption);
                }
                delete pChild;
            }
            else {
                pCaption = (CEditCaptionElement*) pChild;
            }
        }
    }
    AdjustCaption();

    EnsureSelectableSiblings(pBuffer);
}

void CEditTableElement::AdjustCaption() {
    CEditCaptionElement* pCaption = GetCaption();
    if ( pCaption ) {
        // Now move the caption to the start or the end of the table, as appropriate.
        pCaption->Unlink();
        SetCaption(pCaption);
    }
}

void CEditTableElement::StreamOut( IStreamOut *pOut){
    CEditElement::StreamOut( pOut );
    pOut->WriteInt( (int32)m_align );
    pOut->WriteInt( (int32)m_malign );
}

void CEditTableElement::SetData( EDT_TableData *pData ){
    char *pNew = CreateTagData(pData, FALSE);
    CEditElement::SetTagData( pNew );
    m_align = pData->align;
    m_malign = pData->malign; // Keep track of alignment seperately to avoid crashing when editing.
    if ( pNew ) {
        free(pNew); // XP_FREE?
    }
    // Save these since they are not saved in tag data if bWidthPercent = FALSE
    m_iWidthPixels = pData->iWidthPixels;
    m_iWidth = pData->iWidth;
    m_bWidthPercent = pData->bWidthPercent;

    m_iHeightPixels = pData->iHeightPixels;
    m_iHeight = pData->iHeight;
    m_bHeightPercent = pData->bHeightPercent;

    m_iCellSpacing = pData->iCellSpacing;
    m_iCellPadding = pData->iCellPadding;
    if( pData->bBorderWidthDefined && pData->iBorderWidth > 0 )
        m_iCellBorder = 1;
    else 
        m_iCellBorder = 0;

    m_iRows = pData->iRows;
    m_iColumns = pData->iColumns;
}

char* CEditTableElement::CreateTagData(EDT_TableData *pData, XP_Bool bPrinting) {
    char *pNew = 0;
    m_align = pData->align;
    m_malign = pData->malign;
    if( bPrinting && pData->malign != ED_ALIGN_DEFAULT ){
        pNew = PR_sprintf_append( pNew, "ALIGN=%s ", EDT_AlignString(pData->malign) );
    }
    // If you don't mention the border, you get a zero pixel border.
    // If you just say BORDER, it's like BORDER=1
    if ( pData->bBorderWidthDefined ){
        if ( pData->iBorderWidth == 1 ) {
            /* Neatness counts. */
            pNew = PR_sprintf_append( pNew, "BORDER ");
        }
        else {
            pNew = PR_sprintf_append( pNew, "BORDER=%d ", pData->iBorderWidth );
        }
    }
    if ( pData->iCellSpacing != 1 ){
        pNew = PR_sprintf_append( pNew, "CELLSPACING=%d ", pData->iCellSpacing );
    }
    if ( pData->iCellPadding != 1 ){
        pNew = PR_sprintf_append( pNew, "CELLPADDING=%d ", pData->iCellPadding );
    }
    if ( pData->bUseCols ){
        int cols = (int) GetColumns(); //Should we use CountColumns()?
        if ( cols == 0 ) {
            cols = (int) pData->iColumns;
        }
        pNew = PR_sprintf_append( pNew, "COLS=%d ", cols );
    }
    if( pData->bWidthDefined ){
        if( pData->bWidthPercent ){
            pNew = PR_sprintf_append( pNew, "WIDTH=\"%ld%%\" ", 
                                      (long)min(100,max(1,pData->iWidth)) );
        }
        else {
            pNew = PR_sprintf_append( pNew, "WIDTH=\"%ld\" ", (long)pData->iWidth );
        }
    }
    if( pData->bHeightDefined ){
        if( pData->bHeightPercent ){
            pNew = PR_sprintf_append( pNew, "HEIGHT=\"%ld%%\" ", 
                                      (long)min(100,max(1, pData->iHeight)) );
        }
        else {
            pNew = PR_sprintf_append( pNew, "HEIGHT=\"%ld\" ", (long)pData->iHeight );
        }
    }
    if ( pData->pColorBackground ) {
        SetBackgroundColor(EDT_LO_COLOR(pData->pColorBackground));
        pNew = PR_sprintf_append( pNew, "BGCOLOR=\"#%06lX\" ", GetBackgroundColor().GetAsLong() );
    }
    else {
        SetBackgroundColor(ED_Color::GetUndefined());
    }
    if ( pData->pBackgroundImage ) {
        SetBackgroundImage(pData->pBackgroundImage);
        pNew = PR_sprintf_append( pNew, "BACKGROUND=\"%s\" ", pData->pBackgroundImage );
    }
    else {
        SetBackgroundImage(0);
    }
    if ( pData->bBackgroundNoSave) {
        pNew = PR_sprintf_append( pNew, "NOSAVE " );
    }

    if( pData->pExtra  ){
        pNew = PR_sprintf_append( pNew, "%s ", pData->pExtra );
    }

    if( pNew ){
        pNew = PR_sprintf_append( pNew, ">" );
    }
    return pNew;
}

EDT_TableData* CEditTableElement::GetData()
{
    EDT_TableData *pRet;
    PA_Tag* pTag = CEditElement::TagOpen(0);

    pRet = ParseParams( pTag, GetWinCSID() );
    // Alignment isn't kept in the tag data, because tag data is used for editing,
    // and we crash if we try to edit an aligned table, because layout doesn't
    // set up the data structures that we need.
    pRet->align = m_align;
    pRet->malign = m_malign;
    // Return the widths determined by layout
    pRet->iWidth = m_iWidth;
    pRet->iWidthPixels = m_iWidthPixels;

    pRet->iHeight = m_iHeight;
    pRet->iHeightPixels = m_iHeightPixels;

    pRet->iCellSpacing = m_iCellSpacing;
    pRet->iCellPadding = m_iCellPadding;
    pRet->iRows = m_iRows;
    pRet->iColumns = m_iColumns;
    PA_FreeTag( pTag );
    return pRet;
}

static char *tableParams[] = {
    PARAM_ALIGN,
    PARAM_BORDER,
    PARAM_CELLSPACE,
    PARAM_CELLPAD,
    PARAM_COLS,
    PARAM_WIDTH,
    PARAM_HEIGHT,
    PARAM_BGCOLOR,
    PARAM_BACKGROUND,
    PARAM_NOSAVE,
    0
};

EDT_TableData* CEditTableElement::ParseParams( PA_Tag *pTag, int16 csid ){
    EDT_TableData *pData = NewData();
    ED_Alignment malign;
    
    malign = edt_FetchParamAlignment( pTag, ED_ALIGN_DEFAULT, FALSE, csid );
    // Center not supported in 3.0
    if( malign == ED_ALIGN_RIGHT || malign == ED_ALIGN_LEFT || malign == ED_ALIGN_DEFAULT ){
        pData->malign = malign;
    }

    pData->iRows = GetRows();        // Should we use CountRows()? NO THIS IS CALLED EVERY GetData call
    pData->iColumns = GetColumns();  // Should we use CountColumns()?

    // If you just say BORDER, it's the same as BORDER=1
    // If you say BORDER=0, it's different than not saying BORDER at all
    pData->bBorderWidthDefined = edt_FetchParamBoolExist(pTag, PARAM_BORDER, csid);
    pData->iBorderWidth = edt_FetchParamInt(pTag, PARAM_BORDER, 0, 1, csid);
    pData->iCellSpacing = edt_FetchParamInt(pTag, PARAM_CELLSPACE, 1, csid);
    pData->iCellPadding = edt_FetchParamInt(pTag, PARAM_CELLPAD, 1, csid);
    {
        // If we're parsing a table tag, and we've hit a cols= property, we
        // need to remember the number of columns.
        int columns = edt_FetchParamInt(pTag, PARAM_COLS, 0, csid);
        pData->bUseCols = columns > 0;
        if ( pData->bUseCols ) {
            pData->iColumns = columns;
        }
    }
    pData->bWidthDefined = edt_FetchDimension( pTag, PARAM_WIDTH, 
                    &pData->iWidth, &pData->bWidthPercent,
                    100, TRUE, csid );
    pData->bHeightDefined = edt_FetchDimension( pTag, PARAM_HEIGHT, 
                    &pData->iHeight, &pData->bHeightPercent,
                    100, TRUE, csid );

    // If width and/or height are NOT defined, use the
    //    "bPercent" values set by any previous SetData() calls
    if( !pData->bWidthDefined )
        pData->bWidthPercent = m_bWidthPercent;
    if( !pData->bHeightDefined )
        pData->bHeightPercent = m_bHeightPercent;


    pData->pColorBackground = edt_MakeLoColor(edt_FetchParamColor( pTag, PARAM_BGCOLOR, csid ));
    pData->pBackgroundImage = edt_FetchParamString( pTag, PARAM_BACKGROUND, csid );
    pData->bBackgroundNoSave = edt_FetchParamBoolExist( pTag, PARAM_NOSAVE, csid );
    pData->pExtra = edt_FetchParamExtras( pTag, tableParams, csid );

    return pData;
}

EDT_TableData* CEditTableElement::NewData(){
    EDT_TableData *pData = XP_NEW( EDT_TableData );
    if( pData == 0 ){
        // throw();
        return pData;
    }
    pData->align = ED_ALIGN_LEFT;
    pData->malign = ED_ALIGN_DEFAULT;
    // While this helps layout, it causes lots of problems
    //  in layout when COLSPAN > 1 an ROWSPAN > 1
    pData->bUseCols = FALSE;
    pData->iRows = 1;
    pData->iColumns = 1;
    pData->bBorderWidthDefined = TRUE;
    pData->iBorderWidth = ED_DEFAULT_TABLE_BORDER;
    pData->iCellSpacing = ED_DEFAULT_CELL_SPACING;
    pData->iCellPadding = ED_DEFAULT_CELL_PADDING;
    pData->bWidthDefined = FALSE;
    pData->bWidthPercent = FALSE; // Percent mode is a pain
    pData->iWidth = 1;
    pData->iWidthPixels = 1;
    pData->bHeightDefined = FALSE;
    pData->bHeightPercent = FALSE;
    pData->iHeight = 1;
    pData->iHeightPixels = 1;
    pData->pColorBackground = 0;
    pData->pBackgroundImage = 0;
    pData->bBackgroundNoSave = FALSE;
    pData->pExtra = 0;
    return pData;
}

void CEditTableElement::FreeData( EDT_TableData *pData ){
    if( pData->pColorBackground ) XP_FREE( pData->pColorBackground );
    if( pData->pBackgroundImage ) XP_FREE( pData->pBackgroundImage );
    if( pData->pExtra ) XP_FREE( pData->pExtra );
    XP_FREE( pData );
}

void CEditTableElement::SetBackgroundColor( ED_Color iColor ){
    m_backgroundColor = iColor;
}

ED_Color CEditTableElement::GetBackgroundColor(){
    return m_backgroundColor;
}

void CEditTableElement::SetBackgroundImage( char* pImage ){
    delete m_pBackgroundImage;
    m_pBackgroundImage = 0;
    if ( pImage ) {
        m_pBackgroundImage = XP_STRDUP(pImage);
    }
}

char* CEditTableElement::GetBackgroundImage(){
    return m_pBackgroundImage;
}

void CEditTableElement::PrintOpen( CPrintState *pPrintState ){
    pPrintState->m_iCharPos = 0;
    pPrintState->m_pOut->Printf( "\n"); 

    PA_Tag *pTag = InternalTagOpen(0, TRUE);
    while( pTag ){
        char *pData = 0;
        if( pTag->data ){
            PA_LOCK( pData, char*, pTag->data );
        }

        if( pData && *pData != '>' ) {
            pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "<%s %s", 
                    EDT_TagString(pTag->type), pData );
        }
        else {
            pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "<%s>", 
                    EDT_TagString(pTag->type));
        }

        if( pTag->data ){
            PA_UNLOCK( pTag->data );
        }

        PA_Tag* pDelTag = pTag;
        pTag = pTag->next;
        PA_FreeTag(pDelTag);
    }
}

void CEditTableElement::PrintEnd( CPrintState *pPrintState ){
    PA_Tag *pTag = TagEnd();
    while( pTag ){
        char *pData = 0;
        if( pTag->data ){
            PA_LOCK( pData, char*, pTag->data );
        }

        pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "</%s>",
            EDT_TagString( pTag->type ) );

        if( pTag->data ){
            PA_UNLOCK( pTag->data );
        }

        PA_Tag* pDelTag = pTag;
        pTag = pTag->next;
        PA_FreeTag(pDelTag);
    }

    pPrintState->m_iCharPos = 0;
    pPrintState->m_pOut->Printf( "\n"); 
}

LO_TableStruct* CEditTableElement::GetLoTable()
{
    // Find the first leaf element in the table since
    //  it will have pointer to LO_Element
    CEditElement *pLeaf = FindNextElement(&CEditElement::FindLeafAll,0 );
    if( pLeaf )
    {
        // Only leaf edit elements have their pointers saved in LO_Elements
        LO_Element *pLoElement = pLeaf->Leaf()->GetLayoutElement();
        if( pLoElement )
        {
            // Find enclosing LO cell
            LO_TableStruct *pLoTable = lo_GetParentTable(GetEditBuffer()->m_pContext, pLoElement);
            // We shoul ALWAYS find a table
            XP_ASSERT(pLoTable);
            return pLoTable;
        }
    }
    return NULL;
}

void CEditTableElement::GetParentSize(MWContext *pContext, int32 *pWidth, int32 *pHeight, LO_TableStruct *pLoTable)
{
    // This may be supplied -- get it if not
    if( !pLoTable )
        pLoTable = GetLoTable();

    int32 iParentWidth = 1;
    int32 iParentHeight = 1;

    if( pLoTable )
    {
        // Get viewing window
        int32 iXOrigin, iYOrigin;
        FE_GetDocAndWindowPosition(pContext, &iXOrigin, &iYOrigin, &iParentWidth, &iParentHeight);

	    // Get extra margin
	    int32 iMarginWidth;
	    int32 iMarginHeight;
	    LO_GetDocumentMargins(pContext, &iMarginWidth, &iMarginHeight);

        // Check for embeded table - get max width from Containing cell
        LO_CellStruct *pParentCell = lo_GetParentCell( pContext, (LO_Element*)pLoTable );

        if( pParentCell  )
        {
            iParentWidth = pParentCell->width - (2 * pParentCell->border_horiz_space);
            iParentHeight = pParentCell->height - (2 * pParentCell->border_vert_space);
        } else {
            iParentWidth -= 2 * iMarginWidth;
            iParentHeight -= 2 * iMarginHeight;
        }
    }
    if( pWidth )
        *pWidth = iParentWidth;
    
    if( pHeight )
        *pHeight = iParentHeight;
}

void CEditTableElement::SetSizeMode(MWContext *pContext, int iMode)
{
    int32 iParentWidth, iParentHeight;
    XP_Bool bCellPercent = (XP_Bool)(iMode & ED_MODE_CELL_PERCENT);
    XP_Bool bTablePercent = (XP_Bool)(iMode & ED_MODE_TABLE_PERCENT);
    XP_Bool bCellPixels = (XP_Bool)(iMode & ED_MODE_CELL_PIXELS);
    XP_Bool bTablePixels = (XP_Bool)(iMode & ED_MODE_TABLE_PIXELS);
    XP_Bool bUseCols = (XP_Bool)(iMode & ED_MODE_USE_COLS);
    XP_Bool bNoCols = (XP_Bool)(iMode & ED_MODE_NO_COLS);
    XP_Bool bChanged = FALSE;
    XP_Bool bSetTableWidth = iMode & ED_MODE_USE_TABLE_WIDTH;
    XP_Bool bClearTableWidth = iMode & ED_MODE_NO_TABLE_WIDTH;
    XP_Bool bSetTableHeight = iMode & ED_MODE_USE_TABLE_HEIGHT;
    XP_Bool bClearTableHeight = iMode & ED_MODE_NO_TABLE_HEIGHT;

    XP_Bool bSetCellWidth = iMode & ED_MODE_USE_CELL_WIDTH;
    XP_Bool bClearCellWidth = iMode & ED_MODE_NO_CELL_WIDTH;
    XP_Bool bSetCellHeight = iMode & ED_MODE_USE_CELL_HEIGHT;
    XP_Bool bClearCellHeight = iMode & ED_MODE_NO_CELL_HEIGHT;
    
    //XP_TRACE(("SetSizeMode: bPixels=%d, bPercent=%d, bSetCellWidth=%d, bClearCellWidth=%d", bPixels, bPercent, bSetCellWidth, bClearCellWidth));

    // Save current values to restore after resizing
    m_bSaveWidthPercent = m_bWidthPercent;
    m_bSaveHeightPercent = m_bHeightPercent;

    // First set values for the table
    if( (bTablePixels && m_bWidthPercent) || (bTablePercent && !m_bWidthPercent) || 
        (bTablePixels && m_bHeightPercent) || (bTablePercent && !m_bHeightPercent) ||
        bUseCols || bNoCols || bSetTableWidth || bSetTableHeight || 
        bClearTableWidth || bClearTableHeight )
    {
        EDT_TableData *pTableData = GetData();
        if( !pTableData )
            return;

        // Save these to restore after relayout of table
        m_bSaveWidthDefined = pTableData->bWidthDefined;
        m_bSaveHeightDefined = pTableData->bHeightDefined;
        m_bSaveWidthPercent = pTableData->bWidthPercent;
        m_bSaveHeightPercent = pTableData->bHeightPercent;

        GetParentSize(pContext, &iParentWidth, &iParentHeight);

        if( (pTableData->bWidthDefined && bClearTableWidth) ||
            (!pTableData->bWidthDefined && bSetTableWidth) )
        {
            pTableData->bWidthDefined = bSetTableWidth;
        }
        if( (pTableData->bHeightDefined && bClearTableHeight) ||
            (!pTableData->bHeightDefined && bSetTableHeight) )
        {
            pTableData->bHeightDefined = bSetTableHeight;
        }

        if( (bTablePixels && pTableData->bWidthPercent) ||
            (bTablePercent && !pTableData->bWidthPercent) )
        {
            if( bTablePercent )
            {
                pTableData->bWidthPercent = TRUE;
                pTableData->iWidth = m_iWidthPixels * 100 / iParentWidth;
            } else if( bTablePixels ){
                pTableData->bWidthPercent = FALSE;
                pTableData->iWidth = m_iWidthPixels;    
            }
        }
        if( (bTablePixels && pTableData->bHeightPercent) ||
            (bTablePercent && !pTableData->bHeightPercent) )
        {
            pTableData->bHeightPercent = bTablePercent;
            if( bTablePercent )
            {
                pTableData->bHeightPercent = TRUE;
                pTableData->iHeight = m_iHeightPixels * 100 / iParentHeight;
            } else if( bTablePixels ){
                pTableData->bHeightPercent = FALSE;
                pTableData->iHeight = m_iHeightPixels;
            }
        }

        if( bUseCols )
            pTableData->bUseCols = TRUE;
        if( bNoCols )
            pTableData->bUseCols = FALSE;

        SetData(pTableData);
        EDT_FreeTableData(pTableData);
    }

    // Process cells only if asked to set or clear the width or height
    if( bCellPixels || bCellPercent ||
        bSetCellWidth || bClearCellWidth ||
        bSetCellHeight || bClearCellHeight )
    {
        // Scan through entire table to set all cells
        CEditTableCellElement *pCell = GetFirstCell();
        if( pCell )
        {
            iParentWidth = pCell->GetParentWidth();
            iParentHeight = pCell->GetParentHeight();
        }

        while( pCell )
        {
            // Only set cell's data if we change from current settings
            bChanged = FALSE;
            EDT_TableCellData *pCellData = pCell->GetData();
            if( pCellData )
            {
                if( (pCellData->bWidthDefined && bClearCellWidth) ||
                    (!pCellData->bWidthDefined && bSetCellWidth) )
                {
                    pCellData->bWidthDefined = bSetCellWidth;
                    bChanged = TRUE;
                }
                if( (pCellData->bHeightDefined && bClearCellHeight) ||
                    (!pCellData->bHeightDefined && bSetCellHeight) )
                {
                    pCellData->bHeightDefined = bSetCellHeight;
                    bChanged = TRUE;
                }

                // Save the current m_bWidthPercent, m_bHeightPercent, bWidthDefined, and bHeightDefined
                pCell->SaveSizeMode(pCellData->bWidthDefined, pCellData->bHeightDefined);

                if( (bCellPixels && pCellData->bWidthPercent) ||
                    (bCellPercent && !pCellData->bWidthPercent) )
                {
                    if( bCellPercent )
                    {
                        pCellData->bWidthPercent = TRUE;
                        pCellData->iWidth = (pCellData->iWidthPixels * 100) / iParentWidth;
                    } else {
                        pCellData->bWidthPercent = FALSE;
                        pCellData->iWidth = pCellData->iWidthPixels;
                    }
                    bChanged = TRUE;
                }
                if( (bCellPixels && pCellData->bHeightPercent) ||
                    (bCellPercent && !pCellData->bHeightPercent) )
                {
                    if( bCellPercent )
                    {
                        pCellData->bHeightPercent = TRUE;
                        pCellData->iHeight = (pCellData->iHeightPixels * 100) / iParentHeight;
                    } else {
                        pCellData->bHeightPercent = FALSE;
                        pCellData->iHeight = pCellData->iHeightPixels;
                    }
                    bChanged = TRUE;
                }

                if( bChanged )
                    pCell->SetData(pCellData);
            
                EDT_FreeTableCellData(pCellData);
            }
            pCell = GetNextCellInTable();
        }
    }
}

void CEditTableElement::RestoreSizeMode(MWContext *pContext)
{
    int32 iParentWidth;
    int32 iParentHeight;
    EDT_TableData *pTableData = GetData();

    if( pTableData && pContext )
    {
        // First restore the size mode for the table
        GetParentSize(pContext, &iParentWidth, &iParentHeight);
        if( m_bSaveWidthPercent )
        {
            // Convert existing Pixel value to % of parent size
            pTableData->iWidth = (m_iWidthPixels * 100) / iParentWidth;
            pTableData->iHeight = (m_iHeightPixels * 100) / iParentHeight;
        } else {
            // Converting to pixels is simple - just use the pixel value we already have
            pTableData->iWidth = m_iWidthPixels;
            pTableData->iHeight = m_iHeightPixels;
        }
        pTableData->bWidthDefined = m_bSaveWidthDefined;
        pTableData->bHeightDefined = m_bSaveHeightDefined;
        pTableData->bWidthPercent = m_bSaveWidthPercent;
        pTableData->bHeightPercent = m_bSaveHeightPercent;
        SetData(pTableData);
        EDT_FreeTableData(pTableData);
        
        // Scan through entire table to reset all cells
        CEditTableCellElement *pCell = GetFirstCell();
        if( pCell )
        {
            iParentWidth = pCell->GetParentWidth();
            iParentHeight = pCell->GetParentHeight();
        }
        while( pCell )
        {
            pCell->RestoreSizeMode(iParentWidth, iParentHeight);
            pCell = GetNextCellInTable();
        }
        
    }
}

void CEditTableElement::DeleteLayoutData()
{
    intn i;
    intn iColumns = m_ColumnLayoutData.Size();
    intn iRows = m_RowLayoutData.Size();

    for ( i = 0; i < iColumns; i++ )
    {
        if( m_ColumnLayoutData[i] )
            delete m_ColumnLayoutData[i];
    }
    //Note: This does NOT free any memory,
    //      it just sets m_iSize = 0;
    m_ColumnLayoutData.Empty();

    for ( i = 0; i < iRows; i++ )
    {
        if( m_RowLayoutData[i] )
            delete m_RowLayoutData[i];
    }
    m_RowLayoutData.Empty();
}

// Store data for first cell in each column and row in growable arrays
void CEditTableElement::AddLayoutData(CEditTableCellElement *pEdCell, LO_Element *pLoCell)
{
    if( !pEdCell || !pLoCell )
        return;

    EDT_CellLayoutData *pColData;
    EDT_CellLayoutData *pRowData;

    intn i;
    intn iCurrentColumns = m_ColumnLayoutData.Size();
    intn iCurrentRows = m_RowLayoutData.Size();
    // We always add the cell the first time here
    XP_Bool bNewColumn = iCurrentColumns == 0;    
    XP_Bool bNewRow = iCurrentRows == 0;    

    for( i = 0; i < iCurrentColumns; i++ )
    {
        if( pLoCell->lo_cell.x == m_ColumnLayoutData[i]->X )
        {
            // We already have the first cell in this column
            // Just increase the column count
            // Note: We count geometric columns, so we must look at COLSPAN
            m_ColumnLayoutData[i]->iCellsInColumn += lo_GetColSpan(pLoCell);
            break;
        } 
        else if( pLoCell->lo_cell.x > m_ColumnLayoutData[i]->X &&
                 ( i+1 == iCurrentColumns ||
                   pLoCell->lo_cell.x < m_ColumnLayoutData[i+1]->X ) )
        {
            // We are at the last existing cell in array or
            // this cell's X is between current cell's and 
            // the next cell in array
            // so we have a new column to insert between existing columns
            bNewColumn = TRUE;
            // The index to insert at is the NEXT location
            i++;
            break;
        }
    }

    if( bNewColumn )
    {
        pColData = new EDT_CellLayoutData;
        if( !pColData )
            return;
        pColData->X = pLoCell->lo_cell.x;
        pColData->Y = pLoCell->lo_cell.y;
        pColData->pEdCell = pEdCell;
        pColData->pLoCell = pLoCell;
        pColData->iColumn = i;
        pColData->iCellsInColumn = lo_GetColSpan(pLoCell);
        // These are ignored for column data
        pColData->iCellsInRow = pColData->iRow = 0;

        m_ColumnLayoutData.Insert(pColData, i);
    }

    for( i = 0; i < iCurrentRows; i++ )
    {
        if( pLoCell->lo_cell.y == m_RowLayoutData[i]->Y )
        {
            // We already have the first cell in this column
            // Just increase the row count
            m_RowLayoutData[i]->iCellsInRow += lo_GetRowSpan(pLoCell);
            break;
        } 
        else if( pLoCell->lo_cell.y > m_RowLayoutData[i]->Y &&
                 ( i+1 == iCurrentRows ||
                  pLoCell->lo_cell.y < m_RowLayoutData[i+1]->Y ) )
        {
            // We are at the last existing cell in array or
            // this cell's Y is between current cell's and 
            // the next cell in array,
            // so we have a new row to insert between existing rows
            bNewRow = TRUE;
            // The index to insert at is the NEXT location
            i++;
            break;
        }
    }

    if( bNewRow )
    {
        pRowData = new EDT_CellLayoutData;
        if( !pRowData )
            return;

        pRowData->X = pLoCell->lo_cell.x;
        pRowData->Y = pLoCell->lo_cell.y;
        pRowData->pEdCell = pEdCell;
        pRowData->pLoCell = pLoCell;
        pRowData->iRow = i;
        pRowData->iCellsInRow = lo_GetRowSpan(pLoCell);
        // These are ignored for row data
        pRowData->iCellsInColumn = pRowData->iColumn = 0;

        m_RowLayoutData.Insert(pRowData, i);
    }
}

// class CEditTableRowElement

CEditTableRowElement::CEditTableRowElement()
    : CEditElement((CEditElement*) NULL, P_TABLE_ROW),
      m_backgroundColor(),
      m_iBackgroundSaveIndex(0),
      m_pBackgroundImage(0)
{
}

CEditTableRowElement::CEditTableRowElement(intn columns)
    : CEditElement((CEditElement*) NULL, P_TABLE_ROW),
      m_backgroundColor(),
      m_iBackgroundSaveIndex(0),
      m_pBackgroundImage(0)
{
    EDT_TRY {
        for ( intn column = 0; column < columns; column++ ) {
            CEditTableCellElement* pCell = new CEditTableCellElement();
            pCell->InsertAsFirstChild(this);
        }
    }
    EDT_CATCH_ALL {
        Finalize();
        EDT_RETHROW;
    }
}

CEditTableRowElement::CEditTableRowElement(CEditElement *pParent, PA_Tag *pTag, int16 /*csid*/)
    : CEditElement(pParent, P_TABLE_ROW),
      m_backgroundColor(),
      m_iBackgroundSaveIndex(0),
      m_pBackgroundImage(0)
{
    if( pTag ){
        char *locked_buff;
            
        PA_LOCK(locked_buff, char *, pTag->data );
        if( locked_buff ){
            SetTagData( locked_buff );
        }
        PA_UNLOCK(pTag->data);
    }
}

CEditTableRowElement::CEditTableRowElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer)
    : CEditElement(pStreamIn, pBuffer),
      m_backgroundColor(),
      m_iBackgroundSaveIndex(0),
      m_pBackgroundImage(0)
{
}

CEditTableRowElement::~CEditTableRowElement(){
    delete m_pBackgroundImage;
}

CEditTableRowElement *CEditTableRowElement::GetNextRow()
{
    CEditElement *pRow = GetNextSibling();
    if( pRow && pRow->IsTableRow() )
    {
        return pRow->TableRow();
    }
    return NULL;
}

CEditTableCellElement *CEditTableRowElement::GetFirstCell()
{
    CEditElement *pChild = GetChild();
    if( pChild && pChild->IsTableCell() )
    {
        return pChild->TableCell();
    }
    return NULL;
}

// Scan all cells in row and add up m_iColSpan to get 
//   maximum possible cells in the row
intn CEditTableRowElement::GetCells()
{
    intn cells = 0;
    // Get the first cell of the row
    CEditElement* pChild = GetChild();

    if( pChild && pChild->IsTableCell() )
    {
        // Starting number of cells is normally 0,
        //  except when ROWSPAN causes first cell to be indented
        while( pChild )
        {
            if ( pChild->IsTableCell() )
            {
                // We must add column span to get maximum possible columns per row
                cells += ((CEditTableCellElement*)pChild)->GetColSpan();
            }
            pChild = pChild->GetNextSibling();
        }
    }
    return cells;
}

void CEditTableRowElement::FinishedLoad( CEditBuffer* pBuffer ){
    // Wrap anything that's not a table cell in a table cell.
    CEditTableCellElement* pCell = NULL;
    CEditElement* pNext = 0;
    
    if ( ! GetChild() ) {
        // Force a cell
        CEditTableCellElement* pTempCell = new CEditTableCellElement();
        pTempCell->InsertAsFirstChild(this);
    }

    for ( CEditElement* pChild = GetChild();
        pChild;
        pChild = pNext) {
        pNext = pChild->GetNextSibling(); // We might unlink pChild
        if ( ! IsAcceptableChild(*pChild) ){
            if ( ! pCell ){
                pCell = new CEditTableCellElement();
                pCell->InsertAfter(pChild);
            }
            pChild->Unlink();
            pChild->InsertAsLastChild(pCell);
        }
        else {
            if ( pCell ){
                pCell->FinishedLoad(pBuffer);
                pCell = NULL;
            }
        }
        pChild->FinishedLoad(pBuffer);
    }
    if ( pCell ){
//        pBuffer->SetFillNewCellWithSpace();
        pCell->FinishedLoad(pBuffer);
//        pBuffer->ClearFillNewCellWithSpace();
        pCell = NULL;
    }
}

static void edt_InsertCells(intn number, XP_Bool bAfter, CEditTableCellElement *pExisting, CEditTableRowElement* pSource)
{
    XP_ASSERT(pExisting);
    if( !pExisting )
        return;
    CEditBuffer *pBuffer = pExisting->GetEditBuffer();
    if( !pBuffer )
        return;

    // Insert new cells at new insert position
    for ( intn col = 0; col < number; col++ )
    {
        CEditTableCellElement *pInsertCell = NULL;
        if ( pSource )
        {
            // Remove cells from source from left to right
            pInsertCell = pSource->GetFirstCell();
            if( pInsertCell )
            {
                // We found a cell - unlink it from source row
                // Note: When pasting a column, we  get a NULL cell 
                //       if any cell in the column has COLSPAN > 1. 
                //       The cells without colspan will be "missing" in the source
                pInsertCell->Unlink();

                // If inserted cell spans > 1 column, then
                //   we should insert fewer to keep table rectangular
                intn iColSpan = pInsertCell->GetColSpan();
                if( iColSpan > 1 )
                    number -= iColSpan - 1;
            }
        }
        // We didn't find a cell in source 
        // (no source or because of COLSPAN > 1),
        // so insert a blank cell instead
        if( !pInsertCell )
            pInsertCell = new CEditTableCellElement();

        if( pInsertCell )
        {
            if( bAfter )
            {
                pInsertCell->InsertAfter(pExisting);
                // To insert new cells when pSource is used
                //  and we want to insert from left to right,
                //  set the "before" cell to the one just inserted
                pExisting = pInsertCell;
            }
            else
            {
                // Insert before the current column
                // This will add pSource cells from left to right
                pInsertCell->InsertBefore(pExisting);
            }
            // Be sure cell has the empty text element in default paragraph container,
            //  but check if text should be a space
            pBuffer->SetFillNewCellWithSpace();
            pInsertCell->FinishedLoad(pBuffer);
            pBuffer->ClearFillNewCellWithSpace();
        }
    }
}

// Note: iNewX should either be == X (insert before current column)
//       or == X + (insert column's width).
void CEditTableRowElement::InsertCells(int32 X, int32 iNewX, intn number, CEditTableRowElement* pSource)
{
    if ( number <= 0 || X < 0 )
    {
        return;
    }
    CEditTableElement *pTable = (CEditTableElement*)GetParent();
    if( !pTable || !pTable->IsTable() )
        return;

    int32 iInterCellSpace = pTable->GetCellSpacing();
    int32 iCellX;
    if ( pTable->GetColumnX(0) == iNewX )
    {
        // Insert at the start.
        for ( intn col = 0; col < number; col++ )
        {
            CEditTableCellElement* pInsertCell = NULL;
            if ( pSource )
            {
                // Insert cells from the end
                pInsertCell = (CEditTableCellElement*)pSource->GetLastChild();
                pInsertCell->Unlink();

                // If inserted cell spans > 1 column, then
                //   we should insert fewer to keep table rectangular
                intn iColSpan = pInsertCell->GetColSpan();
                if( iColSpan > 1 )
                    number -= iColSpan - 1;
            }
            //Note: Create a new cell if no source or source row was incomplete
            if( !pInsertCell ) 
            {
                pInsertCell = new CEditTableCellElement();
                // Insert space into initial text
                CEditBuffer *pBuffer = GetEditBuffer();
                pBuffer->SetFillNewCellWithSpace();
                pInsertCell->FinishedLoad(pBuffer);
                pBuffer->ClearFillNewCellWithSpace();
            }
            if( pInsertCell )
            {
                pInsertCell->InsertAsFirstChild(this);
            }
        }
    }
    else 
    {
        // Get the cell at the requested column
        CEditTableCellElement* pExisting = FindCell(X, X != iNewX);
        // This may now be NULL for "missing" cells because
        //   of ROWSPAN or COLSPAN effects
        if( pExisting )
        {
            iCellX = pExisting->GetX();
            if( (iCellX < X || X != iNewX) && ((iCellX + pExisting->GetFullWidth()) > iNewX) ) // WAS GetWidth()
            {
                // Cell extends past new column insert point,
                //  so simply increase span and don't add new cell
                pExisting->IncreaseColSpan(number);
            } else {
                // Non-spanned cell found at current column,
                //   so insert before (X==iNewX) or after that cell
                edt_InsertCells(number, X < iNewX, pExisting, pSource);
            }
        } 
        else if( (X != iNewX) && (pExisting = FindCell(iNewX/*+iInterCellSpace*/)) != NULL )
        {
            // No cell in current column, but we found cell at NEW insert column
            //  so put new cell(s) before this
            edt_InsertCells(number, FALSE, pExisting, pSource);
        }
        else
        {
            // We didn't find a cell, either because of cells spanning row
            //   from above, or this row needs new cell(s) at the end

            // First cell in current row
            CEditTableCellElement *pFirstCell = GetFirstCell();
            if( !pFirstCell )
                return;
            // This should be same as pFirstCell, but we need to setup
            //  to find other cells in same geometric row
            CEditTableCellElement *pCell = pTable->GetFirstCellInRow(pFirstCell->GetY());
            XP_ASSERT(pCell == pFirstCell);
            intn iCurrentRow = pFirstCell->GetRow();

            CEditTableCellElement *pCellBefore = NULL;
            CEditTableCellElement *pCellAfter = NULL;

            while(pCell)
            {
                iCellX = pCell->GetX();
                int32 iCellWidth = pCell->GetFullWidth(); // WAS GetWidth()
                if( iCellX == X )
                {
                    // We found a cell from another row that spans current row
                    //  and target column, so just increase its column span instead of inserting
                    if( X != iNewX && (iCellX+iCellWidth) > iNewX )
                    {
                        pCell->IncreaseColSpan(number);
                        return;
                    }
                }
                // Find the closest cells in current row
                //   to our target column             
                if( pCell->GetRow() == iCurrentRow )
                {
                    if( (iCellX + pCell->GetFullWidth()) <= X )
                        pCellBefore = pCell;
                    if( iCellX >= X && !pCellAfter )
                        pCellAfter = pCell;
                }
                pCell = pTable->GetNextCellInRow();
            }
            if( pCellAfter )
            {
                // FALSE = Force inserting before the cell found
                edt_InsertCells(number, FALSE, pCellAfter, pSource);
                return;
            }
            if( pCellBefore )
            {
                // This is tough. We know we need to add cells, but how many?
                // We can't be sure we had enough before so new cells end up
                //   at desired new column. 
                // TODO: USE pTable->m_ColumnLayoutData to figure out how
                //       many cells to add to bring us to desired X
                // TRUE = insert after the cell found
                edt_InsertCells(number, TRUE, pCellBefore, pSource);
            }
        }
    }
}

intn CEditTableRowElement::AppendRow( CEditTableRowElement *pAppendRow, XP_Bool bDeleteRow )
{
    if( !pAppendRow )
        return 0;

    intn iColumnsAppended = 0;
    CEditTableCellElement *pAppendCell = pAppendRow->GetFirstCell();

    while( pAppendCell )
    {
        // Unlink each cell in source row and
        //   append it to current row
        pAppendCell->Unlink();
        pAppendCell->InsertAsLastChild(this);
        iColumnsAppended += pAppendCell->GetColSpan();
        pAppendCell = pAppendRow->GetFirstCell();
    }

    if( bDeleteRow )
        delete pAppendRow;

    return iColumnsAppended;
}

void CEditTableRowElement::PadRowWithEmptyCells( intn iColumns )
{
    CEditTableElement *pTable = (CEditTableElement*)GetParent();
    if( !pTable || !pTable->IsTable() )
        return;
    CEditTableCellElement *pCell = GetFirstCell();
    CEditTableCellElement *pLastCell = NULL;

    // Use number of columns supplied, or get current number in table
    intn iTotalColumns = iColumns ? iColumns : pTable->GetColumns();

    while( pCell )
    {
        pLastCell = pCell;
        pCell = pCell->GetNextCellInLogicalRow();
    }
    if( pLastCell )
    {
        // Get which column the last cell is in
        intn iLastIndex = pTable->GetColumnIndex(pLastCell->GetX());
        if( iLastIndex >= 0 )
        {
            intn iExtraColumns = iTotalColumns - iLastIndex - 1;
            if( iExtraColumns > 0 )
                edt_InsertCells( iExtraColumns, TRUE, pLastCell, NULL );
        }
    }
}

void CEditTableRowElement::DeleteCells(int32 X, intn number, CEditTableRowElement* pUndoContainer){
    if ( number == 0 )
    {
        return; /* A waste of time, but legal. */
    }

    if ( number < 0 || X < 0 )
    {
        /* Illegal. */
        XP_ASSERT(FALSE);
        return;
    }
    CEditTableElement *pTable = (CEditTableElement*)GetParent();
    XP_ASSERT(pTable && pTable->IsTable() );
    CEditTableCellElement* pCell = FindCell(X);
    if( pTable && pCell )
    {
        intn iRemaining = number;
        while( iRemaining > 0 )
        {
            // Get next cell before we delete the one we're at
            CEditTableCellElement *pNextCell = pCell->GetNextCellInLogicalRow();

            int32 iCellX = pCell->GetX();
            int32 iColSpan = pCell->GetColSpan();
 
            if( X == iCellX )
            {
                // Cell is in same column as delete column
                if( iColSpan <= iRemaining )
                {
                    // Simplest case - just delete one cell

                    // Remove any selected cells from our lists
                    //  else relayout blows up
                    // NULL means let function find the corresponding LO_CellStruct
                    if( pCell->IsSelected() )
                        GetEditBuffer()->SelectCell(FALSE, NULL, pCell);

                    if ( pUndoContainer )
                    {
                        pCell->Unlink();
                        pCell->InsertAsFirstChild(pUndoContainer);
                    }
                    else {
                        delete pCell;
                    }
                    iRemaining -= iColSpan;
                } 
                else if( iColSpan > iRemaining )
                {
                    // Cell spans more than we want to delete,
                    //  so reduce span instead of actually deleting
                    pCell->DecreaseColSpan(iRemaining);
                    iRemaining = 0;
                    // Since this cell is in the delete column (same X),
                    //  then we want it to look deleted, so empty contents
                    pCell->DeleteContents();
                }
            } else {
                // Start of cell is before delete column
                //  (but end must be past delete column)
                // Figure out how many columns it spans before delete column.
                int32 iSpanAvailable = iColSpan - pTable->GetColumnsSpanned(iCellX, X);
                if( iSpanAvailable > 0 )
                {
                    // Decrease the column span instead of deleting
                    intn iDecrease = min(iSpanAvailable,iRemaining);
                    pCell->DecreaseColSpan(iDecrease);
                    iRemaining -= iDecrease;
                    // "Undefine" (1st param) the width since it is no longer
                    //   in the same column as before
                    // Keep existing bWidthPercent, the last param = width
                    //  (it doesn't matter what it is now)
                    pCell->SetWidth(FALSE, pCell->GetWidthPercent(), 1);
                    //  then we want to use the width of the 
                }
            }
            // We are done if no more cells in row or nothing remaining to delete
            if( !pNextCell )
                break;

            pCell = pNextCell;
        }
    } else {
        //TODO: WHAT TO DO IF WE DON'T FIND A CELL?
        // Only case this should fail is if number of cells in this row
        //   is less than the column being deleted
        //   So doing nothing is probably OK
        XP_TRACE(("No cell in column at X = %d", X));
    }
}

CEditTableCellElement* CEditTableRowElement::FindCell(int32 X, XP_Bool bIncludeRightEdge)
{
    CEditTableCellElement* pCell = NULL;

    for ( CEditElement* pChild = GetChild();
            pChild;
                pChild = pChild->GetNextSibling()) 
    {
        if ( pChild->IsTableCell() )
        {
            int32 cellX = pChild->TableCell()->GetX();
            int32 cellRight = cellX + pChild->TableCell()->GetFullWidth(); // WAS GetWidth()

            // Back up 1 pixel if we shouldn't match the right edge
            if( !bIncludeRightEdge )
                cellRight--;

            if ( X >= cellX && X <= cellRight )
            {
                if( pCell )
                {
                    return pChild->TableCell();
                } else {
                    // We are here whe we first find a cell
                    // Save it but continue to see if next cell's
                    //   left edge matches - use that instead if it does
                    // (This nonsense is needed for 0 borders where
                    //   right edge of one cell = left edge of next)
                    pCell = pChild->TableCell();
                }
            }
        }
    }
    return pCell;
}

void CEditTableRowElement::SetData( EDT_TableRowData *pData ){
    char *pNew = 0;
    if( pData->align != ED_ALIGN_DEFAULT ){
        pNew = PR_sprintf_append( pNew, "ALIGN=%s ", EDT_AlignString(pData->align) );
    }

    if( pData->valign != ED_ALIGN_DEFAULT ){
        pNew = PR_sprintf_append( pNew, "VALIGN=%s ", EDT_AlignString(pData->valign) );
    }

    if ( pData->pColorBackground ) {
        SetBackgroundColor(EDT_LO_COLOR(pData->pColorBackground));
        pNew = PR_sprintf_append( pNew, "BGCOLOR=\"#%06lX\" ", GetBackgroundColor().GetAsLong() );
    }
    else {
        SetBackgroundColor(ED_Color::GetUndefined());
    }

    if ( pData->pBackgroundImage ) {
        SetBackgroundImage(pData->pBackgroundImage);
        pNew = PR_sprintf_append( pNew, "BACKGROUND=\"%s\" ", pData->pBackgroundImage );
    }
    else {
        SetBackgroundImage(0);
    }
    if ( pData->bBackgroundNoSave) {
        pNew = PR_sprintf_append( pNew, "NOSAVE " );
    }

    if( pData->pExtra  ){
        pNew = PR_sprintf_append( pNew, "%s ", pData->pExtra );
    }

    if( pNew ){
        pNew = PR_sprintf_append( pNew, ">" );
    }
    SetTagData( pNew );
    if ( pNew ) {
        free(pNew);
    }
}

EDT_TableRowData* CEditTableRowElement::GetData( ){
    EDT_TableRowData *pRet;
    PA_Tag* pTag = TagOpen(0);
    pRet = ParseParams( pTag, GetWinCSID() );
    PA_FreeTag( pTag );
    return pRet;
}

static char *tableRowParams[] = {
    PARAM_ALIGN,
    PARAM_VALIGN,
    PARAM_BGCOLOR,
    PARAM_BACKGROUND,
    PARAM_NOSAVE,
    0
};

EDT_TableRowData* CEditTableRowElement::ParseParams( PA_Tag *pTag, int16 csid ){
    EDT_TableRowData* pData = NewData();
    ED_Alignment align;
    
    align = edt_FetchParamAlignment( pTag, ED_ALIGN_DEFAULT, FALSE, csid );
    if( align == ED_ALIGN_RIGHT || align == ED_ALIGN_LEFT || align == ED_ALIGN_ABSCENTER ){
        pData->align = align;
    }
    align = edt_FetchParamAlignment( pTag, ED_ALIGN_DEFAULT, TRUE, csid );
    if( align == ED_ALIGN_ABSTOP || align == ED_ALIGN_ABSBOTTOM || align == ED_ALIGN_ABSCENTER
        || align == ED_ALIGN_BASELINE ){
        pData->valign = align;
    }
    pData->pColorBackground = edt_MakeLoColor(edt_FetchParamColor( pTag, PARAM_BGCOLOR, csid ));
    pData->pBackgroundImage = edt_FetchParamString( pTag, PARAM_BACKGROUND, csid );
    pData->bBackgroundNoSave = edt_FetchParamBoolExist( pTag, PARAM_NOSAVE, csid );
    pData->pExtra = edt_FetchParamExtras( pTag, tableRowParams, csid );

    return pData;
}

EDT_TableRowData* CEditTableRowElement::NewData(){
    EDT_TableRowData* pData = XP_NEW( EDT_TableRowData );
    if( pData == 0 ){
        // throw();
        return pData;
    }
    pData->align = ED_ALIGN_DEFAULT;
    pData->valign = ED_ALIGN_TOP; // ED_ALIGN_DEFAULT; this is centered - stupid!
    pData->pColorBackground = 0;
    pData->pBackgroundImage = 0;
    pData->bBackgroundNoSave = FALSE;
    pData->pExtra = 0;
    return pData;
}

void CEditTableRowElement::FreeData( EDT_TableRowData *pData ){
    if( pData->pColorBackground ) XP_FREE( pData->pColorBackground );
    if( pData->pBackgroundImage ) XP_FREE( pData->pBackgroundImage );
    if( pData->pExtra ) XP_FREE( pData->pExtra );
    XP_FREE( pData );
}

void CEditTableRowElement::SetBackgroundColor( ED_Color iColor ){
    m_backgroundColor = iColor;
}

ED_Color CEditTableRowElement::GetBackgroundColor(){
    return m_backgroundColor;
}

void CEditTableRowElement::SetBackgroundImage( char* pImage ){
    delete m_pBackgroundImage;
    m_pBackgroundImage = 0;
    if ( pImage ) {
        m_pBackgroundImage = XP_STRDUP(pImage);
    }
}

char* CEditTableRowElement::GetBackgroundImage(){
    return m_pBackgroundImage;
}

// class CEditCaptionElement

CEditCaptionElement::CEditCaptionElement()
    : CEditSubDocElement((CEditElement*) NULL, P_CAPTION)
{
}

CEditCaptionElement::CEditCaptionElement(CEditElement *pParent, PA_Tag *pTag, int16 /*csid*/)
    : CEditSubDocElement(pParent, P_CAPTION)
{
    if( pTag ){
        char *locked_buff;
            
        PA_LOCK(locked_buff, char *, pTag->data );
        if( locked_buff ){
            SetTagData( locked_buff );
        }
        PA_UNLOCK(pTag->data);
    }
}

CEditCaptionElement::CEditCaptionElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer)
    : CEditSubDocElement(pStreamIn, pBuffer)
{
}

void CEditCaptionElement::SetData( EDT_TableCaptionData *pData ){
    char *pNew = 0;
    if( pData->align == ED_ALIGN_ABSBOTTOM ){
        pNew = PR_sprintf_append( pNew, "ALIGN=%s ", EDT_AlignString(pData->align) );
    }
    if( pData->pExtra  ){
        pNew = PR_sprintf_append( pNew, "%s ", pData->pExtra );
    }

    if( pNew ){
        pNew = PR_sprintf_append( pNew, ">" );
    }
    SetTagData( pNew );
    if ( pNew ) {
        free(pNew);
    }
}

EDT_TableCaptionData* CEditCaptionElement::GetData( ){
    return GetData( GetWinCSID() );
}

EDT_TableCaptionData* CEditCaptionElement::GetData( int16 csid ){
    EDT_TableCaptionData* pRet;
    PA_Tag* pTag = TagOpen(0);
    pRet = ParseParams( pTag, csid );
    PA_FreeTag( pTag );
    return pRet;
}

static char *tableCaptionParams[] = {
    PARAM_ALIGN,
    0
};

EDT_TableCaptionData* CEditCaptionElement::ParseParams( PA_Tag *pTag, int16 csid ){
    EDT_TableCaptionData* pData = NewData();
    pData->align = edt_FetchParamAlignment( pTag, ED_ALIGN_ABSTOP, FALSE, csid );
    pData->pExtra = edt_FetchParamExtras( pTag, tableCaptionParams, csid );
    return pData;
}

EDT_TableCaptionData* CEditCaptionElement::NewData(){
    EDT_TableCaptionData* pData = XP_NEW( EDT_TableCaptionData );
    if( pData == 0 ){
        // throw();
        return pData;
    }
    pData->align = ED_ALIGN_CENTER;
    pData->pExtra = 0;
    return pData;
}

void CEditCaptionElement::FreeData( EDT_TableCaptionData *pData ){
    if( pData->pExtra ) XP_FREE( pData->pExtra );
    XP_FREE( pData );
}

// class CEditTableCellElement

CEditTableCellElement::CEditTableCellElement()
    : CEditSubDocElement((CEditElement*) NULL, P_TABLE_DATA),
      m_backgroundColor(),
      m_iBackgroundSaveIndex(0),
      m_pBackgroundImage(0),
      m_iWidth(1),
      m_iWidthPixels(1),
      m_iHeight(1),
      m_iHeightPixels(1),
      m_bWidthPercent(FALSE),
      m_bHeightPercent(FALSE),
      m_iColSpan(1),
      m_iRowSpan(1),
      m_iRow(0),
      m_X(0),
      m_Y(0),
      m_pSaveParent(0),
      m_pSaveNext(0),
      m_bSaveWidthPercent(FALSE),
      m_bSaveHeightPercent(FALSE),
      m_bSaveWidthDefined(FALSE),
      m_bSaveHeightDefined(FALSE),
      m_bSelected(FALSE),
      m_bDeleted(FALSE)
{
}

CEditTableCellElement::CEditTableCellElement(XP_Bool bIsHeader)
    : CEditSubDocElement((CEditElement*) NULL, bIsHeader ? P_TABLE_HEADER : P_TABLE_DATA),
      m_backgroundColor(),
      m_iBackgroundSaveIndex(0),
      m_pBackgroundImage(0),
      m_iWidth(1),
      m_iWidthPixels(1),
      m_iHeight(1),
      m_iHeightPixels(1),
      m_bWidthPercent(FALSE),
      m_bHeightPercent(FALSE),
      m_iColSpan(1),
      m_iRowSpan(1),
      m_iRow(0),
      m_X(0),
      m_Y(0),
      m_pSaveParent(0),
      m_pSaveNext(0),
      m_bSaveWidthPercent(FALSE),
      m_bSaveHeightPercent(FALSE),
      m_bSaveWidthDefined(FALSE),
      m_bSaveHeightDefined(FALSE),
      m_bSelected(FALSE),
      m_bDeleted(FALSE)
{
}

CEditTableCellElement::CEditTableCellElement(CEditElement *pParent, PA_Tag *pTag, int16 /*csid*/)
    : CEditSubDocElement(pParent, pTag->type),
      m_backgroundColor(),
      m_iBackgroundSaveIndex(0),
      m_pBackgroundImage(0),
      m_iWidth(1),
      m_iWidthPixels(1),
      m_iHeight(1),
      m_iHeightPixels(1),
      m_bWidthPercent(FALSE),
      m_bHeightPercent(FALSE),
      m_iColSpan(1),
      m_iRowSpan(1),
      m_iRow(0),
      m_X(0),
      m_Y(0),
      m_pSaveParent(0),
      m_pSaveNext(0),
      m_bSaveWidthPercent(FALSE),
      m_bSaveHeightPercent(FALSE),
      m_bSaveWidthDefined(FALSE),
      m_bSaveHeightDefined(FALSE),
      m_bSelected(FALSE),
      m_bDeleted(FALSE)
{
    if( pTag ){
        char *locked_buff;
            
        PA_LOCK(locked_buff, char *, pTag->data );
        if( locked_buff ){
            SetTagData( locked_buff );
        }
        PA_UNLOCK(pTag->data);
    }
}

CEditTableCellElement::CEditTableCellElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer)
    : CEditSubDocElement(pStreamIn, pBuffer),
      m_backgroundColor(),
      m_iBackgroundSaveIndex(0),
      m_pBackgroundImage(0),
      m_iWidth(1),
      m_iWidthPixels(1),
      m_iHeight(1),
      m_iHeightPixels(1),
      m_bWidthPercent(FALSE),
      m_bHeightPercent(FALSE),
      m_iColSpan(1),
      m_iRowSpan(1),
      m_iRow(0),
      m_X(0),
      m_Y(0),
      m_pSaveParent(0),
      m_pSaveNext(0),
      m_bSaveWidthPercent(FALSE),
      m_bSaveHeightPercent(FALSE),
      m_bSaveWidthDefined(FALSE),
      m_bSaveHeightDefined(FALSE),
      m_bSelected(FALSE),
      m_bDeleted(FALSE)
{
    // We need this so we can accurately determine number
    //  of columns in stream
    m_X = pStreamIn->ReadInt();

    // Get width from stream -- needed for pasting tables/cells
    // We must get CSID from buffer since element is not in doc yet
    //TODO: Should we pass in CSID from paste stream instead?
    PA_Tag* pTag = CEditElement::TagOpen(0);
    EDT_TableCellData *pData = ParseParams( pTag, pBuffer->GetRAMCharSetID() );
    if(pData)
    {
        m_iWidth = pData->iWidth;
        m_bWidthPercent = pData->bWidthPercent;
        if( !m_bWidthPercent )
            m_iWidthPixels = m_iWidth;

        m_iHeight = pData->iHeight;
        m_bHeightPercent = pData->bHeightPercent;
        if( !m_bHeightPercent )
            m_iHeightPixels = m_iHeight;

        EDT_FreeTableCellData(pData);
    }
    PA_FreeTag( pTag );
}

CEditTableCellElement::~CEditTableCellElement(){
    // Be sure any cell deleted is not selected
    if( IsSelected() && GetEditBuffer() )
        GetEditBuffer()->SelectCell(FALSE, NULL, this);
    
    delete m_pBackgroundImage;
}

XP_Bool CEditTableCellElement::IsTableCell(){
    return TRUE;
}

EEditElementType CEditTableCellElement::GetElementType(){
    return eTableCellElement;
}

ED_Alignment CEditTableCellElement::GetDefaultAlignment(){
    return ED_ALIGN_DEFAULT;
/*
    EDT_TableCellData* pData = GetData();
    ED_Alignment result = IsTableData() ? ED_ALIGN_LEFT : ED_ALIGN_ABSCENTER;
    if ( pData->align != ED_ALIGN_DEFAULT ) {
        result = pData->align;
    }
    FreeData(pData);
    return result;
*/
}

XP_Bool CEditTableCellElement::IsTableData(){
    return GetType() == P_TABLE_DATA;
}

void CEditTableCellElement::IncreaseColSpan(int32 iIncrease)
{
    EDT_TableCellData *pData = GetData();
    if( pData )
    {
        pData->iColSpan += iIncrease;
        SetData(pData);
        EDT_FreeTableCellData(pData);
    }
}

void CEditTableCellElement::IncreaseRowSpan(int32 iIncrease)
{
    EDT_TableCellData *pData = GetData();
    if( pData )
    {
        pData->iRowSpan += iIncrease;
        SetData(pData);
        EDT_FreeTableCellData(pData);
    }
}

void CEditTableCellElement::DecreaseColSpan(int32 iDecrease)
{
    EDT_TableCellData *pData = GetData();
    if( pData )
    {
        pData->iColSpan = max(1, pData->iColSpan - iDecrease);
        SetData(pData);
        EDT_FreeTableCellData(pData);
    }
}

void CEditTableCellElement::DecreaseRowSpan(int32 iDecrease)
{
    EDT_TableCellData *pData = GetData();
    if( pData )
    {
        pData->iRowSpan = max(1, pData->iRowSpan - iDecrease);
        SetData(pData);
        EDT_FreeTableCellData(pData);
    }
}

// Save size percent and defined params so we can much with them during resizing,
//  and restore them later. Supply bWidthDefined and bHeightDefined
//  for efficiency so we don't have to do GetData() here
void CEditTableCellElement::SaveSizeMode(XP_Bool bWidthDefined, XP_Bool bHeightDefined)
{
    m_bSaveWidthPercent = m_bWidthPercent; 
    m_bSaveHeightPercent = m_bWidthPercent;
    m_bSaveWidthDefined = bWidthDefined;
    m_bSaveHeightDefined = bHeightDefined;
}

// This will restore saved width and height percent modes,
//   readjust m_bWidth an m_bHeight, and call SetData to set tag data
// Note: no need to relayout -- we are just changing modes,
//   not actual size of cell
void CEditTableCellElement::RestoreSizeMode(int32 iParentWidth, int32 iParentHeight)
{
    EDT_TableCellData *pData = GetData();

    if( iParentWidth == 0 )
        iParentWidth = GetParentWidth();
    if( iParentHeight == 0 )
        iParentHeight = GetParentHeight();

    if( pData )
    {
        if( m_bSaveWidthPercent )
        {
            // Convert existing Pixel value to % of parent size
            pData->iWidth = (m_iWidthPixels * 100) / iParentWidth;
            pData->iHeight = (m_iHeightPixels * 100) / iParentHeight;
        } else {
            // Converting to pixels is simple - just use the pixel value we already have
            pData->iWidth = m_iWidthPixels;
            pData->iHeight = m_iHeightPixels;
        }
        pData->bWidthDefined = m_bSaveWidthDefined;
        pData->bHeightDefined = m_bSaveHeightDefined;
        pData->bWidthPercent = m_bSaveWidthPercent;
        pData->bHeightPercent = m_bSaveHeightPercent;
        SetData(pData);
        EDT_FreeTableCellData(pData);
    }
}

// Calculate the Percent size from the iWidthPixels and iHeightPixels in pData
void CEditTableCellElement::CalcPercentWidth(EDT_TableCellData *pData)
{
    if( pData && pData->bWidthDefined )
    {
        if( pData->bWidthPercent )
            pData->iWidth = (pData->iWidthPixels * 100) / GetParentWidth(); 
        else
            pData->iWidth = pData->iWidthPixels;
    }
}

void CEditTableCellElement::CalcPercentHeight(EDT_TableCellData *pData)
{
    if( pData && pData->bHeightDefined ) 
    {
        if( pData->bHeightPercent )
            pData->iHeight = (pData->iHeightPixels * 100) / GetParentHeight();
        else
            pData->iHeight = pData->iHeightPixels;
    }
}

// Calculate the Pixel size from the iWidth and iHeight in pData
void CEditTableCellElement::CalcPixelWidth(EDT_TableCellData *pData)
{
    if( pData && pData->bWidthDefined )
    {
        if( pData->bWidthPercent )
            pData->iWidthPixels = (pData->iWidth * GetParentWidth()) / 100; 
        else
            pData->iWidthPixels = pData->iWidth;
    }
}

void CEditTableCellElement::CalcPixelHeight(EDT_TableCellData *pData)
{
    if( pData && pData->bHeightDefined )
    {
        if( pData->bHeightPercent )
            pData->iHeightPixels = (pData->iHeight * GetParentHeight()) / 100;
        else
            pData->iHeightPixels = pData->iHeight;
    }
}

void CEditTableCellElement::SetWidth(XP_Bool bWidthDefined, XP_Bool bWidthPercent, int32 iWidthPixels)
{
    EDT_TableCellData * pData = GetData(0);
    if( pData )
    {
        pData->bWidthDefined = bWidthDefined;
        pData->bWidthPercent = bWidthPercent;
        pData->iWidthPixels = iWidthPixels;
        CalcPercentWidth(pData);
        SetData(pData);
        EDT_FreeTableCellData(pData);
    }
}

void CEditTableCellElement::SetHeight(XP_Bool bHeightDefined, XP_Bool bHeightPercent, int32 iHeightPixels)
{
    EDT_TableCellData * pData = GetData(0);
    if( pData )
    {
        pData->bHeightDefined = bHeightDefined;
        pData->bHeightPercent = bHeightPercent;
        pData->iHeightPixels = iHeightPixels;
        CalcPercentHeight(pData);
        SetData(pData);
        EDT_FreeTableCellData(pData);
    }
}

// Set width of supplied cell and all other cells sharing or spanning the same right edge
// Also sets the bWidthDefined and bWidthPercent modes
void CEditTableCellElement::SetColumnWidthRight(CEditTableElement *pTable, LO_Element *pLoCell, EDT_TableCellData *pData)
{
    if( !pTable || !pLoCell || !pData || pData->iWidthPixels <= 0 )
        return;

    // Get LoTable to start scanning through cells
    LO_Element *pLoElement = (LO_Element*)lo_GetParentTable(NULL, pLoCell);
    if( !pLoElement )
        return;
    
    // Amount of change in supplied cell
    int32 iDeltaWidth = pData->iWidthPixels - pLoCell->lo_any.width;
    int32 iRightEdge = pLoCell->lo_cell.x + pLoCell->lo_cell.width;

    // Set the width for all cells sharing or spanning the right edge of cell resized
    while (pLoElement && pLoElement->type != LO_LINEFEED )
    {
        if( pLoElement->type == LO_CELL &&
            pLoElement->lo_cell.x < iRightEdge &&
            (pLoElement->lo_cell.x + pLoElement->lo_cell.width) >= iRightEdge )
        {
            CEditTableCellElement *pEdCell = 
                (CEditTableCellElement*)edt_GetTableElementFromLO_Element( pLoElement, LO_CELL );

            if( pEdCell )
            {
                pLoElement->lo_cell.width = max(1, pLoElement->lo_cell.width + iDeltaWidth);
                // Compensate for cell padding, border, and COLSPAN to get 
                //   the value to use for WIDTH tag param
                pEdCell->SetWidth( pData->bWidthDefined, pData->bWidthPercent,
                                   lo_GetCellTagWidth(pLoElement) );
            }
        }
        pLoElement = pLoElement->lo_any.next;
    }
}

// Set height of all cells sharing or spanning the bottom edge
// Also sets the bWidthDefined and bWidthPercent modes
void CEditTableCellElement::SetRowHeightBottom(CEditTableElement *pTable, LO_Element *pLoCell, EDT_TableCellData *pData)
{
    if( !pTable || !pLoCell || !pData || pData->iHeightPixels <= 0 )
        return;

    // Get LoTable to start scanning through cells
    LO_Element *pLoElement = (LO_Element*)lo_GetParentTable(NULL, pLoCell);
    if( !pLoElement )
        return;

    // Amount of change in supplied cell - use this for all cells we will resize
    int32 iDeltaHeight = pData->iHeightPixels - pLoCell->lo_any.height;
    int32 iBottomEdge = pLoCell->lo_cell.y + pLoCell->lo_cell.height;

    // Set the height for all cells sharing or spanning the right edge of cell resized
    while (pLoElement && pLoElement->type != LO_LINEFEED )
    {
        if( pLoElement->type == LO_CELL &&
            pLoElement->lo_cell.y < iBottomEdge &&
            (pLoElement->lo_cell.y + pLoElement->lo_cell.height) >= iBottomEdge )
        {
            CEditTableCellElement *pEdCell = 
                (CEditTableCellElement*)edt_GetTableElementFromLO_Element( pLoElement, LO_CELL );

            if( pEdCell )
            {
                pLoElement->lo_cell.height = max(1, pLoElement->lo_cell.height + iDeltaHeight); 
                pEdCell->SetHeight( pData->bHeightDefined, pData->bHeightPercent,
                                    lo_GetCellTagHeight(pLoElement) );
            }
        }
        pLoElement = pLoElement->lo_any.next;
    }
}

// Set width of supplied cell and all other cells sharing or spanning the same left edge
// Also sets the bWidthDefined and bWidthPercent modes
void CEditTableCellElement::SetColumnWidthLeft(CEditTableElement *pTable, CEditTableCellElement *pCell, EDT_TableCellData *pData)
{
    if( !pTable || !pCell || !pData || pData->iWidthPixels <= 0 )
        return;

    // Amount of change in supplied cell
    int32 iDeltaWidth = pData->iWidthPixels - pCell->GetWidth();

    // TRUE means we include cells that span over cell's left edge,
    //   not just start at that value. 
    pCell = pCell->GetFirstCellInColumn(pCell->GetX(), TRUE);
    while( pCell )
    {
        // Set width - be sure it is at least 1
        pCell->SetWidth( pData->bWidthDefined, pData->bWidthPercent,
                         max(1, pCell->GetWidth() + iDeltaWidth) );
        pCell = pCell->GetNextCellInColumn();
    }
}

// Set height of all cells sharing or spanning the top edge
// Also sets the bWidthDefined and bWidthPercent modes
void CEditTableCellElement::SetRowHeightTop(CEditTableElement *pTable, CEditTableCellElement *pCell, EDT_TableCellData *pData)
{
    if( !pTable || !pCell || !pData || pData->iHeightPixels <= 0 )
        return;

    // Amount of change in supplied cell - use this for all cells we will resize
    int32 iDeltaHeight = pData->iHeightPixels - pCell->GetHeight();

    // TRUE means we include cells that span over current cell's Y,
    //   not just start at Y
    pCell = pCell->GetFirstCellInRow(pCell->GetY(), TRUE);
    while( pCell )
    {
        // Set height - be sure it is at least 1
        pCell->SetHeight( pData->bHeightDefined, pData->bHeightPercent,
                          max(1, pCell->GetHeight() + iDeltaHeight) );
        pCell = pCell->GetNextCellInRow();
    }
}

void CEditTableCellElement::StreamOut(IStreamOut *pOut){
    CEditElement::StreamOut( pOut );
    // We write our X so we can accurately determine
    // the number of columns when streaming in (pasting)
    pOut->WriteInt( m_X );
}

void CEditTableCellElement::SetData( EDT_TableCellData *pData ){
    // bHeader is actually stored as the tag data
    if ( pData->bHeader ) {
        SetType(P_TABLE_HEADER);
    }
    else {
        SetType(P_TABLE_DATA);
    }
    //TODO: Account for interaction between default align and Header style
    char *pNew = 0;
    if( pData->align != ED_ALIGN_DEFAULT ){
        pNew = PR_sprintf_append( pNew, "ALIGN=%s ", EDT_AlignString(pData->align) );
    }
    if( pData->valign != ED_ALIGN_DEFAULT ){
        pNew = PR_sprintf_append( pNew, "VALIGN=%s ", EDT_AlignString(pData->valign) );
    }
    if ( pData->iColSpan != 1 ){
        pNew = PR_sprintf_append( pNew, "COLSPAN=\"%d\" ", pData->iColSpan );
    }
    if ( pData->iRowSpan != 1 ){
        pNew = PR_sprintf_append( pNew, "ROWSPAN=\"%d\" ", pData->iRowSpan );
    }
    if ( pData->bNoWrap ){
        pNew = PR_sprintf_append( pNew, "NOWRAP " );
    }
    if( pData->bWidthDefined ){
        if( pData->bWidthPercent ){
            pNew = PR_sprintf_append( pNew, "WIDTH=\"%ld%%\" ", 
                                      (long)min(100,max(1,pData->iWidth)) );
        }
        else {
            pNew = PR_sprintf_append( pNew, "WIDTH=\"%ld\" ", (long)pData->iWidth );
        }
    }
    if( pData->bHeightDefined ){
        if( pData->bHeightPercent ){
            pNew = PR_sprintf_append( pNew, "HEIGHT=\"%ld%%\" ", 
                                      (long)min(100,max(1, pData->iHeight)) );
        }
        else {
            pNew = PR_sprintf_append( pNew, "HEIGHT=\"%ld\" ", (long)pData->iHeight );
        }
    }

    if ( pData->pColorBackground ) {
        SetBackgroundColor(EDT_LO_COLOR(pData->pColorBackground));
        pNew = PR_sprintf_append( pNew, "BGCOLOR=\"#%06lX\" ", GetBackgroundColor().GetAsLong() );
    }
    else {
        SetBackgroundColor(ED_Color::GetUndefined());
    }

    if ( pData->pBackgroundImage ) {
        SetBackgroundImage(pData->pBackgroundImage);
        pNew = PR_sprintf_append( pNew, "BACKGROUND=\"%s\" ", pData->pBackgroundImage );
    }
    else {
        SetBackgroundImage(0);
    }
    if ( pData->bBackgroundNoSave) {
        pNew = PR_sprintf_append( pNew, "NOSAVE " );
    }

    if( pData->pExtra  ){
        pNew = PR_sprintf_append( pNew, "%s ", pData->pExtra );
    }

    if( pNew ){
        pNew = PR_sprintf_append( pNew, ">" );
    }
    SetTagData( pNew );
    if ( pNew ) {
        free(pNew);
    }

    // Save these since they are not saved in tag data if bWidthPercent = FALSE
    m_X = pData->X;
    m_Y = pData->Y;

    m_iWidthPixels = pData->iWidthPixels;
    m_iHeightPixels = pData->iHeightPixels;

    m_iWidth = pData->iWidth;
    m_bWidthPercent = pData->bWidthPercent;

    m_iHeight = pData->iHeight;
    m_bHeightPercent = pData->bHeightPercent;

    m_iRow = pData->iRow;
    m_iColSpan = pData->iColSpan;
    m_iRowSpan = pData->iRowSpan;
}

EDT_TableCellData* CEditTableCellElement::GetData( int16 csid )
{
    //TODO: Account for interaction between default align and Header style
    EDT_TableCellData *pRet;
    PA_Tag* pTag = TagOpen(0);
    // Must supply csid if cell is not already in the document
    if( csid == 0 )
        csid = GetWinCSID();
    
    // Get params stored in the HTML tag struct
    pRet = ParseParams( pTag, csid );
    PA_FreeTag( pTag );
    // Return the actual X, Y, width, and height
    //    as determined by table layout algorithm
    //   This is set correctly event if bWidthDefined = FALSE
    pRet->X = m_X;
    pRet->Y = m_Y;
    pRet->iRow = m_iRow;
    pRet->iWidth = m_iWidth;
    pRet->iWidthPixels = m_iWidthPixels;
    pRet->iHeight = m_iHeight;
    pRet->iHeightPixels = m_iHeightPixels;

    pRet->iRow = m_iRow;
    pRet->iColSpan = m_iColSpan;
    pRet->iRowSpan = m_iRowSpan;
    pRet->mask = -1;
    pRet->iSelectionType = ED_HIT_NONE;
    pRet->iSelectedCount = 0;
    return pRet;
}

// Include all borders, spacing, and padding up to the next cell
int32 CEditTableCellElement::GetFullWidth(CEditTableElement *pTable)
{
    if( !pTable )
        pTable = GetParentTable();
    if( !pTable )
        return 0;

    return (m_iWidthPixels + 2*(pTable->GetCellBorder() + pTable->GetCellPadding()) +
            m_iColSpan * pTable->GetCellSpacing() );
}

int32 CEditTableCellElement::GetFullHeight(CEditTableElement *pTable)
{
    if( !pTable )
        pTable = GetParentTable();
    if( !pTable )
        return 0;

    return (m_iHeightPixels + 2*(pTable->GetCellBorder() + pTable->GetCellPadding()) +
            m_iRowSpan * pTable->GetCellSpacing() );
}

static XP_Bool edt_CompareLoColors( LO_Color *pLoColor1, LO_Color *pLoColor2 )
{
    // One or the other doesn't have color,
    //  they are different
    if( (!pLoColor1 && pLoColor2) ||
        (pLoColor1 && !pLoColor2 ) )
    {
        return FALSE;
    }
    // Neither has a color - the same
    if( !pLoColor1 && !pLoColor2 )
        return TRUE;

    // Any color is different
    if( pLoColor1->red != pLoColor2->red ||
        pLoColor1->green != pLoColor2->green ||
        pLoColor1->blue != pLoColor2->blue )
    {
        return FALSE;
    }
    // All must be the same
    return TRUE;
}

static XP_Bool edt_CompareStrings( char *pString1, char *pString2, XP_Bool bFilenameCompare )
{
    // One or the other doesn't exist,
    //  they are different
    if( (!pString1 && pString2) ||
        (pString1 && !pString2 ) )
    {
        return FALSE;
    }
    // Neither exists - the same
    if( !pString1 && !pString2 )
    {
        return TRUE;
    }
    
    // Compare the strings
    if( bFilenameCompare )
    {
        // This compare pays NO attention to case differences for Windows only
        return (0 == XP_FILENAMECMP(pString1, pString2));
    }

    return (0 == XP_STRCMP(pString1, pString2));
}

void CEditTableCellElement::MaskData( EDT_TableCellData *pData )
{
    if( !pData )
        return;
    
    EDT_TableCellData *pCurrentData = GetData();

    // Change data only for attributes whose bit is set in data mask
    if( (pData->mask & CF_ALIGN) &&
        pCurrentData->align != pData->align )
    {
        pData->mask &= ~CF_ALIGN;
    }

    if( (pData->mask & CF_VALIGN) &&
        pCurrentData->valign != pData->valign )
    {
        pData->mask &= ~CF_VALIGN;
    }

    if( (pData->mask & CF_COLSPAN) &&
        pCurrentData->iColSpan != pData->iColSpan )
    {
        pData->mask &= ~CF_COLSPAN;
    }

    if( (pData->mask & CF_ROWSPAN) &&
        pCurrentData->iRowSpan != pData->iRowSpan )
    {
        pData->mask &= ~CF_ROWSPAN;
    }

    if( (pData->mask & CF_HEADER) &&
        pCurrentData->bHeader != pData->bHeader )
    {
        pData->mask &= ~CF_HEADER;
    }

    if( (pData->mask & CF_NOWRAP) &&
        pCurrentData->bNoWrap != pData->bNoWrap )
    {
        pData->mask &= ~CF_NOWRAP;
    }

    if( (pData->mask & CF_BACK_NOSAVE) &&
        pCurrentData->bBackgroundNoSave != pData->bBackgroundNoSave )
    {
        pData->mask &= ~CF_BACK_NOSAVE;
    }

    if( (pData->mask & CF_WIDTH) &&
        (pCurrentData->bWidthDefined != pData->bWidthDefined ||
         pCurrentData->iWidth != pData->iWidth ||
         pCurrentData->bWidthPercent != pData->bWidthPercent) )
    {
        pData->mask &= ~CF_WIDTH;
    }

    if( (pData->mask & CF_HEIGHT) &&
        (pCurrentData->bHeightDefined != pData->bHeightDefined ||
         pCurrentData->iHeight != pData->iHeight ||
         pCurrentData->bHeightPercent != pData->bHeightPercent) )
    {
        pData->mask &= ~CF_HEIGHT;
    }

    if( (pData->mask & CF_BACK_COLOR) &&
         !edt_CompareLoColors(pCurrentData->pColorBackground, pData->pColorBackground) )
    {
        pData->mask &= ~CF_BACK_COLOR;
    }

    // This pays NO attention to case differences for Windows only
    if( (pData->mask & CF_BACK_IMAGE) &&
        !edt_CompareStrings(pCurrentData->pBackgroundImage, pData->pBackgroundImage, TRUE) )
    {
        pData->mask &= ~CF_BACK_IMAGE;
    }

    // This pays attention to case differences
    if( (pData->mask & CF_EXTRA_HTML) &&
        !edt_CompareStrings(pCurrentData->pExtra, pData->pExtra, FALSE) )
    {
        pData->mask &= ~CF_EXTRA_HTML;
    }
}

// Add up widths of all cells in the first row of table
// This is available total width for "Percent of Table" calculation
int32 CEditTableCellElement::GetParentWidth()
{
    // Don't return 0 to avoid divide by zero errors
    int32 iParentWidth = 1;

    CEditTableElement *pTable = GetParentTable();
    if( pTable )
    {
        CEditTableCellElement *pCell = pTable->GetFirstCell();
        while( pCell && pCell->IsTableCell() )
        {
            iParentWidth += pCell->GetWidth(); // TODO: SHOULD WE USE GetFullWidth()???
            pCell = (CEditTableCellElement*)(pCell->GetNextSibling());
        }
    }
    return iParentWidth;
}

int32 CEditTableCellElement::GetParentHeight()
{
    // Don't return 0 to avoid divide by zero errors
    int32 iParentHeight = 1;

    CEditTableElement *pTable = GetParentTable();
    if( pTable )
    {
        CEditTableCellElement *pCell = pTable->GetFirstCell();
        while( pCell )
        {
            iParentHeight += pCell->GetHeight();
            pCell = pTable->GetNextCellInColumn(pCell);
        }
    }
    return iParentHeight;
}

static char *tableCellParams[] = {
    PARAM_ALIGN,
    PARAM_VALIGN,
    PARAM_COLSPAN,
    PARAM_ROWSPAN,
    PARAM_NOWRAP,
    PARAM_WIDTH,
    PARAM_HEIGHT,
    PARAM_BGCOLOR,
    PARAM_BACKGROUND,
    PARAM_NOSAVE,
    0
};

EDT_TableCellData* CEditTableCellElement::ParseParams( PA_Tag *pTag, int16 csid ){
    EDT_TableCellData *pData = NewData();
    
    ED_Alignment align;
    
    align = edt_FetchParamAlignment( pTag, ED_ALIGN_DEFAULT, FALSE, csid );
    if( align == ED_ALIGN_RIGHT || align == ED_ALIGN_LEFT || align == ED_ALIGN_ABSCENTER ){
        pData->align = align;
    }

    align = edt_FetchParamAlignment( pTag, ED_ALIGN_DEFAULT, TRUE, csid );
    if( align == ED_ALIGN_ABSTOP || align == ED_ALIGN_ABSBOTTOM || align == ED_ALIGN_ABSCENTER
        || align == ED_ALIGN_BASELINE ){
        pData->valign = align;
    }

    pData->iColSpan = edt_FetchParamInt(pTag, PARAM_COLSPAN, 1, csid);
    pData->iRowSpan = edt_FetchParamInt(pTag, PARAM_ROWSPAN, 1, csid);
    m_iColSpan = pData->iColSpan;
    m_iRowSpan = pData->iRowSpan;
    pData->bHeader = GetType() == P_TABLE_HEADER;
    pData->bNoWrap = edt_FetchParamBoolExist(pTag, PARAM_NOWRAP, csid );

    pData->bWidthDefined = edt_FetchDimension( pTag, PARAM_WIDTH, 
                    &pData->iWidth, &pData->bWidthPercent,
                    100, TRUE, csid );
    pData->bHeightDefined = edt_FetchDimension( pTag, PARAM_HEIGHT, 
                    &pData->iHeight, &pData->bHeightPercent,
                    100, TRUE, csid );
    
    // This and CEditBuffer::FixupTableData() are the only
    //   places member variable should be set
    // If width and/or height are NOT defined, use the
    //    "bPercent" values set by any previous SetData() calls
    if( pData->bWidthDefined )
        m_iWidth = pData->iWidth;
    else
        pData->bWidthPercent = m_bWidthPercent;

    if( pData->bHeightDefined )
        m_iHeight = pData->iHeight;
    else
        pData->bHeightPercent = m_bHeightPercent;

    pData->pColorBackground = edt_MakeLoColor(edt_FetchParamColor( pTag, PARAM_BGCOLOR, csid ));
    pData->pBackgroundImage = edt_FetchParamString( pTag, PARAM_BACKGROUND, csid );
    pData->bBackgroundNoSave = edt_FetchParamBoolExist( pTag, PARAM_NOSAVE, csid );
    pData->pExtra = edt_FetchParamExtras( pTag, tableCellParams, csid );
    return pData;
}

EDT_TableCellData* CEditTableCellElement::NewData(){
    EDT_TableCellData *pData = XP_NEW( EDT_TableCellData );
    if( pData == 0 ){
        // throw();
        return pData;
    }
    pData->align = ED_ALIGN_DEFAULT;
    pData->valign = ED_ALIGN_ABSTOP; //ED_ALIGN_TOP; // was ED_ALIGN_DEFAULT; this is centerred - stupid!
    pData->iColSpan = 1;
    pData->iRowSpan = 1;
    pData->bHeader = FALSE;
    pData->bNoWrap = FALSE;
    pData->bWidthDefined = FALSE;
    pData->bWidthPercent = FALSE;
    pData->iWidth = 1;
    pData->iWidthPixels = 1;
    pData->iRow = 0;
    pData->iHeight = 1;
    pData->iHeightPixels = 1;
    pData->bHeightDefined = FALSE;
    pData->bHeightPercent = FALSE;
    pData->pColorBackground = 0;
    pData->pBackgroundImage = 0;
    pData->bBackgroundNoSave = FALSE;
    pData->pExtra = 0;
    pData->mask = -1;
    pData->iSelectionType = ED_HIT_NONE;
    pData->iSelectedCount = 0;
    pData->X = 0;
    pData->Y = 0;
    return pData;
}

void CEditTableCellElement::FreeData( EDT_TableCellData *pData ){
    if( pData->pColorBackground ) XP_FREE( pData->pColorBackground );
    if( pData->pBackgroundImage ) XP_FREE( pData->pBackgroundImage );
    if( pData->pExtra ) XP_FREE( pData->pExtra );
    XP_FREE( pData );
}

void CEditTableCellElement::SetBackgroundColor( ED_Color iColor ){
    m_backgroundColor = iColor;
}

ED_Color CEditTableCellElement::GetBackgroundColor(){
    return m_backgroundColor;
}

void CEditTableCellElement::SetBackgroundImage( char* pImage ){
    delete m_pBackgroundImage;
    m_pBackgroundImage = 0;
    if ( pImage ) {
        m_pBackgroundImage = XP_STRDUP(pImage);
    }
}

char* CEditTableCellElement::GetBackgroundImage(){
    return m_pBackgroundImage;
}

LO_CellStruct* CEditTableCellElement::GetLoCell()
{
    // Find the first leaf element in the cell since
    //  it will have pointer to LO_Element
    CEditElement *pLeaf = FindNextElement(&CEditElement::FindLeafAll,0 );
    if( pLeaf )
    {
        // Only leaf edit elements have their pointers saved in LO_Elements
        LO_Element *pLoElement = pLeaf->Leaf()->GetLayoutElement();
        if( pLoElement )
        {
            // Find enclosing LO cell
            LO_CellStruct *pLoCell = lo_GetParentCell(GetEditBuffer()->m_pContext, pLoElement);
            // We shoul ALWAYS find a cell
            XP_ASSERT(pLoCell);
            return pLoCell;
        }
    }
    return NULL;
}

CEditTableCellElement* CEditTableCellElement::GetPreviousCellInTable(intn *pRowCounter)
{
    // Simplest case is previous cell in the row
    CEditElement *pPreviousCell = GetPreviousSibling();
    if( !pPreviousCell )
    {
        // Get the last child cell of the previous row
        if( GetParent() && GetParent()->GetPreviousSibling() )
            pPreviousCell = GetParent()->GetPreviousSibling()->GetLastChild(); 
#ifdef DEBUG
        if( pPreviousCell ) XP_ASSERT(pPreviousCell->IsTableCell());
#endif
        // Tell caller we wrapped to the previous row
        if( pRowCounter && pPreviousCell )
            (*pRowCounter)--;
    }
    return (CEditTableCellElement*)pPreviousCell;
}

CEditTableCellElement* CEditTableCellElement::GetNextCellInTable(intn *pRowCounter)
{
    // Simplest case is next cell in the row
    CEditElement *pNextCell = GetNextSibling();
    if( !pNextCell )
    {
        // Get the first child cell of the next row
        if( GetParent() && GetParent()->GetNextSibling() )
            pNextCell = GetParent()->GetNextSibling()->GetChild(); 
#ifdef DEBUG
        if( pNextCell ) XP_ASSERT(pNextCell->IsTableCell());
#endif
        // Tell caller we wrapped to the next row
        if( pRowCounter && pNextCell )
            (*pRowCounter)++;
    }
    return (CEditTableCellElement*)pNextCell;
}

CEditTableCellElement* CEditTableCellElement::GetNextCellInLogicalRow()
{
    CEditElement *pNextCell = GetNextSibling();
    if( pNextCell && pNextCell->IsTableCell() )
        return pNextCell->TableCell();
    
    return NULL;
}

CEditTableCellElement* CEditTableCellElement::GetFirstCellInRow(int32 Y, XP_Bool bSpan)
{
    // No Y supplied - just get it from the cell
    if( Y == -1 )
        Y = GetY();

    CEditTableElement *pTable = GetParentTable();
    if( pTable )
        return pTable->GetFirstCellInRow(Y, bSpan);

    return NULL;
}

CEditTableCellElement* CEditTableCellElement::GetFirstCellInColumn(int32 X, XP_Bool bSpan)
{
    // No X supplied - just get it from the cell
    if( X == -1 )
        X = GetX();

    CEditTableElement *pTable = GetParentTable();
    if( pTable )
        return pTable->GetFirstCellInColumn(X, bSpan);

    return NULL;
}

CEditTableCellElement* CEditTableCellElement::GetNextCellInRow(CEditTableCellElement *pCell)
{
    CEditTableElement *pTable = GetParentTable();
    if( pTable )
        return pTable->GetNextCellInRow(pCell);

    return NULL;
}

CEditTableCellElement* CEditTableCellElement::GetNextCellInColumn(CEditTableCellElement *pCell)
{
    CEditTableElement *pTable = GetParentTable();
    if( pTable )
        return pTable->GetNextCellInColumn(pCell);

    return NULL;
}

XP_Bool CEditTableCellElement::AllCellsInColumnAreSelected()
{
    CEditTableCellElement *pCell = GetFirstCellInColumn();
    XP_Bool bAllSelected = TRUE;
    while( pCell )
    {
        if( !pCell->IsSelected() )
            return FALSE;

        pCell = GetNextCellInColumn();
    }
    return TRUE;
}

XP_Bool CEditTableCellElement::AllCellsInRowAreSelected()
{
    CEditTableCellElement *pCell = GetFirstCellInRow();
    XP_Bool bAllSelected = TRUE;
    while( pCell )
    {
        if( !pCell->IsSelected() )
            return FALSE;

        pCell = GetNextCellInRow();
    }
    return TRUE;
}

XP_Bool CEditTableCellElement::IsEmpty()
{
    CEditElement *pElement = GetFirstMostChild();

    // Should never happen, but if it does,
    //  I guess we can consider it empty!
    if( !pElement )
        return TRUE;

    // We are empty if no more sibling containers
    //   and cell only has an empty string
    if( GetChild()->GetNextSibling() == 0 && pElement->IsText() )
    {
        char *pText = pElement->Text()->GetText();
        if( pText == 0 || XP_STRLEN(pText) == 0 )
        {
            return TRUE;
        }
    }
    return FALSE;
}

// Move all contents of supplied cell into this cell
void CEditTableCellElement::MergeCells(CEditTableCellElement* pCell)
{
    if( !pCell || pCell == this )
        return;

    // Don't merge cells with only single, empty string as only element
    // Just delete the cell to be merged
    if( !pCell->IsEmpty() )
    {
        CEditElement *pLastChild = GetChild();
        if(!pLastChild)
            return;

        // First child container to move
        CEditElement *pMoveChild = pCell->GetChild();

        if( IsEmpty() )
        {
            // We are moving into an empty cell,
            //  so delete the single container
            delete pLastChild;
            // Make the first element the first 
            //  container we are moving
            pMoveChild->SetParent( this );
            SetChild(pMoveChild);
            
            // Setup for moving other containers
            pLastChild = pMoveChild;
            pMoveChild = pMoveChild->GetNextSibling();
        } else {
            // Insert after last existing child of cell,
            // There is often only 1 container under each cell,
            //   except if there are multiple paragraphs or
            //   cell contents already merged
            while( pLastChild->GetNextSibling() )
                pLastChild = pLastChild->GetNextSibling();
        }

        while(pMoveChild)
        {
            // Move container by changing appropriate pointers
            pLastChild->SetNextSibling( pMoveChild );
            pMoveChild->SetParent( this );
            pLastChild = pMoveChild;
            pMoveChild = pMoveChild->GetNextSibling();
        }

        // Clear pointer to children just moved
        pCell->SetChild(0);
    }
    delete pCell;
}

void CEditTableCellElement::DeleteContents(XP_Bool bMarkAsDeleted)
{
    // Get the first conainer in the cell
    CEditElement *pChild = GetChild();

    // Delete all other containers
    while( pChild )
    {
        CEditElement *pNext = pChild->GetNextSibling();
        pChild->Unlink();
        delete pChild;
        pChild = pNext;
    }
    // Set state for new cell contents according to preference
    // (we may put a space in the blank cell)
    GetEditBuffer()->SetFillNewCellWithSpace();

    // Create a default paragraph container,
    //  this will set cell as its parent
    CEditContainerElement *pContainer = CEditContainerElement::NewDefaultContainer(this, ED_ALIGN_DEFAULT);
    
    // Initialize the cell contents
    if( pContainer )
        pContainer->FinishedLoad(GetEditBuffer());

    GetEditBuffer()->ClearFillNewCellWithSpace();

    // Mark this cell as deleted so CEditBuffer::DeleteSelectedCells()
    //  can skip over this during multiple deletion passes
    if( bMarkAsDeleted )
        m_bDeleted = TRUE;
}

// Insert as last child of supplied parent 
//   but first save current parent and next pointers
void CEditTableCellElement::SwitchLinkage(CEditTableRowElement *pParentRow)
{
    // Save current location in tree
    m_pSaveParent = GetParent();
    m_pSaveNext = GetNextSibling();
#ifdef DEBUG
    // This prevents XP_ASSERT when trying to InsertAsLastChild;
    SetParent(0);
#endif
    // Insert into new row
    InsertAsLastChild((CEditElement*)pParentRow);
}

// Switches back to saved parent and next pointers
void CEditTableCellElement::RestoreLinkage()
{
    Unlink();
    SetParent(m_pSaveParent);
    SetNextSibling(m_pSaveNext);
    CEditElement *pParent = GetParent();
}

// Platform-specific end-of-line character(s) added to string,
//  returning in possibly-reallocated buffer that caller must free
char *edt_AppendEndOfLine(char *pText)
{
    //TODO: FIX THIS FOR MAC AND UNIX LINE-END CONVENTIONS
    if( pText )
        return PR_sprintf_append( pText, "\r\n" );
    
    return NULL;
}

char* CEditTableCellElement::GetText(XP_Bool bJoinParagraphs)
{
    char *pCellText = NULL;
    CEditElement *pChild = GetChild();

    //Prevents adding delimiter before first element
    XP_Bool bFirst = TRUE;

    while( pChild )
    {
        if( pChild->IsContainer() )
        {
            char *pText = pChild->Container()->GetText();
            if( pText && *pText )
            {
                if( !bFirst )
                {
                    if( bJoinParagraphs )
                    {
                        // Add 2 spaces between paragraphs instead of CR/LF
                        //  so we can merge all contents in table cell
                        pCellText = PR_sprintf_append( pCellText, "  " );
                    } else {
                        pCellText = edt_AppendEndOfLine( pCellText);
                    }
                }                    
                pCellText = PR_sprintf_append(pCellText, pText);
                XP_FREE(pText);

                bFirst = FALSE;
            }
        }
        pChild = pChild->GetNextSibling();
    }
    // We must follow the rule that every cell must have 
    //   at least 1 empty text element in it,
    //   so return an empty string if nothing found
    if( !pCellText )
        return XP_STRDUP("");

    return pCellText;
}


// class CEditLayerElement

CEditLayerElement::CEditLayerElement()
    : CEditElement(0, P_META) // P_LAYER, 0)
{
    EDT_LayerData* pData = NewData();
    // pData->iColumns = columns;
    SetData(pData);
    FreeData(pData);
}

CEditLayerElement::CEditLayerElement(CEditElement *pParent, PA_Tag *pTag, int16 /*csid*/)
    : CEditElement(pParent, P_META) // P_LAYER)
{
    if( pTag ){
        char *locked_buff;
            
        PA_LOCK(locked_buff, char *, pTag->data );
        if( locked_buff ){
            SetTagData( locked_buff );
        }
        PA_UNLOCK(pTag->data);
    }
}

CEditLayerElement::CEditLayerElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer)
    : CEditElement(pStreamIn, pBuffer)
{
}

void CEditLayerElement::FinishedLoad( CEditBuffer* pBuffer ){
    EnsureSelectableSiblings(pBuffer);
    CEditElement::FinishedLoad(pBuffer);
}

void CEditLayerElement::SetData( EDT_LayerData *pData ){
    char *pNew = 0;
    /* if ( pData->iColumns > 1){
        pNew = PR_sprintf_append( pNew, "COLS=%d ", pData->iColumns );
    } */
    if( pData->pExtra  ){
        pNew = PR_sprintf_append( pNew, "%s ", pData->pExtra );
    }
    if( pNew ){
        pNew = PR_sprintf_append( pNew, ">" );
    }
    SetTagData( pNew );
    if ( pNew ) {
        free(pNew);
    }
}

EDT_LayerData* CEditLayerElement::GetData( ){
    EDT_LayerData *pRet;
    PA_Tag* pTag = TagOpen(0);
    pRet = ParseParams( pTag, GetWinCSID() );
    PA_FreeTag( pTag );
    return pRet;
}

static char *LayerParams[] = {
    // PARAM_COLS,
    0
};

EDT_LayerData* CEditLayerElement::ParseParams( PA_Tag *pTag, int16 csid ){
    EDT_LayerData *pData = NewData();
    
    // pData->iColumns = edt_FetchParamInt(pTag, PARAM_COLS, 1);
    pData->pExtra = edt_FetchParamExtras( pTag, LayerParams, csid );

    return pData;
}

EDT_LayerData* CEditLayerElement::NewData(){
    EDT_LayerData *pData = XP_NEW( EDT_LayerData );
    if( pData == 0 ){
        // throw();
        return pData;
    }
    // pData->iColumns = 1;
    pData->pExtra = 0;
    return pData;
}

void CEditLayerElement::FreeData( EDT_LayerData *pData ){
    if( pData->pExtra ) XP_FREE( pData->pExtra );
    XP_FREE( pData );
}

void CEditLayerElement::PrintOpen( CPrintState *pPrintState ){
    TagType tSave = GetType();
    SetType( P_LAYER );
    CEditElement::PrintOpen( pPrintState );
    SetType( tSave );
}

void CEditLayerElement::PrintEnd( CPrintState *pPrintState ){
    TagType tSave = GetType();
    SetType( P_LAYER );
    CEditElement::PrintEnd( pPrintState );
    SetType( tSave );
}

// CEditDivisionElement

CEditDivisionElement::CEditDivisionElement()
    : CEditElement(0, P_DIVISION, 0)
{
    EDT_DivisionData* pData = NewData();
    SetData(pData);
    FreeData(pData);
}

CEditDivisionElement::CEditDivisionElement(CEditElement *pParent, PA_Tag *pTag, int16 /*csid*/)
    : CEditElement(pParent, P_DIVISION)
{
    if( pTag ){
        char *locked_buff;
            
        PA_LOCK(locked_buff, char *, pTag->data );
        if( locked_buff ){
            SetTagData( locked_buff );
        }
        PA_UNLOCK(pTag->data);
    }
}

CEditDivisionElement::CEditDivisionElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer)
    : CEditElement(pStreamIn, pBuffer)
{
}

XP_Bool CEditDivisionElement::IsDivision() { return TRUE; }
CEditDivisionElement* CEditDivisionElement::Cast(CEditElement* pElement) {
    return pElement && pElement->IsDivision() ? (CEditDivisionElement*) pElement : 0; }

EEditElementType CEditDivisionElement::GetElementType() { return eDivisionElement; }
XP_Bool CEditDivisionElement::IsAcceptableChild(CEditElement& pChild){
    return ! pChild.IsLeaf();
}

void CEditDivisionElement::SetData( EDT_DivisionData *pData ){
    char *pNew = 0;
    if( pData->align != ED_ALIGN_DEFAULT ){
        pNew = PR_sprintf_append( pNew, "ALIGN=%s ", EDT_AlignString(pData->align) );
    }
    if( pData->pExtra  ){
        pNew = PR_sprintf_append( pNew, "%s ", pData->pExtra );
    }
    if( pNew ){
        pNew = PR_sprintf_append( pNew, ">" );
    }
    SetTagData( pNew );
    if ( pNew ) {
        free(pNew);
    }
}

EDT_DivisionData* CEditDivisionElement::GetData( ){
    EDT_DivisionData *pRet;
    PA_Tag* pTag = TagOpen(0);
    pRet = ParseParams( pTag, GetWinCSID() );
    PA_FreeTag( pTag );
    return pRet;
}

ED_Alignment CEditDivisionElement::GetAlignment(){
    ED_Alignment result = ED_ALIGN_DEFAULT;
    EDT_DivisionData* pData = GetData();
    if ( pData ) {
        result = pData->align;
        FreeData(pData);
    }
    return result;
}

void CEditDivisionElement::ClearAlignment(){
    EDT_DivisionData* pData = GetData();
    if ( pData ) {
        pData->align = ED_ALIGN_DEFAULT;
        SetData(pData);
        FreeData(pData);
    }
}

XP_Bool CEditDivisionElement::HasData(){
    // Does this have data other than the ALIGN tag?
    XP_Bool hasData = FALSE;
    EDT_DivisionData* pData = GetData();
    if ( pData ) {
        if ( pData->pExtra || pData->align != ED_ALIGN_DEFAULT ) {
            hasData = TRUE;
        }
        FreeData(pData);
    }
    return hasData;
}

static char *DivisionParams[] = {
    PARAM_ALIGN,
    0
};

EDT_DivisionData* CEditDivisionElement::ParseParams( PA_Tag *pTag, int16 csid ){
    EDT_DivisionData *pData = NewData();
    
    pData->pExtra = edt_FetchParamExtras( pTag, DivisionParams, csid );
    pData->align = edt_FetchParamAlignment( pTag, ED_ALIGN_DEFAULT, FALSE, csid );

    return pData;
}

EDT_DivisionData* CEditDivisionElement::NewData(){
    EDT_DivisionData *pData = XP_NEW( EDT_DivisionData );
    if( pData == 0 ){
        // throw();
        return pData;
    }
    pData->align = ED_ALIGN_DEFAULT;
    pData->pExtra = 0;
    return pData;
}

void CEditDivisionElement::FreeData( EDT_DivisionData *pData ){
    if ( pData ) {
        XP_FREEIF( pData->pExtra );
        XP_FREE( pData );
    }
}

void CEditDivisionElement::PrintOpen( CPrintState *pPrintState ){
    // Don't print <DIV> tag if it is a no-op.
    if ( HasData() ) {
        CEditElement::PrintOpen(pPrintState);
    }
}

void CEditDivisionElement::PrintEnd( CPrintState *pPrintState ){
    // Don't print <DIV> tag if it is a no-op.
    if ( HasData() ) {
        CEditElement::PrintEnd(pPrintState);
    }
}


//-----------------------------------------------------------------------------
// CEditRootDocElement
//-----------------------------------------------------------------------------
void CEditRootDocElement::PrintOpen( CPrintState *pPrintState ){
    m_pBuffer->PrintDocumentHead( pPrintState );
}

void CEditRootDocElement::PrintEnd( CPrintState *pPrintState ){
    m_pBuffer->PrintDocumentEnd( pPrintState );
}

void CEditRootDocElement::FinishedLoad( CEditBuffer *pBuffer ){
    CEditSubDocElement::FinishedLoad(pBuffer);
    if ( ! GetLastChild() || ! GetLastChild()->IsEndContainer() ) {
        CEditEndContainerElement* pEndContainer = new CEditEndContainerElement(NULL);
        // Creating the element automaticly inserts it.
        (void) new CEditEndElement(pEndContainer);
        pEndContainer->InsertAsLastChild(this);
    }
}

#ifdef DEBUG
void CEditRootDocElement::ValidateTree(){
    CEditElement::ValidateTree();
    XP_ASSERT(GetParent() == NULL);
    XP_ASSERT(GetChild() != NULL);
    XP_ASSERT(GetLastMostChild()->GetElementType() == eEndElement);
}

#endif


//-----------------------------------------------------------------------------
// CEditLeafElement
//-----------------------------------------------------------------------------


XP_Bool CEditLeafElement::PrevPosition(ElementOffset iOffset, 
                CEditLeafElement*& pNew, ElementOffset& iNewOffset ){

    XP_Bool bPreviousElement = FALSE;
    XP_Bool bMoved = TRUE;

    iNewOffset = iOffset - 1;
    pNew = this;

    if( iNewOffset == 0 ){

        CEditElement *pPrev = PreviousLeafInContainer();
        if( pPrev ){
            if( pPrev->IsLeaf()){
                bPreviousElement = TRUE;
            }
        }
    }

    //
    // we need to set the position to the end of the previous element.
    //
    if( iNewOffset < 0 || bPreviousElement ){
        CEditLeafElement *pPrev = PreviousLeaf();
        if( pPrev ){
            if( pPrev->IsLeaf()){
                pNew = pPrev;
                iNewOffset = pNew->Leaf()->GetLen();
            }
        }
        else{
            // no previous element, we are at the beginning of the buffer.
            iNewOffset = iOffset;
            bMoved = FALSE;
        }
    }
    return bMoved;
}

XP_Bool CEditLeafElement::NextPosition(ElementOffset iOffset, 
                CEditLeafElement*& pNew, ElementOffset& iNewOffset ){
    LO_Element* pRetText;
    CEditLeafElement *pNext;
    int iRetPos;
    XP_Bool retVal = TRUE;

    pNew = this;
    iNewOffset = iOffset + 1;

    //
    // if the new offset is obviously greater, move past it.  If
    //  Make sure the layout engine actually layed out the element.
    //
    if( iNewOffset > Leaf()->GetLen() ||
        !Leaf()->GetLOElementAndOffset( iOffset, FALSE, pRetText, iRetPos ))
    {
        pNext = NextLeaf();
        if( pNext ){
            pNew = pNext;
            if( pNext->PreviousLeafInContainer() == 0 ){
                iNewOffset = 0;
            }
            else {
                iNewOffset = 1;
            }
        }
        else {
            iNewOffset = iOffset;
            retVal = FALSE;
        }
    }
    return retVal;
}

void CEditLeafElement::DisconnectLayoutElements(LO_Element* /*pElement*/){
    // The intention of this code was to make sure that live layout elements
    // didn't have back-pointers to dead edit elements.
    //
    // Unfortunately, if an edit element is cut out of the root,
    // and then deleted after a relayout,
    // the layout element will be stale. And that will cause
    // a Free Memory Read. This isn't usually a problem, except
    // on NT, where the rolling heap may have already freed the
    // heap that had the stale layout element. This happens
    // most frequently when large tables are deleted.
    //
    // This code doesn't seem to be needed for correct operation of
    // Composer. That's a good thing, because it can't be
    // made to work without a formal set of rules on the
    // lifetime of edit elements and layout elements.
#if 0
    // Clear back pointer out of the edit elements.
    LO_Element* e = pElement;
    while ( e && e->lo_any.edit_element == this ){
        e->lo_any.edit_element = 0;
        while ( e && e->lo_any.edit_element == 0 ){
            e = e->lo_any.next;
        }
    }
#endif
}

void CEditLeafElement::SetLayoutElementHelper( intn desiredType, LO_Element** pElementHolder,
                                              intn iEditOffset, intn lo_type, 
                                              LO_Element* pLoElement){
    // XP_TRACE(("SetLayoutElementHelper this(0x%08x) iEditOffset(%d) pLoElement(0x%08x)", this, iEditOffset, pLoElement));

    if( lo_type == desiredType ){
        // iEditOffset is:
        // 0 for normal case,
        // -1 for end of document element.
        // > 0 for text items after a line wrap.
        //     Don't disconnect layout elements because this is
        //     called for each layout element of a wrapped text
        //     edit element. Since DisconnectLayoutElements goes
        //     ahead and zeros out all the "next" elements with
        //     the same back pointer, we end up trashing later
        //     elements if the elements are set in some order
        //     other than first-to-last. This happens when just the 2nd line of a two-line
        //     text element is being relaid out.
        if ( iEditOffset <= 0 ) {
            // Don't reset the old element, because functions like lo_BreakOldElement depend upon the
            // old element having a good value.
            *pElementHolder = pLoElement;
        }
        pLoElement->lo_any.edit_element = this;
        pLoElement->lo_any.edit_offset = iEditOffset;
    }
    else {
        // We get called back whenever linefeeds are generated. That's so that
        // breaks can find their linefeed. But it also means we are
        // called back more often than nescessary.
    //    XP_TRACE(("  ignoring inconsistent type.\n"));
    }
}

void CEditLeafElement::ResetLayoutElementHelper( LO_Element** pElementHolder, intn iEditOffset, 
            LO_Element* pLoElement ){
    // XP_TRACE(("ResetLayoutElementHelper this(0x%08x) iEditOffset(%d) pLoElement(0x%08x)", this, iEditOffset, pLoElement));
    if ( iEditOffset <= 0 ) {
        if ( *pElementHolder == pLoElement ){
            *pElementHolder = NULL;
        }
        else {
            XP_TRACE(("  Woops -- we're currently pointing to 0x%08x\n", *pElementHolder));
        }
    }
}

CEditElement* CEditLeafElement::Divide(int iOffset){
    CEditElement* pNext;
    if( iOffset == 0 ){
        return this;
    }

    pNext = LeafInContainerAfter();
    if( iOffset >= GetLen() ){
        if( pNext != 0 ){
            return pNext;
        }
        else {
            CEditTextElement* pPrev;
            CEditTextElement* pNew;
            if( IsA( P_TEXT )) {
                pPrev = this->Text();
            }
            else {
                pPrev = PreviousTextInContainer();
            }

            if( pPrev ){
                pNew = pPrev->CopyEmptyText();
            }
            else {
                pNew = new CEditTextElement((CEditElement*)0,0);
            }
            pNew->InsertAfter(this);
            return pNew;
        }
    }

    // offset is in the middle somewhere Must be a TEXT.
    XP_ASSERT( IsA(P_TEXT) );
    CEditTextElement* pNew = Text()->CopyEmptyText();
    pNew->SetText( Text()->GetText()+iOffset );
    Text()->GetText()[iOffset] = 0;
    
    /* update the text block with the new text */
    CEditTextElement * pText = Text();
	if ( pText->m_pTextBlock != NULL ){
		lo_ChangeText ( pText->m_pTextBlock, pText->GetTextWithConvertedSpaces() );
	}

    pNew->InsertAfter(this);
    return pNew;
}

CEditTextElement* CEditLeafElement::CopyEmptyText(CEditElement *pParent){
       return new CEditTextElement(pParent,0);
}

CPersistentEditInsertPoint CEditLeafElement::GetPersistentInsertPoint(ElementOffset offset){
    ElementIndex len = GetPersistentCount();
    if ( offset > len )
    {
#ifdef DEBUG_EDIT_LAYOUT
        XP_ASSERT(FALSE);
#endif
        offset = len;
    }
    return CPersistentEditInsertPoint(GetElementIndex() + offset, offset == 0);
}

XP_Bool CEditLeafElement::IsUnknownHTML(){
    return FALSE;
}

XP_Bool CEditLeafElement::IsComment(){
    return FALSE;
}

XP_Bool CEditLeafElement::IsComment(char* /*prefix*/){
    return FALSE;
}

//
// Default implementation
//
PA_Tag* CEditLeafElement::TagOpen( int /* iEditOffset */ ){
    PA_Tag *pTag = XP_NEW( PA_Tag );
    XP_BZERO( pTag, sizeof( PA_Tag ) );
    if( GetTagData() ){
        SetTagData( pTag, GetTagData() );
    }
    else {
        SetTagData( pTag, ">" );
    }
    return pTag;
}

ElementIndex CEditLeafElement::GetPersistentCount()
{
    return GetLen();
}

void CEditLeafElement::GetAll(CEditSelection& selection) {
    selection.m_start.m_pElement = this;
    selection.m_start.m_iPos = 0;
    selection.m_end.m_pElement = this;
    selection.m_end.m_iPos = Leaf()->GetLen();
    selection.m_bFromStart = FALSE;
}

XP_Bool CEditLeafElement::IsAcceptableChild(CEditElement& /*pChild*/ ) {return FALSE;}
XP_Bool CEditLeafElement::IsContainerContainer() { return FALSE; }

/* Optimized version for leaves. */

XP_Bool CEditLeafElement::IsFirstInContainer(){
    CEditContainerElement* pContainer = FindContainer();
    if ( ! pContainer ) {
        return TRUE;
    }
    CEditElement* pOther = pContainer->GetChild();
    return pOther == this;
}


#ifdef DEBUG
void CEditLeafElement::ValidateTree(){
    CEditElement::ValidateTree();
    CEditElement *pChild = GetChild();
    XP_ASSERT(!pChild); // Must have no children.
    // Parent must exist and be a container
    CEditElement *pParent = GetParent();
    XP_ASSERT(pParent); 
    XP_ASSERT(pParent->IsContainer()); 
}
#endif

XP_Bool CEditLeafElement::IsMisspelled() {
    return (IsText() && Text()->m_tf & TF_SPELL);
}


void CEditLeafElement::StreamToPositionalText( IStreamOut *pOut, XP_Bool bEnd ){ 
    if( !bEnd ){
        XP_ASSERT( GetLen() == 1 );
        pOut->Write(" ",1); 
    }
}

//-----------------------------------------------------------------------------
// CEditContainerElement
//-----------------------------------------------------------------------------
//
// Static constructor to create default element
//
CEditContainerElement* CEditContainerElement::NewDefaultContainer( CEditElement *pParent,
        ED_Alignment align  ){
    PA_Tag *pTag = XP_NEW( PA_Tag );
    XP_BZERO( pTag, sizeof( PA_Tag ) );
#ifdef EDT_DDT
    pTag->type = P_NSDT;
#else
    pTag->type = P_DESC_TITLE;
#endif
    // CSID doesn't matter because there's no data in the pTag, so there's no i18n issues.
    CEditContainerElement *pRet = new CEditContainerElement( pParent, pTag, 0, align );
    PA_FreeTag( pTag );        
    return pRet;
}

CEditContainerElement::CEditContainerElement(CEditElement *pParent, PA_Tag *pTag, int16 csid,
                ED_Alignment defaultAlign ): 
            CEditElement(pParent, pTag ? pTag->type : P_PARAGRAPH), 
            m_defaultAlign( defaultAlign ),
            m_align( defaultAlign ),
            m_hackPreformat(0),
            m_bHasEndTag(FALSE)
{ 

    if( pTag ){
        EDT_ContainerData *pData = ParseParams( pTag, csid );
        SetData( pData );
        FreeData( pData );
    }
}

CEditContainerElement::CEditContainerElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer): 
        CEditElement( pStreamIn, pBuffer )
{
    m_defaultAlign = (ED_Alignment) pStreamIn->ReadInt();
    m_align = (ED_Alignment) pStreamIn->ReadInt();
    m_bHasEndTag = FALSE;
}

void CEditContainerElement::StreamOut( IStreamOut *pOut ){
    CEditElement::StreamOut( pOut );
    pOut->WriteInt( m_defaultAlign );
    pOut->WriteInt( m_align );
}

void CEditContainerElement::StreamInChildren(IStreamIn* pIn, CEditBuffer* pBuffer){
    CEditElement::StreamInChildren(pIn, pBuffer);
    if ( GetChild() == NULL ){
        // I need a dummy child.
        // g++ gets issues a spurious warning about the value not being used.
        // Creating it automaticly inserts it
        (void) new CEditTextElement(this, 0);
    }
}

ElementIndex CEditContainerElement::GetPersistentCount(){
    // One extra element: the end of the last item in the container.
    return CEditElement::GetPersistentCount() + 1;
}



void CEditContainerElement::FinishedLoad( CEditBuffer *pBuffer )
{
    XP_Bool bFillNewCellWithSpace = FALSE;
    if ( GetType() != P_PREFORMAT )
    {
        // Don't allow more than one space before non-text elements
        CEditElement* pNext;
        CEditElement *pChild = GetChild();
        CEditElement* pParent = GetParent();
        
        // Detect the case of a single container with a single
        //  text element with just a space in a table cell
        // This is OK and is needed to show cell border in browser
        XP_Bool bIsTableCellWithOneSpace = FALSE;

        if( pParent && pParent->IsTableCell() && 
            GetNextSibling() == NULL &&
            pChild && pChild->IsText() &&
            (pChild->GetNextSibling() == NULL ||
             pChild->GetNextSibling()->GetType() == P_UNKNOWN) )
        {
            // We have a single container in a table cell with a single text child
            CEditTextElement* pTextChild = pChild->Text();
            char* pText = pTextChild->GetText();
            // If creating a new cell and preference is set,
            //  the empty cell should have a space,
            //  so delete this and a new one will be created below
            bFillNewCellWithSpace = GetEditBuffer() ? GetEditBuffer()->FillNewCellWithSpace() : FALSE;

            if( pTextChild->GetLen() == 0 && bFillNewCellWithSpace )
            {
                delete pChild;
            }
            else if( pTextChild->GetLen() == 1 && pText[0] == ' ')
            {
                // We have a single space, so don't do special trimming below
                bIsTableCellWithOneSpace = TRUE;
            }
        }
        if( ! bIsTableCellWithOneSpace )
        {
            for ( CEditElement* pChild = GetChild();
                pChild;
                pChild = pNext )
            {
                pNext = pChild->GetNextSibling();
                if ( pChild->IsText() && ! (pNext && pNext->IsText()) )
                {
                    CEditTextElement* pTextChild = pChild->Text();
                    char* pText = pTextChild->GetText();
                    int size = pTextChild->GetLen();
                    XP_Bool trimming = FALSE;
                    //do not allow 1 byte of space to be a child
                    if (size == 1 && GetType() == P_NSDT && pText[0] == ' ')
                    {
                        trimming = TRUE;
                        size--;
                    }

                    while ( size >0 && pText[size-1] == '\n' || 
                           (size > 1 && pText[size-1] == ' ' && pText[size-2] == ' ') ) 
                    {
                        // More than one character of white space at the end of
                        // a text element will get laid out as one character.
                        trimming = TRUE;
                        size--;
                    }
                    if ( trimming )
                    {
                        if (size <= 0 )
                        {
                            delete pChild;
                        }
                        else 
                        {
                            char* pCopy = XP_STRDUP(pText);
                            pCopy[size] = '\0';
                            pTextChild->SetText(pCopy);
                            XP_FREE(pCopy);
                        }
                    }
                }
            }
        }
    }

    if ( GetChild() == NULL )
    {
        // I need a dummy child -- might be a space for new cell
        CEditTextElement* pChild = new CEditTextElement(this, bFillNewCellWithSpace ? " " : 0);
        // Creating it automaticly inserts it
		pChild->FinishedLoad(pBuffer);
    }
    // 
    // if the last element in a paragraph is a <br>, it is ignored by the browser.  
    //  So we should trim it.
    //  we should do this only once!!!
    CEditElement *pChild;
    CEditElement* pPrev;
    if( (pChild = GetLastChild())!= 0 && pChild->IsBreak())
    {
        pPrev = pChild->GetPreviousSibling();
        if (! (pPrev && pPrev->IsBreak()) )
            delete pChild;
    }

    CEditElement::FinishedLoad(pBuffer);
}



XP_Bool CEditContainerElement::ForcesDoubleSpacedNextLine() { 
    return BitSet( edt_setContainerHasLineAfter, GetType() );
}

void CEditContainerElement::AdjustContainers( CEditBuffer *pBuffer ){

    if( GetType() == P_PARAGRAPH
        || (pBuffer->IsComposeWindow() 
                && GetType() == P_PREFORMAT
                && m_hackPreformat ) ){
        //
        // Convert to internal paragraph type
        //
        SetType( P_NSDT );

        CEditContainerElement *pPrev = (CEditContainerElement*) GetPreviousSibling();
        //
        // Just some paranoia
        //
        if ( pPrev && !pPrev->IsContainer() ) pPrev = 0;
        
        // 
        // If there is a previous container, append an empty single spaced 
        //  paragraph before this one.
        //
        if( pPrev && !pPrev->ForcesDoubleSpacedNextLine() ){
            CEditContainerElement *pNew;
            pNew = CEditContainerElement::NewDefaultContainer( 0, 
                        GetAlignment() );
            // g++ thinks the following value is not used, but it's mistaken.
            // The constructor auto-inserts the element into the tree.
            (void) new CEditTextElement(pNew, 0);
            pNew->InsertBefore( this );
        }
    }

    if( BitSet( edt_setContainerBreakConvert, GetType() )) {
        // Find any breaks in this paragraph and convert them into paragraphs
        CEditLeafElement *pChild = (CEditLeafElement*) GetChild();
        CEditLeafElement *pPrev = 0;

        while( pChild ){
            CEditLeafElement *pNext = pChild->LeafInContainerAfter();
            if(     pChild->IsBreak() 
#ifdef USE_SCRIPT
                    && !(pPrev && pPrev->IsText() && 
                        (pPrev->Text()->m_tf & (TF_SCRIPT|TF_STYLE|TF_SERVER)) != 0
                        )
                    && !(pNext && pNext->IsText() && 
                        (pNext->Text()->m_tf & (TF_SCRIPT|TF_STYLE|TF_SERVER)) != 0
                        )
#endif
                ){
                //                
                // If the first thing in the paragraph is a break,
                //  make the paragraph contain empty text
                //
                if( pChild->PreviousLeafInContainer() == 0 ){
                    CEditTextElement *pNewText = new CEditTextElement((CEditElement*)0, 0);
                    pNewText->InsertBefore( pChild );
                }

                // Break the chain of children So unlink doesn't try and
                //  merge the chain
                pChild->SetNextSibling(0);

                // Remove the break.
                pChild->Unlink();
                delete pChild;

                pChild = 0;

                // reparent the children.
                if( pNext ){
                    //
                    // Create a new container
                    //
                    CEditContainerElement *pNew;
                    pNew = CEditContainerElement::NewDefaultContainer( 0, 
                            GetAlignment() );
                    pNew->InsertAfter(this);

                    pNew->SetChild(pNext);
                    while( pNext ){
                        pNext->SetParent( pNew );
                        pNext = (CEditLeafElement*)pNext->GetNextSibling();
                    }
                }
            }
            pPrev = pChild;
            pChild = pNext;
        }
    }

    CEditElement::AdjustContainers(pBuffer);
}

PA_Tag* CEditContainerElement::TagOpen( int iEditOffset ){
    PA_Tag *pRet = 0;
    PA_Tag* pTag;

    // create the DIV tag if we need to.
    if( m_align == ED_ALIGN_ABSCENTER || m_align == ED_ALIGN_RIGHT ){
        pTag = XP_NEW( PA_Tag );
        XP_BZERO( pTag, sizeof( PA_Tag ) );
        if( m_align== ED_ALIGN_RIGHT ){
            SetTagData( pTag, "ALIGN=right>");
            pTag->type = P_DIVISION;
        }
        else {
            SetTagData( pTag, ">");
            pTag->type = P_CENTER;
        }
        pRet = pTag;
    }

    // create the actual paragraph tag
    if( GetTagData() ){
        pTag = XP_NEW( PA_Tag );
        XP_BZERO( pTag, sizeof( PA_Tag ) );
        SetTagData( pTag, GetTagData() );
    }
    else {
        pTag = CEditElement::TagOpen( iEditOffset );
    }

    // link the tags together.
    if( pRet == 0 ){
        pRet = pTag;
    }
    else {
        pRet->next = pTag;
    }
    return pRet;
}

PA_Tag* CEditContainerElement::TagEnd( ){
    PA_Tag *pRet = CEditElement::TagEnd();
    if( m_align == ED_ALIGN_ABSCENTER || m_align == ED_ALIGN_RIGHT ){
        PA_Tag* pTag = XP_NEW( PA_Tag );
        XP_BZERO( pTag, sizeof( PA_Tag ) );
        pTag->is_end = TRUE;
        if( m_align == ED_ALIGN_RIGHT ){
            pTag->type = P_DIVISION;
        }
        else {
            pTag->type = P_CENTER;
        }

        if( pRet == 0 ){
            pRet = pTag;
        }
        else {
            pRet->next = pTag;
        }
    }
    return pRet;
}

EEditElementType CEditContainerElement::GetElementType()
{
    return eContainerElement;
}

XP_Bool CEditContainerElement::IsPlainFirstContainer(){
    if ( (GetType() == P_PARAGRAPH || GetType() == P_NSDT)
        && ! HasExtraData()
        && IsFirstContainer() ) {
        ED_Alignment alignment = GetAlignment();
        ED_Alignment defaultAlignment = GetDefaultAlignment();
        if ( alignment == ED_ALIGN_DEFAULT
            || alignment == defaultAlignment
            || ( alignment == ED_ALIGN_LEFT
                && defaultAlignment == ED_ALIGN_DEFAULT )) {
            return TRUE;
        }
    }
    return FALSE;
}

XP_Bool CEditContainerElement::IsFirstContainer(){
    CEditSubDocElement* pSubDoc = GetSubDoc();
    return ! pSubDoc || pSubDoc->GetChild() == this;
}

XP_Bool CEditContainerElement::SupportsAlign(){
    return BitSet( edt_setContainerSupportsAlign, GetType() );
}

XP_Bool CEditContainerElement::IsImplicitBreak() {
    XP_ASSERT( GetType() == P_NSDT );
    CEditElement *pPrev;
    return ( (pPrev = GetPreviousSibling()) == 0 
                || !pPrev->IsContainer()
                || pPrev->Container()->ForcesDoubleSpacedNextLine()
//                || ((pPrev->Container()->GetAlignment() == ED_ALIGN_RIGHT) && (GetAlignment() != ED_ALIGN_RIGHT) )
//                || ((pPrev->Container()->GetAlignment() != ED_ALIGN_RIGHT) && (GetAlignment() == ED_ALIGN_RIGHT) )
                );

}

//helper function
//default == left!
PRIVATE XP_Bool CompareAlignments(ED_Alignment x, ED_Alignment y)
{
    if (x == y)
        return TRUE;
    if ( ((x == ED_ALIGN_ABSCENTER) || (x == ED_ALIGN_CENTER)) 
        && ((y == ED_ALIGN_ABSCENTER) || (y == ED_ALIGN_CENTER)) )
            return TRUE;
    if ( ((x == ED_ALIGN_DEFAULT) || (x == ED_ALIGN_LEFT)) 
        && ((y == ED_ALIGN_DEFAULT) || (y == ED_ALIGN_LEFT)) )
            return TRUE;
    if ( ((x == ED_ALIGN_ABSBOTTOM) || (x == ED_ALIGN_BOTTOM)) 
        && ((y == ED_ALIGN_ABSBOTTOM) || (y == ED_ALIGN_BOTTOM)) )
            return TRUE;
    if ( ((x == ED_ALIGN_ABSTOP) || (x == ED_ALIGN_TOP)) 
        && ((y == ED_ALIGN_ABSTOP) || (y == ED_ALIGN_TOP)) )
            return TRUE;
    return FALSE;
}

// 0..2, where 0 = not in pseudo paragraph.
// 1 = first container of pseudo paragraph.
// 2 = second container of pseudo paragraph.
// 3 = after list, <P> does NOT cause a blank line to be drawn above paragraph. we need <P><BR>
int16 CEditContainerElement::GetPseudoParagraphState(){
    if( GetType() != P_NSDT ){
        return 0;
    }
    CEditElement *pPrev = GetPreviousSibling();
    CEditElement *pPrevPrev;
    if (pPrev)
        pPrevPrev = pPrev->GetPreviousSibling();
    else 
        pPrevPrev=NULL;
    CEditElement *pNext = GetNextSibling();
    if ( !pPrev  || (!pPrev->IsContainer()) && !IsEmpty() ) {
        return 0;
    }
    if (!pPrev->IsContainer())
        return 1;
    CEditContainerElement* pPrevContainer = pPrev->Container();
    CEditContainerElement* pPrevPrevContainer=NULL;
    if ((pPrevPrev) &&(pPrevPrev->IsContainer()))
        pPrevPrevContainer = pPrevPrev->Container();

    if ( IsEmpty() && pPrevContainer->GetType() == P_NSDT && pPrevContainer->IsEmpty()
        && (!pPrevPrevContainer || pPrevPrevContainer->GetType() != P_NSDT) 
        && pNext && pNext->IsContainer() && pNext->Container()->GetType() == P_NSDT  && pNext->Container()->IsEmpty() ) 
        {
        return 5; //special for 3 spaces.  we must settle up the <BR> tags now.  <P> later wont cut it.
    }
   
    if ( IsEmpty() && pPrevContainer->GetType() == P_NSDT && pPrevContainer->IsEmpty()
        && (!pPrevPrevContainer || pPrevPrevContainer->GetType() != P_NSDT) ) 
        {
        return 4;
    }
    if ( !IsEmpty() && pPrevContainer->GetType() == P_NSDT && pPrevContainer->IsEmpty()
        && (!pPrevPrevContainer || pPrevPrevContainer->GetType() != P_NSDT) ) 
        {
        return 3;
    }
    if ( ! IsEmpty() && pPrevContainer->GetType() == P_NSDT
         && pPrevContainer->IsEmpty()  ) {
        return 2;
    }
    // Check for state 1
    if ( IsEmpty() && pNext && pNext->IsContainer() &&
                pNext->Container()->GetType() == P_NSDT
           && ! pNext->Container()->IsEmpty() ) {
        return 1;
    }
    // NOTE: there needs to be a BR tag when the previous non empty container is the same alignment as this one!
    //the helper function is mainly to make sure default and left are equivalent
    pPrevContainer= GetPreviousNonEmptyContainer(); //check nonempty!
    if ( !IsEmpty() && pPrevContainer && !CompareAlignments(pPrevContainer->GetAlignment(),GetAlignment()) )
        return 1; 
/*    if ( IsEmpty() && pNext && pNext->IsContainer() 
           && pNext->Container()->GetType() == P_NSDT
           && pNext->Container()->IsEmpty() 
           && pPrevContainer->IsEmpty()
           ) {
        return 4;
    }*/
    return 0;
}



//NOTES:
/*
It is not necessary to put a BR tag to break after <CENTER> or <DIV ALIGN=right>.
there IS an implicit break of sorts at that point
*/
void CEditContainerElement::PrintOpen( CPrintState *pPrintState ){
    if ( ShouldSkipTags() ) return;

    XP_Bool bHasExtraData = HasExtraData();

#ifdef EDT_DDT
    if( GetType() == P_NSDT ){
        int16 ppState = GetPseudoParagraphState();
        CEditContainerElement* pPrevContainer=GetPreviousNonEmptyContainer();
        if (( GetAlignment() == ED_ALIGN_RIGHT && ! IsEmpty()) && ( !pPrevContainer || (pPrevContainer->GetAlignment() != ED_ALIGN_RIGHT ) )){
            pPrintState->m_pOut->Printf( "\n");
            pPrintState->m_iCharPos = pPrintState->m_pOut->Printf( "<DIV ALIGN=right>"); 
            
        }
        else if (( GetAlignment() == ED_ALIGN_ABSCENTER && !IsEmpty()) && ( !pPrevContainer || (pPrevContainer->GetAlignment() != ED_ALIGN_ABSCENTER ) )){
            pPrintState->m_pOut->Printf( "\n");
            pPrintState->m_iCharPos = pPrintState->m_pOut->Printf( "<CENTER>"); 
        }
        if( !IsImplicitBreak() && ! bHasExtraData){
            switch( ppState ){
            default:
            case 0:
                pPrintState->m_pOut->Printf( "\n");
                pPrintState->m_iCharPos = pPrintState->m_pOut->Printf("<BR>"); 
                break;
            case 1:
                // Do nothing. This is an empty paragraph.
                break;
            case 2:
                pPrintState->m_pOut->Printf( "\n");
                pPrintState->m_iCharPos = pPrintState->m_pOut->Printf( "<P>"); 
                break;
            case 3:
                pPrintState->m_pOut->Printf( "\n"); //used for 1 space after listitem.
                pPrintState->m_iCharPos = pPrintState->m_pOut->Printf( "<P><BR>"); 
                break;
            case 4:
                pPrintState->m_pOut->Printf( "\n"); //used for 2 open spaces after listitems for example
                pPrintState->m_iCharPos = pPrintState->m_pOut->Printf( "<P>&nbsp;"); //only gives us one.  
                //if <P> is comming, we will get 2 line spacing.  if we wanted more, 
                //we should have settled up with a 5 on return from pseudo
                break;
            case 5:
                pPrintState->m_pOut->Printf( "\n"); //used for 3 open spaces after listitems for example
                pPrintState->m_pOut->Printf( "<P>&nbsp;"); //gives 2 spaces after list type item
                pPrintState->m_pOut->Printf( "\n"); 
                pPrintState->m_iCharPos = pPrintState->m_pOut->Printf( "<BR>&nbsp;");
                break;
            }
        }
        if ( bHasExtraData ) {
            EDT_ContainerData* pData = GetData();
            if ( pData && pData->pExtra ) {
                pPrintState->m_pOut->Printf( "\n");
                if (ppState == 2) pPrintState->m_pOut->Printf( "\n");
                char* pTagName = ppState == 2 ? "P" : "DIV";
                pPrintState->m_iCharPos = pPrintState->m_pOut->Printf( "<%s %s>", pTagName, pData->pExtra);
            }
        }
        return;
    }
#endif

    pPrintState->m_iCharPos = 0;
    pPrintState->m_pOut->Printf("\n"); 

    PA_Tag *pTag = TagOpen(0);
    while( pTag ){
        char *pData = 0;
        if( pTag->data ){
            PA_LOCK( pData, char*, pTag->data );
        }

        if( pData && *pData != '>' ) {
            pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "<%s %s", 
                    EDT_TagString(pTag->type), pData );
        }
        else {
            pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "<%s>", 
                    EDT_TagString(pTag->type));
        }

        if( pTag->data ){
            PA_UNLOCK( pTag->data );
        }

        PA_Tag* pDelTag = pTag;
        pTag = pTag->next;
        PA_FreeTag(pDelTag);

        if( GetType() != P_PREFORMAT ){
            pPrintState->m_pOut->Printf( "\n"); 
            pPrintState->m_iCharPos = 0;
        }

    }
}


XP_Bool CEditContainerElement::ShouldSkipTags(){
    XP_Bool parentIsTableCell = GetParent() && GetParent()->IsTableCell();
    return parentIsTableCell && IsPlainFirstContainer();
}

XP_Bool CEditContainerElement::HasExtraData(){
    // Does this have data other than the ALIGN tag?
    XP_Bool hasData = FALSE;
    EDT_ContainerData* pData = GetData();
    if ( pData ) {
        if ( pData->pExtra ) {
            hasData = TRUE;
        }
        FreeData(pData);
    }
    return hasData;
}

void CEditContainerElement::PrintEnd( CPrintState *pPrintState ){
    if ( ShouldSkipTags() ) return;

#ifdef EDT_DDT
    if( GetType() == P_NSDT ){
        int16 ppState = GetPseudoParagraphState();
        XP_Bool bNeedReturn = FALSE;
        CEditContainerElement* pNextContainer=GetNextNonEmptyContainer();
        if ( HasExtraData() ){
            if ( ppState != 2 ){
                pPrintState->m_pOut->Printf( "</DIV>");
                bNeedReturn = TRUE;
            }
        }
        // Keep empty paragraphs from being eaten
        if ( IsEmpty() && ppState == 0) {
            char* space = "&nbsp;";
            pPrintState->m_pOut->Printf( space );
            pPrintState->m_iCharPos += XP_STRLEN(space);
        }
        if( (GetAlignment() == ED_ALIGN_RIGHT && !IsEmpty() )&& (!pNextContainer || !CompareAlignments(pNextContainer->GetAlignment(),GetAlignment())) ) {
            pPrintState->m_pOut->Printf( "</DIV>"); 
            bNeedReturn = TRUE;
        }
        else if(( GetAlignment() == ED_ALIGN_ABSCENTER && !IsEmpty() ) && (!pNextContainer || !CompareAlignments(pNextContainer->GetAlignment(),GetAlignment())) ) {
            pPrintState->m_pOut->Printf( "</CENTER>"); 
            bNeedReturn = TRUE;
        }

        if ( bNeedReturn ) {
            pPrintState->m_iCharPos = 0;
            pPrintState->m_pOut->Printf( "\n");
        }
        return;
    }
#endif


    PA_Tag *pTag = TagEnd();
    while( pTag ){
        char *pData = 0;
        if( pTag->data ){
            PA_LOCK( pData, char*, pTag->data );
        }

        pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "</%s>",
            EDT_TagString( pTag->type ) );

        if( pTag->data ){
            PA_UNLOCK( pTag->data );
        }

        PA_Tag* pDelTag = pTag;
        pTag = pTag->next;
        PA_FreeTag(pDelTag);
    }

    pPrintState->m_iCharPos = 0;
    pPrintState->m_pOut->Printf( "\n"); 
}



CEditElement* CEditContainerElement::Clone( CEditElement* pParent ){
    PA_Tag *pTag = TagOpen(0);

    // LTNOTE: we get division tags at the beginning of paragraphs
    //  for their alignment. Ignore these tags, we get it directly from the
    //  container itself.
    //  Its a total hack.
    //
    if( pTag->type == P_DIVISION || pTag->type == P_CENTER ){
        PA_Tag *pDelTag = pTag;
        pTag = pTag->next;
        PA_FreeTag( pDelTag );
    }
    CEditElement *pRet = new CEditContainerElement(pParent, pTag, GetWinCSID(), m_align );
    PA_FreeTag( pTag );
    return pRet;
}

// Property Getting and Setting Stuff.

EDT_ContainerData* CEditContainerElement::NewData(){
    EDT_ContainerData *pData = XP_NEW( EDT_ContainerData );
    pData->align = ED_ALIGN_LEFT;
    pData->pExtra = 0;
    return pData;
}

void CEditContainerElement::FreeData( EDT_ContainerData *pData ){
    if ( pData ) {
        XP_FREEIF(pData->pExtra);
        XP_FREE( pData );
    }
}

void CEditContainerElement::SetData( EDT_ContainerData *pData ){
    char *pNew = 0;

    m_align = pData->align;
    if( m_align == ED_ALIGN_CENTER ) m_align = ED_ALIGN_ABSCENTER;

    //
    // Never generate ALIGN= stuff anymore.  Use the Div tag and Center Tags
    //
    char* pExtra = "";
    if ( pData && pData->pExtra ) {
        pExtra = pData->pExtra;
    }
    pNew = PR_sprintf_append( pNew, "%s>", pExtra );

    SetTagData( pNew );
    free(pNew);
}

EDT_ContainerData* CEditContainerElement::GetData( ){
    EDT_ContainerData *pRet;
    PA_Tag* pTag = TagOpen(0);
    pRet = ParseParams( pTag, GetWinCSID() );
    EDT_DeleteTagChain( pTag );
    return pRet;
}

static char* containerParams[] = {
    PARAM_ALIGN,
    "nscisaw",
    0
};

EDT_ContainerData* CEditContainerElement::ParseParams( PA_Tag *pTag, int16 csid ){
    EDT_ContainerData *pData = NewData();
    ED_Alignment align;

    m_hackPreformat = edt_FetchParamBoolExist(pTag, "nscisaw", csid);
    
    align = edt_FetchParamAlignment( pTag, m_defaultAlign, FALSE, csid );
    if( align == ED_ALIGN_CENTER ) align =  ED_ALIGN_ABSCENTER;

    if( align == ED_ALIGN_RIGHT || align == ED_ALIGN_LEFT  || align == ED_ALIGN_ABSCENTER){
        pData->align = align;
    }
    pData->pExtra = edt_FetchParamExtras( pTag, containerParams, csid );
    return pData;
}

void CEditContainerElement::SetAlignment( ED_Alignment eAlign ){
    EDT_ContainerData *pData = GetData();
    pData->align = eAlign;
    SetData( pData );
    FreeData( pData );
}

void CEditContainerElement::CopyAttributes(CEditContainerElement *pFrom ){
    SetType( pFrom->GetType() );
    SetAlignment( pFrom->GetAlignment() );
}

XP_Bool CEditContainerElement::IsEmpty(){
    CEditElement *pChild = GetChild();
    return ( pChild == 0 
                || ( pChild->IsA( P_TEXT )
                    && pChild->Text()->GetLen() == 0
                    && pChild->LeafInContainerAfter() == 0 )
            );
}

#ifdef DEBUG
void CEditContainerElement::ValidateTree(){
    CEditElement::ValidateTree();
    CEditElement* pChild = GetChild();
    XP_ASSERT(pChild); // Must have at least one child.
}
#endif

// 
// Sets the alignment if the container is empty (or almost empty).  Used only
//  during the parse phase to handle things like <p><center><img>
//
void CEditContainerElement::AlignIfEmpty( ED_Alignment eAlign ){
    CEditElement *pChild = GetChild();
    while( pChild ){
        if( pChild->IsA( P_TEXT ) 
                && pChild->Leaf()->GetLen() != 0 ){
            return;
        }
        pChild = pChild->GetNextSibling();
    }
    SetAlignment( eAlign );
}

void CEditContainerElement::StreamToPositionalText( IStreamOut *pOut, XP_Bool bEnd ){ 
    if( bEnd ){
        pOut->Write(" ",1); 
    }
}

char* CEditContainerElement::GetText()
{
    char *pText = NULL;
    CEditElement *pChild = GetChild();
    while( pChild )
    {
        if( pChild->IsText() && pChild->Leaf()->GetLen() != 0 )
        {
            pText = PR_sprintf_append( pText, pChild->Text()->GetText() );
        }
        pChild = pChild->GetNextSibling();
    }
    return pText;
}


//-----------------------------------------------------------------------------
// CEditListElement
//-----------------------------------------------------------------------------

CEditListElement::CEditListElement( 
        CEditElement *pParent, PA_Tag *pTag, int16 /*csid*/ ):
            CEditElement( pParent, pTag->type ){
   
    if( pTag ){
        char *locked_buff;
                
        PA_LOCK(locked_buff, char *, pTag->data );
        SetTagData( locked_buff );
        PA_UNLOCK(pTag->data);
    }
    m_pBaseURL = NULL;
}

CEditListElement::CEditListElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer): 
        CEditElement( pStreamIn, pBuffer )
{
    m_pBaseURL = NULL;  
}

CEditListElement::~CEditListElement()
{
  XP_FREEIF(m_pBaseURL);
}

PA_Tag* CEditListElement::TagOpen( int /* iEditOffset */ ){
    PA_Tag *pTag = XP_NEW( PA_Tag );
    XP_BZERO( pTag, sizeof( PA_Tag ) );
    SetTagData( pTag, GetTagData() ? GetTagData() : ">" );
    return pTag;
}

CEditElement* CEditListElement::Clone( CEditElement *pParent ){ 
    PA_Tag* pTag = TagOpen(0);
    CEditElement* pNew = new CEditListElement( pParent, pTag, GetWinCSID() );
    PA_FreeTag( pTag );
    return pNew;
}

XP_Bool CEditListElement::IsCompatableList(CEditElement *pElement)
{
    if( pElement && pElement->IsList() )
    {
        TagType t = pElement->GetType();
        if( t == m_tagType ||
            ((t == P_UNUM_LIST && m_tagType == P_NUM_LIST) ||
             (m_tagType == P_UNUM_LIST && t == P_NUM_LIST)) )
            return TRUE;
    }
    return FALSE;
}

// Property Getting and Setting Stuff.

EDT_ListData* CEditListElement::NewData(){
    EDT_ListData *pData = XP_NEW( EDT_ListData );
    if( pData == 0 ){
        // throw();
        return pData;
    }
    pData->iTagType = P_UNUM_LIST;
    pData->bCompact = FALSE;
    pData->eType = ED_LIST_TYPE_DEFAULT;
    pData->iStart = 1;
    pData->pExtra = 0;
    pData->pBaseURL = NULL;
    return pData;
}

void CEditListElement::FreeData( EDT_ListData *pData ){
    if( pData->pExtra ) XP_FREE( pData->pExtra );
    XP_FREEIF(pData->pBaseURL);
    XP_FREE( pData );
}

void CEditListElement::SetData( EDT_ListData *pData ){
    char *pNew = 0;

    // Copy in pBaseURL.
    XP_FREEIF(m_pBaseURL);
    if (pData->pBaseURL && *pData->pBaseURL) {
      m_pBaseURL = XP_STRDUP(pData->pBaseURL);
    }    

    SetType( pData->iTagType );
    
    if( pData->bCompact ){
        pNew = PR_sprintf_append( pNew, " COMPACT");
    }
    if( pData->iStart != 1 ){
        pNew = PR_sprintf_append( pNew, " START=%d", pData->iStart );
    }
    switch( pData->eType ){
        case ED_LIST_TYPE_DEFAULT:
            break;
        case ED_LIST_TYPE_DIGIT:
            SetType( P_NUM_LIST );
            break;
        case ED_LIST_TYPE_BIG_ROMAN:
            SetType( P_NUM_LIST );
            pNew = PR_sprintf_append( pNew, " TYPE=I" );
            break;
        case ED_LIST_TYPE_SMALL_ROMAN:
            SetType( P_NUM_LIST );
            pNew = PR_sprintf_append( pNew, " TYPE=i" );
            break;
        case ED_LIST_TYPE_BIG_LETTERS:
            SetType( P_NUM_LIST );
            pNew = PR_sprintf_append( pNew, " TYPE=A" );
            break;
        case ED_LIST_TYPE_SMALL_LETTERS:
            SetType( P_NUM_LIST );
            pNew = PR_sprintf_append( pNew, " TYPE=a" );
            break;
        case ED_LIST_TYPE_CIRCLE:
            SetType( P_UNUM_LIST );
            pNew = PR_sprintf_append( pNew, " TYPE=CIRCLE" );
            break;
        case ED_LIST_TYPE_SQUARE:
            SetType( P_UNUM_LIST );
            pNew = PR_sprintf_append( pNew, " TYPE=SQUARE" );
            break;
        case ED_LIST_TYPE_DISC:
            SetType( P_UNUM_LIST );
            pNew = PR_sprintf_append( pNew, " TYPE=DISC" );
            break;
        case ED_LIST_TYPE_CITE:
            SetType( P_BLOCKQUOTE );
            pNew = PR_sprintf_append( pNew, " TYPE=CITE" );
            break;
    }

    if( pData->pExtra  ){
        pNew = PR_sprintf_append( pNew, " %s", pData->pExtra );
    }

    if( pNew ){
        pNew = PR_sprintf_append( pNew, ">" );
    }

    SetTagData( pNew );

    XP_FREEIF(pNew);
}

EDT_ListData* CEditListElement::GetData( ){
    EDT_ListData *pRet;
    PA_Tag* pTag = TagOpen(0);
    pRet = ParseParams( pTag, GetWinCSID() );
    PA_FreeTag( pTag );

    if (pRet && m_pBaseURL) {
      pRet->pBaseURL = XP_STRDUP(m_pBaseURL);
    }
    return pRet;
}

static char* listParams[] = {
    PARAM_TYPE,
    PARAM_COMPACT,
    PARAM_START,
    0
};

EDT_ListData* CEditListElement::ParseParams( PA_Tag *pTag, int16 csid ){
    EDT_ListData *pData = NewData();
    char *pType;

    pData->iTagType = pTag->type;
    pData->bCompact = edt_FetchParamBoolExist( pTag, PARAM_COMPACT, csid );

    pType = edt_FetchParamString( pTag, PARAM_TYPE, csid );
    if( pType == 0 ){
        if( pTag->type == P_NUM_LIST ){
            pData->eType = ED_LIST_TYPE_DIGIT;
        }
        else {
            pData->eType = ED_LIST_TYPE_DEFAULT;
        }
    }
    else {
        switch (*pType){
        case 'A':
            pData->eType = ED_LIST_TYPE_BIG_LETTERS;
            break;
        case 'a':
            pData->eType = ED_LIST_TYPE_SMALL_LETTERS;
            break;
        case 'i':
            pData->eType = ED_LIST_TYPE_SMALL_ROMAN;
            break;
        case 'I':
            pData->eType = ED_LIST_TYPE_BIG_ROMAN;
            break;
        case 'c':
        case 'C':
            if ( XP_STRNCASECMP(pType, "cite", 4) == 0) {
                pData->eType = ED_LIST_TYPE_CITE;
            }
            else {
                pData->eType = ED_LIST_TYPE_CIRCLE;
            }
            break;
        case 'd':
        case 'D':
            pData->eType = ED_LIST_TYPE_DISC;
            break;
        case 's':
        case 'S':
            pData->eType = ED_LIST_TYPE_SQUARE;
            break;
        default:
            // To do: Figure out how to preserve unknown list types.
            pData->eType = ED_LIST_TYPE_DEFAULT;
            break;
        }
    }

    pData->iStart = edt_FetchParamInt( pTag, PARAM_START, 1, csid );
    pData->pExtra = edt_FetchParamExtras( pTag, listParams, csid );
    return pData;
}

void CEditListElement::CopyData(CEditListElement* pOther){
    EDT_ListData* pData = pOther->GetData();
    SetData(pData);
    FreeData(pData);
}

XP_Bool CEditListElement::IsMailQuote(){
    XP_Bool bResult = FALSE;
    EDT_ListData* pData = GetData();
    if ( pData->iTagType == P_BLOCKQUOTE &&
        pData->eType == ED_LIST_TYPE_CITE ) {
        bResult = TRUE;
    }
    FreeData(pData);
    return bResult;
}

#ifdef DEBUG
void CEditListElement::ValidateTree(){
    CEditElement::ValidateTree();
    // Must have at least one child.
    XP_ASSERT(GetChild());
    // We rely on the children to check that
    // they can have a list as a parent.
}
#endif

EEditElementType CEditListElement::GetElementType(){
    return eListElement;
}

//-----------------------------------------------------------------------------
// CEditTextElement
//-----------------------------------------------------------------------------

CEditTextElement::CEditTextElement(CEditElement *pParent, char *pText):
        CEditLeafElement(pParent, P_TEXT), 
        m_pFirstLayoutElement(0),
        m_pTextBlock(0),
        m_tf(TF_NONE),
        m_iFontSize(DEF_FONTSIZE),
        m_href(ED_LINK_ID_NONE),
        m_color(ED_Color::GetUndefined()),
        m_pFace(NULL),
        m_iWeight(ED_FONT_WEIGHT_NORMAL),
        m_iPointSize(ED_FONT_POINT_SIZE_DEFAULT),
        m_pScriptExtra(NULL)

{
    if( pText && *pText ){
        m_pText = XP_STRDUP(pText);             // should be new??
        m_textSize = XP_STRLEN(m_pText)+1;
    }
    else {
        m_pText = 0;
        m_textSize = 0;
    }
}

CEditTextElement::CEditTextElement( IStreamIn *pIn, CEditBuffer* pBuffer ):
        CEditLeafElement(pIn, pBuffer), 
        m_pFirstLayoutElement(0),
        m_pTextBlock(0),
        m_tf(TF_NONE),
        m_iFontSize(DEF_FONTSIZE),
        m_href(ED_LINK_ID_NONE),
        m_pText(0),
        m_color(ED_Color::GetUndefined()),
        m_pFace(NULL),
        m_pScriptExtra(NULL)
{
    m_tf = (ED_TextFormat)pIn->ReadInt( );
    
    m_iFontSize = pIn->ReadInt( );
    
    m_color = pIn->ReadInt( );
    
    if( m_tf & TF_HREF ){
        char* pHref = pIn->ReadZString();
        char* pExtra = pIn->ReadZString();
        SetHREF( pBuffer->linkManager.Add( pHref, pExtra ));
        XP_FREE( pHref );
        XP_FREE( pExtra );
    }

    if ( m_tf & TF_FONT_FACE ) {
        m_pFace = pIn->ReadZString();
    }

    if ( m_tf & TF_FONT_WEIGHT ) {
        m_iWeight = (int16) pIn->ReadInt();
    }

    if ( m_tf & TF_FONT_POINT_SIZE ) {
        m_iPointSize = (int16) pIn->ReadInt();
    }

    if ( EDT_IS_SCRIPT(m_tf) ) {
        m_pScriptExtra = pIn->ReadZString();
    }

    m_pText = pIn->ReadZString();
    if( m_pText ){
        m_textSize = XP_STRLEN( m_pText );
    }
    else {
        m_textSize = 0;
    }
}

CEditTextElement::~CEditTextElement(){
    DisconnectLayoutElements((LO_Element*) m_pFirstLayoutElement);
    if ( m_pText) XP_FREE(m_pText); 
    if( m_href != ED_LINK_ID_NONE ) m_href->Release();
    if ( m_pFace ) {
        XP_FREE(m_pFace);
        m_pFace = NULL;
    }
    if ( m_pScriptExtra ) {
        XP_FREE(m_pScriptExtra);
        m_pScriptExtra = NULL;
    }
}

XP_Bool CEditTextElement::GetLOElementAndOffset( ElementOffset iEditOffset, XP_Bool bEditStickyAfter,
            LO_Element*& pRetText,
            int& pLayoutOffset ){
    return GetLOTextAndOffset( iEditOffset, bEditStickyAfter, *(LO_TextStruct**)&pRetText, pLayoutOffset);
}


void CEditTextElement::StreamOut( IStreamOut *pOut ){
    CEditSelection all;
    GetAll(all);
    PartialStreamOut(pOut, all );
}

void CEditTextElement::PartialStreamOut( IStreamOut *pOut, CEditSelection& selection ){
    CEditSelection local;
    if ( ClipToMe(selection, local) ) {

        // Fake a stream out.
        CEditLeafElement::StreamOut( pOut  );
    
        pOut->WriteInt( m_tf );
    
        pOut->WriteInt( m_iFontSize );
    
        pOut->WriteInt( m_color.GetAsLong() );
    
        if( m_tf & TF_HREF ){
            pOut->WriteZString( m_href->hrefStr );
            pOut->WriteZString( m_href->pExtra );
        }

        if( m_tf & TF_FONT_FACE ){
            pOut->WriteZString( m_pFace );
        }
        if( m_tf & TF_FONT_WEIGHT ){
            pOut->WriteInt(m_iWeight);
        }
        if( m_tf & TF_FONT_POINT_SIZE ){
           pOut->WriteInt(m_iPointSize);
        }

        if( EDT_IS_SCRIPT(m_tf) ){
            pOut->WriteZString( m_pScriptExtra );
        }

        pOut->WritePartialZString( m_pText, local.m_start.m_iPos, local.m_end.m_iPos);

        // No children
        pOut->WriteInt((int32)eElementNone);
    }
}


EEditElementType CEditTextElement::GetElementType()
{
    return eTextElement;
}

void CEditTextElement::SetLayoutElement( intn iEditOffset, intn lo_type, 
                        LO_Element* pLoElement ){
	
	/* this element might be our text block. if so, we want to keep track of it */
	if ( lo_type == LO_TEXTBLOCK ){
#if defined( DEBUG_shannon )
		XP_TRACE(("Setting text block to %lx", pLoElement));
#endif
		XP_ASSERT ( pLoElement != NULL );
		m_pTextBlock = &pLoElement->lo_textBlock;
        pLoElement->lo_any.edit_element = this;
        pLoElement->lo_any.edit_offset = iEditOffset;
	}
	
    SetLayoutElementHelper(LO_TEXT, (LO_Element**) &m_pFirstLayoutElement,
        iEditOffset, lo_type, pLoElement);
}

LO_Element* CEditTextElement::GetLayoutElement(){ 
    return m_pFirstLayoutElement; 
}

void CEditTextElement::ResetLayoutElement( intn iEditOffset, 
            LO_Element* pLoElement ){
    ResetLayoutElementHelper((LO_Element**) &m_pFirstLayoutElement,
        iEditOffset, pLoElement);
}

void edt_RemoveNBSP( int16 csid, char *pString ){
    while( pString && *pString ){
        if( *pString == NON_BREAKING_SPACE ){
            *pString = ' ';
        }
        pString = INTL_NextChar(csid, pString);
    }
}

void CEditTextElement::SetText( char *pText, XP_Bool bConvertSpaces, int16 csid){
    if( m_pText ){
        XP_FREE( m_pText );
    }
    if( pText && *pText ){
        m_pText = XP_STRDUP(pText);             // should be new??
        m_textSize = XP_STRLEN(m_pText)+1;
        if( bConvertSpaces ){
            edt_RemoveNBSP( csid, m_pText );
        }
    }
    else {
        m_pText = 0;
        m_textSize = 0;
    }
}


void CEditTextElement::SetColor( ED_Color iColor ){ 
    m_color = iColor; 
    // Dont set color if we are a script
    if(!iColor.IsDefined() || EDT_IS_SCRIPT(m_tf) ){
        m_tf  &= ~TF_FONT_COLOR;
    }
    else {
        m_tf |= TF_FONT_COLOR;
    }
}

// We seem to have problems when point size gets REALLY big,
//   so limit this for now
#define ED_MAX_POINT_SIZE 104

// Boost the size changes when increasing or decreasing point sizes
void static AdjustPointSizeChange( int& iChange, int iPointSize )
{
    if( iChange == 1 )
    {
        if( iPointSize >= 10 && iPointSize <= 24 )
            iChange = 2;
        else if( iPointSize > 24 )
            iChange = 4;
    }
    else if( iChange == -1 )
    {
        if( iPointSize >= 10 && iPointSize <= 24 )
            iChange = -2;
        else if( iPointSize > 24 )
            iChange = -4;
    }
}

void CEditTextElement::SetFontSize( int iSize, XP_Bool bRelative )
{
    //Override (use default for) ALL font size requests for Java Script
    if( EDT_IS_SCRIPT(m_tf) ){
        iSize = 0;
    } 
    else
    {
        // Calculate the new size and keep within allowable range
        // bRelative = TRUE means that iSize is a change relative
        //   to current size
        if( bRelative )
        {
            if( m_iPointSize != ED_FONT_POINT_SIZE_DEFAULT )
            {
                // We are already using pts, so change that instead of m_iFontSize
                AdjustPointSizeChange( iSize, m_iPointSize );
                SetFontPointSize(min(ED_MAX_POINT_SIZE, max(MIN_POINT_SIZE, m_iPointSize+iSize)));
                return;
            }
#ifdef XP_WIN
            // EXPERIMENTAL:
            // If we are trying to increase past the highest HTML size (7),
            //  we change over to POINT size instead so we can
            //  continue to increase the size.
            // To do this, we need to get the point size of the "7" font,
            if( (m_iFontSize + iSize) > MAX_FONT_SIZE )
            {
                int16 iPointSize = FE_CalcFontPointSize(GetEditBuffer()->m_pContext, MAX_FONT_SIZE, m_tf & TF_FIXED);
                if( iPointSize )
                {
                    AdjustPointSizeChange( iSize, iPointSize );
                    SetFontPointSize(min(ED_MAX_POINT_SIZE, iPointSize+iSize));
                    return;
                }
            }
#endif //XP_WIN
            // We are using relative scale,
            //  set new size relative to current
            iSize = m_iFontSize + iSize;
        }
    }
    iSize = min(MAX_FONT_SIZE, max(MIN_FONT_SIZE,(iSize)));

    // No change from current state
    if( m_iFontSize == iSize )
        return;

    m_iFontSize = iSize; 

    
    // Use 0 (or less) to signify changing to default
    if( iSize <= 0 ){
        m_iFontSize = GetDefaultFontSize();
    }
    if( m_iFontSize == GetDefaultFontSize() ){
        m_tf  &= ~TF_FONT_SIZE;
    }
    else {
        m_tf |= TF_FONT_SIZE;
    }
    // If using relative font size, then absolute point size must be default
    // Note that we do this even if setting to default size,
    //  so this overrides any existing point size
    m_tf &= ~TF_FONT_POINT_SIZE;
    m_iPointSize = ED_FONT_POINT_SIZE_DEFAULT;
}
 
void CEditTextElement::SetFontFace( char* pFace ){
    if ( m_pFace ) {
        XP_FREE(m_pFace);
        m_pFace = NULL;
        m_tf &= ~TF_FONT_FACE;
    }
    if ( pFace ) {
        m_tf |= TF_FONT_FACE;
        // Skip over initial separator signal
        if( *pFace == '_' ){
            pFace++;
        }
        m_pFace = XP_STRDUP(pFace);
    }
}

void CEditTextElement::SetFontWeight( int16 weight ){
    if ( m_iWeight != ED_FONT_WEIGHT_NORMAL ) {
        m_tf &= ~TF_FONT_WEIGHT;
        m_iWeight = ED_FONT_WEIGHT_NORMAL;
    }
    if ( weight != ED_FONT_WEIGHT_NORMAL ) {
        m_tf |= TF_FONT_WEIGHT;
        m_iWeight = weight;
    }
}

void CEditTextElement::SetFontPointSize( int16 pointSize ){
    if ( m_iPointSize != ED_FONT_POINT_SIZE_DEFAULT ) {
        m_tf &= ~TF_FONT_POINT_SIZE;
        m_iPointSize = ED_FONT_POINT_SIZE_DEFAULT;
    }
    if ( pointSize != ED_FONT_POINT_SIZE_DEFAULT ) {
        m_tf |= TF_FONT_POINT_SIZE;
        m_iPointSize = pointSize;
        // If using absolute point size, then relative font size must be default
        m_iFontSize = GetDefaultFontSize();
        m_tf  &= ~TF_FONT_SIZE;
    }
}

void CEditTextElement::SetScriptExtra( char* pScriptExtra ){
    if ( m_pScriptExtra ) {
        XP_FREE(m_pScriptExtra);
        m_pScriptExtra = NULL;
    }
    if ( pScriptExtra ) {
        m_pScriptExtra = XP_STRDUP(pScriptExtra);
    }
}

void CEditTextElement::SetHREF( ED_LinkId id ){
    if( m_href != ED_LINK_ID_NONE ){
        m_href->Release();
    }
    m_href = id;
    // Dont set HREF if we are a script
    if( id != ED_LINK_ID_NONE && !EDT_IS_SCRIPT(m_tf) ){
        id->AddRef();
        m_tf |= TF_HREF;
    }
    else {
        m_tf &= ~TF_HREF;
    }
}

int CEditTextElement::GetFontSize(){ 
    if( m_tf & TF_FONT_SIZE ){
        return m_iFontSize;
    }
    else {
        return GetDefaultFontSize();
    }
}

char* CEditTextElement::GetFontFace(){ 
    return m_pFace;
}

int16 CEditTextElement::GetFontWeight(){ 
    return m_iWeight;
}

int16 CEditTextElement::GetFontPointSize(){ 
    return m_iPointSize;
}

char* CEditTextElement::GetScriptExtra(){ 
    return m_pScriptExtra;
}

// 
// The the current text buffer but convert spaces to non breaking spaces where
//  necessary so the file can be laid out properly. 
//
//  The rules for converting spaces into Non breaking spaces are as follows:
//
//  X = character
//  N = Non breaking space
//  S = Space
//  ^ = End of line
//
//                      1           2           N
//                  ---------------------------------------
//  Line Start          N           NS          NNNS
//  Middle              XS          XNS         XNNNS
//  End                 N^          XNN^        XNNN^
//
//
char* CEditTextElement::GetTextWithConvertedSpaces(){

    // If regular get text doesn't do anything, neither should we.
    if( GetText() == 0 ){
        return 0;
    }

    // grab a working buffer to copy the data into
    char *pBuf = edt_WorkBuf( GetLen()+1 );

    // Out of memory
    if( pBuf == 0 ) return 0;

    CEditElement *pPrev = PreviousLeafInContainer();
    CEditElement *pNext = LeafInContainerAfter();

    XP_Bool bNextIsSpace, bNextIsEOL, bIsSpace, bAtBeginOfLine;
    

    // if we are the beginning of a line
    bAtBeginOfLine = ( pPrev == 0 || pPrev->CausesBreakAfter() );

    char *pDest = pBuf;
    const char *pSrc = GetText();

    while( *pSrc ) {

        bIsSpace = (*pSrc == ' ');

        if( pSrc[1] != 0 ){
            bNextIsEOL = FALSE;
            bNextIsSpace = (pSrc[1] == ' ');
        }
        else if( pNext
                && pNext->IsText()
                && pNext->Text()->GetText()) {
            bNextIsEOL = FALSE;
            bNextIsSpace = ( pNext->Text()->GetText()[0] == ' ' );
        }
        else {
            bNextIsEOL = TRUE;
            bNextIsSpace = FALSE;
        }

        if( bAtBeginOfLine ){
            if( bIsSpace ){
                *pDest = (char) NON_BREAKING_SPACE;
            }
            else {
                *pDest = *pSrc;
            }
            bAtBeginOfLine = FALSE;
        }
        else {
            if( bIsSpace && ( bNextIsSpace || bNextIsEOL )){
                *pDest = (char) NON_BREAKING_SPACE;
            }
            else {
                *pDest = *pSrc;
            }
        }

        pSrc++;
        pDest++;
    }
    *pDest = 0;
    return pBuf;        
}

void CEditTextElement::ClearFormatting(){
    if ( m_tf & TF_HREF) {
        if ( m_href != ED_LINK_ID_NONE ) {
            m_href->Release();
        }
        else {
            XP_ASSERT(FALSE);
        }
    }
    m_href = ED_LINK_ID_NONE;
    m_tf = 0;
    SetFontSize( GetDefaultFontSize() );
    m_color = ED_Color::GetUndefined();
    m_iWeight = ED_FONT_WEIGHT_NORMAL;
    m_iPointSize = ED_FONT_POINT_SIZE_DEFAULT;

    /* We don't preserve the script extra because the SCRIPT attribute is cleared.
     */
    SetScriptExtra(NULL);
}

void CEditTextElement::SetData( EDT_CharacterData *pData ){

    // Either Java Script type
    ED_TextFormat tfJava = TF_SERVER | TF_SCRIPT | TF_STYLE;

    // if we are setting server or script,
    //   then it is the only thing we need to do
    if( (pData->mask & pData->values ) & tfJava ){
        ClearFormatting();
#ifdef USE_SCRIPT
        m_tf = (pData->mask & pData->values ) & tfJava;
        return;
#else
        XP_ASSERT(FALSE); // Front ends should not be setting these styles any more.
        return;
#endif
    }
#ifdef USE_SCRIPT
    // If we are already a Java Script type,
    //   and we are NOT removing that type (mask bit is not set)
    //   then ignore all other attributes
    if( (0 != (m_tf & TF_SERVER) && 
         0 == (pData->mask & TF_SERVER)) ||
        (0 != (m_tf & TF_STYLE) && 
         0 == (pData->mask & TF_STYLE)) ||
        (0 != (m_tf & TF_SCRIPT) && 
         0 == (pData->mask & TF_SCRIPT)) ){
        return;
    }
#endif

    if( pData->mask & TF_HREF ){
        SetHREF( pData->linkId );
    }
    if( pData->mask & TF_FONT_COLOR ){
        if( (pData->values & TF_FONT_COLOR) ){
            SetColor( EDT_LO_COLOR(pData->pColor) );
        }
        else {
            SetColor( ED_Color::GetUndefined() );
        }
    }
    if( pData->mask & TF_FONT_SIZE ){
        if( pData->values & TF_FONT_SIZE ){
            SetFontSize( pData->iSize );
        }
        else {
            SetFontSize( GetDefaultFontSize() );
            XP_ASSERT( (m_tf & TF_FONT_SIZE) == 0 );
        }
    }

    if( pData->mask & TF_FONT_FACE ){
        if( (pData->values & TF_FONT_FACE) ){
            SetFontFace( pData->pFontFace );
        }
        else {
            SetFontFace( 0 );
        }
    }

    if( pData->mask & TF_FONT_WEIGHT ){
        if( (pData->values & TF_FONT_WEIGHT) ){
            SetFontWeight( pData->iWeight );
        }
        else {
            SetFontWeight( ED_FONT_WEIGHT_NORMAL );
        }
    }

    if( pData->mask & TF_FONT_POINT_SIZE ){
        if( (pData->values & TF_FONT_POINT_SIZE) ){
            SetFontPointSize( pData->iPointSize );
        }
        else {
            SetFontPointSize( ED_FONT_POINT_SIZE_DEFAULT );
        }
    }

    // remove the elements we've already handled.
    ED_TextFormat mask = pData->mask & ~( TF_HREF 
                                        | TF_FONT_SIZE
                                        | TF_FONT_COLOR
                                        | TF_FONT_FACE
                                        | TF_FONT_WEIGHT
                                        | TF_FONT_POINT_SIZE
#ifdef USE_SCRIPT
                                        | TF_SCRIPT
                                        | TF_SERVER
                                        | TF_STYLE );
#else
                                        );
#endif

    ED_TextFormat values = pData->values & mask;

    m_tf = (m_tf & ~mask) | values;
}

EDT_CharacterData* CEditTextElement::GetData(){
    EDT_CharacterData *pData = EDT_NewCharacterData();
    pData->mask = -1;
    pData->values = m_tf;
    pData->iSize = GetFontSize();
    if( m_tf & TF_FONT_COLOR ){
        pData->pColor = edt_MakeLoColor( GetColor() );
    }
    if( m_tf & TF_HREF ){
        if( m_href != ED_LINK_ID_NONE ){
            pData->pHREFData = m_href->GetData();
        }
        pData->linkId = m_href;
    }
    if( m_tf & TF_FONT_FACE ){
        XP_ASSERT( m_pFace );
        pData->pFontFace = XP_STRDUP( m_pFace );
    }
    if ( m_tf & TF_FONT_WEIGHT ){
        pData->iWeight = m_iWeight;
    }
    if ( m_tf & TF_FONT_POINT_SIZE ){
        pData->iPointSize = m_iPointSize;
    }
    return pData;
}

void CEditTextElement::MaskData( EDT_CharacterData*& pData ){
    if( pData == 0 ){
        pData = GetData();
        return;
    }
    ED_TextFormat differences = pData->values ^ m_tf;
    if( differences ){
        // We are certain about all bits that are NOT different
        pData->mask &= ~differences;
    }
    // If mask bit is still set, then we THINK the attribute
    //  is the same, but we don't know until we check the
    //  color, href size, or font face directly 
    if( (pData->mask & pData->values & m_tf & TF_FONT_COLOR) 
                && EDT_LO_COLOR( pData->pColor ) != GetColor() ){
        pData->mask &= ~TF_FONT_COLOR;
        XP_FREE( pData->pColor );
        pData->pColor = 0;
    }
    if( (pData->mask & pData->values & m_tf & TF_HREF)
                && (pData->linkId != m_href ) ){
        pData->mask &= ~TF_HREF;
    }
    if( (pData->mask & pData->values & m_tf & TF_FONT_SIZE)
                && (pData->iSize != m_iFontSize) ){
        pData->mask &= ~TF_FONT_SIZE;
    }
    if( (pData->mask & pData->values & m_tf & TF_FONT_POINT_SIZE)
                && (pData->iPointSize != m_iPointSize) ){
        pData->mask &= ~TF_FONT_POINT_SIZE;
    }
    if( (pData->mask & pData->values & m_tf & TF_FONT_FACE) ){
        // Both have font face -- see if it is different
        char * pFontFace = GetFontFace();
        
        // Note: String shouldn't be empty if bit is set!
        XP_ASSERT(pData->pFontFace);
        if( !pData->pFontFace && !pFontFace ){
            // Both are empty so they are the same
            return;
        }
        if( pData->pFontFace
                && (!pFontFace || XP_STRCMP(pData->pFontFace, pFontFace )) ){
            // Font name is different
            pData->mask &= ~TF_FONT_FACE;
        }
    }
}


//
// insert a character into the text buffer. At some point we should make sure we
//  don't insert two spaces together.
//
XP_Bool CEditTextElement::InsertChar( int iOffset, int newChar ){
    char buffer[2];
    buffer[0] = newChar;
    buffer[1] = '\0';
    return InsertChars(iOffset, buffer);
}

int32 CEditTextElement::InsertChars( int iOffset, char* pNewChars ){
    // This is in bytes, not in characters.
    // However, it assumes we've gotten whole characters.
    int32 iNumBytes = pNewChars ? XP_STRLEN(pNewChars) : 0;
    if ( iNumBytes <= 0 ) {
        return 0;
    }
    // guarantee the buffer size
    //
    int32 iOldLen = GetLen();
    int32 iSize = iOldLen  + iNumBytes;
    if( iSize + 1 >= m_textSize ){
        int newSize = GROW_TEXT( iSize + 1 );
        char *pNewBuf = (char*)XP_ALLOC( newSize );
        if( pNewBuf == 0 ){
            // out of memory, should probably throw.
            return 0; 
        }
        if( m_pText ){
            XP_STRCPY( pNewBuf, m_pText );
            XP_FREE( m_pText );
        }
        else {
            pNewBuf[0] = 0;
        }
        m_pText = pNewBuf;
        m_textSize = newSize;
    }

    //
    // create an empty space in the string.  We have to start from the end of 
    //  the string and move backward. Move the null, too
    //
    int32 i;
    for(i = iOldLen; i >= iOffset; i--){
        m_pText[i+iNumBytes] = m_pText[i];
    }
    // Insert the new text into the string.
    for(i = 0; i < iNumBytes; i++){
        m_pText[iOffset+i] = pNewChars[i];
    }
    
    /* update the text block with the new text */
	if ( m_pTextBlock != NULL ){
		lo_ChangeText ( m_pTextBlock, GetTextWithConvertedSpaces() );
	}
	
    return iNumBytes;
}

void CEditTextElement::DeleteChar( MWContext *pContext, int iOffset ){
    
    INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(pContext);
    int i = iOffset;
    char c = 1;
    int clen;

    clen = INTL_CharLen(INTL_GetCSIWinCSID(csi), (unsigned char *)m_pText+iOffset);

    if( m_pText[iOffset] ){
        while( c != 0 ){
            m_pText[i] = c = m_pText[i+clen];
            i++;
        }
    }
}

char* CEditTextElement::DebugFormat(){
    static char buf[1024];
    strcpy(buf, "( " );
    int i;
    if(m_tf & TF_BOLD)         strcat(buf,"B ");
    if(m_tf & TF_ITALIC)       strcat(buf,"I ");
    if(m_tf & TF_FIXED)        strcat(buf,"TT ");
    if(m_tf & TF_SUPER)        strcat(buf,"SUP ");
    if(m_tf & TF_SUB)          strcat(buf,"SUB ");
    if(m_tf & TF_STRIKEOUT)    strcat(buf,"SO ");
    if(m_tf & TF_UNDERLINE)    strcat(buf,"U ");
    if(m_tf & TF_BLINK)        strcat(buf,"BL ");
#ifdef USE_SCRIPT
    if(m_tf & TF_SERVER)       strcat(buf,"SERVER ");
    if(m_tf & TF_SCRIPT)       strcat(buf,"SCRIPT ");
    if(m_tf & TF_STYLE)        strcat(buf,"STYLE ");
#endif
    if(m_tf & TF_SPELL)        strcat(buf,"SPELL ");
    if(m_tf & TF_INLINEINPUT)  strcat(buf,"INLINEINPUT ");
    if(m_tf & TF_INLINEINPUTTHICK)  strcat(buf,"INLINEINPUTTHICK ");
    if(m_tf & TF_INLINEINPUTDOTTED)  strcat(buf,"INLINEINPUTDOTTED ");

    if(m_pScriptExtra){
        strcat(buf, m_pScriptExtra);
        strcat(buf," ");
    }

    if ( m_tf & TF_FONT_FACE && m_pFace ) {
        strcat( buf, m_pFace );
        strcat( buf, " ");
    }

    if ( m_tf & TF_FONT_WEIGHT ){
        char sizeBuf[30];
        PR_snprintf(sizeBuf, 30, "weight=%d ", m_iWeight );
        strcat( buf, sizeBuf);
    }

    if ( m_tf & TF_FONT_POINT_SIZE ){
        char sizeBuf[30];
        PR_snprintf(sizeBuf, 30, "pointsize=%d ", m_iPointSize );
        strcat( buf, sizeBuf);
    }

    if( m_tf & TF_FONT_COLOR ){ 
        char color_str[8];
        PR_snprintf(color_str, 8, "#%06lX ", GetColor().GetAsLong() );
        strcat( buf, color_str );
    }

    if( m_tf & TF_FONT_SIZE ){
        i = XP_STRLEN(buf);
        buf[i] = (GetFontSize() < 3  ? '-': '+');
        buf[i+1] = '0' + abs(GetFontSize() - 3);
        buf[i+2] = ' ';
        buf[i+3] = 0;
    }

    if( m_tf & TF_HREF ){ 
        strcat( buf, m_href->hrefStr );
    }

    strcat(buf, ") " );
    return buf;
}   

//
// Create a list of tags that represents the character formatting for this
//  text element.
//
void CEditTextElement::FormatOpenTags(PA_Tag*& pStart, PA_Tag*& pEnd){


    if(m_tf & TF_BOLD){ edt_AddTag( pStart, pEnd, P_BOLD, FALSE ); }
    if(m_tf & TF_ITALIC){ edt_AddTag( pStart, pEnd, P_ITALIC, FALSE ); }
    if(m_tf & TF_FIXED){ edt_AddTag( pStart, pEnd, P_FIXED, FALSE ); }
    if(m_tf & TF_SUPER){ edt_AddTag( pStart,pEnd, P_SUPER, FALSE ); }
    if(m_tf & TF_SUB){ edt_AddTag( pStart,pEnd, P_SUB, FALSE ); }
    if(m_tf & TF_NOBREAK){ edt_AddTag( pStart,pEnd, P_NOBREAK, FALSE ); }
    if(m_tf & TF_STRIKEOUT){ edt_AddTag( pStart,pEnd, P_STRIKEOUT, FALSE ); }
    if(m_tf & TF_UNDERLINE){ edt_AddTag( pStart,pEnd, P_UNDERLINE, FALSE ); }
    if(m_tf & TF_BLINK){ edt_AddTag( pStart,pEnd, P_BLINK, FALSE ); }
    if(m_tf & TF_SPELL){ edt_AddTag( pStart,pEnd, P_SPELL, FALSE ); }
    if(m_tf & TF_INLINEINPUT){ edt_AddTag( pStart,pEnd, P_INLINEINPUT, FALSE ); }
    if(m_tf & TF_INLINEINPUTTHICK){ edt_AddTag( pStart,pEnd, P_INLINEINPUTTHICK, FALSE ); }
    if(m_tf & TF_INLINEINPUTDOTTED){ edt_AddTag( pStart,pEnd, P_INLINEINPUTDOTTED, FALSE ); }

    // Face and color have to come before size for old browsers.
    if( m_tf & TF_FONT_FACE ){
        char buf[200];
        XP_STRCPY( buf, " FACE=\"" );
        strcat( buf, m_pFace );
        strcat( buf, "\">" );
        edt_AddTag( pStart,pEnd, P_FONT, FALSE, buf );
    }

    if ( m_tf & TF_FONT_WEIGHT ) {
        char buf[20];
        PR_snprintf(buf, 20, " FONT-WEIGHT=%d>", m_iWeight);
        edt_AddTag( pStart,pEnd, P_FONT, FALSE, buf );
    }

    if ( m_tf & TF_FONT_POINT_SIZE ) {
        char buf[20];
        PR_snprintf(buf, 20, " POINT-SIZE=%d>", m_iPointSize);
        edt_AddTag( pStart,pEnd, P_FONT, FALSE, buf );
    }

    if( GetColor().IsDefined() ){ 
        char buf[20];
        PR_snprintf(buf, 20, "COLOR=\"#%06lX\">", GetColor().GetAsLong() );
        edt_AddTag( pStart,pEnd, P_FONT, FALSE, buf );
    }

    if( m_tf & TF_FONT_SIZE ){
        char buf[20];
        XP_STRCPY( buf, " SIZE=" );
        int i = XP_STRLEN(buf);
        buf[i] = (GetFontSize() < 3  ? '-': '+');
        buf[i+1] = '0' + abs(GetFontSize() - 3);
        buf[i+2] = '>';
        buf[i+3] = 0;
        edt_AddTag( pStart,pEnd, P_FONT, FALSE, buf );
    }

    if( m_tf & TF_HREF){ 
        char *pBuf = PR_smprintf("HREF=%s %s\">", edt_MakeParamString(m_href->hrefStr),
            (m_href->pExtra ? m_href->pExtra : "") );        
        edt_AddTag( pStart,pEnd, P_ANCHOR, FALSE, pBuf );
        free( pBuf );
    }

    // if EVAL, hack the character attributes...
#ifdef USE_SCRIPT
    if(m_tf & TF_SERVER){ 
        edt_AddTag( pStart, pEnd, P_SERVER, FALSE );
        return;
    }

    if(m_tf & TF_SCRIPT){ 
        edt_AddTag( pStart, pEnd, P_SCRIPT, FALSE );
        return;
    }

    if(m_tf & TF_STYLE){
        edt_AddTag( pStart, pEnd, P_STYLE, FALSE );
        return;
    }
#endif
}

void CEditTextElement::FormatTransitionTags(CEditTextElement* /* pNext */,
            PA_Tag*& /* pStart */, PA_Tag*& /* pEnd */){
}

void CEditTextElement::FormatCloseTags(PA_Tag*& pStart, PA_Tag*& pEnd){

    // WARNING: order here is real important, Open and Close must match
    //  exactly.

    // if EVAL must be last
#ifdef USE_SCRIPT
    if(m_tf & TF_STYLE){ 
        edt_AddTag( pStart, pEnd, P_STYLE, TRUE );
    }

    if(m_tf & TF_SCRIPT){ 
        edt_AddTag( pStart, pEnd, P_SCRIPT, TRUE );
    }

    if(m_tf & TF_SERVER){ 
        edt_AddTag( pStart, pEnd, P_SERVER, TRUE );
    }
#endif

    //if(m_href){ edt_AddTag( pStart,pEnd, P_ITALIC, FALSE ); }
    if( m_tf & TF_HREF ){ edt_AddTag( pStart,pEnd, P_ANCHOR, TRUE );}
    if( m_tf & TF_FONT_SIZE ){edt_AddTag( pStart,pEnd, P_FONT, TRUE );}
    if( GetColor().IsDefined() ){edt_AddTag( pStart,pEnd, P_FONT, TRUE );}
    if( m_tf & TF_FONT_POINT_SIZE ) edt_AddTag( pStart,pEnd, P_FONT, TRUE );
    if( m_tf & TF_FONT_WEIGHT ) edt_AddTag( pStart,pEnd, P_FONT, TRUE );
    if( m_tf & TF_FONT_FACE ) edt_AddTag( pStart,pEnd, P_FONT, TRUE );

    if(m_tf & TF_INLINEINPUTDOTTED ){ edt_AddTag( pStart,pEnd, P_INLINEINPUTDOTTED, TRUE); }
    if(m_tf & TF_INLINEINPUTTHICK ){ edt_AddTag( pStart,pEnd, P_INLINEINPUTTHICK, TRUE); }
    if(m_tf & TF_INLINEINPUT ){ edt_AddTag( pStart,pEnd, P_INLINEINPUT, TRUE); }
    if(m_tf & TF_SPELL ){ edt_AddTag( pStart,pEnd, P_SPELL, TRUE); }
    if(m_tf & TF_BLINK ){ edt_AddTag( pStart,pEnd, P_BLINK, TRUE); }
    if(m_tf & TF_STRIKEOUT ){ edt_AddTag( pStart,pEnd, P_STRIKEOUT, TRUE); }
    if(m_tf & TF_UNDERLINE ){ edt_AddTag( pStart,pEnd, P_UNDERLINE, TRUE); }
    if(m_tf & TF_NOBREAK ){ edt_AddTag( pStart,pEnd, P_NOBREAK, TRUE); }
    if(m_tf & TF_SUB ){ edt_AddTag( pStart,pEnd, P_SUB, TRUE); }
    if(m_tf & TF_SUPER ){ edt_AddTag( pStart,pEnd, P_SUPER, TRUE); }
    if(m_tf & TF_FIXED ){ edt_AddTag( pStart, pEnd, P_FIXED, TRUE); }
    if(m_tf & TF_ITALIC ){ edt_AddTag( pStart, pEnd, P_ITALIC, TRUE); }
    if(m_tf & TF_BOLD ){ edt_AddTag( pStart, pEnd, P_BOLD, TRUE); }


}

//
// Create a text tag.  We are going to be feeding these tags to the layout 
//  engine.  The layout element this edit element points to is going to change.
//
PA_Tag* CEditTextElement::TagOpen( int iEditOffset ){
    // LTNOTE: I think that we only want to do this if we are the first text
    //  in the container, other wise, we don't do anything.
    PA_Tag *pStart = 0;
    PA_Tag *pEnd = 0;
    XP_Bool bFirst = IsFirstInContainer();

    if( GetLen() == 0 && (!bFirst || (bFirst && LeafInContainerAfter() != 0))){
        return 0;
    }

    PA_Tag *pTag = XP_NEW( PA_Tag );
    XP_BZERO( pTag, sizeof( PA_Tag ) );

    FormatOpenTags( pStart, pEnd );    

    if( pStart == 0 ){
        pStart = pTag;
    }
    if( pEnd ){
        pEnd->next = pTag;
    }
    pEnd = pTag;

    if(GetLen() == 0){
        // As of 3.0b5, a non-breaking space always caused a memory leak when
        // it was laid out. See bug 23404
        // So use a different character here. The character is
        // removed by code in CEditBuffer::Relayout
#ifdef USE_PERIOD_FOR_BLANK_LAYOUT
        SetTagData(pTag, ".");
#else
        SetTagData(pTag, NON_BREAKING_SPACE_STRING);
#endif
        //SetTagData(pTag, "");
    }
    else {
        SetTagData(pTag, GetTextWithConvertedSpaces()+iEditOffset);
    }
    if( iEditOffset==0 ){
        m_pFirstLayoutElement = 0;
    }

    FormatCloseTags( pStart, pEnd );

    return pStart;
}

XP_Bool CEditTextElement::GetLOTextAndOffset( ElementOffset iEditOffset, XP_Bool bStickyAfter, LO_TextStruct*&  pRetText, 
        int& iLayoutOffset ){
    
    LO_TextStruct *pText;
    LO_Element* pLoElement = m_pFirstLayoutElement;
    XP_ASSERT(!pLoElement || pLoElement->lo_any.edit_offset == 0);
    intn len = GetLen();

    while(pLoElement != 0){
        if( pLoElement->type == LO_TEXT ){
            // if we have scanned past our edit element, we can't find the 
            // insert point.
            pText = &(pLoElement->lo_text);
            if( pText->edit_element != this){
                /* assert(0)
                */
                return FALSE;
            }

            int16 textLen = pText->text_len;
            int32 loEnd = pText->edit_offset + textLen;

            //
            // See if we are at the dreaded lopped of the last space case.
            //
            if( loEnd+1 == iEditOffset ){
                // ok, we've scaned past the end of the Element
                if( len == iEditOffset && m_pText[iEditOffset-1] == ' '){
                    // we wraped to the next element.  Return the next Text Element.
                    CEditElement *pNext = FindNextElement(&CEditElement::FindText,0);
                    if( pNext ){
                        return pNext->Text()->GetLOTextAndOffset( 0, FALSE, pRetText, iLayoutOffset );
                    }

                    // we should have all these cases handled now.

                    // jhp Nope. (a least as of Beta 2) If you have a very narrow document,
                    // and you
                    // type in two lines of text, and you're at the end of the
                    // second line, and you type a space, and the
                    // space causes a line wrap, then you end up here.
                    // XP_ASSERT(FALSE);

                    return FALSE;
                }
            }

            if( loEnd > iEditOffset || (loEnd == iEditOffset
                && (!bStickyAfter || len == iEditOffset || textLen == 0 ) ) ){
                // we've found the right element, return the information.
                iLayoutOffset = iEditOffset - pText->edit_offset;
                pRetText = pText;
                return TRUE;
            }
            
        }
        pLoElement = pLoElement->lo_any.next;
    }
    return FALSE;
}

LO_TextStruct* CEditTextElement::GetLOText( int iEditOffset ){
    
    LO_TextStruct *pText;
    LO_Element* pLoElement = m_pFirstLayoutElement;

    while(pLoElement != 0){
        if( pLoElement->type == LO_TEXT ){
            // if we have scanned past our edit element, we can't find the 
            // insert point.
            pText = &(pLoElement->lo_text);
            if( pText->edit_element != this){
                return 0;
            }
            if( pText->edit_offset > iEditOffset ){
                return 0;
            }

            if( pText->edit_offset + pText->text_len >= iEditOffset ){
                // we've found the right element, return the information.
                return pText;
            }
            
        }
        pLoElement = pLoElement->lo_any.next;
    }
    return 0;
}

LO_TextBlock* CEditTextElement::GetTextBlock()
{
	return m_pTextBlock;
}


XP_Bool CEditTextElement::CanReflow() {
	// we can only reflow if we have a text block
	return m_pTextBlock != NULL;
}

void CEditTextElement::PrintTagOpen( CPrintState *pPrintState, TagType t, 
            ED_TextFormat tf, char* pExtra ){
    if ( ! pExtra ) pExtra = ">";
    pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "<%s%s", 
                EDT_TagString(t), pExtra );
    pPrintState->m_formatStack.Push(tf);
    pPrintState->m_elementStack.Push(this);
}


void CEditTextElement::PrintFormatDifference( CPrintState *ps, 
        ED_TextFormat bitsDifferent ){
    if(bitsDifferent & TF_BOLD){ PrintTagOpen( ps, P_BOLD, TF_BOLD ); } 
    if(bitsDifferent & TF_ITALIC){ PrintTagOpen( ps, P_ITALIC, TF_ITALIC ); } 
    if(bitsDifferent & TF_FIXED){ PrintTagOpen( ps, P_FIXED, TF_FIXED ); } 
    if(bitsDifferent & TF_SUPER){ PrintTagOpen( ps, P_SUPER, TF_SUPER ); } 
    if(bitsDifferent & TF_SUB){ PrintTagOpen( ps, P_SUB, TF_SUB ); } 
    if(bitsDifferent & TF_NOBREAK){ PrintTagOpen( ps, P_NOBREAK, TF_NOBREAK ); } 
    if(bitsDifferent & TF_STRIKEOUT){ PrintTagOpen( ps, P_STRIKEOUT, TF_STRIKEOUT ); } 
    if(bitsDifferent & TF_UNDERLINE){ PrintTagOpen( ps, P_UNDERLINE, TF_UNDERLINE ); } 
    if(bitsDifferent & TF_BLINK){ PrintTagOpen( ps, P_BLINK, TF_BLINK ); } 
    if(bitsDifferent & TF_SPELL){ /*do nothing*/ } 
    if(bitsDifferent & TF_INLINEINPUT){ /*do nothing*/ } 
    if(bitsDifferent & TF_INLINEINPUTTHICK){ /*do nothing*/ } 
    if(bitsDifferent & TF_INLINEINPUTDOTTED){ /*do nothing*/ } 

    // Face and color have to preceed FONT_SIZE, or else old browsers will
    // reset the font_size to zero when they encounter the unknown tags.

    if( bitsDifferent & TF_FONT_FACE ){
        ps->m_iCharPos += ps->m_pOut->Printf( "<FONT FACE=\"%s\">", m_pFace);
        ps->m_formatStack.Push(TF_FONT_FACE);
        ps->m_elementStack.Push(this);
    }

    if( bitsDifferent & TF_FONT_WEIGHT ){
        ps->m_iCharPos += ps->m_pOut->Printf( "<FONT FONT-WEIGHT=\"%d\">", m_iWeight);
        ps->m_formatStack.Push(TF_FONT_WEIGHT);
        ps->m_elementStack.Push(this);
    }

    if( bitsDifferent & TF_FONT_POINT_SIZE ){
        ps->m_iCharPos += ps->m_pOut->Printf( "<FONT POINT-SIZE=\"%d\">", m_iPointSize);
        ps->m_formatStack.Push(TF_FONT_POINT_SIZE);
        ps->m_elementStack.Push(this);
    }

    if( bitsDifferent & TF_FONT_COLOR ){
        ps->m_iCharPos += ps->m_pOut->Printf( "<FONT COLOR=\"#%06lX\">", 
                    GetColor().GetAsLong() );
        ps->m_formatStack.Push(TF_FONT_COLOR);
        ps->m_elementStack.Push(this);
    }

    if( bitsDifferent & TF_FONT_SIZE ){
        char buf[4];
        buf[0] = (GetFontSize() < 3  ? '-': '+');
        buf[1] = '0' + abs(GetFontSize() - 3);
        buf[2] = 0;
        ps->m_iCharPos += ps->m_pOut->Printf( "<FONT SIZE=%s>", buf);
        ps->m_formatStack.Push(TF_FONT_SIZE);
        ps->m_elementStack.Push(this);
    }

    if( bitsDifferent & TF_HREF){ 
        // doesn't use the output routine because HREF can be large.
        char *pS1 = "";
        char *pS2 = "";
        if( m_href->pExtra ){
            pS1 = " ";
            pS2 = m_href->pExtra;
        }
        ps->m_iCharPos += ps->m_pOut->Printf( 
            "<A HREF=%s%s%s>", edt_MakeParamString(m_href->hrefStr), pS1, pS2 );

        ps->m_formatStack.Push(TF_HREF);
        ps->m_elementStack.Push(this);
    }

#ifdef USE_SCRIPT
    if(bitsDifferent & TF_STYLE){
        PrintTagOpen( ps, P_STYLE, TF_STYLE, GetScriptExtra() ); }
    if(bitsDifferent & TF_SCRIPT){
        PrintTagOpen( ps, P_SCRIPT, TF_SCRIPT, GetScriptExtra() ); } 
    if(bitsDifferent & TF_SERVER){ PrintTagOpen( ps, P_SERVER, TF_SERVER, GetScriptExtra() ); } 
#endif
}

void CEditTextElement::PrintFormat( CPrintState *ps, 
                                    CEditTextElement *pFirst, 
                                    ED_TextFormat mask ){
    ED_TextFormat bitsCommon = 0;
    ED_TextFormat bitsDifferent = 0;

    ComputeDifference(pFirst, mask, bitsCommon, bitsDifferent);

    CEditTextElement *pNext = (CEditTextElement*) TextInContainerAfter();

    if( bitsCommon ){
        // While we're in a run of the same style, there's nothing to do.
        // Avoid unnescessary recursion so we don't blow the stack on Mac or Win16
        while ( pNext && SameAttributes(pNext) ){
            pNext = (CEditTextElement*) pNext->TextInContainerAfter();
        }
        if( pNext ){
            pNext->PrintFormat( ps, pFirst, bitsCommon );
        }
        else {
            // we hit the end, so we have to open everything
            pFirst->PrintFormatDifference( ps, bitsCommon);
        }
    }
    if( bitsDifferent ){
        pFirst->PrintFormatDifference( ps, bitsDifferent );
    }
}

XP_Bool CEditTextElement::SameAttributes(CEditTextElement *pCompare){
    ED_TextFormat bitsCommon;
    ED_TextFormat bitsDifferent;
    ComputeDifference(pCompare, -1, bitsCommon, bitsDifferent);
    return ( bitsCommon == m_tf);
}

void CEditTextElement::ComputeDifference(CEditTextElement *pFirst, 
        ED_TextFormat mask, ED_TextFormat& bitsCommon, ED_TextFormat& bitsDifferent){
    bitsCommon = m_tf & mask;
    bitsDifferent = mask & ~bitsCommon;

    if( (bitsCommon & TF_FONT_SIZE)&& (GetFontSize() != pFirst->GetFontSize())){
        bitsCommon &= ~TF_FONT_SIZE;
        bitsDifferent |= TF_FONT_SIZE;
    }

    if( (bitsCommon & TF_FONT_COLOR)&& (GetColor() != pFirst->GetColor())){
        bitsCommon &= ~TF_FONT_COLOR;
        bitsDifferent |= TF_FONT_COLOR;
    }

    if( (bitsCommon & TF_FONT_POINT_SIZE)&& (GetFontPointSize() != pFirst->GetFontPointSize())){
        bitsCommon &= ~TF_FONT_POINT_SIZE;
        bitsDifferent |= TF_FONT_POINT_SIZE;
    }

    if( (bitsCommon & TF_FONT_WEIGHT)&& (GetFontWeight() != pFirst->GetFontWeight())){
        bitsCommon &= ~TF_FONT_WEIGHT;
        bitsDifferent |= TF_FONT_WEIGHT;
    }

    //CLM: These pointers were sometimes NULL
    char * pFontFace = GetFontFace();
    char * pFirstFontFace = pFirst->GetFontFace();
    // Assure that we have strings to compare for font face
    if( !pFontFace ){
        pFontFace = " ";
    }
    if( !pFirstFontFace ){
        pFirstFontFace = " ";
    }

    if( (bitsCommon & TF_FONT_FACE) && XP_STRCMP(pFontFace,pFirstFontFace) != 0 ){
        bitsCommon &= ~TF_FONT_FACE;
        bitsDifferent |= TF_FONT_FACE;
    }
    if( bitsCommon & TF_FONT_FACE ) {
        char* a = GetFontFace();
        char* b = pFirst->GetFontFace();                 
        XP_Bool bA = a != NULL;
        XP_Bool bB = b != NULL;
        if ((bA ^ bB) ||
            (bA && bB && XP_STRCMP(a, b) != 0)) {
            bitsCommon &= ~TF_FONT_FACE;
            bitsDifferent |= TF_FONT_FACE;
        }
    }
    if( (bitsCommon & TF_HREF)&& (GetHREF() != pFirst->GetHREF())){
        bitsCommon &= ~TF_HREF;
        bitsDifferent |= TF_HREF;
    }
}

void CEditTextElement::PrintTagClose( CPrintState *ps, TagType t ){
    ps->m_iCharPos += ps->m_pOut->Printf( "</%s>", 
                EDT_TagString(t) );
}

void CEditTextElement::PrintPopFormat( CPrintState *ps, int iStackTop ){

    while( ps->m_formatStack.m_iTop >= iStackTop ){
        
        ED_TextFormat tf = (ED_TextFormat) ps->m_formatStack.Pop();
        ps->m_elementStack.Pop();
#ifdef USE_SCRIPT
        if( tf & TF_STYLE){ PrintTagClose( ps, P_STYLE ); } 
        if( tf & TF_SCRIPT){ PrintTagClose( ps, P_SCRIPT ); } 
        if( tf & TF_SERVER){ PrintTagClose( ps, P_SERVER ); } 
#endif
        if( tf & TF_BOLD){ PrintTagClose( ps, P_BOLD ); } 
        if( tf & TF_ITALIC){ PrintTagClose( ps, P_ITALIC ); } 
        if( tf & TF_FIXED){ PrintTagClose( ps, P_FIXED ); } 
        if( tf & TF_SUPER){ PrintTagClose( ps, P_SUPER ); } 
        if( tf & TF_SUB){ PrintTagClose( ps, P_SUB ); } 
        if( tf & TF_NOBREAK){ PrintTagClose( ps, P_NOBREAK ); } 
        if( tf & TF_STRIKEOUT){ PrintTagClose( ps, P_STRIKEOUT ); } 
        if( tf & TF_UNDERLINE){ PrintTagClose( ps, P_UNDERLINE ); } 
        if( tf & TF_BLINK){ PrintTagClose( ps, P_BLINK ); } 
        if( tf & TF_SPELL){ /* do nothing */ } 
        if( tf & TF_INLINEINPUT){ /* do nothing */ } 
        if( tf & TF_INLINEINPUTTHICK){ /* do nothing */ } 
        if( tf & TF_INLINEINPUTDOTTED){ /* do nothing */ } 
        if( tf & TF_FONT_SIZE){ PrintTagClose( ps, P_FONT ); } 
        if( tf & TF_FONT_COLOR){ PrintTagClose( ps, P_FONT ); } 
        if( tf & TF_FONT_POINT_SIZE){ PrintTagClose( ps, P_FONT ); } 
        if( tf & TF_FONT_WEIGHT){ PrintTagClose( ps, P_FONT ); } 
        if( tf & TF_FONT_FACE){ PrintTagClose( ps, P_FONT ); } 
        if( tf & TF_HREF){ PrintTagClose( ps, P_ANCHOR ); } 
    }
}

ED_TextFormat CEditTextElement::PrintFormatClose( CPrintState *ps ){
    XP_Bool bDiff = FALSE;
    ED_TextFormat bitsNeedFormat = m_tf ;

    if( ps->m_formatStack.IsEmpty()){
        return bitsNeedFormat;        
    }
    int i = -1;
    while( !bDiff && ++i <= ps->m_formatStack.m_iTop ){
        ED_TextFormat tf = (ED_TextFormat) ps->m_formatStack[i];
        if( m_tf & tf ){
            if( tf == TF_FONT_COLOR 
                    && GetColor() != ps->m_elementStack[i]->GetColor()){
                bDiff = TRUE;
            }
            else if( tf == TF_FONT_SIZE 
                    && GetFontSize() != ps->m_elementStack[i]->GetFontSize()){
                bDiff = TRUE;
            }
            else if( tf == TF_FONT_POINT_SIZE
                    && GetFontPointSize() != ps->m_elementStack[i]->GetFontPointSize()){
                bDiff = TRUE;
            }
            else if( tf == TF_FONT_WEIGHT
                    && GetFontWeight() != ps->m_elementStack[i]->GetFontWeight()){
                bDiff = TRUE;
            }
            else if( tf == TF_FONT_FACE 
                    && XP_STRCMP(GetFontFace(), ps->m_elementStack[i]->GetFontFace()) != 0){
                bDiff = TRUE;
            }
            else if( tf == TF_HREF
                    && GetHREF() != ps->m_elementStack[i]->GetHREF() ){
                bDiff = TRUE;
            }
        }
        else {
            bDiff = TRUE;
        }

        // if the stuff is on the stack, then it doesn't need to be formatted
        if( !bDiff ){
            bitsNeedFormat &=  ~tf;
        }
    }
    PrintPopFormat( ps, i );
    return bitsNeedFormat;

}

// for use within 
void CEditTextElement::PrintWithEscapes( CPrintState *ps, XP_Bool bTrimTrailingSpaces ){
    char *p = GetTextWithConvertedSpaces();
    int csid = INTL_DefaultWinCharSetID(ps->m_pBuffer->m_pContext);

    // Trim the last non-breaking spaces at the end of a 
    // top-level paragraph. The spaces just irritate users,
    // especially in HTML mail.
    // (But don't trim the last nbsp if it's the only thing in the paragraph, or
    // else layout will ignore the paragraph.)
    // And don't trim them in tables, where they're used to affect column width.

    if ( bTrimTrailingSpaces && p && ! LeafInContainerAfter() && ! GetSubDocSkipRoot() ) {
        /* find last NBSP by going forward char-by-char */
        char *s, *last_nbsp, *end;
        s = last_nbsp = p;
        end = p + XP_STRLEN(p);
        while ((s <= end) && (*s)) {
            /* if not nbsp then advance both pointers */
            if (*s != NON_BREAKING_SPACE)
                last_nbsp = s = INTL_NextChar(csid, s);
            else
                s = INTL_NextChar(csid, s);
        }
        if (last_nbsp <= end){
            // Don't trim last nbsp if it's the only thing in the paragraph.
            if ( p == last_nbsp && IsFirstInContainer() && last_nbsp < end ){
                last_nbsp++;
            }
            *last_nbsp = '\0'; /* points to last nbsp or end of string */
        }
    }
    if( p == 0 ){
#ifndef EDT_DDT
        p = NON_BREAKING_SPACE_STRING;
#endif
    }
    edt_PrintWithEscapes( ps, p, !InFormattedText() );
}

void CEditTextElement::PrintLiteral( CPrintState *ps ){
    int iLen = GetLen();
    ps->m_pOut->Write( GetText(), iLen );
    ps->m_iCharPos += iLen;
}

void CEditTextElement::PrintOpen( CPrintState *ps){
    ED_TextFormat bitsToFormat = PrintFormatClose( ps );

    CEditTextElement *pNext = (CEditTextElement*) TextInContainerAfter();
    if( pNext ){
        pNext->PrintFormat( ps, this, bitsToFormat );
    }
    else {
        PrintFormatDifference( ps, bitsToFormat );
    }
    
    if ( ps->ShouldPrintSelectionComments(this) ){
        // Up to three pieces
        int32 index = 0;
        if ( ps->ShouldPrintSelectionComment(this, FALSE) ){
            index = ps->m_selection.m_start.m_iPos;
            if ( 0 < index ) {
                PrintRange(ps, 0, index);
            }
            ps->PrintSelectionComment(FALSE, ps->m_selection.m_start.m_bStickyAfter);
        }
        if ( ps->ShouldPrintSelectionComment(this, TRUE) ){
            int32 oldIndex = index;
            index = ps->m_selection.m_end.m_iPos;
            if ( oldIndex < index ) {
                PrintRange(ps, oldIndex, index);
            }
            ps->PrintSelectionComment(TRUE, ps->m_selection.m_end.m_bStickyAfter);
        }
        if ( index < GetLen() ){
            PrintRange(ps, index, GetLen());
        }
    }
    else {
        PrintOpen2(ps, TRUE);
    }

    if( pNext == 0 ) {
        PrintPopFormat( ps, 0 );
    }
}

void CEditTextElement::PrintRange( CPrintState *ps, int32 start, int32 end ){
    // We modify m_pText in place and then call PrintOpen2 to do the
    // actual printing. (And then we restore m_pText to its original
    // condition.)
    //
    int32 len = GetLen();
    if ( start == 0 && end == len ){
        PrintOpen2(ps, FALSE);
    }
    else {
        if ( end > len ) {
            XP_ASSERT(FALSE);
#ifdef DEBUG_akkana
            printf("end = %d len = %d\n", end, len);
#endif
            end = len;
        }
        if ( start < 0 ) {
            XP_ASSERT(FALSE);
            start = 0;
        }
        if ( start < end && m_pText ) {
            char* pText = m_pText;
            char save = pText[end];
            pText[end] = '\0';
            m_pText = pText + start;
            // If GetLen() is ever cached we would need to update the cache.
            XP_ASSERT(GetLen() == end - start);
            PrintOpen2(ps, FALSE);
            m_pText = pText;
            pText[end] = save;
        }
    }
}

void CEditTextElement::PrintOpen2( CPrintState *ps, XP_Bool bTrimTrailingSpaces )
{
#ifdef USE_SCRIPT
    if( m_tf & (TF_SERVER|TF_SCRIPT|TF_STYLE) ){
        PrintLiteral( ps );
    }
    else {
        PrintWithEscapes( ps );
    }
#else
    PrintWithEscapes( ps, bTrimTrailingSpaces );
#endif
}

XP_Bool CEditTextElement::Reduce( CEditBuffer *pBuffer ){
    if( GetLen() == 0 ){
        if( pBuffer->m_pCurrent == this 
                || (IsFirstInContainer()
                        && LeafInContainerAfter() == 0)
            ){
            return FALSE;
        }
        else {
            return TRUE;
        }
    }
    else {
        return FALSE;
    }
}

#ifdef DEBUG
void CEditTextElement::ValidateTree(){
    CEditLeafElement::ValidateTree();
    LO_TextStruct *pText = (LO_TextStruct*) GetLayoutElement();

    if( GetType() != P_TEXT  ){
        XP_ASSERT(FALSE);
    }
    if(m_pText && pText ) {
        if (pText->edit_element != this){
            XP_ASSERT(FALSE);
        }
        if (pText->edit_offset != 0){
            XP_ASSERT(FALSE);
        }
    }
    XP_Bool bHasLinkTag = (m_tf & TF_HREF) != 0;
    XP_Bool bHasLinkRef = (m_href != ED_LINK_ID_NONE);
    if( bHasLinkTag != bHasLinkRef ){
        XP_ASSERT(FALSE);
    };
}
#endif	//	DEBUG

void CEditTextElement::StreamToPositionalText( IStreamOut *pOut, XP_Bool bEnd ){ 
    if( !bEnd ){
        char space = ' ';

        // Ignore Quoted text, unless there is a selection.
        if ( InMailQuote() && !EDT_IsSelected(GetEditBuffer()->m_pContext)) {
            // Write out spaces instead of text. This prevents us from spell checking
            // quoted text.
            int32 len = GetLen();
            for(int32 i = 0; i < len; i++){
                pOut->Write(&space, 1);
            }
        }
        else {
            INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(GetEditBuffer()->m_pContext);
            int16   csid = INTL_GetCSIWinCSID(c);

            if (INTL_CharSetType(csid) == SINGLEBYTE){
                pOut->Write(GetText(),GetLen());
            } 
            else{
                // Replace any multi-byte characters with spaces, to avoid spell checking
                // of Japanese and other far-eastern text.
                int32 textLen, charLen=0;
                char *pText = GetText();

                for (textLen = GetLen(); textLen > 0; textLen -= charLen, pText += charLen){
                    charLen = INTL_CharLen(csid, (unsigned char *)pText);
                    if (charLen > 1){
                        for(int32 i = 0; i < charLen; i++){
                            pOut->Write(&space, 1);
                        }
                    }
                    else{
                        pOut->Write(pText, charLen);
                    }
                }
            }
        }
    }
}


//-----------------------------------------------------------------------------
// Insertion routines.
//-----------------------------------------------------------------------------

//
// Split text object into two text objects.  Parent is the same.
//
CEditElement* CEditTextElement::SplitText( int iOffset ){
    CEditTextElement* pNew = new CEditTextElement( 0, m_pText+iOffset );
    m_pText[iOffset] = 0;
    GetParent()->Split( this, pNew, &CEditElement::SplitContainerTest, 0 );
    return  pNew;
}

CEditTextElement* CEditTextElement::CopyEmptyText( CEditElement *pParent ){
    CEditTextElement* pNew = new CEditTextElement(pParent, 0);

    pNew->m_tf = m_tf;
    pNew->m_iFontSize = m_iFontSize;
    pNew->m_color = m_color;
    if( (m_tf & TF_HREF) && m_href != ED_LINK_ID_NONE){
        pNew->SetHREF(m_href);
    }
    if ( m_pFace ) {
        pNew->SetFontFace(m_pFace);
    }
    pNew->m_iWeight = m_iWeight;
    pNew->m_iPointSize = m_iPointSize;
    if ( m_pScriptExtra ) {
        pNew->SetScriptExtra(m_pScriptExtra);
    }
    return pNew;
}

void CEditTextElement::DeleteText(){
    CEditElement *pKill = this;
    CEditElement *pParent;

    do {
        pParent = pKill->GetParent();
        pKill->Unlink();
        delete pKill;
        pKill = pParent;
    } while( BitSet( edt_setCharFormat, pKill->GetType() ) && pKill->GetChild() == 0 );
}

//----------------------------------------------------------------------------
// CEditImageElement
//----------------------------------------------------------------------------

//
// LTNOTE: these should be static functions on CEditImageData..
//
EDT_ImageData* edt_NewImageData(){
    EDT_ImageData *pData = XP_NEW( EDT_ImageData );
    if( pData == 0 ){
        // throw();
        return pData;
    }
    pData->bIsMap = FALSE;
//    pData->pUseMap = 0;  now in pExtra
    pData->align = ED_ALIGN_DEFAULT;
    pData->pSrc = 0;
    pData->pLowSrc = 0;
    pData->pName = 0;
    pData->pAlt = 0;
    pData->iWidth = 0;
    pData->iHeight = 0;
    pData->iOriginalWidth = 0;
    pData->iOriginalHeight = 0;
    pData->bWidthPercent = FALSE;
    pData->bHeightPercent = FALSE;
    pData->iHSpace = 0;
    pData->iVSpace = 0;
    pData->iBorder = -1;
    pData->bNoSave = FALSE;
    pData->pHREFData = NULL;
    pData->pExtra = 0;
    return pData;
}

EDT_ImageData* edt_DupImageData( EDT_ImageData *pOldData ){
    EDT_ImageData *pData = XP_NEW( EDT_ImageData );

//    pData->pUseMap = edt_StrDup(pOldData->pUseMap);  now in pExtra
    pData->pSrc = edt_StrDup(pOldData->pSrc);
    pData->pName = edt_StrDup(pOldData->pName);
    pData->pLowSrc = edt_StrDup(pOldData->pLowSrc);
    pData->pAlt = edt_StrDup(pOldData->pAlt);
    pData->pExtra = edt_StrDup(pOldData->pExtra);

    pData->bIsMap = pOldData->bIsMap;
    pData->align = pOldData->align;
    pData->iWidth = pOldData->iWidth;
    pData->iHeight = pOldData->iHeight;
    pData->iOriginalWidth = pOldData->iOriginalWidth;
    pData->iOriginalHeight = pOldData->iOriginalHeight;
    pData->bWidthPercent = pOldData->bWidthPercent;
    pData->bHeightPercent = pOldData->bHeightPercent;
    pData->iHSpace = pOldData->iHSpace;
    pData->iVSpace = pOldData->iVSpace;
    pData->iBorder = pOldData->iBorder;
    pData->bNoSave = pOldData->bNoSave;

    if( pOldData->pHREFData ){
        pData->pHREFData = EDT_DupHREFData( pOldData->pHREFData );
    }
    else {
        pData->pHREFData = 0;
    }
        
    return pData;
}


void edt_FreeImageData( EDT_ImageData *pData ){
//    if( pData->pUseMap ) XP_FREE( pData->pUseMap );  now in pExtra
    if( pData->pSrc ) XP_FREE( pData->pSrc );
    if( pData->pLowSrc ) XP_FREE( pData->pLowSrc );
    if( pData->pName ) XP_FREE( pData->pName );
    if( pData->pAlt ) XP_FREE( pData->pAlt );
    if( pData->pExtra ) XP_FREE( pData->pExtra );
    if( pData->pHREFData ) EDT_FreeHREFData( pData->pHREFData );
    XP_FREE( pData );
}

//
// Constructors, Streamers..
//
CEditImageElement::CEditImageElement( CEditElement *pParent, PA_Tag* pTag, int16 csid,
                ED_LinkId href ):
            CEditLeafElement( pParent, P_IMAGE ), 
            m_pLoImage(0),
            m_pParams(0),
            m_iHeight(0),
            m_iWidth(0),
            m_bWidthPercent(0),
            m_bHeightPercent(0),
            m_iSaveIndex(0),
            m_iSaveLowIndex(0),
            m_href(ED_LINK_ID_NONE),
            m_align( ED_ALIGN_DEFAULT),
            m_bSizeWasGiven(FALSE),
            m_bSizeIsBogus(FALSE)
{
    // must set href before calling SetImageData to allow GetDefaultBorder to work
    if( href != ED_LINK_ID_NONE ){
        SetHREF( href );
    }
    if( pTag ){
        EDT_ImageData *pData = ParseParams( pTag, csid );
        SetImageData( pData );
        edt_FreeImageData( pData );
    }
}

CEditImageElement::CEditImageElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer):
            CEditLeafElement(pStreamIn, pBuffer), 
            m_pLoImage(0),
            m_pParams(0), 
            m_href(ED_LINK_ID_NONE),
            m_align(ED_ALIGN_DEFAULT) {
    m_align = (ED_Alignment) pStreamIn->ReadInt();
    m_bSizeWasGiven = (XP_Bool) pStreamIn->ReadInt();
    m_bSizeIsBogus = (XP_Bool) pStreamIn->ReadInt();
    m_iHeight = pStreamIn->ReadInt();
    m_iWidth = pStreamIn->ReadInt();
    m_bWidthPercent =  pStreamIn->ReadInt();
    m_bHeightPercent =  pStreamIn->ReadInt();
    m_pParams = pStreamIn->ReadZString();
    char *pHrefString = pStreamIn->ReadZString();
    char *pExtra = pStreamIn->ReadZString();
    if( pHrefString ){
        SetHREF( pBuffer->linkManager.Add( pHrefString, pExtra ));
        XP_FREE( pHrefString );
        if( pExtra ) XP_FREE( pExtra );
    }
}

CEditImageElement::~CEditImageElement(){
    DisconnectLayoutElements((LO_Element*) m_pLoImage);
    if( m_pParams) XP_FREE( m_pParams ); 
    if( m_href != ED_LINK_ID_NONE ) m_href->Release();
}

// Return TRUE if pURL has changed.
PRIVATE XP_Bool edt_make_image_relative(char *pBase,char **pURL) {
  if (!pBase || !*pBase || !pURL || !*pURL || !**pURL) {
    // Base and pURL must be existing non-empty strings.
    return FALSE;
  }

  if ((*pURL)[0] == '/') {
    // Don't change URL if using absolute pathing.
    return FALSE;
  }

  char *pAbs = NET_MakeAbsoluteURL(pBase,*pURL); // In case pURL was already relative.
  char *pRel;
  NET_MakeRelativeURL(pBase,pAbs,&pRel);
  XP_FREEIF(pAbs);

  if (pRel && XP_STRCMP(pRel,*pURL)) {
    // pRel exists and is different than original value.
    XP_FREEIF(*pURL);
    *pURL = pRel;
    return TRUE;
  }
  else {
    XP_FREEIF(pRel);
    return FALSE;
  }
}

void CEditImageElement::SetLayoutElement( intn iEditOffset, intn lo_type, 
                        LO_Element* pLoElement ){
    SetLayoutElementHelper(LO_IMAGE, (LO_Element**) &m_pLoImage,
        iEditOffset, lo_type, pLoElement);
	// Zero out the image_req field. We "know" that it is garbage at this point.
	pLoElement->lo_image.image_req = 0;

    // Make image URLs relative to document. bug 41009.
    EDT_ImageData *pData = GetImageData();
    CEditBuffer *pBuffer = GetEditBuffer();
    if( pData ) {
      XP_Bool bChanged = FALSE;
      if (pBuffer && !EDT_IS_NEW_DOCUMENT(pBuffer->m_pContext)){
        if (edt_make_image_relative(pBuffer->GetBaseURL(),&pData->pSrc)) {
          bChanged = TRUE;
        }
        if (edt_make_image_relative(pBuffer->GetBaseURL(),&pData->pLowSrc)) {
          bChanged = TRUE;
        }
      }

      if (bChanged) {
        SetImageData(pData);
      }
      edt_FreeImageData(pData);
    }
}

void CEditImageElement::ResetLayoutElement( intn iEditOffset, 
            LO_Element* pLoElement ){
    ResetLayoutElementHelper((LO_Element**) &m_pLoImage,
        iEditOffset, pLoElement);
}

LO_Element* CEditImageElement::GetLayoutElement(){
    return (LO_Element*)m_pLoImage;
}

void CEditImageElement::StreamOut( IStreamOut *pOut){
    CEditLeafElement::StreamOut( pOut );
    
    pOut->WriteInt( (int32)m_align );
    pOut->WriteInt( m_bSizeWasGiven );
    pOut->WriteInt( m_bSizeIsBogus );
    pOut->WriteInt( m_iHeight );
    pOut->WriteInt( m_iWidth );
    pOut->WriteInt( m_bWidthPercent );
    pOut->WriteInt( m_bHeightPercent );

    // We need to make the Image Src and LowSrc Parameters absolute.
    // We do this by first converting the parameter string to and Edit
    //  data structure and then making the paths absolute.
    EDT_ImageData *pData = GetImageData();
    CEditBuffer *pBuffer = GetEditBuffer();
    if( pData && pBuffer && pBuffer->GetBaseURL() ){
        char *p;
        if( pData->pSrc ) {
            p = NET_MakeAbsoluteURL( pBuffer->GetBaseURL(), pData->pSrc );
            if( p ){
                XP_FREE( pData->pSrc );
                pData->pSrc = p;
            }
        }
        if( pData->pLowSrc ) {
            p = NET_MakeAbsoluteURL( pBuffer->GetBaseURL(), pData->pLowSrc );
            if( p ){
                XP_FREE( pData->pLowSrc );
                pData->pLowSrc = p;
            }
        }

        // Format the parameters for output.
        p = FormatParams( pData, TRUE );
        pOut->WriteZString( p );
        XP_FREE(p);
        edt_FreeImageData( pData );
    }
    else {
        pOut->WriteZString( m_pParams );
    }


    if( m_href != ED_LINK_ID_NONE ){
        pOut->WriteZString( m_href->hrefStr );
        pOut->WriteZString( m_href->pExtra );
    }
    else {
        pOut->WriteZString( 0 );
        pOut->WriteZString( 0 );
    }
}

EEditElementType CEditImageElement::GetElementType()
{
    return eImageElement;
}

PA_Tag* CEditImageElement::TagOpen( int /* iEditOffset */ ){
    char *pCur = 0;
    PA_Tag *pRetTag = 0;
    PA_Tag *pEndTag = 0;

    if( m_href != ED_LINK_ID_NONE ){
        char *pBuf = PR_smprintf("HREF=%s %s>", edt_MakeParamString(m_href->hrefStr),
            (m_href->pExtra ? m_href->pExtra : "") );        
        edt_AddTag( pRetTag, pEndTag, P_ANCHOR, FALSE, pBuf );
        free( pBuf );
    }

    if( m_pParams ){
        if( SizeIsKnown() ){    
            // Append the HEIGHT and WIDTH params, taking into account "%" mode
            pCur = XP_STRDUP(m_pParams);

            if( m_bHeightPercent ){
                pCur = PR_sprintf_append( pCur, " HEIGHT=\"%ld%%\" ", 
                                          (long)min(100,max(1,m_iHeight)) );
            }
            else {
                pCur = PR_sprintf_append( pCur, " HEIGHT=\"%ld\" ", (long)m_iHeight );
            }

            if( m_bWidthPercent ){
                pCur = PR_sprintf_append( pCur, " WIDTH=\"%ld%%\" ", 
                                          (long)min(100,max(1,m_iWidth)) );
            }
            else {
                pCur = PR_sprintf_append( pCur, " WIDTH=\"%ld\" ", (long)m_iWidth );
            }
            //CLM: Not sure if this is best behavior, but if size is
            //     already set in HTML, should consider that to be "original"?
            //     Maybe we should fill in here if we will end up getting same values in FinishedLoad
            //m_iOriginalHeight = m_iHeight;
            //m_iOriginalWidth = m_iWidth;
        }
        else {
            // We don't know the size, but if we don't give a size,
            // and if the image isn't known, we will block and
            // crash. So we put in a bogus size here, and take it
            // out in FinishedLoad if there's image data available.
            m_bSizeIsBogus = TRUE;
            pCur = PR_smprintf("%s HEIGHT=24 WIDTH=22", m_pParams );
        }
    }        

    // For editing, we don't support baseline, left or right alignment of images.

    if( m_align != ED_ALIGN_DEFAULT && m_align != ED_ALIGN_BASELINE 
                && m_align != ED_ALIGN_LEFT && m_align != ED_ALIGN_RIGHT ){
        pCur = PR_sprintf_append(pCur, "ALIGN=%s ", EDT_AlignString(m_align) );
    }
    
    pCur = PR_sprintf_append(pCur, ">");

    edt_AddTag( pRetTag, pEndTag, P_IMAGE, FALSE, pCur );
    pEndTag->edit_element = this;

    if( m_href != ED_LINK_ID_NONE ){
        edt_AddTag( pRetTag, pEndTag, P_ANCHOR, TRUE );
    }
    free( pCur );
    return pRetTag;
}

void CEditImageElement::PrintOpen( CPrintState *pPrintState ){
    if( m_href != ED_LINK_ID_NONE ){
        char *pS1 = "";
        char *pS2 = "";
        if( m_href->pExtra ){
            pS1 = " ";
            pS2 = m_href->pExtra;
        }
        pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( 
            "<A HREF=%s%s%s>", edt_MakeParamString(m_href->hrefStr), pS1, pS2 );
    }

    // Use special versions of params which has BORDER removed if it's the default

    char* pParams = NULL;
    {
        EDT_ImageData* pData = GetImageData();
        pParams = FormatParams(pData, TRUE);
        EDT_FreeImageData(pData);
    }

    if( SizeIsKnown() && ! m_bSizeIsBogus ){
        pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "<IMG %s", (pParams ? pParams : " ") );

        if( m_bHeightPercent ){
            pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "HEIGHT=%ld%%", (long)m_iHeight );
        } else {
            pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "HEIGHT=%ld", (long)m_iHeight );
        }
        if( m_bWidthPercent ){
            pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( " WIDTH=%ld%%", (long)m_iWidth );
        } else {
            pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( " WIDTH=%ld", (long)m_iWidth );
        }
    }
    else {
        pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "<IMG %s", 
                    (pParams ? pParams : " " ));
    }

    if ( pParams )
        XP_FREE(pParams);

    if( m_align != ED_ALIGN_DEFAULT && m_align != ED_ALIGN_BASELINE ){
        pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( " ALIGN=%s", 
                EDT_AlignString(m_align));
    }

    pPrintState->m_pOut->Write( ">", 1 );
    pPrintState->m_iCharPos++;
    
    if( m_href != ED_LINK_ID_NONE ){
        pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( 
            "</A>" );        
    }

}

static char *imageParams[] = {
    PARAM_ISMAP,
//    PARAM_USEMAP,     Now in extra
    PARAM_SRC,
    PARAM_LOWSRC,
    PARAM_NAME,
    PARAM_ALT,
    PARAM_WIDTH,
    PARAM_HEIGHT,
    PARAM_ALIGN,
    PARAM_HSPACE,
    PARAM_VSPACE,
    PARAM_BORDER,
    PARAM_ALIGN,
    PARAM_NOSAVE,
    0
};

EDT_ImageData* CEditImageElement::ParseParams( PA_Tag *pTag, int16 csid ){
    PA_Block buff;
    EDT_ImageData* pData = edt_NewImageData();

    buff = PA_FetchParamValue(pTag, PARAM_ISMAP, csid);
    if (buff != NULL){
        pData->bIsMap = TRUE;
        PA_FREE(buff);
    }
    buff = PA_FetchParamValue(pTag, PARAM_NOSAVE, csid);
    if (buff != NULL){
        pData->bNoSave = TRUE;
        PA_FREE(buff);
    }
//    pData->pUseMap = edt_FetchParamString( pTag, PARAM_USEMAP, csid ); now in pExtra
    pData->pSrc = edt_FetchParamString( pTag, PARAM_SRC, csid );
    pData->pLowSrc = edt_FetchParamString( pTag, PARAM_LOWSRC, csid );
    pData->pName = edt_FetchParamString( pTag, PARAM_NAME, csid );
    pData->pAlt = edt_FetchParamString( pTag, PARAM_ALT, csid );
    
    // Get width dimension from string and parse for "%" Default = 100%
    edt_FetchDimension( pTag, PARAM_WIDTH, 
                        &pData->iWidth, &pData->bWidthPercent,
                        0, FALSE, csid );
    edt_FetchDimension( pTag, PARAM_HEIGHT, 
                        &pData->iHeight, &pData->bHeightPercent,
                        0, FALSE, csid );
    m_bSizeWasGiven = pData->iWidth != 0 && pData->iHeight != 0;

    pData->align = edt_FetchParamAlignment( pTag, ED_ALIGN_BASELINE, FALSE, csid );

    pData->iHSpace = edt_FetchParamInt( pTag, PARAM_HSPACE, 0, csid );
    pData->iVSpace = edt_FetchParamInt( pTag, PARAM_VSPACE, 0, csid );
    pData->iBorder = edt_FetchParamInt( pTag, PARAM_BORDER, -1, csid );
    pData->pHREFData = 0;

    pData->pExtra = edt_FetchParamExtras( pTag, imageParams, csid );

    return pData;
}

EDT_ImageData* CEditImageElement::GetImageData(){
    EDT_ImageData *pRet;

    // LTNOTE: hackola time.  We don't want TagOpen to return multiple tags
    //  in this use, so we prevent it by faking out the href ID and restoring
    //  it when we are done.  Not real maintainable.

    ED_LinkId saveHref = m_href;
    m_href = ED_LINK_ID_NONE;
    PA_Tag* pTag = TagOpen(0);
    m_href = saveHref;

    pRet = ParseParams( pTag, GetWinCSID() );

    if ( m_bSizeIsBogus ) {
        pRet->iWidth = 0; pRet->bWidthPercent = FALSE;
        pRet->iHeight = 0; pRet->bHeightPercent = FALSE;
    }

    //CLM: We stored this when image was first loaded:
    int originalWidth = 0;
    int originalHeight = 0;
    if ( m_pLoImage ) {
        IL_GetNaturalDimensions( m_pLoImage->image_req, &originalWidth, &originalHeight);
    }
    pRet->iOriginalWidth = originalWidth;
    pRet->iOriginalHeight = originalHeight;
    
    pRet->bWidthPercent = m_bWidthPercent;
    pRet->bHeightPercent = m_bHeightPercent;
    
    if( m_href != ED_LINK_ID_NONE ){
        pRet->pHREFData = m_href->GetData();
    }
    pRet->align = m_align;
    PA_FreeTag( pTag );
    return pRet;
}

// border is weird.  If we are in a link the border is default 2, if 
//  we are not in a link, border is default 0.  In order not to foist this
//  weirdness on the user, we are always explicit on our image borders.
//
int32 CEditImageElement::GetDefaultBorder(){
    return ( m_href != ED_LINK_ID_NONE ) ? 2 : 0;
}

XP_Bool CEditImageElement::GetLOElementAndOffset( ElementOffset iEditOffset, XP_Bool /*bEditStickyAfter*/ ,
            LO_Element*& pRetElement,
            int& pLayoutOffset ){
    pLayoutOffset = 0;
    pRetElement = (LO_Element*)m_pLoImage;
    pLayoutOffset = (int) iEditOffset;
    return TRUE;
}

void CEditImageElement::SetImageData( EDT_ImageData *pData ){
    char* pCur = FormatParams(pData, FALSE);

    if( m_pParams ){
        XP_FREE( m_pParams );
    }
    m_pParams = pCur;

    m_align = pData->align;
    
    if (pData->iHeight != 0 && pData->iWidth != 0 ) {
        m_bSizeWasGiven = TRUE;
        m_bSizeIsBogus = FALSE;
        m_bHeightPercent = pData->bHeightPercent;
        m_bWidthPercent = pData->bWidthPercent;
    }
    else {
        m_bSizeWasGiven = FALSE;
        //CLM: We were given Width or Height = 0
        //     so we will get REAL dimensions
        // Clearing these willl allow us to retrieve actual dimensions
        //  on an image that had WIDTH and/or HEIGHT values on 1st load
        m_bHeightPercent = 0;
        m_bWidthPercent = 0;
    }
    m_iHeight = pData->iHeight;
    m_iWidth = pData->iWidth;
}

char* CEditImageElement::FormatParams(EDT_ImageData* pData, XP_Bool bForPrinting){
    char *pCur = 0;

    if( pData->bIsMap ){ 
        pCur = PR_sprintf_append( pCur, "ISMAP " );
    }

/*  USEMAP now in pExtra
    if( pData->pUseMap ){
        pCur = PR_sprintf_append( pCur, "USEMAP=%s ", edt_MakeParamString(pData->pUseMap) );
    }
*/

    if( pData->pSrc ){
        pCur = PR_sprintf_append( pCur, "SRC=%s ", edt_MakeParamString( pData->pSrc ) );
    }

    if( pData->pLowSrc ){
        pCur = PR_sprintf_append( pCur, "LOWSRC=%s ", edt_MakeParamString( pData->pLowSrc ) );
    }

    if( pData->pName ){
        pCur = PR_sprintf_append( pCur, "NAME=%s ", edt_MakeParamString( pData->pName ) );
    }

    if( pData->pAlt ){
        pCur = PR_sprintf_append( pCur, "ALT=%s ", edt_MakeParamString( pData->pAlt ) );
    }

    if( pData->iHSpace ){
        pCur = PR_sprintf_append( pCur, "HSPACE=%ld ", (long)pData->iHSpace );
    }

    if( pData->iVSpace ){
        pCur = PR_sprintf_append( pCur, "VSPACE=%ld ", (long)pData->iVSpace );
    }

    if( pData->bNoSave ){ 
        pCur = PR_sprintf_append( pCur, "NOSAVE " );
    }

    // border is weird.  If we are in a link the border is default 2, if 
    // we are not in a link, border is default 0.  When printing, we only write out a
    // border if it's different than the default.
    //
    // Other times, we always write it out because we want it to stay the same even if the
    // default border size changes. Whew!
    {
        if( !bForPrinting || ( pData->iBorder >=0 )){
            pCur = PR_sprintf_append( pCur, "BORDER=%ld ", (long)pData->iBorder );
        }
    }

    if( pData->pExtra  != 0 ){
        pCur = PR_sprintf_append( pCur, "%s ", pData->pExtra);
    }
    return pCur;
}

void CEditImageElement::FinishedLoad( CEditBuffer *pBuffer ){
    if( m_pLoImage ){
        if ( ! m_bSizeWasGiven || m_bSizeIsBogus ) {
            // The user didn't specify a size. Look at what we got.
            m_iHeight = pBuffer->m_pContext->convertPixY * m_pLoImage->height;
            m_iWidth = pBuffer->m_pContext->convertPixX * m_pLoImage->width;
        }
    }
}

void CEditImageElement::SetHREF( ED_LinkId id ){
    if( m_href != ED_LINK_ID_NONE ){
        m_href->Release();
    }
    m_href = id;
    if( id != ED_LINK_ID_NONE ){
        id->AddRef();
    }
}

XP_Bool CEditImageElement::SizeIsKnown() {
    return m_iHeight > 0;
}

void CEditImageElement::MaskData( EDT_CharacterData*& pData ){
    if( pData == 0 ){
        pData = GetCharacterData();
        return;
    }
    
    // An image can have an HREF text format
    ED_TextFormat tf = ( m_href == ED_LINK_ID_NONE ) ? 0 : TF_HREF;

    ED_TextFormat differences = pData->values ^ tf;
    if( differences ){
        pData->mask &= ~differences;
    }
    if( (pData->mask & pData->values & tf & TF_HREF)
                && (pData->linkId != m_href ) ){
        pData->mask &= ~TF_HREF;
    }
}

EDT_CharacterData* CEditImageElement::GetCharacterData(){
    EDT_CharacterData *pData = EDT_NewCharacterData();
    // We are only interested in HREF?
    // pData->mask = TF_HREF;
    pData->mask = -1;
    if( m_href != ED_LINK_ID_NONE ){
        pData->pHREFData = m_href->GetData();
        pData->values = TF_HREF;
    }
    pData->linkId = m_href;
    return pData;
}
//-----------------------------------------------------------------------------
// CEditHorizRuleElement
//-----------------------------------------------------------------------------


CEditHorizRuleElement::CEditHorizRuleElement( CEditElement *pParent, PA_Tag* pTag, int16 /*csid*/ ):
            CEditLeafElement( pParent, P_HRULE ), 
            m_pLoHorizRule(0)
            {

    if( pTag ){
        char *locked_buff;
                
        PA_LOCK(locked_buff, char *, pTag->data );
        if( locked_buff ){
            SetTagData( locked_buff );
        }
        PA_UNLOCK(pTag->data);
    }
}

CEditHorizRuleElement::CEditHorizRuleElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer):
            CEditLeafElement(pStreamIn, pBuffer), 
            m_pLoHorizRule(0)
            {
}

CEditHorizRuleElement::~CEditHorizRuleElement(){
    DisconnectLayoutElements((LO_Element*) m_pLoHorizRule);
}


LO_Element* CEditHorizRuleElement::GetLayoutElement(){
    return (LO_Element*)m_pLoHorizRule;
}

void CEditHorizRuleElement::SetLayoutElement( intn iEditOffset, intn lo_type, 
                        LO_Element* pLoElement ){
    SetLayoutElementHelper(LO_HRULE, (LO_Element**) &m_pLoHorizRule,
        iEditOffset, lo_type, pLoElement);
}

void CEditHorizRuleElement::ResetLayoutElement( intn iEditOffset, 
            LO_Element* pLoElement ){
    ResetLayoutElementHelper((LO_Element**) &m_pLoHorizRule,
        iEditOffset, pLoElement);
}

void CEditHorizRuleElement::StreamOut( IStreamOut *pOut ){
    CEditLeafElement::StreamOut( pOut );
}

EEditElementType CEditHorizRuleElement::GetElementType()
{
    return eHorizRuleElement;
}

XP_Bool CEditHorizRuleElement::GetLOElementAndOffset( ElementOffset iEditOffset, XP_Bool /* bEditStickyAfter */,
            LO_Element*& pRetElement,
            int& pLayoutOffset ){
    pRetElement = (LO_Element*)m_pLoHorizRule;
    pLayoutOffset = (int) iEditOffset;
    return TRUE;
}

// Property Getting and Setting Stuff.

EDT_HorizRuleData* CEditHorizRuleElement::NewData(){
    EDT_HorizRuleData *pData = XP_NEW( EDT_HorizRuleData );
    if( pData == 0 ){
        // throw();
        return pData;
    }
    pData->align = ED_ALIGN_CENTER;
    pData->size = DEFAULT_HR_THICKNESS;
    pData->bNoShade = 0;
    pData->iWidth = 100;
    pData->bWidthPercent = TRUE;
    pData->pExtra = 0;
    return pData;
}

void CEditHorizRuleElement::FreeData( EDT_HorizRuleData *pData ){
    if( pData->pExtra ) XP_FREE( pData->pExtra );
    XP_FREE( pData );
}

void CEditHorizRuleElement::SetData( EDT_HorizRuleData *pData ){
    char *pNew = 0;

    if( pData->align == ED_ALIGN_RIGHT || pData->align == ED_ALIGN_LEFT ){
        pNew = PR_sprintf_append( pNew, "ALIGN=%s ", EDT_AlignString(pData->align) );
    }
    if( pData->size != DEFAULT_HR_THICKNESS ){
        pNew = PR_sprintf_append( pNew, "SIZE=%ld ", (long)pData->size );
    }
    if( pData->bNoShade ){
        pNew = PR_sprintf_append( pNew, "NOSHADE " );
    }
    if( pData->iWidth ){
        if( pData->bWidthPercent ){
            pNew = PR_sprintf_append( pNew, "WIDTH=\"%ld%%\" ", 
                                      (long)min(100,max(1,pData->iWidth)) );
        }
        else {
            pNew = PR_sprintf_append( pNew, "WIDTH=\"%ld\" ", (long)pData->iWidth );
        }
    }
    if( pData->pExtra){
        pNew = PR_sprintf_append( pNew, "%s ", pData->pExtra );
    }

    pNew = PR_sprintf_append( pNew, ">" );

    SetTagData( pNew );        
    free(pNew);
}

EDT_HorizRuleData* CEditHorizRuleElement::GetData( ){
    EDT_HorizRuleData *pRet;
    PA_Tag* pTag = TagOpen(0);
    pRet = ParseParams( pTag, GetWinCSID() );
    PA_FreeTag( pTag );
    return pRet;
}

static char *hruleParams[] = {
    PARAM_ALIGN,
    PARAM_NOSHADE,
    PARAM_WIDTH,
    PARAM_SIZE,
    0
};

EDT_HorizRuleData* CEditHorizRuleElement::ParseParams( PA_Tag *pTag, int16 csid ){
    EDT_HorizRuleData *pData = NewData();
    ED_Alignment align;
    
    align = edt_FetchParamAlignment( pTag, ED_ALIGN_CENTER, FALSE, csid );
    if( align == ED_ALIGN_RIGHT || align == ED_ALIGN_LEFT ){
        pData->align = align;
    }
    // Get width dimension from string and parse for "%" Default = 100%
    edt_FetchDimension( pTag, PARAM_WIDTH, 
                        &pData->iWidth, &pData->bWidthPercent,
                        100, TRUE, csid );
    pData->bNoShade = edt_FetchParamBoolExist( pTag, PARAM_NOSHADE, csid );
    pData->size = edt_FetchParamInt( pTag, PARAM_SIZE, DEFAULT_HR_THICKNESS, csid );
    pData->pExtra = edt_FetchParamExtras( pTag, hruleParams, csid );
    return pData;
}

//-----------------------------------------------------------------------------
// CEditIconElement
//-----------------------------------------------------------------------------
static char *ppIconTags[] = {
    "align=absbotom border=0 src=\"internal-edit-named-anchor\" alt=\"%s\">",
    "align=absbotom border=0 src=\"internal-edit-form-element\">",
    "src=\"internal-edit-unsupported-tag\" alt=\"<%s\"",
    "src=\"internal-edit-unsupported-end-tag\" alt=\"</%s\"",
    "align=absbotom border=0 src=\"internal-edit-java\">",
    "align=absbotom border=0 src=\"internal-edit-plugin\">",
    0
};

CEditIconElement::CEditIconElement( CEditElement *pParent, int32 iconTag, 
                        PA_Tag* pTag, int16 /*csid*/ )
            :
                CEditLeafElement( pParent, P_IMAGE ), 
                m_originalTagType( pTag ? pTag->type : P_UNKNOWN ),
                m_iconTag( iconTag ),
                m_bEndTag( pTag ? pTag->is_end : 0 ),
                m_pSpoofData(0),
                m_pLoIcon(0),
                m_piSaveIndices(NULL)
            {

    if( pTag ){
        char *locked_buff;
                
        PA_LOCK(locked_buff, char *, pTag->data );
        if( locked_buff ){
            char *p = locked_buff;
            if( m_originalTagType != P_ANCHOR 
                    && m_originalTagType != P_UNKNOWN ){

                while( *p == ' ' ) p++;
                char *pSpace = " ";
                if( *p == '>' ) pSpace = "";

                char *pTagData = PR_smprintf("%s%s%s", 
                        EDT_TagString(m_originalTagType), pSpace, p );
                SetTagData( pTagData );
                SetTagData( pTag, pTagData );
                m_originalTagType = P_UNKNOWN;
                free( pTagData );
            }
            else {
                SetTagData( locked_buff );
            }
        }
        PA_UNLOCK(pTag->data);
        SetSpoofData( pTag );
    }
}

CEditIconElement::~CEditIconElement(){
    DisconnectLayoutElements((LO_Element*) m_pLoIcon);
    if( m_pSpoofData ){
        free( m_pSpoofData );
    }
    XP_FREEIF(m_piSaveIndices);
}

CEditIconElement::CEditIconElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer):
            CEditLeafElement(pStreamIn, pBuffer), 
            m_pLoIcon(0),
            m_piSaveIndices(NULL)
            {
    m_originalTagType = (TagType) pStreamIn->ReadInt();
    m_iconTag = pStreamIn->ReadInt();
    m_bEndTag = (XP_Bool) pStreamIn->ReadInt();
    m_pSpoofData = pStreamIn->ReadZString();
}

void CEditIconElement::SetLayoutElement( intn iEditOffset, intn lo_type, 
                        LO_Element* pLoElement ){
    SetLayoutElementHelper(LO_IMAGE, (LO_Element**) &m_pLoIcon,
        iEditOffset, lo_type, pLoElement);
}

void CEditIconElement::ResetLayoutElement( intn iEditOffset, 
            LO_Element* pLoElement ){
    ResetLayoutElementHelper((LO_Element**) &m_pLoIcon,
        iEditOffset, pLoElement);
}

void CEditIconElement::StreamOut( IStreamOut *pOut ){
    CEditLeafElement::StreamOut( pOut );
    pOut->WriteInt( m_originalTagType );
    pOut->WriteInt( m_iconTag );
    pOut->WriteInt( m_bEndTag );
    pOut->WriteZString( m_pSpoofData );
}

XP_Bool CEditIconElement::GetLOElementAndOffset( ElementOffset iEditOffset, XP_Bool /* bEditStickyAfter */,
            LO_Element*& pRetElement,
            int& pLayoutOffset ){
    pLayoutOffset = 0;
    pRetElement = (LO_Element*)m_pLoIcon;
    pLayoutOffset = iEditOffset;
    return TRUE;
}

PA_Tag* CEditIconElement::TagOpen( int /* iEditOffset */ ){
    PA_Tag *pTag = XP_NEW( PA_Tag );
    XP_BZERO( pTag, sizeof( PA_Tag ) );
    SetTagData( pTag, m_pSpoofData );
    return pTag;
}

#define IS_WHITE(p) ((p) == ' ' || (p) == '\r' || (p) == '\n')

PRIVATE XP_Bool IsScriptText2(char* pData, char* pName){
    char *p = pData;

    while( *p && IS_WHITE(*p) ){
        p++;
    }
    if ( !*p || *p != '<' ) {
        return FALSE;
    }

    p++;
    char *n = pName;

    while ( *p && * n && ! IS_WHITE(*p) && XP_TO_LOWER(*p) == XP_TO_LOWER(*n) ) {
        p++;
        n++;
    }
    if ( *n != 0 || (*p != '>' && ! IS_WHITE(*p)) ) {
        // We didn't match the tag!
        return FALSE;
    }
    // Go to the end of the string.
    while ( *p ) {
        p++;
    }
    p--;
    // Back up to the last '>'. Nothing but whitespace to last '>'
    while ( p > pData && (*p != '>' && IS_WHITE(*p) ) ) {
        --p;
    }
    if ( p <= pData || *p != '>' ) {
        return FALSE;
    }
    // Back-match to last '<'
    while ( *p && *p != '<' && p > pData ) {
        p--;
    }
    if ( p <= pData || *p != '<' ) {
        return FALSE;
    }
    if ( *++p != '/' ) {
        return FALSE;
    }
    if ( strcasestr(++p, pName) == NULL ) {
        return FALSE;
    }
    p += XP_STRLEN(pName);
    if ( *p != '>' && ! IS_WHITE(*p) ) {
        return FALSE;
    }
    return TRUE;
}

PRIVATE XP_Bool IsScriptText(char* pData) {
    // Return TRUE if pData is <SCRIPT>...</SCRIPT>, or <STYLE>...</STYLE>, or <SERVER>...</SERVER>
    return IsScriptText2(pData, "SCRIPT") || IsScriptText2(pData, "STYLE") || IsScriptText2(pData, "SERVER");
}
//
// Do some heuristic validation to make sure 
//
ED_TagValidateResult CEditIconElement::ValidateTag( char *pData, XP_Bool bNoBrackets ){
    if ( IsScriptText(pData) ) {
        return ED_TAG_OK;
    }
    char *p = pData;

    while( *p && IS_WHITE(*p) ){
        p++;
    }
    if( !bNoBrackets ){
        if( *p != '<' ){
            return ED_TAG_UNOPENED;
        }
        else {
            p++;
        }
        if( *p == '/' ){
            p++;
        }

        if( IS_WHITE(*p) ){
            return ED_TAG_TAGNAME_EXPECTED;
        }
    }

    // Is this a comment?
    XP_Bool isComment = p[0] == '!' && p[1] == '-' && p[2] == '-';

    if ( isComment ) {
        char* start = p;
        while( *p ){
            p++;
        }
        if ( ! bNoBrackets ) {
            --p;
        }

        while ( IS_WHITE(*p) && p > start ) {
            p--;
        }

        XP_Bool endComment = (p >= start + 6) && p[-2] == '-' && p[-1] == '-';
        if ( ! endComment ) {
            return ED_TAG_PREMATURE_CLOSE; // ToDo: Use a specific error message
        }
    }
    else {
        // look for unterminated strings.
        while( *p && *p != '>' ){
            if( *p == '"' || *p == '\'' ){
                char quote = *p++;
                while( *p && *p != quote ){
                    p++;
                }
                if( *p == 0 ){
                    return ED_TAG_UNTERMINATED_STRING;
                }
            }
            p++;
        }
    }
    if( bNoBrackets ){
        if( *p == 0 ){
            return ED_TAG_OK;
        }
        else {
            XP_ASSERT( *p == '>' );
            return ED_TAG_PREMATURE_CLOSE;
        }
    }   
    else if( *p == '>' ){
        p++;
        while( IS_WHITE( *p ) ){
            p++;
        }
        if( *p != 0 ){
            return ED_TAG_PREMATURE_CLOSE;
        }
        else {
            return ED_TAG_OK;
        }
    }
    else {
        XP_ASSERT( *p == 0 );
        return ED_TAG_UNCLOSED;
    }
}

//
// Restore the original tag type so we save the right tag type when we output
//  we print.
//
void CEditIconElement::PrintOpen( CPrintState *pPrintState ){
    if( m_originalTagType != P_UNKNOWN ){
        TagType tSave = GetType();
        SetType( m_originalTagType );
        CEditLeafElement::PrintOpen( pPrintState );
        SetType( tSave );
    }
    else {
        if( m_bEndTag ){
            pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "</%s", 
                GetTagData());
        }
        else {
            pPrintState->m_iCharPos += pPrintState->m_pOut->Printf( "<%s", 
                GetTagData());
        }
    }
}

void CEditIconElement::PrintEnd( CPrintState *pPrintState ){
    if( m_originalTagType != P_UNKNOWN ){
        TagType tSave = GetType();
        SetType( m_originalTagType );
        CEditLeafElement::PrintEnd( pPrintState );
        SetType( tSave );
    }
}

char* CEditIconElement::GetData(){
    int iLen = XP_STRLEN( GetTagData() );
    char *p = (char*)XP_ALLOC( iLen+3 );
    char *pRet = p;
    *p++ = '<';
    if( m_bEndTag ){
        *p++ = '/';
    }
    XP_STRCPY( p, GetTagData() );
    return pRet;
}


PRIVATE
void edt_addToStringLists(char **list1,char *start1,char *end1,
                          char **list2,char *start2,char *end2,
                          int index) {
  int len1 = end1 - start1; 
  int len2 = end2 - start2;

  // Don't allow empty strings.
  if ((len1 > 0) && (len2 > 0)) {
    // space for trailing NULL.
    list1[index] = (char *)XP_ALLOC(len1 + 1);
    list2[index] = (char *)XP_ALLOC(len2 + 1);
    if (!list1[index] || !list2[index]) {
      XP_ASSERT(0);
      return;
    }
    // copy strings.
    XP_MEMCPY(list1[index],start1,len1);
    list1[index][len1] = '\0';
    XP_MEMCPY(list2[index],start2,len2);
    list2[index][len2] = '\0';
  }
}

int CEditIconElement::ParseLocalData(char ***mimeTypes,char ***URLs) {
  int retVal = 0;      
  *mimeTypes = NULL;
  *URLs = NULL;

  // Don't want the version with the spoofData, so call ancestor.
  PA_Tag *pTag = CEditElement::TagOpen(0);

  if (pTag) {
    // Don't use edt_FetchParamString because it strips out interior 
    // white space.
    PA_Block buff = PA_FetchParamValue(pTag,PARAM_LOCALDATA,GetWinCSID());
    if (buff) {
      char *pLocalData;
      PA_LOCK(pLocalData,char *,buff);

      // maxFiles = 1+number of '+' characters in pLocalData,
      // gives upper bound on size of returned lists.
      int maxFiles = 1;
      char *p = pLocalData;
      while (*p && (p = XP_STRCHR(p,'+')) != NULL) {
        maxFiles++;
        p++; // move past found '+'
      }

      // Create lists.
      *mimeTypes = (char **)XP_ALLOC(maxFiles * sizeof(char *));
      if (!*mimeTypes) {
        return 0;
      }
      *URLs = (char **)XP_ALLOC(maxFiles * sizeof(char *));
      if (!*URLs) {
        XP_FREEIF(*mimeTypes);  // Will zero *mimeTypes.
        return 0;
      }


      // reset p.
      p = pLocalData;

      char *mimeStart,*mimeEnd,*URLStart,*URLEnd;

      // State machine to read in string
      int state = BeforeMIME;
      while(*p) {
        switch (state) {
          case BeforeMIME:
            if (!XP_IS_SPACE(*p)) {
              state = InMIME;
              mimeStart = p;
            }
          break;
          case InMIME:
            if (XP_IS_SPACE(*p)) {
              mimeEnd = p;
              state = BeforeURL;
            }
          break;
          case BeforeURL:
            if (!XP_IS_SPACE(*p)) {
              URLStart = p;
              state = InURL;
            }
          break;
          case InURL:
            if (XP_IS_SPACE(*p)) {
              URLEnd = p;
              XP_ASSERT(retVal < maxFiles);
              edt_addToStringLists(*mimeTypes,mimeStart,mimeEnd,
                                   *URLs,URLStart,URLEnd,
                                   retVal);
              retVal++;
              state = AfterURL;
            }
            if (*p == '+') {
              URLEnd = p;
              XP_ASSERT(retVal < maxFiles);
              edt_addToStringLists(*mimeTypes,mimeStart,mimeEnd,
                                   *URLs,URLStart,URLEnd,
                                   retVal);
              retVal++;
              state = BeforeMIME;
            }
          break;
          case AfterURL:
            if (*p == '+') {
              state = BeforeMIME;
            }
          break;
          default:
            XP_ASSERT(0);
          break;
        }

        // Move forward.
        p++;
      }

      // Close out final mime/URL pair.
      if (state == InURL) {
        URLEnd = p;
        XP_ASSERT(retVal < maxFiles);
        edt_addToStringLists(*mimeTypes,mimeStart,mimeEnd,
                             *URLs,URLStart,URLEnd,
                             retVal);
        retVal++;
      }


      PA_UNLOCK(buff);
      PA_FREE(buff);
    }
    PA_FreeTag(pTag);
  }

  return retVal;
}

void CEditIconElement::FreeLocalDataLists(char **mimeTypes,char **URLs,int count) {
  for (int n = 0; n < count; n++) {
    XP_FREEIF(mimeTypes[n]);
    XP_FREEIF(URLs[n]);
  }
  XP_FREEIF(mimeTypes);
  XP_FREEIF(URLs);
}

void CEditIconElement::ReplaceParamValues(char *pOld,char *pNew) {
  // Not changing anything.
  if (!XP_STRCMP(pOld,pNew)) return;
  
  // Illegal.
  if (!pOld || !pNew || pOld[0] == '=' || pOld[0] == '\"') return;

  char *pTagData = GetTagData();
  if (!pTagData) return;

  int state = OutsideValue;
  char *last = pTagData; // last part copied to pNewTagData.
  char *scan = pTagData; // Scans ahead of last.
  char *pNewTagData = NULL;
  int pOldLen = XP_STRLEN(pOld);

  StrAllocCat(pNewTagData,"<");

  while (*scan) {
    XP_ASSERT(scan);

    switch (state) {
      case OutsideValue: 
        if (*scan == '=') {
          state = BeforeValue;
        }
        break;
      case InsideValue:
        if (XP_IS_SPACE(*scan)) {
          // end of non-quoted value.
          state = OutsideValue;
        }
        break;
      case InsideValueQuote:
        if (*scan == '\"') {
          // closing quote.
          state = OutsideValue;
        }
        break;
      case BeforeValue:
        if (*scan == '\"') {
          state = InsideValueQuote;
        }
        else if (!XP_IS_SPACE(*scan)) {
          state = InsideValue;
        }
        // else still BeforeValue
        break;
      default:
        XP_ASSERT(0);
    }

    if (state == InsideValue || state == InsideValueQuote) {
      // Found an instance.
      if (!XP_STRNCMP(scan,pOld,pOldLen)) {
        // flush last part.
        if (last != scan) {
          char store = *scan;
          *scan = '\0';
          StrAllocCat(pNewTagData,last);
          *scan = store;
        }
        StrAllocCat(pNewTagData,pNew);
        scan += pOldLen; // skip past prev string.
        last = scan;

        scan--; // To counteract following scan++.
      }
    }

    scan++;
  }
  
  // flush last part.
  if (last != scan) {
    StrAllocCat(pNewTagData,last);
    last = scan;
  }

  SetData(pNewTagData);  
  XP_FREEIF(pNewTagData);
}


PRIVATE
XP_Bool IsHTMLCommentTag(char* string) {
    return string && string[0] == '!'
        && string[1] == '-'
        && string[2] == '-';
}

void CEditIconElement::SetSpoofData( PA_Tag* pTag ){
    
    if( m_iconTag == EDT_ICON_UNSUPPORTED_TAG 
            || m_iconTag == EDT_ICON_UNSUPPORTED_END_TAG ){

        char *pTagString;
        PA_LOCK(pTagString, char *, pTag->data );
    
        char *pData = PR_smprintf( ppIconTags[m_iconTag], 
                    edt_QuoteString( pTagString ));
        
        XP_Bool bDimensionFound = FALSE;
        XP_Bool bComment = m_originalTagType == P_UNKNOWN &&
            IsHTMLCommentTag(pTagString);

        if ( ! bComment ) {
            int32 height;
            int32 width;
            XP_Bool bPercent;
        
            edt_FetchDimension( pTag, PARAM_HEIGHT, &height, &bPercent, -1, TRUE, GetWinCSID() );
            if( height != -1 ){
                pData = PR_sprintf_append( pData, " HEIGHT=%d%s", height, 
                            (bPercent ? "%" : "" ) );
                bDimensionFound = TRUE;
            }

            edt_FetchDimension( pTag, PARAM_WIDTH, &width, &bPercent, -1, TRUE, GetWinCSID() );
            if( width != -1 ){
                pData = PR_sprintf_append( pData, " WIDTH=%d%s", width, 
                        (bPercent ? "%" : "" ) );
                bDimensionFound = TRUE;
            }
        }

        if( bDimensionFound ){
            pData = PR_sprintf_append(pData, " BORDER=2>");
        }
        else {
            pData = PR_sprintf_append(pData, " BORDER=0 ALIGN=ABSBOTTOM>");
        }

        if( m_pSpoofData ){
            free( m_pSpoofData );
        }
        m_pSpoofData = pData;
        PA_UNLOCK(pTag->data);
    }
    else {
        m_pSpoofData = XP_STRDUP( ppIconTags[m_iconTag] );
    }

}

void CEditIconElement::SetSpoofData( char* pData ){
    char *pNewData = PR_smprintf( ppIconTags[m_iconTag], 
                edt_QuoteString( pData ));
    if( m_pSpoofData ){
        XP_FREE( m_pSpoofData );
    }
    m_pSpoofData = pNewData;
}

void CEditIconElement::SetData( char *pData ){
    // Since validater skips white space, so should SetData
    while( *pData && IS_WHITE(*pData) ){
        pData++;
    }

    // If you assert here, you aren't validating your tag.
    if( pData[0] != '<' ) {
        XP_ASSERT(FALSE);
        return;
    }

    pData++;

    if( *pData == '/' ){
        pData++;
        m_bEndTag = TRUE;
    }
    SetTagData( pData );


    // Build up a tag that we can fetch parameter strings from.
    PA_Tag *pTag = XP_NEW( PA_Tag );
    XP_BZERO( pTag, sizeof( PA_Tag ) );
    SetTagData( pTag, pData );
    pTag->is_end = m_bEndTag;

    SetSpoofData( pTag );
    PA_FreeTag( pTag );
}

//
// We need to convert a tag handed to us by the parser into a tag we can
//  display in the editor.  In order to do this, we 'morph' the tag by swaping
//  the contents with desired tag.  We need an additional tag for swap space.
//  All this is done to insure that the memory location of 'pTag' doesn't 
//  change.  Perhaps this routine should be implemented at the CEditElement
//  layer.
//             
void CEditIconElement::MorphTag( PA_Tag *pTag ){
    PA_Tag *pNewTag = TagOpen(0);
    PA_Tag tempTag;

    tempTag = *pTag;
    *pTag = *pNewTag;
    *pNewTag = tempTag;

    PA_FreeTag( pNewTag );
}             
   
XP_Bool CEditIconElement::IsUnknownHTML(){
    return m_iconTag == EDT_ICON_UNSUPPORTED_TAG
        || m_iconTag == EDT_ICON_UNSUPPORTED_END_TAG;
}

XP_Bool CEditIconElement::IsComment(){
    return IsUnknownHTML() && IsHTMLCommentTag(GetTagData());
}

XP_Bool CEditIconElement::IsComment(char* prefix){
    if ( ! IsComment() ) return FALSE;
    char* pData = GetTagData();
    if ( ! prefix || ! pData ) return FALSE;
    int32 prefixLen = XP_STRLEN(prefix);
    int32 dataLen = XP_STRLEN(pData);
    // Skip past "!--"
    if ( dataLen <= 3 ) return FALSE;
    dataLen -= 3;
    pData += 3;
    // Skip whitespace
    while ( dataLen > 0 && *pData == ' ' ){
        pData++;
        dataLen--;
    }
    if ( prefixLen > dataLen ) return FALSE;
    if ( XP_STRNCMP(prefix, pData, prefixLen) == 0 ){
        return TRUE;
    }
    return FALSE;
}

void CEditIconElement::SetSize(XP_Bool bWidthPercent, int32 iWidth,
                               XP_Bool bHeightPercent, int32 iHeight){
    
    // Do NOT use TagOpen -- this  gets the "m_pSpoofData" junk
    PA_Tag *pTag = XP_NEW( PA_Tag );
    XP_BZERO( pTag, sizeof( PA_Tag ) );
    // This string has initial "<"
    SetTagData( pTag, GetData() );

    if( pTag ){    
        char *pWidth = 0;
        char *pHeight = 0;
        if( iWidth > 0 ){
            pWidth = PR_sprintf_append( pWidth, "%ld%s", (long)iWidth, bWidthPercent ? "%%" : "" );
            if( pWidth ){
                edt_ReplaceParamValue( pTag, "WIDTH", pWidth, GetWinCSID());
            }
        }
        if( iHeight > 0){
            pHeight = PR_sprintf_append( pHeight, "%ld%s", (long)iHeight, bHeightPercent ? "%%" : "" );
            if( pHeight ){
                edt_ReplaceParamValue( pTag, "HEIGHT", pHeight, GetWinCSID());
            }
        }
        if( pWidth || pHeight ){
            SetData((char*)pTag->data);
            if( pWidth ) XP_FREE(pWidth);
            if( pHeight ) XP_FREE(pHeight);
        }
        PA_FREE(pTag);
    }
}


//-----------------------------------------------------------------------------
// CEditTargetElement
//-----------------------------------------------------------------------------
CEditTargetElement::CEditTargetElement(CEditElement *pParent, PA_Tag* pTag, int16 csid)
        :
            CEditIconElement( pParent, EDT_ICON_NAMED_ANCHOR,  pTag )
        {
        m_originalTagType = P_ANCHOR;
        // To get the spoof tag set up correctly
        EDT_TargetData* pData = GetData(csid);
        SetData( pData );
        FreeTargetData(pData);
}

CEditTargetElement::CEditTargetElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer)
        :
            CEditIconElement( pStreamIn, pBuffer )
        {
}

void CEditTargetElement::StreamOut( IStreamOut *pOut){
    CEditIconElement::StreamOut( pOut );
}

PA_Tag* CEditTargetElement::TagOpen( int iEditOffset ){
    // We need to create the actual anchor tag set.
    return CEditIconElement::TagOpen( iEditOffset );
}

void CEditTargetElement::SetName( char* pName, int16 csid ){
    if ( pName ) {
        EDT_TargetData* pData = GetData(csid);
        if ( pData ) {
            if ( pData->pName ) {
                XP_FREE(pData->pName);
            }
            pData->pName = XP_STRDUP(pName);
        }
        SetData(pData);
        FreeTargetData(pData);
    }
}

void CEditTargetElement::SetData( EDT_TargetData *pData ){
    char *pNew = 0;
    if ( pData->pName) {
        pNew = PR_sprintf_append( pNew, "NAME=%s ", edt_MakeParamString(pData->pName));
    }

    if( pData->pExtra  ){
        pNew = PR_sprintf_append( pNew, "%s ", pData->pExtra );
    }

    if( pNew ){
        pNew = PR_sprintf_append( pNew, ">" );
    }
    SetTagData( pNew );
    if ( pNew ) {
        free(pNew);
    }
    // Set the spoof data for the icon.
    CEditIconElement::SetSpoofData( pData->pName ? pData->pName : "" );
}

char* CEditTargetElement::GetName(){
    char* pName = 0;
    EDT_TargetData* pData = GetData();
    if (pData && pData->pName) {
        pName = XP_STRDUP(pData->pName);
    }
    FreeTargetData(pData);
    return pName;
}

EDT_TargetData* CEditTargetElement::GetData(){
    return GetData(GetWinCSID());
}

EDT_TargetData* CEditTargetElement::GetData(int16 csid){
    EDT_TargetData *pRet;
    // Get the actual tag data, not the faked up stuff at the Icon layer.
    PA_Tag* pTag = CEditLeafElement::TagOpen(0);
    pRet = ParseParams( pTag, csid );
    PA_FreeTag( pTag );
    return pRet;
}


static char *targetParams[] = {
    PARAM_NAME,
    0
};

EDT_TargetData* CEditTargetElement::ParseParams( PA_Tag *pTag, int16 csid ){
    EDT_TargetData* pData = NewTargetData();
    pData->pName = edt_FetchParamString( pTag, PARAM_NAME, csid );
    pData->pExtra = edt_FetchParamExtras( pTag, targetParams, csid );

    return pData;
}

EDT_TargetData* CEditTargetElement::NewTargetData(){
    EDT_TargetData* pData = (EDT_TargetData*) XP_ALLOC(sizeof(EDT_TargetData));
    if ( pData ) {
        pData->pName = 0;
        pData->pExtra = 0;
    }
    return pData;
}

void CEditTargetElement::FreeTargetData(EDT_TargetData* pData){
    if ( pData ) {
        if ( pData->pName ) {
            XP_FREE(pData->pName);
        }
        if ( pData->pExtra ) {
            XP_FREE(pData->pExtra);
        }
        XP_FREE(pData);
    }
}

//-----------------------------------------------------------------------------
// CEditBreakElement
//-----------------------------------------------------------------------------

CEditBreakElement::CEditBreakElement( CEditElement *pParent, PA_Tag* pTag, int16 /*csid*/ ):
    CEditLeafElement( pParent, P_LINEBREAK ),
    m_pLoLinefeed(0)
{
    if( pTag ){
        char *locked_buff;
                
        PA_LOCK(locked_buff, char *, pTag->data );
        if( locked_buff ){
            SetTagData( locked_buff );
        }
        PA_UNLOCK(pTag->data);
    }
}

CEditBreakElement::CEditBreakElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer):
    CEditLeafElement(pStreamIn, pBuffer),
    m_pLoLinefeed(0)
{
}

CEditBreakElement::~CEditBreakElement(){
    DisconnectLayoutElements((LO_Element*) m_pLoLinefeed);
}


void CEditBreakElement::StreamOut( IStreamOut *pOut){
    CEditLeafElement::StreamOut( pOut );
}

EEditElementType CEditBreakElement::GetElementType(){
    return eBreakElement;
}

void CEditBreakElement::SetLayoutElement( intn iEditOffset, intn lo_type, 
                        LO_Element* pLoElement ){
    SetLayoutElementHelper(LO_LINEFEED, (LO_Element**) &m_pLoLinefeed,
        iEditOffset, lo_type, pLoElement);
}

void CEditBreakElement::ResetLayoutElement( intn iEditOffset, 
            LO_Element* pLoElement ){
    ResetLayoutElementHelper((LO_Element**) &m_pLoLinefeed,
        iEditOffset, pLoElement);
}

LO_Element* CEditBreakElement::GetLayoutElement(){
    return (LO_Element*)m_pLoLinefeed;
}

XP_Bool CEditBreakElement::GetLOElementAndOffset( ElementOffset iEditOffset, XP_Bool /* bStickyAfter */,
            LO_Element*& pRetElement, 
            int& pLayoutOffset ){ 
    pLayoutOffset = 0; 
    pRetElement = (LO_Element*)m_pLoLinefeed;
    pLayoutOffset = iEditOffset;
    return TRUE;
}

//
// if we are within PREFORMAT (or the like) breaks are returns.
//
void CEditBreakElement::PrintOpen( CPrintState *ps ){
    CEditElement *pPrev = PreviousLeafInContainer();
    while( pPrev && pPrev->IsA(P_LINEBREAK) ){
        pPrev = pPrev->PreviousLeafInContainer();
    }
    CEditElement *pNext = LeafInContainerAfter();
    while( pNext && pNext->IsA(P_LINEBREAK) ){
        pNext = pNext->LeafInContainerAfter();
    }
#ifdef USE_SCRIPT
    XP_Bool bScriptBefore = pPrev 
                && pPrev->IsA(P_TEXT) 
                && (pPrev->Text()->m_tf & (TF_SERVER|TF_SCRIPT|TF_STYLE));
    XP_Bool bScriptAfter = pNext && pNext->IsA(P_TEXT) && (pNext->Text()->m_tf & (TF_SERVER|TF_SCRIPT|TF_STYLE));
    
    if ( !bScriptBefore && bScriptAfter ){
        // Don't print <BR>s before <SCRIPT> tags. This swallows both legal <BR> tags,
        // and also swallows '\n' at the start of scripts. If we didn't do this,
        // then we would turn <SCRIPT>\nFoo into <BR><SCRIPT>Foo.
        // The case of a legitimate <BR> before a <SCRIPT> tag is much rarer than the
        // case of a \n just after the <SCRIPT> tag. So we err on the side that lets
        // more pages work.
        // Take this code out when text styles are extended to all leaf elements.
        return;
    }
#endif
#ifdef USE_SCRIPT
    if( InFormattedText() 
            || bScriptBefore ){
#else
    if( InFormattedText() ){
#endif
        ps->m_pOut->Write( "\n", 1 );
        ps->m_iCharPos = 0;
    }
    else {
        CEditLeafElement::PrintOpen( ps );
        // if we are the last break in a container, it is ingnored by the 
        //  browser, so emit another one.
        if( GetNextSibling() == 0 ){
            ps->m_pOut->Write( "<BR>", 4 );
        }
    }
}

CEditInternalAnchorElement::CEditInternalAnchorElement(CEditElement *pParent)
: CEditLeafElement(pParent, P_UNKNOWN)
{
}

CEditInternalAnchorElement::~CEditInternalAnchorElement()
{
}

XP_Bool CEditInternalAnchorElement::Reduce( CEditBuffer* /* pBuffer */ ){
    return FALSE;
}

XP_Bool CEditInternalAnchorElement::ShouldStreamSelf( CEditSelection& /*local*/, CEditSelection& /*selection*/){
    return FALSE;
}

void CEditInternalAnchorElement::StreamOut( IStreamOut * /*pOut*/){
    XP_ASSERT(FALSE);
}

void CEditInternalAnchorElement::SetLayoutElement( intn /*iEditOffset*/, intn /*lo_type*/, 
                LO_Element* /*pLoElement*/ ){
        XP_ASSERT(FALSE);
}

void CEditInternalAnchorElement::ResetLayoutElement( intn /*iEditOffset*/, 
            LO_Element* /*pLoElement*/ ){
}

LO_Element* CEditInternalAnchorElement::GetLayoutElement(){
    return NULL;
}

XP_Bool CEditInternalAnchorElement::GetLOElementAndOffset( ElementOffset /*iEditOffset*/, XP_Bool /*bEditStickyAfter*/,
            LO_Element*& /*pRetElement*/, 
            int& /*pLayoutOffset*/ ){
    return FALSE;
}

EEditElementType CEditInternalAnchorElement::GetElementType() {
    return eInternalAnchorElement;
}

void CEditInternalAnchorElement::PrintOpen( CPrintState * /*pPrintState*/ ){
}

void CEditInternalAnchorElement::PrintEnd( CPrintState * /*pPrintState*/ ){
}

//---------------------------------------------------------------------------
// CEditEndContainerElement
//---------------------------------------------------------------------------

CEditEndContainerElement::CEditEndContainerElement(CEditElement *pParent) :
        CEditContainerElement(pParent, NULL, 0 /* Never used for an end container */, ED_ALIGN_LEFT)
        {
}

void CEditEndContainerElement::StreamOut( IStreamOut * /*pOut*/ ) {
    XP_ASSERT(FALSE);
}

XP_Bool CEditEndContainerElement::ShouldStreamSelf( CEditSelection& /* local */, CEditSelection& /* selection */ ) {
    return FALSE;
}
XP_Bool CEditEndContainerElement::IsAcceptableChild(CEditElement& pChild){
    return pChild.GetElementType() == eEndElement;
}
void CEditEndContainerElement::PrintOpen( CPrintState * /* pPrintState */ ){
}
void CEditEndContainerElement::PrintEnd( CPrintState * /* pPrintState */ ){
}
XP_Bool CEditEndContainerElement::IsEndContainer() {
    return TRUE;
}
void CEditEndContainerElement::AdjustContainers( CEditBuffer* /* pBuffer */ ){
}

#endif //EDITOR
