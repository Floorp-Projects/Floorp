/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 *   Ashish Bhatt <ashishbhatt@netscape.com> 
 */

// File Overview....
//
// Test cases for the nsISelection Interface

#include "stdafx.h"
#include "QaUtils.h"
#include <stdio.h>
#include "Selection.h"

CSelection::CSelection(nsIWebBrowser* mWebBrowser)
{
	qaWebBrowser = mWebBrowser;
}


CSelection::~CSelection()
{
}


void CSelection::OnStartTests(UINT nMenuID)
{
	// Calls  all or indivdual test cases on the basis of the 
	// option selected from menu.

	switch(nMenuID)
	{
		case ID_INTERFACES_NSISELECTION_RUNALLTESTS	:
			RunAllTests();
			break ;
		case ID_INTERFACES_NSISELECTION_GETANCHORNODE :
			GetAnchorNode();
			break ;
		case ID_INTERFACES_NSISELECTION_GETANCHOROFFSET :
			GetAnchorOffset();
			break ;
		case ID_INTERFACES_NSISELECTION_GETFOCUSNODE :
			 GetFocusNode() ;
			break ;
		case ID_INTERFACES_NSISELECTION_GETFOCUSOFFSET :
			GetFocusOffset() ;
			break ;
		case ID_INTERFACES_NSISELECTION_GETISCOLLAPSED :
			GetIsCollapsed() ;
			break ;
		case ID_INTERFACES_NSISELECTION_GETRANGECOUNT :
			GetRangecount() ;
			break ;
		case ID_INTERFACES_NSISELECTION_GETRANGEAT :
			GetRangeAt() ;
			break ;
		case ID_INTERFACES_NSISELECTION_COLLAPSE :
			Collapse() ;
			break ;
		case ID_INTERFACES_NSISELECTION_EXTEND :
			Extend() ;
			break ;
		case ID_INTERFACES_NSISELECTION_COLLAPSETOSTART :
			CollapseToStart() ;
			break ;
		case ID_INTERFACES_NSISELECTION_COLLAPSETOEND :
			CollapseToEnd() ;
			break ;
		case ID_INTERFACES_NSISELECTION_CONTAINSNODE :
			ContainsNode() ;
			break ;
		case ID_INTERFACES_NSISELECTION_SELECTALLCHILDREN :
			SelectAllChildren();
			break ;
		case ID_INTERFACES_NSISELECTION_ADDRANGE :
			Addrange();
			break ;
		case ID_INTERFACES_NSISELECTION_REMOVERANGE :
			RemoveRange();
			break ;
		case ID_INTERFACES_NSISELECTION_REMOVEALLRANGES :
			RemoveAllRanges();
			break ;
		case ID_INTERFACES_NSISELECTION_DELETEFROMDOCUMENT :
			DeleteFromDocument();
			break ;
		case ID_INTERFACES_NSISELECTION_SELECTIONLANGUAGECHANGE :
			SelectionLanguageChange();
			break ;
		case ID_INTERFACES_NSISELECTION_TOSTRING :
			ToString();
			break ;
	}

}

void CSelection::RunAllTests()
{

	PRInt32 ancoroffset = 0;
	PRInt32 focusoffset = 0;
	PRInt32 rangecount = 0;
	PRInt32 index = 0;
	PRBool  bisCollapsed = false;
	PRBool  bContains = false;
	nsXPIDLString szText;
	
	nsCOMPtr<nsISelection> oNsSelection ;
	nsCOMPtr<nsIDOMNode> oNsDOMAnchorNode ;
	nsCOMPtr<nsIDOMNode> oNsDOMFocusNode ;
	nsCOMPtr<nsIDOMRange> oNsDOMRange ;

	oNsSelection = GetSelectionObject(true);
	if (!oNsSelection)
		return ;

//---------------- Test case for AnchorNode Attribute

	rv = oNsSelection->GetAnchorNode(getter_AddRefs(oNsDOMAnchorNode));
    RvTestResultDlg(rv, "nsISelection::GetAnchorNode() ");
	if (!oNsDOMAnchorNode)
		RvTestResultDlg(rv, "------>No Selection made to get the AnchorNode object");

//---------------- Test case for AnchorOffset Attribute

	rv = oNsSelection->GetAnchorOffset(&ancoroffset);
	RvTestResultDlg(rv, "nsISelection::GetAnchorOffset()");


//---------------- Test case for FocusNode Attribute

	rv = oNsSelection->GetFocusNode(getter_AddRefs(oNsDOMFocusNode));
    RvTestResultDlg(rv, "nsISelection::GetFocusNode() ");
	if (!oNsDOMFocusNode)
		RvTestResultDlg(rv, "------>No Selection made to get the FocusNode object");

//---------------- Test case for FocusOffset Attribute

	rv = oNsSelection->GetFocusOffset(&focusoffset);
	RvTestResultDlg(rv, "nsISelection::GetFocusOffset()");	

//---------------- Test case for isCollapsed Attribute

	rv = oNsSelection->GetIsCollapsed(&bisCollapsed);
	RvTestResultDlg(rv, "nsISelection::GetIsCollapsed()");	
	if(bisCollapsed)
		RvTestResultDlg(rv, "------> Returned value for IsCollapsed is true");	
	else
		RvTestResultDlg(rv, "------> Returned value for IsCollapsed is false");	

	
//---------------- Test case for isCollapsed Attribute

	rv = oNsSelection->GetRangeCount(&rangecount);
	RvTestResultDlg(rv, "nsISelection::GetRangeCount()");	


//---------------- Test case for GetRangeAt function

		rv = oNsSelection->GetRangeAt(index,getter_AddRefs(oNsDOMRange));
		RvTestResultDlg(rv, "nsISelection::GetRangeAt() ");
		if (!oNsDOMRange)
			RvTestResultDlg(rv, "------>Not able to get nsIDOMRange object");


//---------------- Test case for Collapse function

	rv = oNsSelection->Collapse(oNsDOMFocusNode,1) ;
	    RvTestResultDlg(rv, "nsISelection::Collapse() ");

//---------------- Test case for Extend function

	rv = oNsSelection->Extend(oNsDOMFocusNode,10) ;
	    RvTestResultDlg(rv, "nsISelection::Extend() ");
	
//---------------- Test case for CollapseToStart function

	rv = oNsSelection->CollapseToStart();
	    RvTestResultDlg(rv, "nsISelection::CollapseToStart() ");

//---------------- Test case for CollapseToEnd function

	rv = oNsSelection->CollapseToEnd();
	    RvTestResultDlg(rv, "nsISelection::CollapseToEnd() ");

//---------------- Test case for ContainsNode function

	rv = oNsSelection->ContainsNode(oNsDOMFocusNode,PR_TRUE, &bContains);
	    RvTestResultDlg(rv, "nsISelection::ContainsNode() ");
	if(bContains)
		RvTestResultDlg(rv, "------> Returned value for ContainsNode is true");	
	else
		RvTestResultDlg(rv, "------> Returned value for ContainsNode is false");


//---------------- Test case for SelectAllChildren function

	rv = oNsSelection->SelectAllChildren(oNsDOMFocusNode);
	    RvTestResultDlg(rv, "nsISelection::SelectAllChildren() ");

//---------------- Test case for Addrange function

	rv = oNsSelection->AddRange(oNsDOMRange);
	    RvTestResultDlg(rv, "nsISelection::Addrange() ");

//---------------- Test case for RemoveRange function

	rv = oNsSelection->RemoveRange(oNsDOMRange);
	    RvTestResultDlg(rv, "nsISelection::RemoveRange() ");

//---------------- Test case for RemoveAllRanges function

	rv = oNsSelection->RemoveAllRanges();
	    RvTestResultDlg(rv, "nsISelection::RemoveAllRanges() ");

//---------------- Test case for DeleteFromDocument function

	rv = oNsSelection->DeleteFromDocument();
	    RvTestResultDlg(rv, "nsISelection::DeleteFromDocument() ");

//---------------- Test case for SelectionLanguageChange function

	rv = oNsSelection->SelectionLanguageChange(PR_FALSE);
	    RvTestResultDlg(rv, "nsISelection::SelectionLanguageChange() ");

//---------------- Test case for ToString function

	rv = oNsSelection->ToString(getter_Copies(szText));
	    RvTestResultDlg(rv, "nsISelection::ToString() ");
}


void CSelection::GetAnchorNode() 
{

	nsCOMPtr<nsIDOMNode> oNsDOMAnchorNode ;
	nsCOMPtr<nsISelection> oSelection ;

	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;

	rv = oSelection->GetAnchorNode(getter_AddRefs(oNsDOMAnchorNode));
    RvTestResult(rv, "nsISelection::GetAnchorNode() ",0);
	if (!oNsDOMAnchorNode)
		RvTestResult(rv, "------>No Selection made to get the AnchorNode object",0);
}


void CSelection::GetAnchorOffset()
{

	PRInt32 ancoroffset = 0;
	//char szstr[10] ;

	nsCOMPtr<nsISelection> oSelection ;

	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;

	rv = oSelection->GetAnchorOffset(&ancoroffset);
	RvTestResult(rv, "nsISelection::GetAnchorOffset()",0);
	//ltoa(ancoroffset,	szstr,10);
	//AfxMessageBox(szstr);
}

void CSelection::GetFocusNode() 
{
	nsCOMPtr<nsIDOMNode> oNsDOMFocusNode ;
	nsCOMPtr<nsISelection> oSelection ;

	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;

	rv = oSelection->GetFocusNode(getter_AddRefs(oNsDOMFocusNode));
    RvTestResult(rv, "nsISelection::GetFocusNode() ",0);
	if (!oNsDOMFocusNode)
		RvTestResult(rv, "------>No Selection made to get the FocusNode object",0);
}

void CSelection::GetFocusOffset() 
{
	PRInt32 focusoffset = 0;
	nsCOMPtr<nsISelection> oSelection ;

	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;

	rv = oSelection->GetFocusOffset(&focusoffset);
	RvTestResult(rv, "nsISelection::GetFocusOffset()",0);	

}

void CSelection::GetIsCollapsed() 
{
	PRBool  bisCollapsed = false;

	nsCOMPtr<nsISelection> oSelection ;
	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;

	rv = oSelection->GetIsCollapsed(&bisCollapsed);
	RvTestResult(rv, "nsISelection::GetIsCollapsed()",0);	
	if(bisCollapsed)
		RvTestResult(rv, "------> Returned value for IsCollapsed is true",0);	
	else
		RvTestResult(rv, "------> Returned value for IsCollapsed is false",0);	
}

void CSelection::GetRangecount() 
{
	PRInt32 rangecount = 0;
	nsCOMPtr<nsISelection> oSelection ;

	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;
	rv = oSelection->GetRangeCount(&rangecount);
	RvTestResult(rv, "nsISelection::GetRangeCount()",0);	
	char szstr[10] ;
	ltoa(rangecount,	szstr,10);
	AfxMessageBox(szstr);
}


void CSelection::GetRangeAt() 
{
	PRInt32 index = 0;
	nsCOMPtr<nsIDOMRange> oNsDOMRange ;

	nsCOMPtr<nsISelection> oSelection ;
	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;
	rv = oSelection->GetRangeAt(index,getter_AddRefs(oNsDOMRange));
	RvTestResult(rv, "nsISelection::GetRangeAt() ",0);
	if (!oNsDOMRange)
		RvTestResult(rv, "------>Not able to get nsIDOMRange object",0);


}

void CSelection::Collapse() 
{

	nsCOMPtr<nsIDOMNode> oNsDOMFocusNode ;
	nsCOMPtr<nsISelection> oSelection ;

	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;

	rv = oSelection->GetFocusNode(getter_AddRefs(oNsDOMFocusNode));
    RvTestResult(rv, "nsISelection::GetFocusNode() ",0);
	if (!oNsDOMFocusNode)
		RvTestResult(rv, "------>No Selection made to get the FocusNode object",0);

	rv = oSelection->Collapse(oNsDOMFocusNode,1) ;
    RvTestResult(rv, "nsISelection::Collapse() ",0);

}

void CSelection::Extend() 
{
	nsCOMPtr<nsIDOMNode> oNsDOMFocusNode ;
	nsCOMPtr<nsISelection> oSelection ;

	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;

	rv = oSelection->GetFocusNode(getter_AddRefs(oNsDOMFocusNode));
    RvTestResult(rv, "nsISelection::GetFocusNode() ",0);
	if (!oNsDOMFocusNode)
		RvTestResult(rv, "------>No Selection made to get the FocusNode object",0);


	rv = oSelection->Extend(oNsDOMFocusNode,10) ;
    RvTestResult(rv, "nsISelection::Extend() ",0);


}

void CSelection::CollapseToStart()
{
	nsCOMPtr<nsISelection> oSelection ;
	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;
	rv = oSelection->CollapseToStart();
	    RvTestResult(rv, "nsISelection::CollapseToStart() ",0);

}

void CSelection::CollapseToEnd()
{
	nsCOMPtr<nsISelection> oSelection ;
	oSelection = GetSelectionObject();
	rv = oSelection->CollapseToEnd();
	    RvTestResult(rv, "nsISelection::CollapseToEnd() ",0);
}

void CSelection::ContainsNode()
{
	PRBool  bContains = false;

	nsCOMPtr<nsIDOMNode> oNsDOMFocusNode ;
	nsCOMPtr<nsISelection> oSelection ;

	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;

	rv = oSelection->GetFocusNode(getter_AddRefs(oNsDOMFocusNode));
    RvTestResult(rv, "nsISelection::GetFocusNode() ",0);
	if (!oNsDOMFocusNode)
		RvTestResult(rv, "------>No Selection made to get the FocusNode object",0);


	rv = oSelection->ContainsNode(oNsDOMFocusNode,PR_TRUE, &bContains);
	    RvTestResult(rv, "nsISelection::ContainsNode() ",0);
	if(bContains)
		RvTestResult(rv, "------> Returned value for ContainsNode is true",0);	
	else
		RvTestResult(rv, "------> Returned value for ContainsNode is false",0);
}

void CSelection::SelectAllChildren()
{
	nsCOMPtr<nsIDOMNode> oNsDOMFocusNode ;
	nsCOMPtr<nsISelection> oSelection ;

	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;

	rv = oSelection->GetFocusNode(getter_AddRefs(oNsDOMFocusNode));
    RvTestResult(rv, "nsISelection::GetFocusNode() ",0);
	if (!oNsDOMFocusNode)
		RvTestResult(rv, "------>No Selection made to get the FocusNode object",0);


	rv = oSelection->SelectAllChildren(oNsDOMFocusNode);
	    RvTestResult(rv, "nsISelection::SelectAllChildren() ",0);

}

void CSelection::Addrange()
{	
	PRInt32 index= 0;
	
	nsCOMPtr<nsIDOMRange> oNsDOMRange ;
	nsCOMPtr<nsISelection> oSelection ;

	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;

	rv = oSelection->GetRangeAt(index,getter_AddRefs(oNsDOMRange));
	RvTestResult(rv, "nsISelection::GetRangeAt() ",0);
	if (!oNsDOMRange)
		RvTestResult(rv, "------>Not able to get nsIDOMRange object",0);

	rv = oSelection->AddRange(oNsDOMRange);
	    RvTestResult(rv, "nsISelection::Addrange() ",0);

}

void CSelection::RemoveRange()
{ 
	PRInt32 index= 0;

	nsCOMPtr<nsIDOMRange> oNsDOMRange ;
	nsCOMPtr<nsISelection> oSelection ;

	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;

	rv = oSelection->GetRangeAt(index,getter_AddRefs(oNsDOMRange));
	RvTestResult(rv, "nsISelection::GetRangeAt() ",0);
	if (!oNsDOMRange)
		RvTestResult(rv, "------>Not able to get nsIDOMRange object",0);

	rv = oSelection->RemoveRange(oNsDOMRange);
	    RvTestResult(rv, "nsISelection::RemoveRange() ",0);
}

void CSelection::RemoveAllRanges()
{ 
	nsCOMPtr<nsISelection> oSelection ;
	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;
	rv = oSelection->RemoveAllRanges();
	    RvTestResult(rv, "nsISelection::RemoveAllRanges() ",0);

}

void CSelection::DeleteFromDocument()
{ 
	nsCOMPtr<nsISelection> oSelection ;
	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;
	rv = oSelection->DeleteFromDocument();
	    RvTestResult(rv, "nsISelection::DeleteFromDocument() ",0);

}

void CSelection::SelectionLanguageChange()
{ 
	nsCOMPtr<nsISelection> oSelection ;
	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;

	rv = oSelection->SelectionLanguageChange(PR_TRUE);
	    RvTestResult(rv, "nsISelection::SelectionLanguageChange() ",0);

}

void CSelection::ToString()
{ 
	nsXPIDLString szText;
	nsCOMPtr<nsISelection> oSelection ;

	oSelection = GetSelectionObject();
	if (!oSelection)
		return ;
	rv = oSelection->ToString(getter_Copies(szText));
    RvTestResult(rv, "nsISelection::ToString() ",0);
}


nsISelection * CSelection::GetSelectionObject(BOOL bShowDialog)
{
	nsCOMPtr<nsISelection> oNsSelection ;

	nsCOMPtr<nsIDOMWindow> oDomWindow;

	// Get the nsIDOMWindow interface in order to get nsISelection 
	rv = qaWebBrowser->GetContentDOMWindow(getter_AddRefs(oDomWindow));

	if(bShowDialog)
	   RvTestResultDlg(rv, "nsIWebBrowser::GetContentDOMWindow()' rv test",true);
	else
       RvTestResult(rv, "nsIWebBrowser::GetContentDOMWindow()' rv test", 0);
	if (!oDomWindow)
	{
		if(bShowDialog)
		    RvTestResultDlg(rv, "------>Cannot get the  nsIDOMNode Object");
		else
	       RvTestResult(rv, "------>Cannot get the  nsIDOMNode Object", 0);
		return NULL ;
	}

	if(bShowDialog)
		RvTestResultDlg(rv, "------>nsIDOMWindow is required in order to get the nsISelection interface");
	else
       RvTestResult(rv, "nsIDOMWindow is required in order to get the nsISelection interface", 0);
	
	// Now get the nsISelection interface
	rv = oDomWindow->GetSelection(getter_AddRefs(oNsSelection));

	if(bShowDialog)
	    RvTestResultDlg(rv, "nsIDOMWindow::GetSelection()");
	else
       RvTestResult(rv, "nsIDOMWindow::GetSelection()", 0);

	if (!oNsSelection)
	{
		if(bShowDialog)
		    RvTestResultDlg(rv, "------>Cannot get the  Selection Object");
		else
	       RvTestResult(rv, "Cannot get the  Selection Object", 0);

		return NULL ;
	}
	
	return oNsSelection ;
}