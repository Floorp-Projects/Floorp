#ifndef ENDERWRP_H
#define ENDERWRP_H
#include "lo_ele.h"

class CNetscapeEditView;
class CEditToolBarController;

class CEnderView: public CView
{
public:
    CEnderView(CAbstractCX *p_pCx);
    CEnderView(){};
    ~CEnderView();
#ifdef MOZ_ENDER_MIME
    BOOL Create(CWnd *pParent, lo_FormElementHtmlareaData *pData, LO_TextAttr *pTextAttr);
#else
    BOOL Create(CWnd *pParent, lo_FormElementTextareaData *pData, LO_TextAttr *pTextAttr);
#endif
    virtual void OnDraw(CDC *pDC);
    CNetscapeEditView *GetEditView(){return m_pEditView;}
protected:
    DECLARE_DYNCREATE(CEnderView)
    //{{AFX_MSG(CEnderView)
    afx_msg void OnDestroy();
    afx_msg LONG OnToolController(UINT,LONG);
	//}}AFX_MSG
private:
    CNetscapeEditView *m_pEditView;
    CEditToolBarController *m_pToolBarController;
    CAbstractCX *m_pCX;
    DECLARE_MESSAGE_MAP()
};


#endif //ENDERWRP_H

