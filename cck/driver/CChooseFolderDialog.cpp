#include "stdafx.h"
#include "CChooseFolderDialog.h"

/////////////////////////////////////////////////////////////////////////////
// CChooseFolderDialog	
//	   

BEGIN_MESSAGE_MAP(CChooseFolderDialog, CDialog)
    ON_BN_CLICKED(IDC_NEW_FOLDER, OnNewFolder)
	ON_CBN_SELCHANGE(IDC_COMBO_SERVERS, OnSelectServer)
	ON_CBN_SELCHANGE(IDC_COMBO_FOLDERS, OnSelectFolder)
END_MESSAGE_MAP()

CChooseFolderDialog::CChooseFolderDialog(CWnd *pParent, char* pFolderPath, int nTypeID)
	: CDialog(IDD, pParent)
{
	m_pFolderPath =  pFolderPath;
	m_nTypeID = nTypeID;

	if (nTypeID == TYPE_SENTMAIL || nTypeID == TYPE_SENTNEWS)
		m_nDefaultID = MK_MSG_SENT_L10N_NAME;
	else if (nTypeID == TYPE_DRAFT)
		m_nDefaultID = MK_MSG_DRAFTS_L10N_NAME;
	else if (nTypeID == TYPE_TEMPLATE)
		m_nDefaultID = MK_MSG_TEMPLATES_L10N_NAME;
}

BOOL CChooseFolderDialog::OnInitDialog()
{
	BOOL ret = CDialog::OnInitDialog();

	if (m_nTypeID == TYPE_SENTNEWS)
		SetDlgItemText(IDC_STATIC_TITLE, szLoadString(IDS_COPY_NEWS_MSG));
	else if (m_nTypeID == TYPE_DRAFT)
		SetDlgItemText(IDC_STATIC_TITLE, szLoadString(IDS_COPY_DRAFTS));
	else if (m_nTypeID == TYPE_TEMPLATE)
		SetDlgItemText(IDC_STATIC_TITLE, szLoadString(IDS_COPY_TEMPLATES));

	CString formatString, defaultTitle;
	formatString.LoadString(IDS_SPECIAL_FOLDER);
	defaultTitle.Format(LPCTSTR(formatString), XP_GetString(m_nDefaultID));
	SetDlgItemText(IDC_RADIO_SENT, LPCTSTR(defaultTitle));

	if ( ret ) {
		// Subclass Server combo
		m_ServerCombo.SubclassDlgItem( IDC_COMBO_SERVERS, this );
		m_ServerCombo.NoPrettyName();
		m_ServerCombo.PopulateMailServer( WFE_MSGGetMaster() );
		if (SetServerComboCurSel(m_ServerCombo.GetSafeHwnd(), 
			m_pFolderPath, m_nDefaultID))
			CheckDlgButton(IDC_RADIO_SENT, TRUE);
		else
			CheckDlgButton(IDC_RADIO_OTHER, TRUE);

		// Subclass folder combo
		m_FolderCombo.SubclassDlgItem( IDC_COMBO_FOLDERS, this );
		m_FolderCombo.PopulateMail( WFE_MSGGetMaster() );
		SetFolderComboCurSel(m_FolderCombo.GetSafeHwnd(), 
			m_pFolderPath, m_nDefaultID);
	}
	
	return ret;
}

void CChooseFolderDialog::OnOK() 
{
	MSG_FolderInfo *pFolder = NULL;
	MSG_FolderLine folderLine;
	MSG_Master* pMaster = WFE_MSGGetMaster();

	if (IsDlgButtonChecked(IDC_RADIO_SENT))
	{
		pFolder = (MSG_FolderInfo*)m_ServerCombo.GetItemData(m_ServerCombo.GetCurSel());
		m_szFolder = XP_GetString(m_nDefaultID);
		if (MSG_GetFolderLineById(pMaster, pFolder, &folderLine)) 
			m_szServer = folderLine.name;
		URL_Struct *url = MSG_ConstructUrlForFolder(NULL, pFolder);
		if (MK_MSG_SENT_L10N_NAME == m_nDefaultID && 
			MAILBOX_TYPE_URL == NET_URL_Type(url->address))
		{  //local mail
			int nPos = strlen("mailbox:/");
			m_szPrefUrl = &url->address[nPos];
			m_szPrefUrl += "\\";
			m_szPrefUrl += m_szFolder;
			LPTSTR pBuffer = m_szPrefUrl.GetBuffer(m_szPrefUrl.GetLength());
			UnixToDosString(pBuffer);
			m_szPrefUrl.ReleaseBuffer();
		}
		else
		{	//imap
			m_szPrefUrl = url->address; 
			// m_szPrefUrl += "/";
			// m_szPrefUrl += m_szFolder;
		}
	}
	else if (IsDlgButtonChecked(IDC_RADIO_OTHER))
	{
		pFolder = (MSG_FolderInfo*)m_FolderCombo.GetItemData(m_FolderCombo.GetCurSel());

		if (MSG_GetFolderLineById(pMaster, pFolder, &folderLine)) 
		{
			m_szFolder = folderLine.name;
			MSG_FolderInfo*  pHostFolderInfo = GetHostFolderInfo(pFolder);
			if (pHostFolderInfo)
			{	//imap
				if (MSG_GetFolderLineById(pMaster, pHostFolderInfo, &folderLine)) 
					m_szServer = folderLine.name;
				URL_Struct *url = MSG_ConstructUrlForFolder(NULL, pFolder);
				m_szPrefUrl = url->address;
			}
			else
			{	//local mail
				m_szServer = XP_GetString(MK_MSG_LOCAL_MAIL);
				URL_Struct *url = MSG_ConstructUrlForFolder(NULL, pFolder);
				if (MK_MSG_SENT_L10N_NAME == m_nDefaultID)
				{
					int nPos = strlen("mailbox:/");
					m_szPrefUrl = &url->address[nPos]; 
					LPTSTR pBuffer = m_szPrefUrl.GetBuffer(m_szPrefUrl.GetLength());
					UnixToDosString(pBuffer);
					m_szPrefUrl.ReleaseBuffer();
				}
				else
					m_szPrefUrl = url->address;
			}
		}
	}

	CDialog::OnOK();
}

void CChooseFolderDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

void CChooseFolderDialog::OnNewFolder()
{   
    CPrefNewFolderDialog newFolderDlg( this, NULL);
	if (IDOK == newFolderDlg.DoModal())
	{
		MSG_FolderInfo *pNewFolder = newFolderDlg.GetNewFolder();
		m_FolderCombo.PopulateMail(WFE_MSGGetMaster());

		for (int i = m_FolderCombo.GetCount(); i >= 0 ; i--)
		{
			MSG_FolderInfo *pFolderInfo = (MSG_FolderInfo*)m_FolderCombo.GetItemData(i);
			if (pNewFolder == pFolderInfo)
			{
				m_FolderCombo.SetCurSel(i);
				break;
			}
		}
		OnSelectFolder(); //check the radio button
	}
}                                     

void CChooseFolderDialog::OnSelectServer()
{   
	if (!IsDlgButtonChecked(IDC_RADIO_SENT))
	{
		CheckDlgButton(IDC_RADIO_SENT, TRUE);
		CheckDlgButton(IDC_RADIO_OTHER, FALSE);
	}
}                                     

void CChooseFolderDialog::OnSelectFolder()
{   
	if (!IsDlgButtonChecked(IDC_RADIO_OTHER))
	{
		CheckDlgButton(IDC_RADIO_OTHER, TRUE);
		CheckDlgButton(IDC_RADIO_SENT, FALSE);
	}
}                                     

