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

#include "stdafx.h"

#include "helper.h"
#include "ownedlst.h"
#include "ngdwtrst.h"
#include "mozilla.h"
#include "shcut.h"

// Registry key constants
static const CString strMARKUP_KEY = "NetscapeMarkup";
static const CString strOPEN_CMD_FMT = "%s\\shell\\open\\command";
static const CString strDDE_EXEC_FMT = "%s\\shell\\open\\ddeexec";
static const CString strDDE_APP_FMT = "%s\\shell\\open\\ddeexec\\Application";
static const CString strDDE_APP_NAME = "NSShell";
static const CString strDDE_OLDAPP_NAME = "Netscape";
static const CString strDEF_ICON_FMT = "%s\\DefaultIcon";
static const CString strDDE_EXEC_VALUE = "%1";
static const CString strDDE_TOPIC_FMT = "%s\\shell\\open\\ddeexec\\Topic";
static const CString strEDIT_CMD_FMT = "%s\\shell\\edit\\command";

COwnedLostItem::COwnedLostItem(CString mimeType)
:m_csMimeType(mimeType), m_bIgnored(FALSE), m_bBroken(FALSE)
{
	SetHandledViaNetscape();	
}

COwnedLostItem::COwnedLostItem(CString mimeType, CString ignored)
:m_csMimeType(mimeType), m_bBroken(FALSE)
{
	m_bIgnored = (ignored == "Ignored");
	SetHandledViaNetscape();
}

void COwnedLostItem::GiveControlToNetscape()
{	
	char buffer[_MAX_PATH];
	::GetModuleFileName(theApp.m_hInstance, buffer, _MAX_PATH);
	
	// Get the app's directory into a short file name
	char shortBuffer[_MAX_PATH];
	GetShortPathName(buffer, shortBuffer, _MAX_PATH);

	CString directoryName(shortBuffer);
	directoryName.MakeUpper();  // This is what we'll write to the registry

	CString strValueName, strCmdPath;

	// Special Internet Shortcut check
	if (IsInternetShortcut())
	{
		CInternetShortcut internetShortcut;
		if (internetShortcut.ShellSupport())
		{
			// Need to take over lots of stuff
			CString strType = GetInternetShortcutFileClass();
			
			// Set the open command path
			strValueName.Format(strOPEN_CMD_FMT, (const char *)strType);
			strCmdPath = directoryName + " -h \"%1\"";
			FEU_RegistryWizard(HKEY_CLASSES_ROOT, strValueName, strCmdPath);
		
			// Set the DDE exec value
			strValueName.Format(strDDE_EXEC_FMT, (const char *)strType);
			FEU_RegistryWizard(HKEY_CLASSES_ROOT, strValueName, strDDE_EXEC_VALUE);

			// Set the DDE app name
			strValueName.Format(strDDE_APP_FMT, (const char *)strType);
			FEU_RegistryWizard(HKEY_CLASSES_ROOT, strValueName, strDDE_APP_NAME);
							
			// Set the DDE topic
			strValueName.Format(strDDE_TOPIC_FMT, (const char *)strType);
			CString strDDETopic;
			strDDETopic.LoadString(IDS_DDE_OPENURL);
			FEU_RegistryWizard(HKEY_CLASSES_ROOT, strValueName, strDDETopic);

			// Set the Default Icon
			CString strIconPath;
			if ((strType == "news") || (strType == "snews"))
			{
				// Use the news icon from URL.DLL
				::GetSystemDirectory(buffer, _MAX_PATH);
				strIconPath = CString(buffer) + "\\URL.DLL,1";
			}  
			else
			{
				// Use the document icon
				strIconPath = CString(buffer) + ",1";
			}  
			strValueName.Format(strDEF_ICON_FMT, (const char *)strType);
			FEU_RegistryWizard(HKEY_CLASSES_ROOT, strValueName, strIconPath);
			
			// Take over printing (applies to ALL Internet Shortcuts.  If we own one, we'll take
			// over printing for ALL of them).
			CString csMunge = directoryName + " /print(\"%1\")";
			FEU_RegistryWizard(HKEY_CLASSES_ROOT, "InternetShortcut\\shell\\print\\command", csMunge);
            FEU_RegistryWizard(HKEY_CLASSES_ROOT, "InternetShortcut\\shell\\print\\ddeexec", "[print(\"%1\")]");
            FEU_RegistryWizard(HKEY_CLASSES_ROOT, "InternetShortcut\\shell\\print\\ddeexec\\Application", strDDE_APP_NAME);

			//  The PrintTo Command.
            csMunge = directoryName + " /printto(\"%1\",\"%2\",\"%3\",\"%4\")";
            FEU_RegistryWizard(HKEY_CLASSES_ROOT, "InternetShortcut\\shell\\PrintTo\\command", csMunge);
            FEU_RegistryWizard(HKEY_CLASSES_ROOT, "InternetShortcut\\shell\\PrintTo\\ddeexec", "[printto(\"%1\",\"%2\",\"%3\",\"%4\")]");
            FEU_RegistryWizard(HKEY_CLASSES_ROOT, "InternetShortcut\\shell\\PrintTo\\ddeexec\\Application", strDDE_APP_NAME);
		}

		return;
	}

	CPtrList* allHelpers = &(CHelperApp::m_cplHelpers);

	for (POSITION pos = allHelpers->GetHeadPosition(); pos != NULL;)
	{
		CHelperApp* app = (CHelperApp*)allHelpers->GetNext(pos);
		CString helperMime(app->cd_item->ci.type);

		if (helperMime == m_csMimeType)
		{
			// Found the helper app.  Get the file class.
			CString fileClass(app->strFileClass);
			if (fileClass != "")
			{
				// We have some registry work to do.
				// In the case where this is text/html, we point .htm and .html to
				// NetscapeMarkup.
				HKEY hKey;
				DWORD dwDisp;

				if (m_csMimeType == "text/html")
				{
					::RegCreateKeyEx(HKEY_CLASSES_ROOT,
						".htm", 0L, NULL,
						REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
						&hKey, &dwDisp);
					::RegSetValueEx(hKey, NULL, 0L, REG_SZ,
						(const BYTE *)((const char *)strMARKUP_KEY),
						strMARKUP_KEY.GetLength() + 1);
					::RegCloseKey(hKey);
					
					::RegCreateKeyEx(HKEY_CLASSES_ROOT,
						".html", 0L, NULL,
						REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
						&hKey, &dwDisp);
					::RegSetValueEx(hKey, NULL, 0L, REG_SZ,
						(const BYTE *)((const char *)strMARKUP_KEY),
						strMARKUP_KEY.GetLength() + 1);
					::RegCloseKey(hKey);	
				}

				// In the case where this is application/x-unknown-content-type-NetscapeMarkup, 
				// we point .shtml to NetscapeMarkup.
				else if (m_csMimeType == "application/x-unknown-content-type-NetscapeMarkup")
				{
					::RegCreateKeyEx(HKEY_CLASSES_ROOT,
						".shtml", 0L, NULL,
						REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
						&hKey, &dwDisp);
					::RegSetValueEx(hKey, NULL, 0L, REG_SZ,
						(const BYTE *)((const char *)strMARKUP_KEY),
						strMARKUP_KEY.GetLength() + 1);
					::RegCloseKey(hKey);
					
				}

				// In all other cases, we should use the existing file class
				else
				{
					// Need to take over lots of stuff
					CString strType = fileClass;
					if (strType == "NetscapeMarkup")
					  return; // Don't let ANYTHING mess with NetscapeMarkup.
							  // Someone might point something to it later, and
							  // we don't want this code changing the stuff that's already there.

					// Set the open command path
					strValueName.Format(strOPEN_CMD_FMT, (const char *)strType);
					strCmdPath = directoryName + " \"%1\"";
					FEU_RegistryWizard(HKEY_CLASSES_ROOT, strValueName, strCmdPath);
		
					// Set the DDE exec value
					strValueName.Format(strDDE_EXEC_FMT, (const char *)strType);
					FEU_RegistryWizard(HKEY_CLASSES_ROOT, strValueName, strDDE_EXEC_VALUE);

					// Set the DDE app name
					strValueName.Format(strDDE_APP_FMT, (const char *)strType);
					FEU_RegistryWizard(HKEY_CLASSES_ROOT, strValueName, strDDE_APP_NAME);
							
					// Set the DDE topic
					strValueName.Format(strDDE_TOPIC_FMT, (const char *)strType);
					CString strDDETopic;
					strDDETopic.LoadString(IDS_DDE_OPENURL);
					FEU_RegistryWizard(HKEY_CLASSES_ROOT, strValueName, strDDETopic);

					// Set the Default Icon
					CString strIconPath;
					CString iconString = ",1";
					if (m_csMimeType == "text/x-javascript" || m_csMimeType == "application/x-javascript")
						iconString = ",7";
					else if (m_csMimeType.Left(5) == "image")
						iconString = ",6";

					strIconPath = CString(buffer) + iconString;
					  
					strValueName.Format(strDEF_ICON_FMT, (const char *)strType);
					FEU_RegistryWizard(HKEY_CLASSES_ROOT, strValueName, strIconPath);
				}
			}

			return;
		}
	}

}

void COwnedLostItem::SetHandledViaNetscape()
{
	m_nHandleMethod = OL_OTHER_APP;

	// Get the app's name.
	CString netscapeName;
	char buffer[_MAX_PATH+1];
	::GetModuleFileName(theApp.m_hInstance, buffer, _MAX_PATH);
	char *pSlash = ::strrchr(buffer, '\\');
	netscapeName = (char*)(pSlash+1);
	netscapeName.MakeUpper();

	// Get the app's directory. Adequate heuristic for version-checking Netscape
	CString directoryName(buffer);
	directoryName.MakeUpper();

	// Special Internet Shortcut check
	if (IsInternetShortcut())
	{
		CInternetShortcut internetShortcut;
		if (!internetShortcut.ShellSupport())
		{
			m_nHandleMethod = OL_CURRENT_NETSCAPE;
			return;
		}
		
		CString fileClass = GetInternetShortcutFileClass();
		SetHandleMethodViaFileClass(fileClass, netscapeName, directoryName);
		return;
	}

	CPtrList* allHelpers = &(CHelperApp::m_cplHelpers);

	for (POSITION pos = allHelpers->GetHeadPosition(); pos != NULL;)
	{
		CHelperApp* app = (CHelperApp*)allHelpers->GetNext(pos);
		CString helperMime(app->cd_item->ci.type);

		if (helperMime == m_csMimeType)
		{
			// Found the helper app.  See if Netscape is truly handling this mime type.
			CString fileClass(app->strFileClass);

			if (fileClass != "")
			{
				SetHandleMethodViaFileClass(fileClass, netscapeName, directoryName);
				return;
			}
			else m_bBroken = TRUE; // Treat as if ignored. Don't want to pop up a dialog over this.

			if (app->how_handle == HANDLE_VIA_NETSCAPE)
				m_nHandleMethod = OL_CURRENT_NETSCAPE;
			else m_nHandleMethod = OL_OTHER_APP;

			return;
		}
	}

	m_bBroken = TRUE; // Didn't even find this mime type. Don't want to fool with it.
}

void COwnedLostItem::SetHandleMethodViaFileClass(const CString& fileClass, 
												 const CString& netscapeName,
												 const CString& directoryName)
{
	// We have a place to look in the registry
	char lpszOpenCommand[_MAX_PATH+1];

	CString theString = fileClass + "\\shell\\open\\command";
				
	LONG size = _MAX_PATH;
	if (::RegQueryValue(HKEY_CLASSES_ROOT, theString, lpszOpenCommand, &size) !=
						ERROR_SUCCESS)
	{
		m_nHandleMethod = OL_OTHER_APP;
		return;
	}

	CString openCommand(lpszOpenCommand);
	openCommand.MakeUpper();

	if (openCommand.Find(netscapeName) != -1)
	{
		// Handled by a version of Netscape.
		char shortBuffer[_MAX_PATH];
		GetShortPathName(directoryName, shortBuffer, _MAX_PATH);
		CString shortName(shortBuffer);
		shortName.MakeUpper();

		if (openCommand.Find(directoryName) != -1 ||
			openCommand.Find(shortName) != -1)	
		{
			// Handled by current version.
			m_nHandleMethod = OL_CURRENT_NETSCAPE;
		}
		else m_nHandleMethod = OL_OLD_NETSCAPE;
	}
	else m_nHandleMethod = OL_OTHER_APP;
}

CString COwnedLostItem::GetInternetShortcutFileClass()
{
	int startIndex = m_csMimeType.Find('-');
	int endIndex = m_csMimeType.ReverseFind('-');
	
	return m_csMimeType.Mid(startIndex+1, endIndex-startIndex-1);
}

void COwnedLostItem::FetchPrettyName()
{
	if (IsInternetShortcut())
		m_csPrettyName = MakeInternetShortcutName();

	CPtrList* allHelpers = &(CHelperApp::m_cplHelpers);

	for (POSITION pos = allHelpers->GetHeadPosition(); pos != NULL;)
	{
		CHelperApp* app = (CHelperApp*)allHelpers->GetNext(pos);
		CString helperMime(app->cd_item->ci.type);

		if (helperMime == m_csMimeType)
		{
			CString returnString = app->cd_item->ci.desc;
			if (returnString == "")
				returnString = "File Type";

			if (app->cd_item->num_exts > 0)
			{
				returnString += " (";
				for (int i = 0; i < app->cd_item->num_exts; i++)	
				{
					returnString += "*.";
					returnString += app->cd_item->exts[i];

					if (i < app->cd_item->num_exts-1)
					  returnString += "; ";
				}
				returnString += ")";
			}

			m_csPrettyName = returnString;
		}
	}
}

BOOL COwnedLostItem::IsInternetShortcut()
{
	return (m_csMimeType == "application/x-http-protocol" ||
			m_csMimeType == "application/x-https-protocol" ||
			m_csMimeType == "application/x-ftp-protocol" ||
			m_csMimeType == "application/x-news-protocol" ||
			m_csMimeType == "application/x-snews-protocol" ||
			m_csMimeType == "application/x-gopher-protocol");
}

CString COwnedLostItem::MakeInternetShortcutName()
{
	CString protocolName = GetInternetShortcutFileClass();
	protocolName.MakeUpper();

	return protocolName + CString(" Internet Shortcuts");
}

// =====================================================================================
// COWNEDANDLOSTLIST 
// =====================================================================================

COwnedAndLostList::~COwnedAndLostList()
{
	int count = m_OwnedList.GetSize();
	for (int i = 0; i < count; i++)
	{
		COwnedLostItem* theItem = (COwnedLostItem*)(m_OwnedList[0]);
		m_OwnedList.RemoveAt(0);
		delete theItem;
	}

	count = m_LostList.GetSize();
	for (i = 0; i < count; i++)
	{
		COwnedLostItem* theItem = (COwnedLostItem*)(m_LostList[0]);
		m_LostList.RemoveAt(0);
		delete theItem;
	}
}


void COwnedAndLostList::ConstructLists()
{
	CString pathString;
	pathString.LoadString(IDS_NETHELP_REGISTRY); // Software\Netscape\Netscape Navigator

	// Read in the owned list
	// Look for owned subkey
	// If subkey exists, iterate over its subkeys and build the owned list
	// If subkey does not exist, the owned list should contain a certain
	// list of built-in types.  These are the types that Netscape defends initially.
	HKEY ownedKey, lostKey;
	CString ownerPath = pathString + "Owned";
	LONG result = ::RegOpenKey(HKEY_CURRENT_USER, ownerPath, &ownedKey);
	
	// Prepopulate our list with some types we want to defend.

	int dwIndex;
	char nameBuffer[_MAX_PATH];
	unsigned long nameBufferSize, valueBufferSize;
	unsigned char valueBuffer[_MAX_PATH];
	unsigned long typeCodeBuffer = 0;

	if (result != ERROR_SUCCESS)
	{
		m_OwnedList.Add(new COwnedLostItem("text/html"));
		m_OwnedList.Add(new COwnedLostItem("image/jpeg"));
		m_OwnedList.Add(new COwnedLostItem("image/pjpeg"));
		m_OwnedList.Add(new COwnedLostItem("image/gif"));
		m_OwnedList.Add(new COwnedLostItem("application/x-javascript"));
		m_OwnedList.Add(new COwnedLostItem("image/x-xbitmap"));
		
		// Be prepared to defend Internet Shortcuts if they are ever installed
		// later! 
		m_OwnedList.Add(new COwnedLostItem("application/x-http-protocol"));
		m_OwnedList.Add(new COwnedLostItem("application/x-https-protocol"));
		m_OwnedList.Add(new COwnedLostItem("application/x-news-protocol"));
		m_OwnedList.Add(new COwnedLostItem("application/x-snews-protocol"));
		m_OwnedList.Add(new COwnedLostItem("application/x-ftp-protocol"));
		m_OwnedList.Add(new COwnedLostItem("application/x-gopher-protocol"));
	}
	else
	{
		// Read in the owned list
		dwIndex = 0;
		valueBufferSize = sizeof(valueBuffer);
		nameBufferSize = sizeof(nameBuffer);
		while (RegEnumValue(ownedKey, dwIndex, nameBuffer, 
				&nameBufferSize, NULL, &typeCodeBuffer, valueBuffer,
				&valueBufferSize) != ERROR_NO_MORE_ITEMS)
		{
			m_OwnedList.Add(new COwnedLostItem(nameBuffer, valueBuffer));
			dwIndex++;
			valueBufferSize = sizeof(valueBuffer);
			nameBufferSize = sizeof(nameBuffer);
		}

	}

	// Read in the lost list
	// Look for lost subkey
	// If subkey exists, iterate over its subkeys and build the lost list
	// If subkey does not exist, the lost list is initially empty.  Do nothing.
	CString lostPath = pathString + "Lost";
	result = ::RegOpenKey(HKEY_CURRENT_USER, lostPath, &lostKey);
	
	if (result == ERROR_SUCCESS)
	{
		// Read in the lost list
		dwIndex = 0;
		valueBufferSize = sizeof(valueBuffer);
		nameBufferSize = sizeof(nameBuffer);
		while (RegEnumValue(lostKey, dwIndex, nameBuffer, 
				&nameBufferSize, NULL, &typeCodeBuffer, valueBuffer,
				&valueBufferSize) != ERROR_NO_MORE_ITEMS)
		{
			m_LostList.Add(new COwnedLostItem(nameBuffer, valueBuffer));
			dwIndex++;
			valueBufferSize = sizeof(valueBuffer);
			nameBufferSize = sizeof(nameBuffer);
		}
	}

	// Iterate over the owned list. Look up each entry in the helper app list.  If
	// it is not handled by the current Netscape, then move it to the lost list.
	int count = m_OwnedList.GetSize();
	for (int i = 0; i < count; i++)
	{
		COwnedLostItem* theItem = (COwnedLostItem*)(m_OwnedList[i]);
		
		if (theItem->m_nHandleMethod != OL_CURRENT_NETSCAPE)
		{
			// Move to the lost list
			void* thePtr = m_OwnedList[i];
			m_OwnedList.RemoveAt(i);
			m_LostList.Add(thePtr);
			i--;
			count--;
		}
	}

	// Iterate over the lost list.  If any entry is now handled by the current Netscape, move it to
	// the owner list
	count = m_LostList.GetSize();
	for (i = 0; i < count; i++)
	{
		COwnedLostItem* theItem = (COwnedLostItem*)(m_LostList[i]);
		
		if (theItem->m_nHandleMethod == OL_CURRENT_NETSCAPE)
		{
			// Move to the owned list
			m_LostList.RemoveAt(i);
			m_OwnedList.Add(theItem);
			i--;
			count--;
		}
	}

	// Iterate over all the helper apps and find any additional entries that aren't in the
	// owned list or lost list (and that should be).
	
	CPtrList* allHelpers = &(CHelperApp::m_cplHelpers);
	for (POSITION pos = allHelpers->GetHeadPosition(); pos != NULL;)
	{
		CHelperApp* app = (CHelperApp*)allHelpers->GetNext(pos);
		CString helperMime(app->cd_item->ci.type);

		COwnedLostItem theItem(helperMime);
		if (theItem.m_nHandleMethod == OL_CURRENT_NETSCAPE &&
			!IsInOwnedList(helperMime))
		{
			// This should be in the owned list, since we apparently control it.
			COwnedLostItem* realItem = new COwnedLostItem(helperMime);
			m_OwnedList.Add(realItem);
		}
		else if (theItem.m_nHandleMethod == OL_OLD_NETSCAPE &&
			!IsInLostList(helperMime))
		{
			// This item is currently used by the old Netscape.  Let's
			// offer to update to the current version of NS.
			COwnedLostItem* realItem = new COwnedLostItem(helperMime);
			m_LostList.Add(realItem);
		}
	}

	// Netscape will automatically wrest control of HTML files from a previous version
	// so we don't REALLY want HTML in our lost list if it is controlled by an older version
	// of netscape.  It should be moved to our OWNED list instead, and the handle method
	// should be updated.
	if (IsInLostList("text/html"))
	{
		COwnedLostItem* theItem = RemoveFromLostList("text/html");
		if (theItem)
		{
			if (theItem->m_nHandleMethod == OL_OLD_NETSCAPE)
			{
				theItem->m_nHandleMethod = OL_CURRENT_NETSCAPE;
				m_OwnedList.Add(theItem);  // Move it to the owned list
			}
			else m_LostList.Add(theItem); // Put it back
		}
	}

	if (IsInLostList("application/x-unknown-content-type-NetscapeMarkup"))
	{
		COwnedLostItem* theItem = RemoveFromLostList("application/x-unknown-content-type-NetscapeMarkup");
		if (theItem)
		{
			if (theItem->m_nHandleMethod == OL_OLD_NETSCAPE)
			{
				theItem->m_nHandleMethod = OL_CURRENT_NETSCAPE;
				m_OwnedList.Add(theItem);  // Move it to the owned list
			}
			else m_LostList.Add(theItem); // Put it back
		}
	}
}

void COwnedAndLostList::WriteLists()
{
	// Need to write each of these lists.
	CString pathString;
	pathString.LoadString(IDS_NETHELP_REGISTRY); // Software\Netscape\Netscape Navigator

	// Write the ignored list and lost lists
	CString path = pathString + "Lost";
	::RegDeleteKey(HKEY_CURRENT_USER, path);

	HKEY key;
	
	if (::RegCreateKey(HKEY_CURRENT_USER, path, &key) == ERROR_SUCCESS)
	{
		// Write out all the lost elements
		int count = m_LostList.GetSize();
		
		for (int i = 0; i < count; i++)
		{
			COwnedLostItem* theItem = (COwnedLostItem*)(m_LostList[i]);
			CString regValue = theItem->GetRegistryValue();

			RegSetValueEx(key, theItem->m_csMimeType, NULL, REG_SZ, 
				(const unsigned char*)(const char*)regValue, regValue.GetLength() + 1); 
		}
	}

	::RegCloseKey(key);

	path = pathString + "Owned";
	::RegDeleteKey(HKEY_CURRENT_USER, path);

	if (::RegCreateKey(HKEY_CURRENT_USER, path, &key) == ERROR_SUCCESS)
	{
		// Write out all the lost elements
		int count = m_OwnedList.GetSize();
		
		for (int i = 0; i < count; i++)
		{
			COwnedLostItem* theItem = (COwnedLostItem*)(m_OwnedList[i]);
			CString regValue = theItem->GetRegistryValue();

			RegSetValueEx(key, theItem->m_csMimeType, NULL, REG_SZ, 
				(const unsigned char*)(const char*)regValue, regValue.GetLength() + 1); 
		}
	}

	::RegCloseKey(key);
}


BOOL COwnedAndLostList::IsInOwnedList(const CString& mimeType)
{
	int count = m_OwnedList.GetSize();
	for (int i = 0; i < count; i++)
	{
		COwnedLostItem* theItem = (COwnedLostItem*)m_OwnedList[i];

		if (theItem->m_csMimeType == mimeType)
			return TRUE;
	}

	return FALSE;
}

BOOL COwnedAndLostList::IsInLostList(const CString& mimeType)
{
	int count = m_LostList.GetSize();
	for (int i = 0; i < count; i++)
	{
		COwnedLostItem* theItem = (COwnedLostItem*)m_LostList[i];

		if (theItem->m_csMimeType == mimeType)
			return TRUE;
	}

	return FALSE;
}

COwnedLostItem* COwnedAndLostList::RemoveFromLostList(const CString& mimeType)
{
	int count = m_LostList.GetSize();
	for (int i = 0; i < count; i++)
	{
		COwnedLostItem* theItem = (COwnedLostItem*)(m_LostList[i]);
		if (mimeType == theItem->m_csMimeType)
		{
			m_LostList.RemoveAt(i);
			return theItem;
		}
	}

	return NULL;
}

BOOL COwnedAndLostList::NonemptyLostIgnoredIntersection()
{
	int count = m_LostList.GetSize();
	for (int i = 0; i < count; i++)
	{
		COwnedLostItem* theItem = (COwnedLostItem*)(m_LostList[i]);
		if (!theItem->m_bIgnored && !theItem->m_bBroken)
			return TRUE;
	}

	return FALSE;
}
