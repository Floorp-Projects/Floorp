/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Mike Judge <mjudge@netscape.com>
 *   Chak Nanga <chak@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "stdafx.h"
#include "MfcEmbed.h"
#include "BrowserFrm.h"
#include "EditorFrm.h"
#include "Dialogs.h"
#include "nsComponentManagerUtils.h"
#include "nsMemory.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"

//------------------------------------------------------------
//              Editor Command/Parameter Names
//------------------------------------------------------------
#define BOLD_COMMAND            "cmd_bold"
#define ITALIC_COMMAND          "cmd_italic"
#define UNDERLINE_COMMAND       "cmd_underline"
#define INDENT_COMMAND          "cmd_indent"
#define OUTDENT_COMMAND         "cmd_outdent"
#define FONTCOLOR_COMMAND       "cmd_fontColor"
#define BACKGROUNDCOLOR_COMMAND "cmd_backgroundColor"
#define COMMAND_NAME            "cmd_name"
#define INCREASEFONT_COMMAND    "cmd_increaseFont"
#define DECREASEFONT_COMMAND    "cmd_decreaseFont"
#define FONTFACE_COMMAND        "cmd_fontFace"
#define ALIGN_COMMAND           "cmd_align"
#define UNDO_COMMAND            "cmd_undo"
#define REDO_COMMAND            "cmd_redo"


//states
#define STATE_ALL           "state_all"
#define STATE_MIXED         "state_mixed"
#define STATE_ATTRIBUTE     "state_attribute"
#define STATE_ENABLED       "state_enabled"

//colors
#define COLOR_RED       "#FF0000"
#define COLOR_BLACK     "#000000"

//fonts
#define FONT_ARIAL            "Helvetica, Arial, sans-serif"
#define FONT_TIMES            "Times New Roman, Times, serif"
#define FONT_COURIER        "Courier New, Courier, monospace"

//align
#define ALIGN_LEFT      "left"
#define ALIGN_RIGHT     "right"
#define ALIGN_CENTER    "center"


//value
#define STATE_EMPTY        ""

IMPLEMENT_DYNAMIC(CEditorFrame, CBrowserFrame)

BEGIN_MESSAGE_MAP(CEditorFrame, CBrowserFrame)
    //{{AFX_MSG_MAP(CEditorFrame)
    ON_COMMAND(ID_BOLD, OnBold)
    ON_UPDATE_COMMAND_UI(ID_BOLD, OnUpdateBold)
    ON_COMMAND(ID_ITALICS, OnItalics)
    ON_UPDATE_COMMAND_UI(ID_ITALICS, OnUpdateItalics)
    ON_COMMAND(ID_UNDERLINE, OnUnderline)
    ON_UPDATE_COMMAND_UI(ID_UNDERLINE, OnUpdateUnderline)
    ON_COMMAND(ID_INDENT, OnIndent)
    ON_UPDATE_COMMAND_UI(ID_INDENT, OnUpdateIndent)
    ON_COMMAND(ID_OUTDENT, OnOutdent)
    ON_UPDATE_COMMAND_UI(ID_OUTDENT, OnUpdateOutdent)
    ON_COMMAND(ID_FONTRED, OnFontred)
    ON_UPDATE_COMMAND_UI(ID_FONTRED, OnUpdateFontred)
    ON_COMMAND(ID_FONTBLACK, OnFontblack)
    ON_UPDATE_COMMAND_UI(ID_FONTBLACK, OnUpdateFontblack)
    ON_COMMAND(ID_BGCOLOR, OnBgcolor)
    ON_UPDATE_COMMAND_UI(ID_BGCOLOR, OnUpdateBgcolor)
    ON_COMMAND(ID_NOBGCOLOR, OnNobgcolor)
    ON_UPDATE_COMMAND_UI(ID_NOBGCOLOR, OnUpdateNobgcolor)
    ON_COMMAND(ID_FONTSIZEINCREASE, OnFontsizeincrease)
    ON_COMMAND(ID_FONTSIZEDECREASE, OnFontsizedecrease)
    ON_COMMAND(ID_ARIAL, OnArial)
    ON_COMMAND(ID_TIMES, OnTimes)
    ON_COMMAND(ID_COURIER, OnCourier)
    ON_COMMAND(ID_ALIGNLEFT, OnAlignleft)
    ON_UPDATE_COMMAND_UI(ID_ALIGNLEFT, OnUpdateAlignleft)
    ON_COMMAND(ID_ALIGNRIGHT, OnAlignright)
    ON_UPDATE_COMMAND_UI(ID_ALIGNRIGHT, OnUpdateAlignright)
    ON_COMMAND(ID_ALIGNCENTER, OnAligncenter)
    ON_UPDATE_COMMAND_UI(ID_ALIGNCENTER, OnUpdateAligncenter)
    ON_COMMAND(ID_INSERTLINK, OnInsertlink)
    ON_COMMAND(ID_EDITOR_UNDO, OnEditUndo)
    ON_COMMAND(ID_EDITOR_REDO, OnEditRedo)
    ON_UPDATE_COMMAND_UI(ID_EDITOR_REDO, OnUpdateEditRedo)
    ON_UPDATE_COMMAND_UI(ID_EDITOR_UNDO, OnUpdateEditUndo)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

CEditorFrame::CEditorFrame(PRUint32 chromeMask)
{
    m_chromeMask = chromeMask;

    mCommandManager = nsnull;
}

CEditorFrame::~CEditorFrame()
{
}

BOOL CEditorFrame::InitEditor()
{
    // Get and save off nsICommandManager for later use
    //
    nsresult rv;
    mCommandManager = do_GetInterface(m_wndBrowserView.mWebBrowser, &rv);

    rv = MakeEditable();
 
    return (NS_SUCCEEDED(rv));
}

void CEditorFrame::OnBold() 
{
    ExecuteStyleCommand(BOLD_COMMAND);
}

void CEditorFrame::OnUpdateBold(CCmdUI* pCmdUI) 
{
    UpdateStyleToolBarBtn(BOLD_COMMAND, pCmdUI);
}

void CEditorFrame::OnItalics() 
{
    ExecuteStyleCommand(ITALIC_COMMAND);
}

void CEditorFrame::OnUpdateItalics(CCmdUI* pCmdUI) 
{
    UpdateStyleToolBarBtn(ITALIC_COMMAND, pCmdUI);
}

void CEditorFrame::OnUnderline() 
{
    ExecuteStyleCommand(UNDERLINE_COMMAND);
}

void CEditorFrame::OnUpdateUnderline(CCmdUI* pCmdUI) 
{
    UpdateStyleToolBarBtn(UNDERLINE_COMMAND, pCmdUI);
}


//Called to make a nsICommandParams with the 1 value pair of command name
NS_METHOD
CEditorFrame::MakeCommandParams(const char *aCommand,nsICommandParams **aParams)
{
    nsresult rv;
    nsCOMPtr<nsICommandParams> params = do_CreateInstance(NS_COMMAND_PARAMS_CONTRACTID,&rv);
    if (NS_FAILED(rv))
        return rv;
    if (!params)
        return NS_ERROR_FAILURE;
    *aParams = params;
    NS_ADDREF(*aParams);
    return rv;
}


// Called in response to the ON_COMMAND messages generated
// when style related toolbar buttons(bold, italic etc)
// are clicked
//
NS_METHOD
CEditorFrame::ExecuteStyleCommand(const char *aCommand)
{
  return DoCommand(aCommand,0);
}

// Called in response to the UPDATE_COMMAND_UI messages for 
// style related toolbar buttons(bold, italic etc.)
// to update their current state
//
void CEditorFrame::UpdateStyleToolBarBtn(const char *aCommand, CCmdUI* pCmdUI)
{
    nsCOMPtr<nsICommandParams> params;
    nsresult rv;
    rv = MakeCommandParams(aCommand,getter_AddRefs(params));
    if (NS_FAILED(rv) || !params)
        return;
    rv = GetCommandState(aCommand,params);
    if (NS_SUCCEEDED(rv))
    {
        // Does our current selection span mixed styles?
        // If so, set the toolbar button to an indeterminate
        // state
        //
        PRBool bMixedStyle = PR_FALSE;
        rv = params->GetBooleanValue(STATE_MIXED, &bMixedStyle);
        if (NS_SUCCEEDED(rv))
        {
            if(bMixedStyle)
            {
                pCmdUI->SetCheck(2);
                return;
            }
        }

        // We're not in STATE_MIXED. Enable/Disable the
        // toolbar button based on it's current state
        //
        PRBool bCmdEnabled = PR_FALSE;
        rv = params->GetBooleanValue(STATE_ALL, &bCmdEnabled);
        if (NS_SUCCEEDED(rv))
        {
            bCmdEnabled ? pCmdUI->SetCheck(1) : pCmdUI->SetCheck(0);
        }
    }
}

NS_METHOD
CEditorFrame::MakeEditable()
{
    nsresult rv;

    m_wndBrowserView.mWebBrowser->GetContentDOMWindow(getter_AddRefs(mDOMWindow));
    if (!mDOMWindow)
        return NS_ERROR_FAILURE;

    mEditingSession = do_GetInterface(m_wndBrowserView.mWebBrowser);
    if (!mEditingSession)
        return NS_ERROR_FAILURE;
  
    rv= mEditingSession->MakeWindowEditable(mDOMWindow, NULL, PR_TRUE);
  
    return rv;  
}

NS_METHOD
CEditorFrame::DoCommand(const char *aCommand, nsICommandParams *aCommandParams)
{
    nsCOMPtr<nsIDOMWindow> domWindow;
    m_wndBrowserView.mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    return mCommandManager ? mCommandManager->DoCommand(aCommand, aCommandParams, domWindow) : NS_ERROR_FAILURE;
}

NS_METHOD
CEditorFrame::IsCommandEnabled(const char *aCommand, PRBool *retval)
{
    nsCOMPtr<nsIDOMWindow> domWindow;
    m_wndBrowserView.mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    return mCommandManager ? mCommandManager->IsCommandEnabled(aCommand, domWindow, retval) : NS_ERROR_FAILURE;
}


NS_METHOD
CEditorFrame::GetCommandState(const char *aCommand, nsICommandParams *aCommandParams)
{
    nsCOMPtr<nsIDOMWindow> domWindow;
    m_wndBrowserView.mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    return mCommandManager ? mCommandManager->GetCommandState(aCommand, domWindow, aCommandParams) : NS_ERROR_FAILURE;
}

NS_METHOD
CEditorFrame::ExecuteNoParam(const char *aCommand)
{
    return DoCommand(aCommand,0);
}

void CEditorFrame::OnIndent() 
{
    ExecuteNoParam(INDENT_COMMAND);    
}

void CEditorFrame::OnUpdateIndent(CCmdUI* pCmdUI) 
{
    // TODO: Add your command update UI handler code here
}

void CEditorFrame::OnOutdent() 
{
    ExecuteNoParam(OUTDENT_COMMAND);    
}

void CEditorFrame::OnUpdateOutdent(CCmdUI* pCmdUI) 
{
    // TODO: Add your command update UI handler code here
}

NS_METHOD
CEditorFrame::ExecuteAttribParam(const char *aCommand, const char *aAttribute)
{
  nsresult rv;
  nsCOMPtr<nsICommandParams> params;
  rv = MakeCommandParams(aCommand,getter_AddRefs(params));
  if (NS_FAILED(rv))
   return rv;
  if (!params)
    return NS_ERROR_FAILURE;
  params->SetCStringValue(STATE_ATTRIBUTE, aAttribute);
  return DoCommand(aCommand,params);
}

NS_METHOD
CEditorFrame::GetAttributeParamValue(const char *aCommand, nsEmbedCString &aValue)
{
  nsresult rv;
  nsCOMPtr<nsICommandParams> params;
  rv = MakeCommandParams(aCommand,getter_AddRefs(params));
  if (NS_FAILED(rv))
    return rv;
  if (!params)
    return NS_ERROR_FAILURE;
  rv = GetCommandState(aCommand, params);
  if (NS_SUCCEEDED(rv))
  {
    char *tchar;
    rv = params->GetCStringValue(STATE_ATTRIBUTE,&tchar);
    aValue.Assign(tchar);
    nsMemory::Free(tchar);
    return rv;
  }
  return rv;
}


void CEditorFrame::OnFontred() 
{
    ExecuteAttribParam(FONTCOLOR_COMMAND,COLOR_RED);    
}

void CEditorFrame::OnUpdateFontred(CCmdUI* pCmdUI) 
{
    // TODO: Add your command update UI handler code here
}

void CEditorFrame::OnFontblack() 
{
    ExecuteAttribParam(FONTCOLOR_COMMAND,COLOR_BLACK);        
}

void CEditorFrame::OnUpdateFontblack(CCmdUI* pCmdUI) 
{
    // TODO: Add your command update UI handler code here   
}

void CEditorFrame::OnBgcolor() 
{
    ExecuteAttribParam(BACKGROUNDCOLOR_COMMAND,COLOR_RED);
}

void CEditorFrame::OnUpdateBgcolor(CCmdUI* pCmdUI) 
{
    // TODO: Add your command update UI handler code here
}

void CEditorFrame::OnNobgcolor() 
{
    ExecuteAttribParam(BACKGROUNDCOLOR_COMMAND,STATE_EMPTY);
}

void CEditorFrame::OnUpdateNobgcolor(CCmdUI* pCmdUI) 
{
    // TODO: Add your command update UI handler code here
}

void CEditorFrame::OnFontsizeincrease() 
{
    ExecuteNoParam(INCREASEFONT_COMMAND);
}

void CEditorFrame::OnFontsizedecrease() 
{
    ExecuteNoParam(DECREASEFONT_COMMAND);    
}

void CEditorFrame::OnArial() 
{
    ExecuteAttribParam(FONTFACE_COMMAND,FONT_ARIAL);
}

void CEditorFrame::OnTimes() 
{
    ExecuteAttribParam(FONTFACE_COMMAND,FONT_TIMES);    
}

void CEditorFrame::OnCourier() 
{
    ExecuteAttribParam(FONTFACE_COMMAND,FONT_COURIER);
}

void CEditorFrame::OnAlignleft() 
{
    ExecuteAttribParam(ALIGN_COMMAND,ALIGN_LEFT);    
}


void CEditorFrame::OnUpdateAlignleft(CCmdUI* pCmdUI) 
{
  nsEmbedCString tValue;

  nsresult rv = GetAttributeParamValue(ALIGN_COMMAND,tValue);
  if (NS_SUCCEEDED(rv))
  {
    if (strcmp(tValue.get(), ALIGN_LEFT) == 0)
      pCmdUI->SetCheck(1);
    else
      pCmdUI->SetCheck(0);
  }
}

void CEditorFrame::OnAlignright() 
{
    ExecuteAttribParam(ALIGN_COMMAND,ALIGN_RIGHT);
}

void CEditorFrame::OnUpdateAlignright(CCmdUI* pCmdUI) 
{
  nsEmbedCString tValue;
  nsresult rv = GetAttributeParamValue(ALIGN_COMMAND,tValue);
  if (NS_SUCCEEDED(rv))
  {
    if (strcmp(tValue.get(), ALIGN_RIGHT) == 0)
      pCmdUI->SetCheck(1);
    else
      pCmdUI->SetCheck(0);
  }
}

void CEditorFrame::OnAligncenter() 
{
    ExecuteAttribParam(ALIGN_COMMAND,ALIGN_CENTER);
}

void CEditorFrame::OnUpdateAligncenter(CCmdUI* pCmdUI) 
{
  nsEmbedCString tValue;
  nsresult rv = GetAttributeParamValue(ALIGN_COMMAND,tValue);
  if (NS_SUCCEEDED(rv))
  {
    if (strcmp(tValue.get(), ALIGN_CENTER) == 0)
      pCmdUI->SetCheck(1);
    else
      pCmdUI->SetCheck(0);
  }
}

void CEditorFrame::OnInsertlink() 
{
    if (InLink())
       ShowEditLinkDlg();
    else
       ShowInsertLinkDlg();
}

void CEditorFrame::ShowInsertLinkDlg()
{
    CLinkPropertiesDlg dlg;

    if(dlg.DoModal() == IDOK)
    {
        InsertLink(dlg.m_LinkText, dlg.m_LinkLocation);
    }
}

void CEditorFrame::InsertLink(CString& linkText, CString& linkLocation)
{
    CString html = " &nbsp;<A HREF=\"" + linkLocation + "\">" + linkText + "</A>&nbsp; ";

    InsertHTML(html);
}

void CEditorFrame::InsertHTML(CString& str)
{
    nsEmbedString htmlToInsert;
#ifdef _UNICODE
    htmlToInsert.Assign(str.GetBuffer(0));
#else
    NS_CStringToUTF16(nsEmbedCString(str.GetBuffer(0)),
                      NS_CSTRING_ENCODING_ASCII,
                      htmlToInsert);
#endif

    nsCOMPtr<nsIHTMLEditor> htmlEditor;
    GetHTMLEditor(getter_AddRefs(htmlEditor)); 
    if (htmlEditor) 
       htmlEditor->InsertHTML(htmlToInsert);
}

void CEditorFrame::ShowEditLinkDlg()
{
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchorElement;
    CLinkPropertiesDlg dlg;

    if (GetCurrentLinkInfo(dlg.m_LinkText, dlg.m_LinkLocation, getter_AddRefs(anchorElement)))
    {
        if(dlg.DoModal() == IDOK)
        {
            //Select the link being edited and then insert the
            //new link into it's place i.e. essentially a replace operation
            nsCOMPtr<nsIHTMLEditor> htmlEditor;
            GetHTMLEditor(getter_AddRefs(htmlEditor)); 
            if (htmlEditor) 
               htmlEditor->SelectElement(anchorElement);

            InsertLink(dlg.m_LinkText, dlg.m_LinkLocation);
        }
    }
}

BOOL CEditorFrame::GetCurrentLinkInfo(CString& strLinkText, CString& strLinkLocation, nsIDOMHTMLAnchorElement** anchorElement)
{
    USES_CONVERSION;

    nsCOMPtr<nsIHTMLEditor> htmlEditor;

    GetHTMLEditor(getter_AddRefs(htmlEditor));
    if (!htmlEditor)
        return FALSE;

    nsCOMPtr<nsIDOMElement> domElement;
    htmlEditor->GetElementOrParentByTagName(nsEmbedString(L"href"), 
                                            nsnull,
                                            getter_AddRefs(domElement));
    if (!domElement)
        return FALSE;

    nsEmbedString linkLocation, linkText;
    nsresult rv = NS_ERROR_FAILURE;

    // Determine linkLocation
    nsCOMPtr<nsIDOMHTMLAnchorElement> linkElement(do_QueryInterface(domElement, &rv));
    if (NS_SUCCEEDED(rv))
        rv = linkElement->GetHref(linkLocation);
    if (NS_FAILED(rv))
        return FALSE;

    // Determine linkText
    nsCOMPtr<nsIDOMNode> firstChild;
    rv = linkElement->GetFirstChild(getter_AddRefs(firstChild));
    if (NS_FAILED(rv))
        return FALSE;
    firstChild->GetNodeValue(linkText);
    if (NS_FAILED(rv))
        return FALSE;

    strLinkText = W2CT(linkText.get());
    strLinkLocation = W2CT(linkLocation.get());

    *anchorElement = linkElement;
    NS_ADDREF(*anchorElement);

	return TRUE;
}

void CEditorFrame::OnEditUndo() 
{
    ExecuteNoParam(UNDO_COMMAND);
}

void CEditorFrame::OnEditRedo() 
{
    ExecuteNoParam(REDO_COMMAND);    
}

void CEditorFrame::OnUpdateEditRedo(CCmdUI* pCmdUI) 
{
  nsresult rv;
  nsCOMPtr<nsICommandParams> params;
  rv = MakeCommandParams(REDO_COMMAND,getter_AddRefs(params));
  if (NS_FAILED(rv))
    return;
  if (!params)
    return;
  rv = GetCommandState(REDO_COMMAND, params);
  if (NS_SUCCEEDED(rv))
  {
    PRBool tValue;
    params->GetBooleanValue(STATE_ENABLED,&tValue);
    if (tValue)
    {
      pCmdUI->Enable(TRUE);
      return;
    }
  }
  pCmdUI->Enable(FALSE);    
}

void CEditorFrame::OnUpdateEditUndo(CCmdUI* pCmdUI) 
{
  nsresult rv;
  nsCOMPtr<nsICommandParams> params;
  rv = MakeCommandParams(UNDO_COMMAND,getter_AddRefs(params));
  if (NS_FAILED(rv))
    return;
  if (!params)
    return;
  rv = GetCommandState(UNDO_COMMAND, params);
  if (NS_SUCCEEDED(rv))
  {
    PRBool tValue;
    params->GetBooleanValue(STATE_ENABLED,&tValue);
    if (tValue)
    {
      pCmdUI->Enable(TRUE);
      return;
    }
  }
  pCmdUI->Enable(FALSE);        
}

void CEditorFrame::GetEditor(nsIEditor** editor)
{
    mEditingSession->GetEditorForWindow(mDOMWindow, editor);
}

void CEditorFrame::GetHTMLEditor(nsIHTMLEditor** htmlEditor)
{
    *htmlEditor = 0;

    nsCOMPtr<nsIEditor> editor;
    GetEditor(getter_AddRefs(editor));
    if (!editor) 
	{
        return;
    }

    editor->QueryInterface(NS_GET_IID(nsIHTMLEditor), (void**)htmlEditor);
}

BOOL CEditorFrame::InLink()
{
    nsCOMPtr<nsIHTMLEditor> htmlEditor;

    GetHTMLEditor(getter_AddRefs(htmlEditor));

    if (htmlEditor) 
	{
        nsCOMPtr<nsIDOMElement> domElememt;
        htmlEditor->GetElementOrParentByTagName(nsEmbedString(L"href"), 
                                                nsnull,
                                                getter_AddRefs(domElememt));
        return domElememt ? TRUE : FALSE;
    }

    return false;
}
