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

// edprops.h : header file
//
#ifdef EDITOR
#ifndef _EDPROPS_H
#define _EDPROPS_H

#include "edtrccln.h"
#include "property.h"

// Tell color-picker to use page's background color as "default"
#define BACKGROUND_COLORREF 0xFFFFFFFE
// Note: other _COLORREF defines are in winproto.h for wider access

// Use by Property dialogs and Preference dialog
BOOL wfe_ValidateImage(MWContext * pMWContext, CString& csImageURL, BOOL bCheckFileExists = FALSE);

//prototype used by editor DLL judge
void wfe_GetLayoutViewSize(MWContext * pMWContext, int32 * pWidth, int32 * pHeight);

// For no-brainer setting and clearing of text format style bits
#define FE_CLEAR_BIT(x,tf) x &= ~tf
#define FE_SET_BIT(x,tf)   x |= tf

// Font shared by all comboboxes in dialogs and toolbars
// Should be 1-pixel, 8pt MS Sans Serif or default GUI or font on foreing systems
extern CFont * wfe_pFont;
extern CFont * wfe_pBoldFont;  // Bold version of same font

extern int wfe_iFontHeight;

// Style states for m_iFontSizeMode
enum {
    ED_FONTSIZE_POINTS,         // Convert -2 to +4 into current point size (based on FontFace base size)
    ED_FONTSIZE_RELATIVE,       // Show -2 to +4
    ED_FONTSIZE_ADVANCED         // Show -2 to +4, and absolute point sizes for POINT-SIZE tag param
};
extern int32 wfe_iFontSizeMode;
extern CFont wfe_pThinFont;

// iSize should be  1 to MAX_FONT_SIZE
// String formated slightly different for menu:  "+1 (14pts)"
// Returns pointer to static string - DON'T FREE RESULT!
char * wfe_GetFontSizeString(MWContext * pMWContext, int iSize,  BOOL bFixedWidth = FALSE, BOOL bMenu = FALSE);

#define   MAX_TRUETYPE_FONTS  100

// Currently 10 columns of 7 colors each
// Now defined in edttypes.h
//#define   MAX_NS_COLORS        70

// Number of colors in custom palette
#define   MAX_CUSTOM_COLORS    10

// Add 1 for Current and 1 for Last-Picked and 10 for custom color palette
#define   MAX_COLORS  (MAX_NS_COLORS+MAX_CUSTOM_COLORS+2)

extern char **wfe_ppTrueTypeFonts; // The Sorted list of fonts in combobox or menu
extern int wfe_iTrueTypeFontBase;
extern int wfe_iTrueTypeFontCount;

// Build a sorted list of TrueType fonts
// Should be done only ONCE per app since strings are static
//   and used by all FontFace lists.
// TODO: If we want to build new list (user installs new fonts),
//       then we must call wfe_FillFaceCombo again for ALL windows
extern int  wfe_InitTrueTypeArray(CDC *pDC);    
extern void wfe_FreeTrueTypeArray();

// Global index within ppFontArray if "Other..." was needed (too many fonts)
extern int wfe_iFontFaceOtherIndex;

// Global strings used in font attribute owner-draw listboxes
extern char * ed_pOther;
extern char * ed_pMixedFonts;
extern char * ed_pDontChange;

// Fill the supplied combobox with Default, XP Font sets, and local Vector fonts
// Sets values for wfe_iTrueTypeFontBase and wfe_iTrueTypeFontCount
// Returns index to list where "Other..." is used instead of actual fonts (if too many fonts installed)
// Optionally returns width of longest string found
extern int wfe_FillFontComboBox(CComboBox * pCombo, int * pMaxWidth = NULL);

// Total font sizes with Netscape extension -- SHOULD GET THIS FROM A COMMON H FILE!
#define	  MAX_FONT_SIZE            7

// We map {-2 ... +4} onto absolute sizes {1 ... 7}
// This is minimum of scale
#define   MIN_FONT_SIZE_RELATIVE  -2

// Allocates a new LoColor structure if it does not exist
// Set RGB values
void wfe_SetLO_ColorPtr( COLORREF crColor, LO_Color **ppLoColor );

// Fill a LoColor structure
void wfe_SetLO_Color( COLORREF crColor, LO_Color *pLoColor );

// Array of colors to show in common color dialog
extern COLORREF wfe_CustomPalette[16];
// Last-Picked colors in color-picker: Saved in prefs
extern COLORREF wfe_crLastColorPicked;
extern COLORREF wfe_crLastBkgrndColorPicked;

// Define makes more sense in some circumstances
// We either don't want to show color because of mixed state in selection (in lists and comboboxes)
//    or don't draw a color swatch or button
#define NO_COLORREF  MIXED_COLORREF

char * wfe_GetDefaultColorString(COLORREF crDefColor);

/*convert image p_file (in URLFORM) and return the resultant filename, or null if failure/cancel*/
char * wfe_ConvertImage(char *p_fileurl,void *p_parentwindow,MWContext *p_pMWContext);

/////////////////////////////////////////////////////////////////////////////
// CColorButton
// Simple button to draw color rect on surface for launching a color picker

class CColorButton : public CButton
{
	DECLARE_DYNAMIC(CColorButton)
// Construction
public:
	CColorButton(COLORREF * pColorRef = NULL, COLORREF * pSetFocusColor = NULL);

// Attributes
private:
    // Point to color - owned by parent window
    COLORREF  * m_pColorRef;
    // If we set focus to ourselves, return the color here
    COLORREF  * m_pSetFocusColor;
    COLORREF    m_crDefaultColor;
    HPALETTE    m_hPal;
    BOOL        m_bColorSwatchMode;
    char        m_pTipText[80];
	CNSToolTip2 *m_pToolTip;

// Operations
public:
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CColorButton)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CColorButton();

    //   pColorRef points to color we display on a button
    // If bColorSwatchMode = TRUE, 
    //    draw a shaded border that looks like a deppressed well like Window's color picker
    // If FALSE, draw what looks like a Combobox: deppressed well with color 
    //   and button with downward-pointing triangle
    BOOL Create(RECT& rect, CWnd * pParentWnd, UINT nID, COLORREF * pColorRef, BOOL bColorSwatchMode = FALSE);
    // Use this when button control already exists in a dialog template
    //   (remember to set Owner-Draw style!)
    BOOL Subclass(CWnd * pParentWnd, UINT nID, COLORREF * pColorRef, BOOL bColorSwatchMode = FALSE);
    COLORREF GetColor() { return *m_pColorRef;}
    
    // Call when color has changed to redraw button and tooltip
    void Update();

private:
    // Add a tooltip to show color if in true "button" mode,
    //   not "color swatch" mode
    void AddToolTip();
    
#ifdef XP_WIN32
    // OnNotifyEx handler to get tooltip text
    BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
#endif

protected:
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDIS);
    virtual BOOL PreTranslateMessage(MSG* pMsg);

	// Generated message map functions
	//{{AFX_MSG(CColorButton)
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    //}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////////
// General User-draw Custom Combobox
// Draws a separator under strings beginning with "_"

class CNSComboBox : public CComboBox
{
public:
    CNSComboBox();
    ~CNSComboBox() {XP_FREEIF(m_pNotInListText);}

    BOOL Create(RECT& rect, CWnd* pParentWnd, UINT nID);

    BOOL Subclass(CWnd* pParentWnd, UINT nID);
    
    // Clear search params - parent must call this in ON_CBN_DROPDOWN handler
    //   to enable search feature
    void InitSearch();
    
    // Find the supplied string and set selection to it,
    //  if not found write text to closed listbox display
    int FindSelectedOrSetText(char * pText, int iStartAt = 0);

private:
    char        m_pSearchBuf[256];
    int         m_iSearchPos;
    BOOL        m_bAllowSearch;

    void MeasureItem(LPMEASUREITEMSTRUCT lpMIS);
	void DrawItem(LPDRAWITEMSTRUCT lpDIS);
	int  CompareItem(LPCOMPAREITEMSTRUCT lpCIS);

    DWORD   m_dwButtonDownTime;
    BOOL    m_bCheckTime;
    
    char * m_pNotInListText;

protected:
    // To trap button down/up messages to enforce "sticky" list
    virtual BOOL PreTranslateMessage(MSG* pMsg);

    //{{AFX_MSG(CNSComboBox)
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    //}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

// Font sizes are relative to current font base size,
//  so we need to refill list before each showing
// Returns index of current font size in list
// bFixedWidth needed to get correct base size for default fixed-width font
int wfe_FillFontSizeCombo(MWContext *pMWContext, CNSComboBox * pCombo, BOOL bFixedWidth = FALSE);

/////////////////////////////////////////////////////////////////////////////////
// Custom Combobox for selecting colors
class CColorComboBox : public CComboBox
{
public:
    CColorComboBox();

// Attributes:
    COLORREF   m_crColor;

    // Create combobox, fill with global Netscape color list
    //   MWContext is need to access parent's palette
    //   and display status messages
    //   A 1-pixel MS Sans Serif font will be used if pFont is NULL
    //  Returns number of items in color list
    BOOL CreateAndFill(RECT& rect, CWnd* pParentWnd, 
                       UINT nID, COLORREF crDefColor);

    // Use this with a CComboBox already created in a dialog template,
    //  attaches dialog and our class to the dialog's combobox
    //  Returns number of items in color list
    int SubclassAndFill(CWnd* pParentWnd, UINT nID, COLORREF crDefColor);
    
    // Called by each of above to fill the list of colors
    //  Returns number of items in color list
    int FillList(COLORREF crDefColor);
    
    // Sets the selected color to display in combobox.
    // Also find a match to color in color list
    //  and set current combobox selection to the index of that color
    int  SetColor(COLORREF cr);

    inline COLORREF GetColor() { return m_crColor; }

private:
    HPALETTE    m_hPal;
    CWnd *      m_pParent;
    // Set only by SetColor when color was not found in our list...
    BOOL    m_bCustomColor;
    // ...or when we have a selection with > 1 colors
    BOOL    m_bMixedColors;
    BOOL    m_bOtherColor;
	
    void MeasureItem(LPMEASUREITEMSTRUCT lpMIS);
	void DrawItem(LPDRAWITEMSTRUCT lpDIS);
	int  CompareItem(LPCOMPAREITEMSTRUCT lpCIS);

protected:
    // We trap all commands that might cause list to dropdown
	// Generated message map functions
	//{{AFX_MSG(CColorComboBox)
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CColorPicker 

class CColorPicker : public CWnd
{
// Construction
public:
	CColorPicker(CWnd * pParent, 
              MWContext * pMWContext,
              COLORREF    crCurrentColor,
              COLORREF    crDefColor,           // Actual color or DEFAULT_COLORREF or BACKGROUND_COLORREF
              UINT        nIDCaption = IDS_TEXT_COLOR,  // Defines what color we are setting
              RECT      * pCallerRect = NULL);  // If null, normal centerred dialog,
                                                //  else positions like a dropdown from caller's rect

// Dialog Data
	//{{AFX_DATA(CColorPicker)
	enum { IDD = IDD_COLOR_DLG };
	//}}AFX_DATA

    virtual ~CColorPicker();

private:    
    RECT            m_CallerRect;
    CWnd           *m_pParent;
    COLORREF        m_crColors[MAX_COLORS];
    CColorButton   *m_pColorButtons[MAX_COLORS];
    CButton         m_DefaultButton;
    CButton         m_OtherButton;
    CButton         m_HelpButton;
    CStatic         m_CurrentLabel;
    CStatic         m_LastUsedLabel;
    CStatic         m_CustomColorsLabel;
    CStatic         m_SaveColorLabel;
    CStatic         m_CustomColorNumber[MAX_CUSTOM_COLORS];
    BOOL            m_bBackground;
    UINT            m_nIDCaption;
    BOOL            m_bColorChanged[MAX_CUSTOM_COLORS];
    BOOL            m_bMouseDown;
    int             m_iMouseDownColorIndex;
    COLORREF        m_crDragColor;

    // Cache tooltip text to supply quicky OnTooltipNotify
    char            m_ppTipText[MAX_COLORS][80];

    // Result is put here OnOK,
    //  and returned via GetColor();
    COLORREF        m_crColor;

    COLORREF        m_crCurrentColor;
    COLORREF        m_crDefColor;
    LO_Color        m_LoColor;

    MWContext * m_pMWContext;

    // Save state when button is pressed down
    BOOL          m_bOtherDown;
    BOOL          m_bAutoDown;
    BOOL          m_bHelpDown;

	CNSToolTip2  *m_pToolTip;
	
	// Implementation 
public:
    // The one and only function to call after construction
    // Works like CDialog:;DoModal() but automatically
    //   exits if mouse clicked outside the picker
    COLORREF GetColor(LO_Color * pLoColor = NULL);

    // Need this to pass on tool tip messages
    virtual BOOL PreTranslateMessage(MSG* pMsg);

private:
#ifdef XP_WIN32
    // OnNotifyEx handler to get tooltip text
    BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
#endif

    // Stays TRUE while window is up.
    // Set to FALSE to destroy window and return synchronously to caller
    BOOL m_bRunning;
    
    // For suppessing color selection on first mouse up in Current Color button
    BOOL m_bFirstMouseUp;

protected:
    // Helpers for hit-testing during mouse management
    BOOL IsMouseOverDlg(CPoint cPoint);
    BOOL IsMouseOverButton(CPoint cPoint, UINT nID);
    // Return index to color swatch, or -1 if not over one
    int  GetMouseOverColorIndex(CPoint cPoint);
    
    void SetButtonState(CPoint cPoint, UINT nID, BOOL* pButtonDown);
    
    // Equivalent to OnOK() and OnCancel for a dialog
    void SetColorAndExit();
    void CancelAndExit();
    void OnColorHelp();

    // Generated message map functions
	//{{AFX_MSG(CColorPicker)
    virtual void PostNcDestroy();
	afx_msg void OnChooseColor();
    afx_msg void OnDefaultColor();
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnPaint();
//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CTagDlg dialog -- Arbitrary tag editor
class CTagDlg : public CDialog
{
// Construction
public:
	CTagDlg(CWnd* pParent,
            MWContext* pMWContext,    // MUST be supplied!
            char *pTagData = NULL);

// Dialog Data
	//{{AFX_DATA(CTagDlg)
	enum { IDD = IDD_PROPS_HTML_TAG };
	CString m_csTagData;
	//}}AFX_DATA

private:
    BOOL              m_bInsert;
    MWContext        *m_pMWContext;
    BOOL  m_bValidTag;

    int32   m_iFullWidth;
    int32   m_iFullHeight;

    // This will change resource hInstance to Editor dll (in constructor)
    CEditorResourceSwitcher m_ResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTagDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CTagDlg)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnHelp();
	afx_msg void OnVerifyHtml();
	//}}AFX_MSG
#ifdef XP_WIN32
    afx_msg BOOL OnHelpInfo(HELPINFO *);
#endif
	DECLARE_MESSAGE_MAP()
//  Tag templates not implemented:
//	afx_msg void OnAddNewTag();
//	afx_msg void OnSelchangeTagList();

    Bool DoVerifyTag( char* pTagString );
};


/////////////////////////////////////////////////////////////////////////////
// CHRuleDlg dialog

class CHRuleDlg : public CDialog
{
// Construction
public:
	CHRuleDlg(CWnd* pParent, 
	          MWContext *pMWContext,         // MUST be supplied!
              EDT_HorizRuleData* pData = NULL );    // Insert new if not supplied
// Dialog Data
	//{{AFX_DATA(CHRuleDlg)
	enum { IDD = IDD_PROPS_HRULE };
	int		m_nAlign;
	BOOL	m_bShading;
	int	    m_iWidth;
	int	    m_iHeight;
	int		m_iWidthType;
	//}}AFX_DATA

private:    
    BOOL               m_bInsert;
    EDT_HorizRuleData *m_pData;
    MWContext         *m_pMWContext;

    int32   m_iFullWidth;

    // This will change resource hInstance to Editor dll (in constructor)
    CEditorResourceSwitcher m_ResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CHRuleDlg)
	public:
	virtual BOOL DestroyWindow();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CHRuleDlg)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnHelp();
	afx_msg void OnExtraHTML();
	//}}AFX_MSG
#ifdef XP_WIN32
    afx_msg BOOL OnHelpInfo(HELPINFO *);
#endif
	DECLARE_MESSAGE_MAP()
};

typedef enum {
    ED_BORDER_NONE,
    ED_BORDER_BLACK,
    ED_BORDER_WHITE,
    ED_BORDER_RAISED,
    ED_BORDER_LOWERED
} ED_BorderStyle;


/////////////////////////////////////////////////////////////////////////////
// CTargetDlg dialog

class CTargetDlg : public CDialog
{
// Construction
public:
	CTargetDlg(CWnd* pParent,
               MWContext* pMWContext, // MUST be supplied!
               char *pName = NULL);          // Existing target, NULL for new

// Dialog Data
	//{{AFX_DATA(CTargetDlg)
	enum { IDD = IDD_PROPS_TARGET };
	CString	m_csName;
	//}}AFX_DATA

private:
    MWContext *m_pMWContext;
    BOOL       m_bInsert;
    char      *m_pTargetList;

    // This will change resource hInstance to Editor dll (in constructor)
    CEditorResourceSwitcher m_ResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTargetDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CTargetDlg)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnHelp();
	afx_msg void OnChangeTargetName();
	//}}AFX_MSG
#ifdef XP_WIN32
    afx_msg BOOL OnHelpInfo(HELPINFO *);
#endif
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// Special class to control background color of static control
// CColorStatic window

class CColorStatic : public CStatic
{
// Construction
public:
	CColorStatic(COLORREF crTextColor = RGB(0,0,0),        // Default is black text
	             COLORREF crBackColor = RGB(192,192,192),   // on gray background
	             ED_BorderStyle nStyle = ED_BORDER_NONE);

// Attributes
private:
    CBrush         m_brush;
    COLORREF       m_crTextColor;
    COLORREF       m_crBackColor;
    ED_BorderStyle m_nBorderStyle;

public:
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CColorStatic)
    BOOL OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam,
                       LRESULT* pLResult);	
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CColorStatic();

	// Generated message map functions
protected:
	//{{AFX_MSG(CColorStatic)
		// NOTE - the ClassWizard will add and remove member functions here.
    //}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// Property page dialogs
/////////////////////////////////////////////////////////////////////////////
//
// Document Properties
// First page = general info
//    "Fixed" Meta Tags we always include
///////////////////////////////////////////////////////
class CDocInfoPage : public CNetscapePropertyPage
{
public:
    CDocInfoPage(CWnd *pParent, MWContext * pMWContext = NULL,
                 CEditorResourceSwitcher * pResourceSwitcher = NULL,
                 EDT_PageData * pPageData = NULL);

	//{{AFX_DATA(CDocInfoPage)
	enum { IDD = IDD_PAGE_DOC_INFO };
	CString	m_csTitle;
	CString	m_csURL;
	CString	m_csAuthor;
	CString	m_csDescription;
// TODO: Add these to this page?
//    CString	m_csCreateDate;
//    CString	m_csUpdateDate;
	CString	m_csKeyWords;
	CString	m_csClassification;
	//}}AFX_DATA
    

// Implementation
protected:              
    BOOL m_bActivated;
    MWContext * m_pMWContext;
    EDT_PageData * m_pPageData;
    int  m_iEquivCount;
    int  m_iMetaCount;

    BOOL OnSetActive();
    void OnOK();
    void OnHelp();
    void SetMetaData(char * pName, char * pValue);

private:
    // This is used to change resource hInstance back to EXE
    CEditorResourceSwitcher * m_pResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDocInfoPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

    void EnableApply();

	// Generated message map functions
	//{{AFX_MSG(CDocInfoPage)
	//}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

// This should be moved to XP code in the future
typedef struct _EDT_ColorSchemeData {
    char *     pSchemeName;
    LO_Color   ColorText;
    LO_Color   ColorLink;
    LO_Color   ColorActiveLink;
    LO_Color   ColorFollowedLink;
    LO_Color   ColorBackground;
    char *     pBackgroundImage;
} EDT_ColorSchemeData;


/////////////////////////////////////////////////////////////////////////////
// Document Color and Background Properties
//
class CDocColorPage : public CNetscapePropertyPage
{
public:
    CDocColorPage(CWnd *pParent, 
                  UINT nIDCaption = 0,             // Allows us to change tab text
                  UINT nIDFocus = 0,
                  MWContext * pMWContext = NULL,
                  CEditorResourceSwitcher * pResourceSwitcher = NULL,
                  EDT_PageData * pPageData = NULL);
	~CDocColorPage();

    void OnOK();
    void OnHelp();

	//{{AFX_DATA(CDocColorPage)
	enum { IDD = IDD_PAGE_DOC_COLORS };
	CString	m_csBackgroundImage;
	CString	m_csSelectedScheme;
    BOOL    m_bNoSave;
	//}}AFX_DATA


    BOOL       m_bImageChanged;
    BOOL       m_bValidImage;

private:
    HPALETTE     m_hPal;
    CColorButton m_TextColorButton;
    CColorButton m_LinkColorButton;
    CColorButton m_ActiveLinkColorButton;
    CColorButton m_FollowedLinkColorButton;
    CColorButton m_BackgroundColorButton;

    COLORREF   m_crBackground;
    COLORREF   m_crText;
    COLORREF   m_crLink;
    COLORREF   m_crActiveLink;
    COLORREF   m_crFollowedLink;
    
    // To save/restore current custom settings
    //   when changing to/from Browser colors 
    COLORREF   m_crCustomBackground;
    COLORREF   m_crCustomText;
    COLORREF   m_crCustomLink;
    COLORREF   m_crCustomActiveLink;
    COLORREF   m_crCustomFollowedLink;

    COLORREF   m_crBrowserBackground;
    COLORREF   m_crBrowserText;
    COLORREF   m_crBrowserLink;
    COLORREF   m_crBrowserFollowedLink;
    CString    m_csBrowserBackgroundImage;

   	// True if we are setting any colors or 
    //  background image. Following checkbox
    //  reflects user's choice for this
    BOOL       m_bCustomColors;
    // Flag to store last state to avoid
    //  unnessesary repainting
    BOOL       m_bWasCustomColors;

    // Build a list of schemes from a file
    //  with theire names and colors - 
    //  put names in a listbox
    XP_List  * m_pSchemeData;
    CString    m_csSaveScheme;

    // This is used to change resource hInstance back to EXE
    CEditorResourceSwitcher * m_pResourceSwitcher;

// Implementation
public:
    // Called after saving background image file to disk 
    // Has new image path+name
    void SetImageFileSaved(char * pImageURL);

protected:              
    BOOL           m_bActivated;
    MWContext    * m_pMWContext;
    EDT_PageData * m_pPageData;
    
    BOOL OnSetActive();

    // Get a color using our CColorPicker color picker
    //  Use string from nIDCaption for the dialog caption
    //  Return TRUE if OK is pressed
    BOOL ChooseColor(COLORREF * pColor, CColorButton * pButton, COLORREF crDefault, BOOL bBackground = FALSE);

    void UseBrowserColors(BOOL bRedraw = TRUE);
    void UseCustomColors(BOOL bRedraw = TRUE, BOOL bUnselectScheme = TRUE);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDocColorPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnKillActive();
	//}}AFX_VIRTUAL

	// Generated message map functions
	//{{AFX_MSG(CDocColorPage)
	afx_msg void OnColorsRadioButtons();
	afx_msg void OnUseBkgrndImage();
	afx_msg void OnChooseBkgrndImage();
	afx_msg void OnColorChoose();
	afx_msg void OnRemoveScheme();
	afx_msg void OnSaveScheme();
	afx_msg void OnSelchangeSchemeList();
	afx_msg void OnChangeBkgrndImage();
	afx_msg void OnKillfocusBkgrndImage();
	afx_msg void OnPaint();
	afx_msg void OnChooseTextColor();
	afx_msg void OnChooseLinkColor();
	afx_msg void OnChooseActivelinkColor();
	afx_msg void OnChooseFollowedlinkColor();
	afx_msg void OnChooseBkgrndColor();
	afx_msg void OnUseAsDefault();
	afx_msg void OnNoSave();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//////////////////////////////////////////////////////
// User-Named HTTP-EQUIV and META tags:
///////////////////////////////////////////////////////
class CDocMetaPage : public CNetscapePropertyPage
{
public:
    CDocMetaPage(CWnd *pParent, MWContext * pMWContext = NULL,
                 CEditorResourceSwitcher * pResourceSwitcher = NULL);

	//{{AFX_DATA(CDocMetaPage)
	enum { IDD = IDD_PAGE_DOC_ADVANCED };
	//}}AFX_DATA

// Implementation
protected:              
    BOOL m_bActivated;
    MWContext * m_pMWContext;
    EDT_PageData * m_pPageData;
    int  m_iEquivCount;
    int  m_iMetaCount;

    BOOL OnSetActive();
    void OnOK();
    void OnHelp();
    void EnableButtons(BOOL bEnable);
    void GetMetaData();
    void ClearNameAndValue();
    void SetMetaData(BOOL bHttpEquiv, char * pName, char * pValue);

private:
    // This is used to change resource hInstance back to EXE
    CEditorResourceSwitcher * m_pResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDocMetaPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	// Generated message map functions
	//{{AFX_MSG(CDocMetaPage)
	afx_msg void OnSelchangeEquivList();
	afx_msg void OnSelchangeMetaList();
	afx_msg void OnVarSet();
	afx_msg void OnVarDelete();
	afx_msg void OnVarNew();
	afx_msg void OnChangeVarNameOrValue();
	afx_msg void OnSetfocusEquivList();
	afx_msg void OnSetfocusMetaList();
	//}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

//////////////////////////////////////////////////////
// 3-Page Context properties pages
//    First page = Character or Image
//    2nd and 3rd are always Link, Paragraph
//
//////////////////////////////////////////////////////
class CCharacterPage : public CNetscapePropertyPage
{
public:
    CCharacterPage(CWnd *pParent,
                   MWContext * pMWContext = NULL,     // MUST be supplied
                   CEditorResourceSwitcher * pResourceSwitcher = NULL,
                   EDT_CharacterData *pData = NULL);  // MUST be supplied

    void OnOK();
    void OnHelp();

	//{{AFX_DATA(CCharacterPage)
    enum { IDD = IDD_PAGE_CHARACTER };
	//}}AFX_DATA

// Implementation
protected:              
    BOOL               m_bActivated;
    MWContext         *m_pMWContext;
    EDT_CharacterData *m_pData;
    EDT_PageData      *m_pPageData;
    COLORREF           m_crColor;
    COLORREF           m_crDefault;
    BOOL               m_bCustomColor;
    int                m_iNoChangeFontFace;
    int                m_iNoChangeFontSize;
    int                m_iOtherIndex;    
    BOOL               m_bMultipleFonts;
    BOOL               m_bFixedWidth;
    CColorButton       m_TextColorButton;
    int                m_bUseDefault;

    BOOL OnSetActive();
    
    void InitCheckbox(CButton *pButton, ED_TextFormat tf);
    void SetCharacterStyle(CButton *pButton, ED_TextFormat tf);
    int  SetCheck(CButton *pButton, ED_TextFormat tf);
    BOOL GetAndValidateFontSizes();
    
    CNSComboBox         m_FontSizeCombo;
private:
    // This is used to change resource hInstance back to EXE
    CEditorResourceSwitcher * m_pResourceSwitcher;
    
    // We use our user-drawn FontFace combo
    CNSComboBox  m_FontFaceCombo;

    // Display a message describing the current font face
    void SetFontFaceMessage(int iSel);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCharacterPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnKillActive();
	//}}AFX_VIRTUAL

	void OnChooseLocalFont();
    void EnableApply();
	
	// Generated message map functions
	//{{AFX_MSG(CCharacterPage)
	afx_msg void OnChooseColor();
	afx_msg void OnSelchangeFontColor();
	afx_msg void OnCheckSubscript();
	afx_msg void OnCheckSuperscript();
	afx_msg void OnClearAllStyles();
	afx_msg void OnClearTextStyles();
	afx_msg void OnSelchangeFontSize();
	afx_msg void OnSelchangeFontFace();
	afx_msg void OnChangeFontFace();
    afx_msg void OnSelendokFontFace();
    afx_msg void OnFontSizeDropdown();
 	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

///////////////////////////////////////////////////////
class CParagraphPage : public CNetscapePropertyPage
{
public:
    CParagraphPage(CWnd *pParent, MWContext * pMWContext = NULL,
                   CEditorResourceSwitcher * pResourceSwitcher = NULL);

    void OnOK();
    void OnHelp();

	//{{AFX_DATA(CParagraphPage)
	enum { IDD = IDD_PAGE_PARAGRAPH };
	int		m_iStartNumber;
	BOOL	m_bCompact;
	//}}AFX_DATA

// Implementation
protected:              
    BOOL          m_bActivated;
    MWContext    *m_pMWContext;
    ED_Alignment  m_Align;
    EDT_ListData *m_pListData;
    TagType       m_ParagraphStyle;
    
    TagType       m_ParagraphFormat;
    int           m_iParagraphStyle;
    int           m_iContainerStyle;
    int           m_iListStyle;
    int           m_iBulletStyle;
    int           m_iNumberStyle;
    int           m_iMixedStyle;

    BOOL OnSetActive();

    void UpdateLists();

private:
    // This is used to change resource hInstance back to EXE
    CEditorResourceSwitcher * m_pResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CParagraphPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	// Generated message map functions
	//{{AFX_MSG(CParagraphPage)
	afx_msg void OnAlignCenter();
	afx_msg void OnAlignLeft();
	afx_msg void OnAlignRight();
	afx_msg void OnSelchangeContainerStyle();
	afx_msg void OnSelchangeListItemStyle();
	afx_msg void OnChangeListStartNumber();
	afx_msg void OnSelchangeListStyles();
	afx_msg void OnSelchangeParagraphStyles();
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
// CBitmapPushButton
// This extends MFC CBitmapButton so that it can retain a pushed-down state
//   to act like a toolbar button

class CBitmapPushButton : public CBitmapButton
{
// Construction
public:
	CBitmapPushButton(BOOL bNoBorder = FALSE);

// Attributes
private:
    // TRUE when button is pushed down
    BOOL      m_bDown;
    BOOL      m_bFocus;
    BOOL      m_bSelected;
    HPALETTE  m_hPal;
    // For "borderless" button style
    BOOL      m_bNoBorder;

// Operations
public:
   // Load a single bitmap to use for all button states,
   //  which we draw instead of using mutliple bitmaps
   BOOL LoadBitmap(UINT nBitmapID);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBitmapPushButton)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CBitmapPushButton();
    
    // This is analogous to Checkbox or Radio button SetCheck()
    void SetCheck(BOOL bCheck);

	// Generated message map functions
protected:
	//{{AFX_MSG(CBitmapPushButton)
		// NOTE - the ClassWizard will add and remove member functions here.
    afx_msg int OnCreate( LPCREATESTRUCT lpCreateStruct );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	virtual void DrawItem(LPDRAWITEMSTRUCT lpDIS);

    // Allow access from our toolbar class
    friend class CDropdownToolbar;
};
//////////////////////////////////////////////////////////////////////////
// Include this in any dialog that can use the 
//   Alignment/Sizing/Border control set
//   Member variables are directly accessed by dialog,
//   but this handles the alignment BitmapButtons 
//
class CAlignControls {

// For simplicity, let owner dialog set/get data
//   directly from us
public:
    UINT           m_nIDAlign;        /* ID of current button that is down */
    ED_Alignment   m_EdAlign;         /* Align type used by edit core */

private:
    CWnd              *m_pParent;
    CBitmapPushButton  m_BtnAlignTop; 
    CBitmapPushButton  m_BtnAlignCenter; 
    CBitmapPushButton  m_BtnAlignCenterBaseline; 
    CBitmapPushButton  m_BtnAlignBottomBaseline; 
    CBitmapPushButton  m_BtnAlignBottom; 
    CBitmapPushButton  m_BtnAlignLeft; 
    CBitmapPushButton  m_BtnAlignRight; 

public:
    CAlignControls();
    BOOL Init(CWnd *pParent = NULL);
    BOOL OnAlignButtonClick(UINT nID);
    // Use member data to set dialog controls
    void SetAlignment();
    // Retrieve from controls
    ED_Alignment GetAlignment();
};


typedef struct {
    UINT                nCommandID;
    char              * pButtonName;
    UINT                nBitmapID;
    CBitmapPushButton * pButton;
} DropdownToolbarData;

/////////////////////////////////////////////////////////////////////////////
// CDropdownToolbar 
//   Popup window to give vertical fly-out 
//   ("sub-toolbar") appearance on toolbar

class CDropdownToolbar : public CWnd
{
// Construction
public:
	CDropdownToolbar(CWnd      * pParent = NULL, 
                     MWContext * pMWContext = NULL,     // Supply this to write status messages
                     RECT      * pCallerRect = NULL,  // If null, use mouse position to locate toolbar
                     UINT      nCallerID = 0,         // For tooltip over caller 
                     UINT      nInitialID = 0);

    virtual ~CDropdownToolbar();

private:    
    UINT      m_nButtonCount;
    UINT      m_nAllocatedCount;
    UINT      m_nInitialID;
    UINT      m_nCallerID;
    RECT      m_CallerRect;
    CWnd    * m_pParent;
    BOOL      m_bFirstButtonUp;

    MWContext * m_pMWContext;
    DropdownToolbarData * m_pData;

	CNSToolTip2 *m_pToolTip;
	
	// Implementation 
public:
    BOOL AddButton(LPSTR pButtonName, UINT nCommandID);
    BOOL AddButton(UINT nBitmapID, UINT nCommandID);
    void Show();

    // Need this to pass on tool tip messages
    virtual BOOL PreTranslateMessage(MSG* pMsg);

private:
#ifdef XP_WIN32
    // OnNotifyEx handler to get tooltip text
    BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
#endif

protected:
	// Generated message map functions
	//{{AFX_MSG(CDropdownToolbar)
    virtual void PostNcDestroy();
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);

//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

///////////////////////////////////////////////////
class CImagePage : public CNetscapePropertyPage
{
public:
    CImagePage(CWnd *pParent, MWContext * pMWContext = NULL,
               CEditorResourceSwitcher * pResourceSwitcher = NULL,
               EDT_ImageData * pData = NULL, BOOL bInsert = FALSE);

    BOOL m_bActivated;

    void OnOK();
    void OnHelp();
    BOOL OnSetActive();

	//{{AFX_DATA(CImagePage)
	enum { IDD = IDD_PAGE_IMAGE };
	CString	m_csHref;
	CString	m_csImage;
    BOOL    m_bNoSave;
    BOOL    m_bSetAsBackground;
	int		m_iHeight;
	int		m_iWidth;
	int		m_iHSpace;
	int		m_iVSpace;
	int		m_iBorder;
    BOOL    m_bDefaultBorder;
	int		m_iHeightPixOrPercent;
	int		m_iWidthPixOrPercent;
	//}}AFX_DATA
    UINT    m_nIDBitmap;

private:
	// These are changed via CImageAltDlg dialog
    CString	       m_csLowRes;
	CString	       m_csAltText;
    
    MWContext     *m_pMWContext;
    EDT_ImageData *m_pData;
    BOOL           m_bInsert;
    UINT           m_IDBitmap;
	CString	       m_csHrefStart;

	CString	       m_csImageStart;
    CString        m_csLastValidImage;
    CString        m_csLastValidLowRes;
    BOOL           m_bValidImage;
    BOOL           m_bImageChanged;
    BOOL           m_bLockAspect;
    BOOL           m_bOriginalButtonPressed;
    int32          m_iOriginalWidth;
    int32          m_iOriginalHeight;
    int32          m_iFullWidth;
    int32          m_iFullHeight;


private:
    // This is used to change resource hInstance back to EXE
    CEditorResourceSwitcher * m_pResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CImagePage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnKillActive();
	//}}AFX_VIRTUAL

// Implementation
public:
    // Called after saving file to disk -- has new image path+name
    void SetImageFileSaved(char * pImageURL, BOOL bMainImage);

protected:

    void SetLockAspectEnable();

    // Controls common to Image, Java, Plugin property pages
    CAlignControls m_AlignControls;

	// Generated message map functions
	//{{AFX_MSG(CImagePage)
	afx_msg void OnImageFile();
	afx_msg void OnChangeImageURL();
    afx_msg void OnSetAsBackground();
	afx_msg void OnKillfocusImage();
	afx_msg void OnNoSave();
	afx_msg void OnRemoveIsmap();
	afx_msg void OnImageOriginalSize();
	afx_msg void OnEditImage();
	afx_msg void OnAltTextLowRes();
	afx_msg void OnAlignBaseline();
	afx_msg void OnAlignBottom();
	afx_msg void OnAlignCenter();
	afx_msg void OnAlignLeft();
	afx_msg void OnAlignRight();
	afx_msg void OnAlignTop();
	afx_msg void OnAlignCenterBaseline();
	afx_msg void OnChangeHeight();
	afx_msg void OnChangeWidth();
	afx_msg void OnSelchangeHeightPixOrPercent();
	afx_msg void OnSelchangeWidthPixOrPercent();
	afx_msg void OnChangeSpaceHoriz();
	afx_msg void OnChangeSpaceVert();
	afx_msg void OnChangeBorder();
	afx_msg void OnExtraHTML();
    afx_msg void OnLockAspect();
    //}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CAltImageDlg dialog (modal popup over CImagePage for Alt text and Lowres Image)

class CImageAltDlg : public CDialog
{
// Construction
public:
	CImageAltDlg(CWnd *pParent, MWContext *pMWContext,
                 CString& csAltText, CString& csLowRes );
    ~CImageAltDlg();

 // Dialog Data
	//{{AFX_DATA(CImageAltDlg)
	enum { IDD = IDD_IMAGE_ALT };
	CString	m_csLowRes;
	CString	m_csAltText;
	//}}AFX_DATA

    MWContext     *m_pMWContext;
    BOOL           m_bImageChanged;

private:
    // This will change resource hInstance to Editor dll (in constructor)
    CEditorResourceSwitcher m_ResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CImageAltDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CImageAltDlg)
	afx_msg void OnHelp();
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg void OnLowResFile();
	afx_msg void OnChangeLowResURL();
	afx_msg void OnEditImage();
	//}}AFX_MSG
#ifdef XP_WIN32
    afx_msg BOOL OnHelpInfo(HELPINFO *);
#endif
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
// CExtraHTMLDlg dialog (modal popup over CImagePage or CLinkPage for Extra HTML

class CExtraHTMLDlg : public CDialog
{
// Construction
public:
	CExtraHTMLDlg(CWnd *pParent, 
                  char **ppExtraHTML,   // existing string passed in; new string put here
                  UINT nIDTagType);     // ID for tag string (e.g, IMG or HRER)
    ~CExtraHTMLDlg();

 // Dialog Data
	//{{AFX_DATA(CExtraHTMLDlg)
	enum { IDD = IDD_EXTRA_HTML };
	CString	m_csExtraHTML;
	//}}AFX_DATA

    // A PropertyPage caller should check this after DoModal() 
    //  and call SetModified(TRUE) if data changed
    BOOL    m_bDataChanged;

private:
    char **m_ppExtraHTML;   // We return results here
    UINT   m_nIDTagType;

    // This will change resource hInstance to Editor dll (in constructor)
    CEditorResourceSwitcher m_ResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CExtraHTMLDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CExtraHTMLDlg)
	afx_msg void OnHelp();
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
#ifdef XP_WIN32
    afx_msg BOOL OnHelpInfo(HELPINFO *);
#endif
	DECLARE_MESSAGE_MAP()
};
/////////////////////////////////////////////////////////////////////////////


class CLinkPage : public CNetscapePropertyPage
{
public:
    CLinkPage(CWnd *pParent, MWContext *pMWContext = NULL,
              CEditorResourceSwitcher * pResourceSwitcher = NULL,
              EDT_HREFData *pData = NULL, BOOL bInsert = FALSE,
              BOOL bMayHaveOtherLinks = FALSE, char **ppImage = NULL);

    ~CLinkPage();

    void OnOK();
    void OnHelp();

	//{{AFX_DATA(CLinkPage)
    enum { IDD = IDD_PAGE_LINK };   // Put Dialog ID here
	CString	m_csHref;
	CString	m_csAnchorEdit;
	CString	m_csAnchor;
	//}}AFX_DATA

private:
    BOOL          m_bActivated;
    MWContext    *m_pMWContext;
    EDT_HREFData *m_pData;
    BOOL          m_bInsert;
	CString	      m_csLastValidHref;
	BOOL	      m_bNewAnchorText;
    char         *m_szBaseDocument;
    char         *m_pTargetList;
    CString       m_csTargetFile;
    int           m_iTargetCount;
    char        **m_ppImage;
    BOOL          m_bHrefChanged;
    BOOL          m_bValidHref;
    BOOL          m_iCaretMovedBack;

    CNetscapeView *m_pView;

    // Selected text has link and other things,
    //  maybe other links. We use this
    //  to allow removing all links in selection
    BOOL           m_bMayHaveOtherLinks;    

// Implementation
protected:
    void ValidateHref();
    BOOL OnSetActive();
    int GetTargetsInDoc();
    int GetTargetsInFile();
    void SetHrefData();


private:
    // This is used to change resource hInstance back to EXE
    CEditorResourceSwitcher * m_pResourceSwitcher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLinkPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnKillActive();
	//}}AFX_VIRTUAL

	// Generated message map functions
	//{{AFX_MSG(CLinkPage)
	afx_msg void OnHrefFile();
	afx_msg void OnHrefUnlink();
	afx_msg void OnSelchangeTargetList();
	afx_msg void OnChangeHrefUrl();
	afx_msg void OnKillfocusHrefUrl();
	afx_msg void OnTargetsInCurrentDoc();
	afx_msg void OnTargetsInFile();
	afx_msg void OnExtraHTML();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////
#endif // _EDPROPS_H
#endif // EDITOR
