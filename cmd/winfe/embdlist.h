#ifndef EMBDLIST_H
#define EMBDLIST_H
class CEmbeddedAttachList : public CListBox
{
public:
    DECLARE_DYNCREATE(CEmbeddedAttachList)
    HFONT m_cfTextFont;
    CEmbeddedAttachList();
    ~CEmbeddedAttachList();

    void AttachFile();
    void RemoveAttachment();
    void AttachUrl(char *pUrl = NULL);
    void AddAttachment(char * pName);
    char **AllocFillAttachList();
    virtual BOOL Create(CWnd *pWnd, UINT id);

	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	virtual void DeleteItem(LPDELETEITEMSTRUCT lpDeleteItemStruct);

    afx_msg void    OnDropFiles( HDROP hDropInfo );
protected:
    UINT ItemFromPoint(CPoint pt, BOOL& bOutside) const;		
    
    afx_msg int     OnCreate( LPCREATESTRUCT lpCreateStruct );
    afx_msg void    OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) ;
    afx_msg void    OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void    OnDelete();
    afx_msg void    OnDestroy();

    DECLARE_MESSAGE_MAP()
protected:
    char **m_attachmentlist;
    int m_numattachments;
};
#endif //EMBDLIST_H

