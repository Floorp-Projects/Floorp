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

#ifndef _EDITORFRM_H_
#define _EDITORFRM_H_

class nsICommandParams;
class nsIEditor;
class nsIHTMLEditor;

#include "nsIEditingSession.h"
#include "nsICommandManager.h"

class CEditorFrame : public CBrowserFrame
{    
public:
    CEditorFrame(PRUint32 chromeMask);
    virtual ~CEditorFrame();

protected: 
    DECLARE_DYNAMIC(CEditorFrame)

public:
    BOOL InitEditor();
    NS_METHOD MakeEditable();
    NS_METHOD DoCommand(const char *aCommand, nsICommandParams *aCommandParams);
    NS_METHOD IsCommandEnabled(const char *aCommand, PRBool *retval);
    NS_METHOD GetCommandState(const char *aCommand, nsICommandParams *aCommandParams);

    void GetEditor(nsIEditor** editor);
    void GetHTMLEditor(nsIHTMLEditor** htmlEditor);
    BOOL InLink();
    void ShowInsertLinkDlg();
    void ShowEditLinkDlg();
    BOOL GetCurrentLinkInfo(CString& strLinkText, CString& strLinkLocation, nsIDOMHTMLAnchorElement** anchorElement);
    void InsertLink(CString& linkText, CString& linkLocation);
    void InsertHTML(CString& str);

// Generated message map functions
protected:
    //{{AFX_MSG(CEditorFrame)
    afx_msg void OnBold();
    afx_msg void OnUpdateBold(CCmdUI* pCmdUI);
    afx_msg void OnItalics();
    afx_msg void OnUpdateItalics(CCmdUI* pCmdUI);
    afx_msg void OnUnderline();
    afx_msg void OnUpdateUnderline(CCmdUI* pCmdUI);
    afx_msg void OnIndent();
    afx_msg void OnUpdateIndent(CCmdUI* pCmdUI);
    afx_msg void OnOutdent();
    afx_msg void OnUpdateOutdent(CCmdUI* pCmdUI);
    afx_msg void OnFontred();
    afx_msg void OnUpdateFontred(CCmdUI* pCmdUI);
    afx_msg void OnFontblack();
    afx_msg void OnUpdateFontblack(CCmdUI* pCmdUI);
    afx_msg void OnBgcolor();
    afx_msg void OnUpdateBgcolor(CCmdUI* pCmdUI);
    afx_msg void OnNobgcolor();
    afx_msg void OnUpdateNobgcolor(CCmdUI* pCmdUI);
    afx_msg void OnFontsizeincrease();
    afx_msg void OnFontsizedecrease();
    afx_msg void OnArial();
    afx_msg void OnTimes();
    afx_msg void OnCourier();
    afx_msg void OnAlignleft();
    afx_msg void OnUpdateAlignleft(CCmdUI* pCmdUI);
    afx_msg void OnAlignright();
    afx_msg void OnUpdateAlignright(CCmdUI* pCmdUI);
    afx_msg void OnAligncenter();
    afx_msg void OnUpdateAligncenter(CCmdUI* pCmdUI);
    afx_msg void OnInsertlink();
    afx_msg void OnEditUndo();
    afx_msg void OnEditRedo();
    afx_msg void OnUpdateEditRedo(CCmdUI* pCmdUI);
    afx_msg void OnUpdateEditUndo(CCmdUI* pCmdUI);
    //}}AFX_MSG

    DECLARE_MESSAGE_MAP()

private:
    NS_METHOD ExecuteStyleCommand(const char *aCommand);
    NS_METHOD ExecuteNoParam(const char *aCommand);
    NS_METHOD MakeCommandParams(const char *aCommand,nsICommandParams **aParams);
    NS_METHOD ExecuteAttribParam(const char *aCommand, const char *aAttribute);
    NS_METHOD GetAttributeParamValue(const char *aCommand, nsEmbedCString &aValue);

    void UpdateStyleToolBarBtn(const char *aCommand, CCmdUI* pCmdUI);

private:
    nsCOMPtr<nsICommandManager> mCommandManager;
    nsCOMPtr<nsIDOMWindow> mDOMWindow;
	nsCOMPtr<nsIEditingSession> mEditingSession;
};

#endif //_EDITORFRM_H_
