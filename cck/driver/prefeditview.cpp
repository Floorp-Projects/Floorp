// PrefEditView.cpp : implementation of the CPrefEditView class
//
// Reads the prefs from an XML files and display them in a tree control.
// Saves back to an XML file.
//
// Uses Expat XML parser. 
//

#include "stdafx.h"
#include "xmlparse.h"
#include "PrefsElement.h"
#include "PrefEditView.h"
#include "DlgEditPrefStr.h"
#include "DlgFind.h"
#include "DlgAdd.h"
#include "Encoding.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// bitmaps to use for the tree control as defined in IB_TREEIMAGES
const int bm_closedFolder = 0;
const int bm_openFolder = 1;
const int bm_lockedPad = 2;
const int bm_unlockedPad = 3;
const int bm_unlockedPadGray = 4;

// submenus
const int menu_pref = 0;
const int menu_group = 1;
const int menu_userAddedPref = 2;
/////////////////////////////////////////////////////////////////////////////
// CPrefEditView

IMPLEMENT_DYNCREATE(CPrefEditView, CTreeView)

BEGIN_MESSAGE_MAP(CPrefEditView, CTreeView)
	//{{AFX_MSG_MAP(CPrefEditView)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnDblclk)
	ON_NOTIFY_REFLECT(NM_RCLICK, OnRclick)
  ON_NOTIFY_REFLECT(TVN_ITEMEXPANDED, OnExpanded)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_COMMAND(ID_EDITPREFITEM, OnEditPrefItem)
  ON_COMMAND(ID_FINDPREF, OnFindPref)
  ON_COMMAND(ID_FINDNEXTPREF, OnFindNextPref)
  ON_COMMAND(ID_ADDPREF, OnAddPref)
  ON_COMMAND(ID_DELPREF, OnDelPref)
	ON_WM_KEYDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPrefEditView construction/destruction

// Protected.
CPrefEditView::CPrefEditView()
: m_hgroup(NULL), m_pParsingPrefElement(NULL), m_hNextFind(NULL)
{

}


CPrefEditView::CPrefEditView(CString strXMLFile)
: m_strXMLFile(strXMLFile), m_hgroup(NULL), m_pParsingPrefElement(NULL), m_hNextFind(NULL)
{
}

CPrefEditView::~CPrefEditView()
{

}

BOOL CPrefEditView::PreCreateWindow(CREATESTRUCT& cs)
{
  cs.style |= TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS;
	return CTreeView::PreCreateWindow(cs);
}



/////////////////////////////////////////////////////////////////////////////
// CPrefEditView public operations
//
// These are for controls outside this view to call, for example, buttons 
// on the same wizard page as this view.
//

// Expand the tree, or open the selected item for edit.
void CPrefEditView::DoOpenItem()    
{
  CTreeCtrl &treeCtrl = GetTreeCtrl();
	HTREEITEM hTreeCtrlItem = treeCtrl.GetSelectedItem();
  if (!treeCtrl.ItemHasChildren(hTreeCtrlItem)) // no children == leaf node == pref 
  {
    EditSelectedPrefsItem();
  }
  else
  {
    treeCtrl.Expand(hTreeCtrlItem, TVE_TOGGLE);
    
    // This doesn't call OnExpanded() as you'd expect. Fix up the images.
    if (treeCtrl.GetItemState(hTreeCtrlItem, TVIS_EXPANDED) & TVIS_EXPANDED) 
      treeCtrl.SetItemImage(hTreeCtrlItem, bm_openFolder, bm_openFolder);
    else
      treeCtrl.SetItemImage(hTreeCtrlItem, bm_closedFolder, bm_closedFolder);
  }

}

// Open the Find Pref dialog.
void CPrefEditView::DoFindFirst()   
{
  OnFindPref();
}

// Find next item.
void CPrefEditView::DoFindNext()
{
  OnFindNextPref();
}

// Open the Add Pref dialog.
void CPrefEditView::DoAdd()
{
  OnAddPref();
}



/////////////////////////////////////////////////////////////////////////////
// CPrefEditView diagnostics

#ifdef _DEBUG
void CPrefEditView::AssertValid() const
{
	CTreeView::AssertValid();
}

void CPrefEditView::Dump(CDumpContext& dc) const
{
	CTreeView::Dump(dc);
}

#endif //_DEBUG


// ItemData in some tree control element was created with new, so we need to 
// delete it.
void CPrefEditView::DeleteTreeCtrl(HTREEITEM hParent)
{
  ASSERT(hParent);

  CTreeCtrl &treeCtrl = GetTreeCtrl();

  // Delete the prefelement object we created with new.
  CPrefElement* pe = (CPrefElement*)treeCtrl.GetItemData(hParent);
  delete pe;

  // Now call recursively for all children.
  HTREEITEM hCurrent = treeCtrl.GetNextItem(hParent, TVGN_CHILD);
  while (hCurrent != NULL)
  {
    DeleteTreeCtrl(hCurrent);
    hCurrent = treeCtrl.GetNextItem(hCurrent, TVGN_NEXT);
  }
}


/////////////////////////////////////////////////////////////////////////////
// CPrefEditView message handlers


// Given a pref name, returns the tree control item for it.
HTREEITEM CPrefEditView::FindTreeItemFromPrefname(HTREEITEM hItem, CString& rstrPrefName)
{

	if(!hItem)
		return NULL;

  // Get the pref name associated with this tree ctrl item.
  CTreeCtrl &treeCtrl = GetTreeCtrl();
  CPrefElement* pe = (CPrefElement*)treeCtrl.GetItemData(hItem);

  if (pe && pe->GetPrefName().CompareNoCase(rstrPrefName) == 0)
		return hItem;

	HTREEITEM hRet = NULL;
	HTREEITEM hChild = treeCtrl.GetChildItem(hItem);
	if(hChild)
		hRet = FindTreeItemFromPrefname(hChild, rstrPrefName);

	if(hRet == NULL)
	{
		HTREEITEM hSibling = treeCtrl.GetNextSiblingItem(hItem);
		if(hSibling)
			hRet = FindTreeItemFromPrefname(hSibling, rstrPrefName);
	}

	return hRet;
  
}


// Open a dialog to edit the pref.
void CPrefEditView::OnDblclk(NMHDR* pNMHDR, LRESULT* pResult) 
{
  CTreeCtrl &treeCtrl = GetTreeCtrl();
	HTREEITEM hTreeCtrlItem = treeCtrl.GetSelectedItem();
  if (!treeCtrl.ItemHasChildren(hTreeCtrlItem)) // no children == leaf node == pref 
  {
    EditSelectedPrefsItem();
    *pResult = 0;
  }

} 



// Called by the XML parser when a new element is read. 
// Turn around and call the prefView object.
static void startElementC(void *userData, const char *name, const char **atts)
{
  ((CPrefEditView*)userData)->startElement(name, atts);
}

// Called by the XML parser when new data is read.
// Turn around and call the prefView object.
static void characterDataC(void *userData, const XML_Char *s, int len)
{
  ((CPrefEditView*)userData)->characterData(s, len);
}

// Called by the XML parser when an element close tag is read.
// Turn around and call the prefView object.
static void endElementC(void *userData, const char *name)
{
  ((CPrefEditView*)userData)->endElement(name);
}

// XML parser helper. Called when start of an element tag is encountered.
void CPrefEditView::startElement(const char *name, const char **atts)
{
  // If new prefsgroup, add an item to the tree control to hold other items.
  if (stricmp(name, "PREFSGROUP") == 0)
  {
    // Assumes this element has one tag and that it's the uiname for the group.
    CString strLabel = atts[1];

    // Open a new level in the tree control.
    CTreeCtrl &treeCtrl = GetTreeCtrl();
    m_hgroup = treeCtrl.InsertItem( strLabel, bm_closedFolder, bm_closedFolder, m_hgroup, TVI_LAST);

  }

  // If new pref element, create a new pref element, and let it handle its own 
  // initialization from the atts.
  else if (stricmp(name, "PREF") == 0)
  {
    // Can't have nested pref elements.
    ASSERT(m_pParsingPrefElement == NULL);
    m_pParsingPrefElement = new CPrefElement;
  }

  else if (stricmp(name, "METAPREFS") == 0)
  {
    ASSERT(atts[2]);  // there must be a clientversion and subversion attribute

    // Process the list of attribute/value pairs.
    int i = 0;
    while(atts[i])
    {
      const char* attrName = atts[i++];
      const char* attrVal = atts[i++];

      if (stricmp(attrName, "clientversion") == 0)
        m_strXMLVersion = attrVal;
      else if (stricmp(attrName, "subversion") == 0)
        m_strXMLSubVersion = attrVal;
    }

  }

  // Pass on all subelements to the pref element object to handle.
  if (m_pParsingPrefElement)
    m_pParsingPrefElement->startElement(name, atts);

}

// XML parser helper. Called when non-tag stuff is encountered.
void CPrefEditView::characterData(const XML_Char *s, int len)
{
  // Let the prefs element handle it.
  if (m_pParsingPrefElement)
    m_pParsingPrefElement->characterData(s, len);
}

// XML parser helper. Called when closing tag is found.
void CPrefEditView::endElement(const char *name)
{
  CTreeCtrl &treeCtrl = GetTreeCtrl();

  if (stricmp(name, "PREFSGROUP") == 0)
  {
    // back up one level in the tree control
    ASSERT(m_hgroup);
    m_hgroup = treeCtrl.GetParentItem(m_hgroup);
    
  }
  else if (stricmp(name, "PREF") == 0)
  {
    ASSERT(m_pParsingPrefElement);

    if (m_pParsingPrefElement)
      m_pParsingPrefElement->endElement(name);

    // Pref tag is closed. Add this pref to current group in the tree control.
    InsertPrefElement(m_pParsingPrefElement, m_hgroup);

    m_pParsingPrefElement = NULL;

  }
    
}

// Insert pref element into the tree control in the specified group.
HTREEITEM CPrefEditView::InsertPrefElement(CPrefElement* pe, HTREEITEM group)
{
  ASSERT(pe);
  ASSERT(group);

  int imageIndex = bm_closedFolder;

  if (pe->IsDefault())
    imageIndex = bm_unlockedPadGray;
  else
    imageIndex = bm_unlockedPad;

  if (pe->IsLocked() && pe->IsLockable())
    imageIndex = bm_lockedPad;

  CTreeCtrl &treeCtrl = GetTreeCtrl();
  HTREEITEM hNewItem = treeCtrl.InsertItem(ConvertUTF8toANSI(pe->GetPrettyNameValueString()), imageIndex, imageIndex, group, TVI_LAST);

  // Save pointer to this pref element in the tree.
  treeCtrl.SetItemData(hNewItem, (DWORD)pe);

  return hNewItem;

}

// Load the tree control with data from the XML file.
BOOL CPrefEditView::LoadTreeControl()
{
  CTreeCtrl &treeCtrl = GetTreeCtrl();
  m_hgroup = treeCtrl.GetRootItem();

  XML_Parser parser = XML_ParserCreate(NULL);
  XML_SetElementHandler(parser, startElementC, endElementC);
  XML_SetCharacterDataHandler(parser, characterDataC);
  XML_SetUserData(parser, this);

  // Load the XML from the file.
  CString strPrefsXML;
  FILE* pFile = fopen(m_strXMLFile, "r");
  if (!pFile)
  {
    CString strMsg;
    strMsg.Format("Can't open the file %s.",  m_strXMLFile);
    AfxMessageBox(strMsg, MB_OK);
    return FALSE;
  }

  // obtain file size.
  fseek(pFile , 0 , SEEK_END);
  long lSize = ftell(pFile);
  rewind(pFile);

  // allocate memory to contain the whole file.
  char* buffer = (char*) malloc (lSize + 1);
  if (buffer == NULL)
  {
    AfxMessageBox("Memory allocation error.", MB_OK);
    return FALSE;
  }

  buffer[lSize] = '\0';

  // copy the file into the buffer.
  size_t len = fread(buffer,1,lSize,pFile);
  
  // The whole file is loaded in the buffer.

  int done = 0;
  if (!XML_Parse(parser, buffer, len, done)) 
  {
    CString strMsg;
    strMsg.Format("%s in file %s at line %d.",XML_ErrorString(XML_GetErrorCode(parser)), m_strXMLFile, XML_GetCurrentLineNumber(parser));
    AfxMessageBox(strMsg, MB_OK);
    free(buffer);
    return FALSE;  
  }

  XML_ParserFree(parser);

  free(buffer);
  return TRUE;
  
}

int CPrefEditView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CTreeView::OnCreate(lpCreateStruct) == -1)
		return -1;
	
  // Get the tree control.
	CTreeCtrl &treeCtrl = GetTreeCtrl();

  // create the image list for the tree control
  m_imageList.Create(IDB_TREEIMAGES, 17, 1, RGB(0,255,255));
  treeCtrl.SetImageList (&m_imageList, TVSIL_NORMAL);

  HTREEITEM hRoot = treeCtrl.GetRootItem();
  if (hRoot)
    DeleteTreeCtrl(hRoot);

  // Load the tree control so it matches the XML.
  LoadTreeControl();

  treeCtrl.Expand(treeCtrl.GetRootItem(), TVE_EXPAND);

	return 0;
}


void CPrefEditView::OnDestroy() 
{
  // Clean up tree ctrl pref name memory allocations.	
	CTreeCtrl &treeCtrl = GetTreeCtrl();
  HTREEITEM hRoot = treeCtrl.GetRootItem();
  if (hRoot)
    DeleteTreeCtrl(hRoot);

	CTreeView::OnDestroy();
	
}

// Find a pref containing a string in one of its fields.
BOOL CPrefEditView::FindFirst(CString& rstrFind)
{
  CTreeCtrl &treeCtrl = GetTreeCtrl();
  m_hNextFind = treeCtrl.GetRootItem();
  m_strFind = rstrFind;

  return FindNext();
}

// Returns the next item in the tree control.
HTREEITEM CPrefEditView::GetNextItem(HTREEITEM hItem)
{
  HTREEITEM hti;

  CTreeCtrl &treeCtrl = GetTreeCtrl();
  if (treeCtrl.ItemHasChildren(hItem))
  {
    return treeCtrl.GetChildItem(hItem);           // return first child
  }
  else
  {
    // return next sibling item
    // Go up the tree to find a parent's sibling if needed.
    while((hti = treeCtrl.GetNextSiblingItem(hItem)) == NULL)
    {
      if((hItem = treeCtrl.GetParentItem(hItem)) == NULL)
        return NULL;
    }
  }
  return hti;
}



// Find next match. Assumes FindFirst was called first.
BOOL CPrefEditView::FindNext()
{

  if ((m_hNextFind == NULL) || m_strFind.IsEmpty())
    return FALSE;
  CTreeCtrl &treeCtrl = GetTreeCtrl();

  while (m_hNextFind)
  {
    CPrefElement* pe = (CPrefElement*)treeCtrl.GetItemData(m_hNextFind);

    if (pe && pe->FindString(m_strFind))
    {
      treeCtrl.SelectItem(m_hNextFind);
      m_hNextFind = GetNextItem(m_hNextFind);
      return TRUE;
    }

    m_hNextFind = GetNextItem(m_hNextFind);

  }

  AfxMessageBox("No more matches.", MB_OK);
  return FALSE;
}

// Save this item and all children to a file as XML. Recursive.
void CPrefEditView::WriteXMLItem(FILE* fp, int iLevel, HTREEITEM hItem)
{
  ASSERT(fp);
  ASSERT(iLevel >= 0);
  ASSERT(hItem != NULL);

  CTreeCtrl &treeCtrl = GetTreeCtrl();

  const int iIndentSize = 2;
  int iIndent = iLevel * iIndentSize;

  // for this item and all siblings 
  while (hItem)
  {
    // if has children, then it's a PREFSGROUP
    if (treeCtrl.ItemHasChildren(hItem))
    {
      // write the opening group tag
      CString strTag;
      strTag.Format("%*s<PREFSGROUP uiname=\"%s\">\n\n", iIndent, " ", treeCtrl.GetItemText(hItem));
      fwrite(strTag, strTag.GetLength(), 1, fp);

      // call self on first child
      WriteXMLItem(fp, iLevel+1, treeCtrl.GetChildItem(hItem));

      // write the closing tag
      strTag.Format("%*s</PREFSGROUP>\n\n", iIndent, " ");
      fwrite(strTag, strTag.GetLength(), 1, fp);
    }

    // if no children, then it's a PREF
    else
    {
      // write the opening pref tag
      // write the info
      // write the closing pref tag

      CPrefElement* pe = (CPrefElement*)treeCtrl.GetItemData(hItem);
      ASSERT(pe);
      if (pe)
      {
        CString strElement = pe->XML(iIndentSize, iLevel);
        strElement += "\n";
        fwrite(strElement, strElement.GetLength(), 1, fp);
      }
    }

    hItem = treeCtrl.GetNextSiblingItem(hItem);
  }
}


// Save the XML to a file.
BOOL CPrefEditView::DoSavePrefsTree(CString strFile)
{
  FILE* fp = fopen(strFile, "w");
  if (!fp)
    return FALSE;

  CString strPreamble("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n");
  if (!fwrite(strPreamble, strPreamble.GetLength(), 1, fp))
  {
    fclose(fp);
    return FALSE;
  }

  // start the outermost tag 
  CString strOuter;
  strOuter.Format("<METAPREFS clientversion=\"%s\" subversion=\"%s\">\n\n", m_strXMLVersion, m_strXMLSubVersion);
  fwrite(strOuter, strOuter.GetLength(), 1, fp);


  CTreeCtrl &treeCtrl = GetTreeCtrl();
  HTREEITEM hRoot = treeCtrl.GetRootItem();
  WriteXMLItem(fp, 1, hRoot);

  // close the outermost tag
  strOuter = "</METAPREFS>";
  fwrite(strOuter, strOuter.GetLength(), 1, fp);

  fclose(fp);
  return TRUE;
}



// Pop up menu for Pref items.
void CPrefEditView::ShowPopupMenu( CPoint& point, int submenu )
{
  if (point.x == -1 && point.y == -1)
  {
    //keystroke invocation
    CRect rect;
    GetClientRect(rect);
    ClientToScreen(rect);

    point = rect.TopLeft();
    point.Offset(5, 5);
  }

  CMenu menu;
  VERIFY(menu.LoadMenu(IDR_POPUP_PREFTREE_MENU));

  CMenu* pPopup = menu.GetSubMenu(submenu);
  ASSERT(pPopup != NULL);
  CWnd* pWndPopupOwner = this;

  pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, pWndPopupOwner);
}

// "Edit Pref" context menu command selected.
void CPrefEditView::OnEditPrefItem() 
{
  EditSelectedPrefsItem();	
}

// "Find Pref" context menu command selected.
void CPrefEditView::OnFindPref()
{
  CDlgFind dlg;
  if (dlg.DoModal() == IDOK)
    if (!dlg.m_strFind.IsEmpty())
      FindFirst(ConvertANSItoUTF8(dlg.m_strFind));
}

// "Find Next Pref" context menu command selected.
void CPrefEditView::OnFindNextPref()
{
  // If FindFirst not done before FindNext, call FindFirst first.
  if (m_hNextFind == NULL)
    OnFindPref();
  else
    FindNext();
}


// User added prefs go into the root group.
HTREEITEM CPrefEditView::AddPref(CString& rstrPrefName, CString& rstrPrefDesc, CString& rstrPrefType)
{
  // Create it.
  CPrefElement* newPref = new CPrefElement(rstrPrefName, rstrPrefDesc, rstrPrefType);

  newPref->SetUserAdded(TRUE);
    
  // Add it to the tree.
  CTreeCtrl &treeCtrl = GetTreeCtrl();
  HTREEITEM hRoot = treeCtrl.GetRootItem();
  ASSERT(hRoot);
  if (hRoot)
  {
    HTREEITEM hNewItem = InsertPrefElement(newPref, hRoot);
    treeCtrl.SelectItem(hNewItem);

    EditSelectedPrefsItem();
  }

  return NULL;
}

// Open dialog to add a new pref, then open another to edit it.
void CPrefEditView::OnAddPref()
{
  CDlgAdd dlg;
  if (dlg.DoModal() == IDOK)
  {
    if (dlg.m_strPrefName.IsEmpty())
      return;

    CString strPrefType;
    if (dlg.m_intPrefType == 0)
      strPrefType = "string";
    else if (dlg.m_intPrefType == 1)
      strPrefType = "int";
    else
      strPrefType = "bool";

    // Disallow if the pref name already exists.
    CTreeCtrl &treeCtrl = GetTreeCtrl();
    HTREEITEM hRoot = treeCtrl.GetRootItem();
    HTREEITEM hTreeCtrlItem = FindTreeItemFromPrefname(hRoot, ConvertANSItoUTF8(dlg.m_strPrefName));    
    if (hTreeCtrlItem)
    {
      MessageBox("A pref of this name already exists.", MB_OK);
      treeCtrl.SelectItem(hTreeCtrlItem);
    }
    else
    {
      // Anything in the XML file must be in UTF-8. User might have entered Hi-ASCII and that would throw off XML parser.
      AddPref(ConvertANSItoUTF8(dlg.m_strPrefName), ConvertANSItoUTF8(dlg.m_strPrefDesc), ConvertANSItoUTF8(strPrefType));
    }


  }

}

// Delete the user-added pref.
void CPrefEditView::OnDelPref()
{
  CTreeCtrl &treeCtrl = GetTreeCtrl();
	HTREEITEM hTreeCtrlItem = treeCtrl.GetSelectedItem();

  // Can't edit a pref group--only a pref (leaf).
  if (treeCtrl.ItemHasChildren(hTreeCtrlItem))
    return;

  // If user added-pref, delete the CPrefElement that was created with new.
  CPrefElement* pe = (CPrefElement*)treeCtrl.GetItemData(hTreeCtrlItem);
  ASSERT(pe); // There must be a CPrefElement object for each pref in the tree.
  if ((pe == NULL) || !pe->IsUserAdded())
    return;

  delete pe;

  // Remove item from the tree.
  treeCtrl.DeleteItem(hTreeCtrlItem);
}

// Open the edit dialog box for the selected pref item. A pref should be selected,
// not a group.
void CPrefEditView::EditSelectedPrefsItem()
{
  CTreeCtrl &treeCtrl = GetTreeCtrl();
	HTREEITEM hTreeCtrlItem = treeCtrl.GetSelectedItem();

  ASSERT(!treeCtrl.ItemHasChildren(hTreeCtrlItem)); // no children == leaf node == pref 

  // Can't edit a pref group--only a pref (leaf).
  if (treeCtrl.ItemHasChildren(hTreeCtrlItem))
    return;

  CPrefElement* pe = (CPrefElement*)treeCtrl.GetItemData(hTreeCtrlItem);

  ASSERT(pe);

  if (!pe)
    return;

  CDlgEditPrefStr dlg;
  dlg.m_strType = pe->GetPrefType();      // internal use only -- always ANSI
  dlg.m_strTitle = ConvertUTF8toANSI(pe->GetUIName());
  dlg.m_strDescription = ConvertUTF8toANSI(pe->GetPrefDescription());
  dlg.m_strPrefName = ConvertUTF8toANSI(pe->GetPrefName());
  dlg.m_strValue = ConvertUTF8toANSI(pe->GetPrefValue());
  dlg.m_bLocked = pe->IsLocked();
  dlg.m_pstrChoices = pe->GetChoiceStringArray(); // internal use only -- always ANSI
  dlg.m_strSelectedChoice = pe->GetSelectedChoiceString();
  dlg.m_bChoose = pe->IsChoose();
  dlg.m_bRemoteAdmin = pe->IsRemoteAdmin();
  dlg.m_bLockable = pe->IsLockable();
  if (pe->IsChoose())
    dlg.m_strDefault = pe->GetDefaultChoiceString();  // internal use only -- always ANSI
  else
    dlg.m_strDefault = ConvertUTF8toANSI(pe->GetDefaultValue());

  if (dlg.DoModal() == IDOK)
  {

    // Adjust the prefs tree to reflect the changes. The dialog always 
    // sets m_strValue to a string which can go directly into the prefs
    // tree element. For example, bools set 'true' or 'false' and choices
    // set '0' or '1' or whatever the value for the selected choice.
    pe->SetPrefValue(ConvertANSItoUTF8(dlg.m_strValue));
    pe->SetLocked(dlg.m_bLocked);
    pe->SetRemoteAdmin(dlg.m_bRemoteAdmin);

    // Adjust the tree control to reflect the changes.
    treeCtrl.SetItemText(hTreeCtrlItem, ConvertUTF8toANSI(pe->GetPrettyNameValueString()));

    int imageIndex = 0;     // tree ctrl images

    if (pe->IsDefault())
      imageIndex = bm_unlockedPadGray;
    else
      imageIndex = bm_unlockedPad;

    if (pe->IsLocked() && pe->IsLockable())
      imageIndex = bm_lockedPad;

    treeCtrl.SetItemImage(hTreeCtrlItem, imageIndex, imageIndex);

  }
}

// This is to make context menus work properly--there is a quirk in 
// CTreeView where you have to double-right click for context menus
// to work.
void CPrefEditView::OnRclick(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CTreeCtrl & treeCtrl = GetTreeCtrl();

	// Get the cursor position for this message 
	DWORD dwPos = GetMessagePos();

	// Convert the coords into a CPoint structure 
	CPoint pt(LOWORD( dwPos ), HIWORD( dwPos ));
	CPoint spt;

	spt = pt;

	// convert to screen coords for the hittesting to work 
	treeCtrl.ScreenToClient( &spt );

	UINT test;
	HTREEITEM hTreeCtrlItem = treeCtrl.HitTest(spt, &test);

	if (hTreeCtrlItem != NULL)
	{
		// Is the click actually *on* the item? 
		if (test & TVHT_ONITEM)
		{
      // Select the item.
      treeCtrl.SelectItem(hTreeCtrlItem);

      int submenu;
      if (!treeCtrl.ItemHasChildren(hTreeCtrlItem)) // no children == leaf node == pref 
      {
        CPrefElement* pe = (CPrefElement*)treeCtrl.GetItemData(hTreeCtrlItem);
        ASSERT(pe); // there must be a CPrefElement object for each leaf node.
        if (!pe)
          return;

        if (pe->IsUserAdded())
          submenu = menu_userAddedPref;
        else
          submenu = menu_pref;
      }
      else
      {
        submenu = menu_group;
      }

      ShowPopupMenu(pt, submenu);
		}
	}
	
	*pResult = 0;
}

void CPrefEditView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
  if (nChar == VK_F3) 
  {
    OnFindNextPref();
  }

  else if ((nChar == 0x46) && GetAsyncKeyState(VK_CONTROL)) // Ctrl+F
  {
    OnFindPref();
  }

  else if (nChar == VK_DELETE)
  {
    OnDelPref();
  }

  else
  {
	  CTreeView::OnKeyDown(nChar, nRepCnt, nFlags);
  }
}

// Check to see if any of the prefs were marked for remote admin
// if any have, set the global RemoteAdminFound to true

BOOL CPrefEditView::CheckForRemoteAdmins()
{
	CTreeCtrl &treeCtrl = GetTreeCtrl();
	HTREEITEM hRoot = treeCtrl.GetRootItem();

	return (IsRemoteAdministered(hRoot) ? TRUE : FALSE);
}


// Given a tree item, returns TRUE if it or any children are remote
// administered 
bool CPrefEditView::IsRemoteAdministered(HTREEITEM hItem)
{

	if(!hItem)
		return FALSE;

    // Get the pref name associated with this tree ctrl item.
    CTreeCtrl &treeCtrl = GetTreeCtrl();
    CPrefElement* pe = (CPrefElement*)treeCtrl.GetItemData(hItem);

    if (pe && pe->IsRemoteAdmin())
		return TRUE;

	bool bRemoted = FALSE;
	HTREEITEM hChild = treeCtrl.GetChildItem(hItem);
	
	if(hChild)
		bRemoted = IsRemoteAdministered(hChild);

	if(bRemoted == FALSE)
	{
		HTREEITEM hSibling = treeCtrl.GetNextSiblingItem(hItem);
		if(hSibling)
			bRemoted = IsRemoteAdministered(hSibling);
	}

	return bRemoted;
  
}

// Show expanded groups as open folder, collapsed groups as closed folder.
void CPrefEditView::OnExpanded(NMHDR* pNMHDR, LRESULT* pResult)
{
  LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)pNMHDR;

  if (pnmtv)
  {
    int imageIndex = 0;     // tree ctrl images

    HTREEITEM hTreeCtrlItem = pnmtv->itemNew.hItem;
    if (hTreeCtrlItem)
    {
      CTreeCtrl &treeCtrl = GetTreeCtrl();
      if ((pnmtv->itemNew.state & TVIS_EXPANDED) == TVIS_EXPANDED)
        treeCtrl.SetItemImage(hTreeCtrlItem, bm_openFolder, bm_openFolder);
      else
        treeCtrl.SetItemImage(hTreeCtrlItem, bm_closedFolder, bm_closedFolder);

    }

  }

  *pResult = 0;

 }

