// PrefEditView.cpp : implementation of the CPrefEditView class
//
//   In this code, the "tree control" is the just that which lives in the view, 
//    and the "prefs tree" is an XML DOM tree representing the preferences and 
//    their layout.
//
//   The key to go from a tree control item to it's corresponding object in the
//    XML tree is the pref name, which is saved in the tree control item data
//    area. The code assumes the pref names are unique. 
//

#include "stdafx.h"
#include "PrefElement.h"
#include "PrefEditView.h"
#include "DlgEditPrefStr.h"
#include "DlgFind.h"
#include "DlgAdd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CPrefEditView

IMPLEMENT_DYNCREATE(CPrefEditView, CTreeView)

BEGIN_MESSAGE_MAP(CPrefEditView, CTreeView)
	//{{AFX_MSG_MAP(CPrefEditView)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnDblclk)
	ON_NOTIFY_REFLECT(NM_RCLICK, OnRclick)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_COMMAND(ID_EDITPREFITEM, OnEditPrefItem)
  ON_COMMAND(ID_FINDPREF, OnFindPref)
  ON_COMMAND(ID_FINDNEXTPREF, OnFindNextPref)
  ON_COMMAND(ID_ADDPREF, OnAddPref)
	ON_WM_KEYDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPrefEditView construction/destruction

// Protected.
CPrefEditView::CPrefEditView()
: m_pPrefXMLTree(NULL), m_pPrefsList(NULL), m_iNextElement(-1)
{

}


CPrefEditView::CPrefEditView(CString strXMLFile)
: m_pPrefXMLTree(NULL), m_strXMLFile(strXMLFile), m_pPrefsList(NULL), m_iNextElement(-1)
{
  InitXMLTree();
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

// expand the tree, or open the selected item for edit
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
  }

}

// open the Find Pref dialog
void CPrefEditView::DoFindFirst()   
{
  OnFindPref();
}

// find next item
void CPrefEditView::DoFindNext()
{
  OnFindNextPref();
}

// open the Add Pref dialog
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


BOOL CPrefEditView::InitXMLTree()
{
  // Create XML DOM instance.
  HRESULT hr = m_pPrefXMLTree.CreateInstance(__uuidof(DOMDocument));
  if (FAILED(hr))
  {
    MessageBox("Error creating MS XML DOM.", "Error", MB_OK);
    return FALSE;
  }

  // Load the prefs metadata. This is a representation of the prefs tree as
  // it should appear in the tree control.
  if (m_pPrefXMLTree)
  {
    CString strPrefsFileURL;
    strPrefsFileURL.Format("FILE://%s", m_strXMLFile);

    if (!m_pPrefXMLTree->load(strPrefsFileURL.GetBuffer(0)))
    {
      CString strError;
      strError.Format("Error loading preferences metadata %s.", strPrefsFileURL);
      MessageBox(strError, "Error", MB_OK);
      m_pPrefXMLTree = NULL;
      return FALSE;
    }
    if (m_pPrefXMLTree->parseError->errorCode != 0)
    {
      CString strError;
      strError.Format("Bad XML in %s.", strPrefsFileURL);
      MessageBox(strError, "Error", MB_OK);
      m_pPrefXMLTree = NULL;
      return FALSE;
    }
  }

  return TRUE;
}

// ItemData in each tree control element was created with new, so we need to 
// delete it.
void CPrefEditView::DeleteTreeCtrl(HTREEITEM hParent)
{
  ASSERT(hParent);

  CTreeCtrl &treeCtrl = GetTreeCtrl();

  // Delete the CString ojbect we created with new.
  CString* pstr = (CString*)treeCtrl.GetItemData(hParent);
  delete pstr;

  // Now call recursively for all children.
  HTREEITEM hCurrent = treeCtrl.GetNextItem(hParent, TVGN_CHILD);
  while (hCurrent != NULL)
  {
    DeleteTreeCtrl(hCurrent);
    hCurrent = treeCtrl.GetNextItem(hCurrent, TVGN_NEXT);
  }
}


// Given a pref node, add it to the tree ctrl. The pref name is
// later used to search for the pref node when the name is selected
// for edit.
HTREEITEM CPrefEditView::AddNodeToTreeCtrl(IXMLDOMNodePtr prefsTreeNode, HTREEITEM hTreeCtrlParent)
{
  ASSERT(prefsTreeNode != NULL);

  HTREEITEM hNewItem = NULL;

  //MessageBox(prefsTreeNode->xml, "XML", MB_OK);
  if (prefsTreeNode->nodeType == NODE_ELEMENT)
  {
    IXMLDOMElementPtr prefsTreeElement = (IXMLDOMElementPtr)prefsTreeNode;     
    CElementNode elementNode(prefsTreeElement);
    CString strNodeName = elementNode.GetNodeName();

    if ((strNodeName.CompareNoCase("PREFSGROUP") == 0) || (strNodeName.CompareNoCase("PREF") == 0))
    {
      // Will contain the pref name for pref elements, saved in the tree ctrl.
      CString* pstrPrefName = NULL;

      // Put this element in the tree ctrl.
      CString strLabel = elementNode.GetAttribute("uiname");
     
      int imageIndex = 0;     // tree ctrl images
      int imageIndexSel = 0;  // 0 is a closed folder

      // If this is a PREF element, create tree ctrl text, and save with a 
      // different image.
      if (strNodeName.CompareNoCase("PREF") == 0)
      {
        
        CPrefElement prefElement(prefsTreeElement);
        strLabel = prefElement.CreateTreeCtrlLabel();

        if (prefElement.IsLocked())
          imageIndexSel = imageIndex = 2;   // a locked padlock
        else
          imageIndexSel = imageIndex = 3;   // an unlocked padlock

   
        // This gets deleted in DeleteTreeCtrl(). This is how we get back to 
        // pref node when a tree ctrl node is selected for editing.
        pstrPrefName = new CString(prefElement.GetAttribute("prefname"));

      }

      CTreeCtrl &treeCtrl = GetTreeCtrl();
      hNewItem = treeCtrl.InsertItem( strLabel, imageIndex, imageIndexSel, hTreeCtrlParent, TVI_LAST);

      // Save a pointer to the prefname string so we can find the node in the
      // pref tree from the tree ctrl when the item is selected for edit.
      treeCtrl.SetItemData(hNewItem, (DWORD)pstrPrefName);

    }
  }

  // Recursively call for children.
  IXMLDOMNodeListPtr children = prefsTreeNode->GetchildNodes();
  if (children)
  {
    int numChildren = children->Getlength();
    for (int i = 0; i < numChildren; i++)
    {
      IXMLDOMNodePtr child = children->Getitem(i);
      AddNodeToTreeCtrl(child, hNewItem);
    }
  }

  return hNewItem;
}

/////////////////////////////////////////////////////////////////////////////
// CPrefEditView message handlers


// Given the pref name, for example browser.general.example, returns the 
// prefs tree element for that pref.
IXMLDOMElementPtr CPrefEditView::FindElementFromPrefname(CString& rstrPrefString)
{
  ASSERT(rstrPrefString.GetLength() > 0);
  ASSERT(m_pPrefXMLTree != NULL);

  IXMLDOMNodeListPtr prefsList = m_pPrefXMLTree->getElementsByTagName("PREF");
  for(int i = 0; i < prefsList->length; i++)
  {
    IXMLDOMElementPtr element = prefsList->Getitem(i);
    CPrefElement elementNode(element);
    if (rstrPrefString.CompareNoCase(elementNode.GetPrefName()) == 0)
      return element;
  }
  return NULL;
}

// Given a pref element node, returns the tree control item for it.
HTREEITEM CPrefEditView::FindTreeItemFromPrefname(HTREEITEM hItem, CString& rstrPrefName)
{

	if(!hItem)
		return NULL;

  // Get the pref name associated with this tree ctrl item.
  CTreeCtrl &treeCtrl = GetTreeCtrl();
  CString* pstrPrefName = (CString*)treeCtrl.GetItemData(hItem);

  if (pstrPrefName && pstrPrefName->CompareNoCase(rstrPrefName) == 0)
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

int CPrefEditView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
  // InitXMLTree() has to complete successfully first.
  ASSERT(m_pPrefXMLTree != NULL);

  if (m_pPrefXMLTree == NULL)
    return -1;

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

  ASSERT(m_pPrefXMLTree != NULL);


   // Load the tree control so it matches the XML tree.
  AddNodeToTreeCtrl(m_pPrefXMLTree->documentElement, NULL);

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
  ASSERT(m_pPrefXMLTree != NULL);

  m_pPrefsList = m_pPrefXMLTree->getElementsByTagName("PREF");
  m_iNextElement = 0;
  m_strFind = rstrFind;

  return FindNext();
}


// Assumes FindFirst was called first.
BOOL CPrefEditView::FindNext()
{
  ASSERT(m_pPrefXMLTree != NULL);

  if (m_pPrefsList == NULL || m_iNextElement == -1)
    return FALSE;

  while (m_iNextElement < m_pPrefsList->length)
  {
    IXMLDOMElementPtr pElement = m_pPrefsList->Getitem(m_iNextElement);

    CPrefElement elementNode(pElement);

    // Find the string in any field (pref name, value, description, etc.)
    if (elementNode.FindString(m_strFind))
    {
      // Select this item.
      SelectPref(elementNode.GetPrefName()); 
      m_iNextElement++;
      return TRUE;
    }

    m_iNextElement++;
  }

  m_iNextElement = -1;
  
  AfxMessageBox("No more matches.", MB_OK);

  return FALSE;
}



// Save the XML to a file.
BOOL CPrefEditView::DoSavePrefsTree(CString strFile)
{
  ASSERT(m_pPrefXMLTree != NULL);

  CElementNode root(m_pPrefXMLTree->documentElement);
  CString strXML = root.GetXML();

  FILE* fp = fopen(strFile, "w");
  if (!fp)
    return FALSE;

  CString strPreamble("<?xml version=\"1.0\"?>");
  if (!fwrite(strPreamble, strPreamble.GetLength(), 1, fp))
    return FALSE;

  if (!fwrite(strXML, strXML.GetLength(), 1, fp))
    return FALSE;

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
      FindFirst(dlg.m_strFind);
}

// "Find Next Pref" context menu command selected.
void CPrefEditView::OnFindNextPref()
{
  // If FindFirst not done before FindNext, call FindFirst first.
  if (m_iNextElement == -1)
    OnFindPref();
  else
    FindNext();
}


// User added prefs go into the root group.
HTREEITEM CPrefEditView::AddPref(CString& rstrPrefName, CString& rstrPrefDesc, CString& rstrPrefType)
{
  IXMLDOMNodePtr prefsTreeRootNode = m_pPrefXMLTree->documentElement;

  // Make sure the node is the root node element.
  if (prefsTreeRootNode->nodeType == NODE_ELEMENT)  
  {
    // and that it's a group.
    CElementNode group(prefsTreeRootNode);
    CString strNodeName = group.GetNodeName();
    if (strNodeName.CompareNoCase("PREFSGROUP") == 0)
    {
      
      // Create an XML element node.
      IXMLDOMNodePtr newNode = group.AddNode("PREF");

      // Modify it.
      CPrefElement newPref(newNode);
      newPref.Initialize(rstrPrefName, rstrPrefDesc, rstrPrefType);
      
      // Add it to the tree.
      CTreeCtrl &treeCtrl = GetTreeCtrl();
      HTREEITEM hRoot = treeCtrl.GetRootItem();
      ASSERT(hRoot);
      if (hRoot)
      {
        HTREEITEM hNewItem = AddNodeToTreeCtrl(newNode, hRoot);
        treeCtrl.SelectItem(hNewItem);

        EditSelectedPrefsItem();
      }
    }
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
    HTREEITEM hTreeCtrlItem = FindTreeItemFromPrefname(hRoot, dlg.m_strPrefName);    
    if (hTreeCtrlItem)
    {
      MessageBox("A pref of this name already exists.", MB_OK);
      treeCtrl.SelectItem(hTreeCtrlItem);
    }
    else
    {
      AddPref(dlg.m_strPrefName, dlg.m_strPrefDesc, strPrefType);
    }


  }

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

  // Get the pref name associated with this tree ctrl item.
  CString* pstrPrefName = (CString*)treeCtrl.GetItemData(hTreeCtrlItem);

  // All prefs should specify the prefname attribute in the XML pref element.
  // This assertion means that the pref's prefname attribute was not set, or
  // that perhaps a prefgroup doesn't have any children (if should, otherwise
  // it shouldn't be a group.)
  ASSERT(pstrPrefName);

  if (!pstrPrefName)
    return;

  // Get the pref tree node with that same pref name.
  IXMLDOMElementPtr prefsTreeElement = FindElementFromPrefname(*pstrPrefName);
  if (prefsTreeElement)
  {
    CPrefElement elementNode(prefsTreeElement);
    CString* pstrChoices = elementNode.MakeChoiceStringArray();
    
  	CDlgEditPrefStr dlg;
    dlg.m_strType = elementNode.GetPrefType();
    dlg.m_strTitle = elementNode.GetUIName();
    dlg.m_strDescription = elementNode.GetPrefDescription();
    dlg.m_strPrefName = *pstrPrefName;
    dlg.m_strValue = elementNode.GetPrefValue();
    dlg.m_bLocked = elementNode.IsLocked();
    dlg.m_pstrChoices = pstrChoices;
    dlg.m_strSelectedChoice = elementNode.GetSelectedChoiceString();
    dlg.m_strPrefFile = elementNode.GetPrefFile();
    dlg.m_strInstallFile = elementNode.GetInstallFile();
    dlg.m_bChoose = elementNode.IsChoose();


    if (dlg.DoModal() == IDOK)
    {

      // Adjust the prefs tree to reflect the changes. The dialog always 
      // sets m_strValue to a string which can go directly into the prefs
      // tree element. For example, bools set 'true' or 'false' and choices
      // set '0' or '1' or whatever the value for the selected choice.
      elementNode.SetPrefValue(dlg.m_strValue);
      elementNode.SetLocked(dlg.m_bLocked);
      elementNode.SetPrefFile(dlg.m_strPrefFile);
      elementNode.SetInstallFile(dlg.m_strInstallFile);

      // Adjust the tree control to reflect the changes.
      treeCtrl.SetItemText(hTreeCtrlItem, elementNode.CreateTreeCtrlLabel());
      treeCtrl.SetItemImage(hTreeCtrlItem, dlg.m_bLocked? 2 : 3, dlg.m_bLocked? 2 : 3);
      
    }

    delete [] pstrChoices;
	  
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
        submenu = 0;
      else
        submenu = 1;

      ShowPopupMenu(pt, submenu);
		}
	}
	
	*pResult = 0;
}

// Given a pref name, select the tree control item with that pref name.
void CPrefEditView::SelectPref(CString& rstrPrefName)
{
  CTreeCtrl &treeCtrl = GetTreeCtrl();
  HTREEITEM hRoot = treeCtrl.GetRootItem();

  HTREEITEM hTreeCtrlItem = FindTreeItemFromPrefname(hRoot, rstrPrefName);
  treeCtrl.SelectItem(hTreeCtrlItem);
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

  else
  {
	  CTreeView::OnKeyDown(nChar, nRepCnt, nFlags);
  }
}
