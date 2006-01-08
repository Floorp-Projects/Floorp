/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Srilatha Moturi <srilatha@netscape.com>
 *   Jungshik Shin <jshin@mailaps.org>
 *   Scott MacGregor <mscott@mozilla.org>
 *   David Bienvenu < bienvenu@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#undef UNICODE
#undef _UNICODE

#include "nsIServiceManager.h"
#include "msgMapiImp.h"
#include "msgMapiMain.h"
#include "nsMapiRegistryUtils.h"
#include "nsString.h"
#include "nsIStringBundle.h"
#include "nsIPromptService.h"
#include "nsXPIDLString.h"
#include "nsSpecialSystemDirectory.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsNativeCharsetUtils.h"
#include "nsEmbedCID.h"
#include <mbstring.h>

#define EXE_EXTENSION ".EXE" 
#define MOZ_HWND_BROADCAST_MSG_TIMEOUT 5000
#define MOZ_CLIENT_MAIL_KEY "Software\\Clients\\Mail"
#define MOZ_CLIENT_NEWS_KEY "Software\\Clients\\News"

nsMapiRegistryUtils::nsMapiRegistryUtils()
{
   m_mapiStringBundle = nsnull ;

   mRestrictedRegAccess = verifyRestrictedAccess();
}
    
const char * nsMapiRegistryUtils::thisApplication()
{
    if (m_thisApp.IsEmpty()) {
        char buffer[MAX_PATH] = {0};
        DWORD len = ::GetModuleFileName(NULL, buffer, MAX_PATH);
        if (!len) return nsnull ; 
        len = ::GetShortPathName(buffer, buffer, MAX_PATH);
        if (!len) return nsnull ; 
        m_thisApp = buffer;
        ToUpperCase(m_thisApp);
    }

    return m_thisApp.get() ;
}

void nsMapiRegistryUtils::getVarValue(const PRUnichar * varName, nsXPIDLString & result)
{
    nsresult rv;
    nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(
                                         NS_STRINGBUNDLE_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv) && bundleService) {
        nsCOMPtr<nsIStringBundle> brandBundle;
        rv = bundleService->CreateBundle(
                    "chrome://branding/locale/brand.properties",
                    getter_AddRefs(brandBundle));
        if (NS_SUCCEEDED(rv)) {
            brandBundle->GetStringFromName(
                       varName,
                       getter_Copies(result));
        }
    }
}

const PRUnichar * nsMapiRegistryUtils::brandName() 
{
    if (m_brand.IsEmpty())
        getVarValue(NS_LITERAL_STRING("brandFullName").get(), m_brand);
    return m_brand.get();
}

const nsString& nsMapiRegistryUtils::vendorName() 
{
    if (m_vendor.IsEmpty())
        getVarValue(NS_LITERAL_STRING("vendorShortName").get(), m_vendor);
    return m_vendor;
}


PRBool nsMapiRegistryUtils::verifyRestrictedAccess() {
    char   subKey[] = "Software\\Mozilla - Test Key";
    PRBool result = PR_FALSE;
    DWORD  dwDisp = 0;
    HKEY   key;
    // Try to create/open a subkey under HKLM.
    DWORD rc = ::RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                                subKey,
                                0,
                                NULL,
                                REG_OPTION_NON_VOLATILE,
                                KEY_WRITE,
                                NULL,
                                &key,
                                &dwDisp);

    if (rc == ERROR_SUCCESS) {
        // Key was opened; first close it.
        ::RegCloseKey(key);
        // Delete it if we just created it.
        switch(dwDisp) {
            case REG_CREATED_NEW_KEY:
                ::RegDeleteKey(HKEY_LOCAL_MACHINE, subKey);
                break;
            case REG_OPENED_EXISTING_KEY:
                break;
        }
    } else {
        // Can't create/open it; we don't have access.
        result = PR_TRUE;
    }
    return result;
}

nsresult nsMapiRegistryUtils::SetRegistryKey(HKEY baseKey, const char * keyName, 
                        const char * valueName, char * value)
{
    nsresult result = NS_ERROR_FAILURE;
    HKEY   key;
    LONG   rc = ::RegCreateKey(baseKey, keyName, &key);
  
    if (rc == ERROR_SUCCESS) {
        rc = ::RegSetValueEx(key, valueName, NULL, REG_SZ, 
                                 (LPBYTE)(const char*)value, strlen(value));
        if (rc == ERROR_SUCCESS) {
            result = NS_OK;
        }
        ::RegCloseKey(key);
    }
    return result;
}

nsresult nsMapiRegistryUtils::DeleteRegistryValue(HKEY baseKey, const char * keyName, 
                        const char * valueName)
{
    nsresult result = NS_ERROR_FAILURE;
    HKEY   key;
    LONG   rc = ::RegOpenKey(baseKey, keyName, &key);
  
    if (rc == ERROR_SUCCESS) {
        rc = ::RegDeleteValue(key, valueName);
        if (rc == ERROR_SUCCESS)
            result = NS_OK;
        ::RegCloseKey(key);
    }
    return result;
}

void nsMapiRegistryUtils::GetRegistryKey(HKEY baseKey, const char * keyName, 
                         const char * valueName, nsCAutoString & value)
{
    HKEY   key;
    LONG   rc = ::RegOpenKey(baseKey, keyName, &key);
    if (rc == ERROR_SUCCESS) {
        char buffer[MAX_PATH] = {0};
        DWORD len = sizeof buffer;
        rc = ::RegQueryValueEx(key, valueName, NULL, NULL, 
                               (LPBYTE)buffer, &len);
        if (rc == ERROR_SUCCESS) {
            if (len)
                value = buffer;
        }
        ::RegCloseKey(key);
     }
}

nsresult nsMapiRegistryUtils::recursiveDeleteKey(HKEY hKeyParent, const char* lpszKeyChild)
{
    // Open the child.
    HKEY hKeyChild ;
    nsresult rv = NS_OK;

    LONG lRes = ::RegOpenKeyEx(hKeyParent, lpszKeyChild, 0, KEY_ALL_ACCESS, &hKeyChild);
    if (lRes != ERROR_SUCCESS)
        return NS_ERROR_FAILURE;

    // Enumerate all of the decendents of this child.
    FILETIME time;
    char szBuffer[MAX_PATH+1];
    DWORD dwSize = MAX_PATH+1;
    while (::RegEnumKeyEx(hKeyChild, 0, szBuffer, &dwSize, NULL, NULL, NULL, &time) == S_OK)
    {
        // Delete the decendents of this child.
        rv = recursiveDeleteKey(hKeyChild, szBuffer);
        if (NS_FAILED(rv))
        {
            // Cleanup before exiting.
            ::RegCloseKey(hKeyChild);
            return rv;
        }
        
        dwSize = MAX_PATH+1;
    }

    // Close the child.
    ::RegCloseKey(hKeyChild);

    // Delete this child.
    ::RegDeleteKey(hKeyParent, lpszKeyChild);
    return rv;
}

/* static */
void nsMapiRegistryUtils::RegCopyKey(HKEY aSrcKey, HKEY aDestKey, const char* aSubKeyName)
{
  HKEY srcSubKey, destSubKey;
  char valueName[MAX_PATH + 1], keyName[MAX_PATH + 1];
  BYTE valueData[MAX_PATH + 1];
  DWORD nameSize, dataSize, keySize, valueType;

  // open source key
  if (::RegOpenKeyEx(aSrcKey, aSubKeyName, NULL, KEY_ALL_ACCESS, &srcSubKey) != ERROR_SUCCESS)
    return;

  // create target key
  if (::RegCreateKeyEx(aDestKey, aSubKeyName, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &destSubKey, NULL) == ERROR_SUCCESS) {
    for (DWORD valueIndex = 0;
         nameSize = MAX_PATH + 1,
         dataSize = MAX_PATH + 1,
         ::RegEnumValue(srcSubKey, valueIndex, valueName, &nameSize, NULL, &valueType, valueData, &dataSize) == ERROR_SUCCESS;
         valueIndex++)
      ::RegSetValueEx(destSubKey, valueName, NULL, valueType, valueData, dataSize);

    for (DWORD keyIndex = 0;
         keySize = MAX_PATH + 1,
         ::RegEnumKeyEx(srcSubKey, keyIndex, keyName, &keySize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS;
         keyIndex++)
      RegCopyKey(srcSubKey, destSubKey, keyName);

    ::RegCloseKey(destSubKey);
  }
  ::RegCloseKey(srcSubKey);
}

PRBool nsMapiRegistryUtils::IsDefaultMailClient()
{
    if (!isSmartDll() && !isMozDll()) 
        return PR_FALSE;

    //first try to get the users default mail client
    nsCAutoString name; 
    // old mail clients like netscape 4.x never channge the current user key, so it is possible for them to become
    // the default mail client and we don't notice because we see that thunderbird still owns the HKCU key...
    // doh!
    // GetRegistryKey(HKEY_CURRENT_USER, "Software\\Clients\\Mail", "", name);

    //if that fails then get the machine's default client
    GetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Clients\\Mail", "", name);

    if (!name.IsEmpty()) {
         nsCAutoString keyName("Software\\Clients\\Mail\\");
         keyName += name.get(); 
         keyName += "\\Protocols\\mailto\\shell\\open\\command";

         nsCAutoString result;
         GetRegistryKey(HKEY_LOCAL_MACHINE, keyName.get(), "", result);
         if (!result.IsEmpty()) {
             nsCAutoString strExtension;
             strExtension.Assign(EXE_EXTENSION);
             ToUpperCase(result);
             PRInt32 index = result.RFind(strExtension.get());
             if (index != kNotFound) {
                 result.Truncate(index + strExtension.Length());
             }
             
             nsCAutoString thisApp (thisApplication());             
             // if result == thisApp, that by itself isn't a strong enough indication that everything
             // is ok...also check HKLM\Software\Classes\mailto\shell\open\command
             if (result == thisApp)
             {
               keyName = "Software\\Classes\\mailto\\shell\\open\\command";
               nsCAutoString result;
               GetRegistryKey(HKEY_LOCAL_MACHINE, keyName.get(), "", result);
               if (!result.IsEmpty()) {
                  nsCAutoString strExtension;
                  strExtension.Assign(EXE_EXTENSION);
                  ToUpperCase(result);
                  PRInt32 index = result.RFind(strExtension.get());
                  if (index != kNotFound)
                    result.Truncate(index + strExtension.Length());    
                  return (result == thisApp);
               }
             }
        }
    }
    return PR_FALSE;

}

PRBool nsMapiRegistryUtils::IsDefaultNewsClient()
{
    //first try to get the users default news client
    nsCAutoString name; 
    GetRegistryKey(HKEY_CURRENT_USER, "Software\\Clients\\News", "", name);

     //if that fails then get the machine's default client
    if(name.IsEmpty()) {
        GetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Clients\\News", "", name);  
    }     

    if (!name.IsEmpty()) {
         nsCAutoString keyName("Software\\Clients\\News\\");
         keyName += name.get(); 

         // let's only check news and instead of news, nntp and snews.
         keyName += "\\Protocols\\news\\shell\\open\\command";
         nsCAutoString result;
         GetRegistryKey(HKEY_LOCAL_MACHINE, keyName.get(), "", result);
         if (!result.IsEmpty()) {
             nsCAutoString strExtension;
             strExtension.Assign(EXE_EXTENSION);
             ToUpperCase(result);
             PRInt32 index = result.RFind(strExtension.get());
             if (index != kNotFound) {
                 result.Truncate(index + strExtension.Length());
             }
             nsCAutoString thisApp (thisApplication()) ;
             return (result == thisApp);
        }
    }

    return PR_FALSE;
}

nsresult nsMapiRegistryUtils::saveDefaultNewsClient()
{
    nsCAutoString name ;
    GetRegistryKey(HKEY_LOCAL_MACHINE,"Software\\Clients\\News", "", name);
    return SetRegistryKey(HKEY_LOCAL_MACHINE, 
                            kAppDesktopKey, 
                            "HKEY_LOCAL_MACHINE\\Software\\Clients\\News", 
                            name.IsEmpty() ? "" : (char *)name.get());
}

nsresult nsMapiRegistryUtils::saveDefaultMailClient()
{
    nsCAutoString name ;
    GetRegistryKey(HKEY_LOCAL_MACHINE,"Software\\Clients\\Mail", "", name);
    return SetRegistryKey(HKEY_LOCAL_MACHINE, 
                            kAppDesktopKey, 
                            "HKEY_LOCAL_MACHINE\\Software\\Clients\\Mail", 
                            name.IsEmpty() ? "" : (char *)name.get());
} 

nsresult nsMapiRegistryUtils::saveUserDefaultNewsClient()
{
    nsCAutoString name ;
    GetRegistryKey(HKEY_CURRENT_USER,"Software\\Clients\\News", "", name);
    return SetRegistryKey(HKEY_LOCAL_MACHINE, 
                            kAppDesktopKey, 
                            "HKEY_CURRENT_USER\\Software\\Clients\\News", 
                            name.IsEmpty() ? "" : (char *)name.get());
}

nsresult nsMapiRegistryUtils::saveUserDefaultMailClient()
{
    nsCAutoString name ;
    GetRegistryKey(HKEY_CURRENT_USER,"Software\\Clients\\Mail", "", name);
    return SetRegistryKey(HKEY_LOCAL_MACHINE, 
                            kAppDesktopKey, 
                            "HKEY_CURRENT_USER\\Software\\Clients\\Mail", 
                            name.IsEmpty() ? "" : (char *)name.get());
}

/**
 * Check whether it is a smart dll or not. Smart dll is the one installed by
 * IE5 or Outlook Express which forwards the MAPI calls to the dll based on the 
 * registry key setttings.
 * Returns TRUE if is a smart dll.
 */

typedef HRESULT (FAR PASCAL GetOutlookVersionFunc)(); 
PRBool nsMapiRegistryUtils::isSmartDll() 
{ 
    char buffer[MAX_PATH] = {0};
    if (GetSystemDirectory(buffer, sizeof(buffer)) == 0) 
        return PR_FALSE;
    PL_strcatn(buffer, sizeof(buffer), "\\Mapi32.dll");
    
    HINSTANCE hInst; 
    GetOutlookVersionFunc *doesExist = nsnull;
    hInst = LoadLibrary(buffer); 
    if (hInst == nsnull) 
        return PR_FALSE;
        
    doesExist = (GetOutlookVersionFunc *) GetProcAddress (hInst, "GetOutlookVersion"); 
    FreeLibrary(hInst); 

    return (doesExist != nsnull); 
} 

typedef HRESULT (FAR PASCAL GetMapiDllVersion)(); 
/**
 * Checks whether mapi32.dll is installed by this app. 
 * Returns TRUE if it is.
 */
PRBool nsMapiRegistryUtils::isMozDll() 
{ 
    char buffer[MAX_PATH] = {0};
    if (GetSystemDirectory(buffer, sizeof(buffer)) == 0) 
        return PR_FALSE;
    PL_strcatn(buffer, sizeof(buffer), "\\Mapi32.dll"); 

    HINSTANCE hInst; 
    GetMapiDllVersion *doesExist = nsnull;
    hInst = LoadLibrary(buffer); 
    if (hInst == nsnull) 
        return PR_FALSE;
        
    doesExist = (GetMapiDllVersion *) GetProcAddress (hInst, "GetMapiDllVersion"); 
    FreeLibrary(hInst); 

    return (doesExist != nsnull); 
} 

/** Renames Mapi32.dl in system directory to Mapi32_moz_bak.dll
 *  copies the mozMapi32.dll from bin directory to the system directory
 */
nsresult nsMapiRegistryUtils::CopyMozMapiToWinSysDir()
{
    nsresult rv;
    char buffer[MAX_PATH] = {0};
    if (GetSystemDirectory(buffer, sizeof(buffer)) == 0) 
        return NS_ERROR_FAILURE;

    nsCAutoString filePath(buffer);
    filePath.AppendLiteral("\\Mapi32_moz_bak.dll");

    nsCOMPtr<nsILocalFile> pCurrentMapiFile = do_CreateInstance (NS_LOCAL_FILE_CONTRACTID, &rv);
    if (NS_FAILED(rv) || !pCurrentMapiFile) return rv;        
    pCurrentMapiFile->InitWithNativePath(filePath);

    nsCOMPtr<nsIFile> pMozMapiFile;
    nsCOMPtr<nsIProperties> directoryService =
          do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (!directoryService) return NS_ERROR_FAILURE;
    rv = directoryService->Get(NS_OS_CURRENT_PROCESS_DIR,
                              NS_GET_IID(nsIFile),
                              getter_AddRefs(pMozMapiFile));

    if (NS_FAILED(rv)) return rv;
    pMozMapiFile->AppendNative(NS_LITERAL_CSTRING("mozMapi32.dll"));

    PRBool bExist;
    rv = pMozMapiFile->Exists(&bExist);
    if (NS_FAILED(rv) || !bExist) return rv;

    rv = pCurrentMapiFile->Exists(&bExist);
    if (NS_SUCCEEDED(rv) && bExist)
    {
        rv = pCurrentMapiFile->Remove(PR_FALSE);
    }
    if (NS_FAILED(rv)) return rv;
    filePath.Assign(buffer);
    filePath.AppendLiteral("\\Mapi32.dll");
    pCurrentMapiFile->InitWithNativePath(filePath);
    rv = pCurrentMapiFile->Exists(&bExist);
    if (NS_SUCCEEDED(rv) && bExist)
    {
        rv = pCurrentMapiFile->MoveToNative(nsnull, NS_LITERAL_CSTRING("Mapi32_moz_bak.dll"));
        if (NS_FAILED(rv)) return rv;
        nsCAutoString fullFilePath(buffer);
        fullFilePath.AppendLiteral("\\Mapi32_moz_bak.dll");
        rv = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                            kAppDesktopKey, 
                            "Mapi_backup_dll", 
                            (char *)fullFilePath.get());
        if (NS_FAILED(rv)) {
             RestoreBackedUpMapiDll();
             return rv;
        }
    }
    
    NS_NAMED_LITERAL_CSTRING(fileName, "Mapi32.dll");
    filePath.Assign(buffer);
    pCurrentMapiFile->InitWithNativePath(filePath);
    rv = pMozMapiFile->CopyToNative(pCurrentMapiFile, fileName);
    if (NS_FAILED(rv))
        RestoreBackedUpMapiDll();
    return rv;
}

/** deletes the Mapi32.dll in system directory and renames Mapi32_moz_bak.dll
 *  to Mapi32.dll
 */
nsresult nsMapiRegistryUtils::RestoreBackedUpMapiDll()
{
    nsresult rv;
    char buffer[MAX_PATH] = {0};
    if (GetSystemDirectory(buffer, sizeof(buffer)) == 0) 
        return NS_ERROR_FAILURE;

    nsCAutoString filePath(buffer);
    nsCAutoString previousFileName(buffer);
    filePath.AppendLiteral("\\Mapi32.dll");
    previousFileName.AppendLiteral("\\Mapi32_moz_bak.dll");

    nsCOMPtr <nsILocalFile> pCurrentMapiFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
    if (NS_FAILED(rv) || !pCurrentMapiFile) return NS_ERROR_FAILURE;        
    pCurrentMapiFile->InitWithNativePath(filePath);
    
    nsCOMPtr<nsILocalFile> pPreviousMapiFile = do_CreateInstance (NS_LOCAL_FILE_CONTRACTID, &rv);
    if (NS_FAILED(rv) || !pPreviousMapiFile) return NS_ERROR_FAILURE;       
    pPreviousMapiFile->InitWithNativePath(previousFileName);

    PRBool bExist;
    rv = pCurrentMapiFile->Exists(&bExist);
    if (NS_SUCCEEDED(rv) && bExist) {
        rv = pCurrentMapiFile->Remove(PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }

    rv = pPreviousMapiFile->Exists(&bExist);
    if (NS_SUCCEEDED(rv) && bExist)
        rv = pPreviousMapiFile->MoveToNative(nsnull, NS_LITERAL_CSTRING("Mapi32.dll"));
    if (NS_SUCCEEDED(rv))
        DeleteRegistryValue(HKEY_LOCAL_MACHINE,
                            kAppDesktopKey, 
                            "Mapi_backup_dll");
    return rv;
}

// aDefaultAppRegKey points to something like Software\Clients\News\Mozilla\Protocols
// we will copy the keys into Software\Clases
nsresult nsMapiRegistryUtils::setProtocolHandler(const char * aDefaultAppRegKey, const char * protocolName)
{
    HKEY srcKey;
    HKEY trgKey;

    ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, aDefaultAppRegKey, 0, KEY_READ, &srcKey);
    ::RegOpenKeyEx(HKEY_LOCAL_MACHINE,"Software\\Classes", 0, KEY_READ, &trgKey);

    RegCopyKey(srcKey, trgKey, protocolName);

    RegCloseKey(srcKey);
    RegCloseKey(trgKey);
    return NS_OK;
}

nsresult nsMapiRegistryUtils::setupFileExtension(const char * aDefaultAppRegKey, const char * aExtension)
{

  // aDefaultAppRegKey is "Software\\Classes\\"
    nsCAutoString keyName;
    keyName = aDefaultAppRegKey;
    keyName.Append(aExtension);
   

    nsCAutoString appName;
    appName.AssignWithConversion(brandName());

    // save the current file extension settings to HKEY_LOCAL_MACHINE\Software\Clients\Thunderbird\<extension>

    nsCAutoString saveKeyName("Software\\Clients\\Mail\\");
    saveKeyName.Append(appName);
    saveKeyName.AppendLiteral("\\");

    HKEY srcKey;
    HKEY trgKey;
    ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, aDefaultAppRegKey, 0, KEY_READ, &srcKey);
    ::RegOpenKeyEx(HKEY_LOCAL_MACHINE,saveKeyName.get(), 0, KEY_READ, &trgKey);

    RegCopyKey(srcKey, trgKey, aExtension);

    nsresult rv = SetRegistryKey(HKEY_LOCAL_MACHINE, keyName.get(), "", "");


    if (NS_SUCCEEDED(rv)) 
    {
      // Classes\<extension>\shell\open\command value
      nsCAutoString appPath (thisApplication());

      appPath += " ";
      
      appPath += "\"%1\"";
      nsCAutoString shellOpenKey (keyName);
      shellOpenKey.AppendLiteral("\\shell\\open\\command");

      rv = SetRegistryKey(HKEY_LOCAL_MACHINE, shellOpenKey.get(), "", (char *)appPath.get());

      // Classes\<extension>\DefaultIcon value
      nsCAutoString iconPath(thisApplication());
      iconPath += ",0";
      nsCAutoString iconKey (keyName);
      iconKey.AppendLiteral("\\DefaultIcon");
      rv = SetRegistryKey(HKEY_LOCAL_MACHINE, iconKey.get(), "", (char *)iconPath.get());
    }

    return rv;
}

nsresult nsMapiRegistryUtils::restoreFileExtension(const char * aDefaultAppRegKey, const char * aExtension)
{

  // aDefaultAppRegKey is "Software\\Classes\\"
  nsCAutoString keyName;
  keyName = aDefaultAppRegKey;
  keyName.Append(aExtension);
 

  nsCAutoString appName;
  appName.AssignWithConversion(brandName());

  // restore the file extension settings from HKEY_LOCAL_MACHINE\Software\Clients\Mail\Mail & News\<extension>

  nsCAutoString saveKeyName("Software\\Clients\\Mail\\");
  saveKeyName.Append(appName);
  saveKeyName.AppendLiteral("\\");

  recursiveDeleteKey(HKEY_LOCAL_MACHINE, keyName.get()); 
  nsresult rv = SetRegistryKey(HKEY_LOCAL_MACHINE, keyName.get(), "", "");

  HKEY srcKey;
  HKEY trgKey;
  ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, saveKeyName.get(), 0, KEY_READ, &srcKey);
  ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, aDefaultAppRegKey, 0, KEY_READ, &trgKey);

  RegCopyKey(srcKey, trgKey, aExtension);

  return rv;
}


nsresult nsMapiRegistryUtils::setupDefaultProtocolKey(const char * aDefaultAppRegKey, const char * aProtocol, const char * aProtocolEntryValue, const char * aCmdLineText)
{
    nsCAutoString keyName;
    keyName = aDefaultAppRegKey;
    keyName.Append(aProtocol);

    nsCAutoString temp; 
    temp = aProtocolEntryValue;  // XXX this is bogus. SetRegistryKey should not require a char *..it should take a const char *
   
    // Protocols\<proto scheme> default value (i.e. URL:News Protocol)
    nsresult rv = SetRegistryKey(HKEY_LOCAL_MACHINE, keyName.get(), "", (char *) temp.get());
    
    SetRegistryKey(HKEY_LOCAL_MACHINE, keyName.get(), "URL Protocol", "");


    if (NS_SUCCEEDED(rv)) 
    {
      // Protocols\<protocol scheme>\shell\open\command value
      nsCAutoString appPath (thisApplication());

      appPath += " ";
      if (aCmdLineText) 
      {
        appPath += aCmdLineText;
        appPath += " ";
      }
      
      appPath += "%1";
      nsCAutoString shellOpenKey (keyName);
      shellOpenKey.AppendLiteral("\\shell\\open\\command");

      rv = SetRegistryKey(HKEY_LOCAL_MACHINE, shellOpenKey.get(), "", (char *)appPath.get());

      // Protocols\<protocol scheme>\DefaultIcon value
      nsCAutoString iconPath(thisApplication());
      iconPath += ",0";
      nsCAutoString iconKey (keyName);
      iconKey.AppendLiteral("\\DefaultIcon");
      rv = SetRegistryKey(HKEY_LOCAL_MACHINE, iconKey.get(),"", (char *)iconPath.get());
    }

    return rv;
}

nsresult nsMapiRegistryUtils::registerMailApp(PRBool aForceRegistration)
{
    nsCAutoString keyName("Software\\Clients\\Mail\\");
    nsCAutoString appName;
    NS_CopyUnicodeToNative(vendorName(), appName);
    nsresult rv = NS_OK;

    if (mRestrictedRegAccess) 
      return NS_ERROR_FAILURE;

    // Be SMART! If we have already set these keys up, don't do it again. Check the registeredAsNewsApp flag
    // that we stored in our desktop registry scratchpad
    nsCAutoString registeredAsMailApp;
    GetRegistryKey(HKEY_LOCAL_MACHINE, kAppDesktopKey, "registeredAsMailApp", registeredAsMailApp);
    
    if (!aForceRegistration && registeredAsMailApp.Equals("1")) 
        return NS_OK;
    
    // (1) Add our app to the list of keys under Software\\Clients\\Mail so we show up as a possible News application
    if (!appName.IsEmpty()) {
        keyName.Append(appName.get());

        nsCOMPtr<nsIStringBundle> bundle;
        rv = MakeMapiStringBundle (getter_AddRefs (bundle)) ;
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        nsXPIDLString defaultMailTitle;
        // Use vendorName instead of brandname since brandName is product name
        // and has more than just the name of the application
        const PRUnichar *keyValuePrefixStr[] = { vendorName().get() };
        NS_NAMED_LITERAL_STRING(defaultMailTitleTag, "defaultMailDisplayTitle");
        rv = bundle->FormatStringFromName(defaultMailTitleTag.get(), keyValuePrefixStr, 1, getter_Copies(defaultMailTitle));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        nsCAutoString nativeTitle;
        NS_CopyUnicodeToNative(defaultMailTitle, nativeTitle);
        rv = SetRegistryKey(HKEY_LOCAL_MACHINE, keyName.get(), "", NS_CONST_CAST(char *, nativeTitle.get()) ); 
    }
    else
        rv = NS_ERROR_FAILURE;

    if (NS_SUCCEEDED(rv)) {
        nsCAutoString thisApp (thisApplication()) ;
        nsCAutoString dllPath (thisApp) ;
        char* pathSep = (char *) _mbsrchr((const unsigned char *) thisApp.get(), '\\');
        if (pathSep) 
            dllPath.Truncate(pathSep - thisApp.get() + 1);
        dllPath += "mozMapi32.dll";
        
        // (2) set the DllPath attribute on our new entry for MAPI
        SetRegistryKey(HKEY_LOCAL_MACHINE, keyName.get(), "DLLPath", (char *)dllPath.get());
        
        // (3) now that we have added Software\\Clients\\Mail\\<app name> add subkeys for each protocol mail supports
        setupDefaultProtocolKey(nsCString(keyName + NS_LITERAL_CSTRING("\\Protocols\\")).get(), "mailto", "URL:MailTo Protocol", "-compose");


        setupFileExtension("Software\\Classes\\", ".eml");

        // (4) Software\Clients\News\<app name>\shell\open\command value 
        nsCAutoString appKeyName;
        appKeyName.Assign(keyName);
        appKeyName.AppendLiteral("\\shell\\open\\command");
        nsCAutoString mailAppPath(thisApp);
        mailAppPath += " -mail";
        SetRegistryKey(HKEY_LOCAL_MACHINE, appKeyName.get(), "", (char *)mailAppPath.get());

        // (5) Add a properties key that launches our options chrome
#ifdef MOZ_THUNDERBIRD
        appKeyName.Assign(keyName);
        appKeyName.AppendLiteral("\\shell\\properties\\command");
        nsCAutoString optionsPath(thisApp);
        optionsPath += " -options";
        SetRegistryKey(HKEY_LOCAL_MACHINE, appKeyName.get(), "", (char *)optionsPath.get());

        nsCOMPtr<nsIStringBundle> bundle;
        rv = MakeMapiStringBundle (getter_AddRefs (bundle));
        if (bundle)
        {
          appKeyName.Assign(keyName);
          appKeyName.AppendLiteral("\\shell\\properties");

          nsXPIDLString brandShortName;
          getVarValue(NS_LITERAL_STRING("brandShortName").get(), brandShortName);

          const PRUnichar* brandNameStrings[] = { brandShortName.get() };

          nsXPIDLString optionsTitle;
          bundle->FormatStringFromName(NS_LITERAL_STRING("optionsLabel").get(),
                                       brandNameStrings, 1, getter_Copies(optionsTitle));
          nsCAutoString nativeOptionsTitle;
          NS_CopyUnicodeToNative(optionsTitle, nativeOptionsTitle);
          SetRegistryKey(HKEY_LOCAL_MACHINE, appKeyName.get(), "", (char *) nativeOptionsTitle.get());
        }
#endif
                        
        // (5) add a default icon entry to Software\\Clients\\Mail\\<app name>
        nsCAutoString iconPath(thisApp);
        iconPath += ",0";
        nsCAutoString iconKeyName (keyName);
        iconKeyName.AppendLiteral("\\DefaultIcon");
        SetRegistryKey(HKEY_LOCAL_MACHINE, iconKeyName.get(),"", (char *)iconPath.get());

        // if we got this far, set a flag in the registry so we know we have registered ourself as a default mail application
        SetRegistryKey(HKEY_LOCAL_MACHINE, kAppDesktopKey, "registeredAsMailApp", "1");
    }

    return rv;
}

nsresult nsMapiRegistryUtils::registerNewsApp(PRBool aForceRegistration)
{
    nsresult rv = NS_OK;
    nsCAutoString keyName("Software\\Clients\\News\\");
    nsCAutoString appName;
    NS_CopyUnicodeToNative(vendorName(), appName);

    // Be SMART! If we have already set these keys up, don't do it again. Check the registeredAsNewsApp flag
    // that we stored in our desktop registry scratchpad
    nsCAutoString registeredAsNewsApp;
    GetRegistryKey(HKEY_LOCAL_MACHINE, kAppDesktopKey, "registeredAsNewsApp", registeredAsNewsApp);

    if (!aForceRegistration && registeredAsNewsApp.Equals("1")) 
        return NS_OK;

    // (1) Add our app to the list of keys under Software\\Clients\\News so we show up as a possible News application  
    if (!appName.IsEmpty()) {
        keyName.Append(appName.get());

        nsCOMPtr<nsIStringBundle> bundle;
        rv = MakeMapiStringBundle (getter_AddRefs (bundle)) ;
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        nsXPIDLString defaultMailTitle;
        // Use vendorName instead of brandname since brandName is product name
        // and has more than just the name of the application
        const PRUnichar *keyValuePrefixStr[] = { vendorName().get() };
        NS_NAMED_LITERAL_STRING(defaultMailTitleTag, "defaultMailDisplayTitle");
        rv = bundle->FormatStringFromName(defaultMailTitleTag.get(),
                                      keyValuePrefixStr, 1,
                                      getter_Copies(defaultMailTitle));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        nsCAutoString nativeTitle;
        NS_CopyUnicodeToNative(defaultMailTitle, nativeTitle);

        rv = SetRegistryKey(HKEY_LOCAL_MACHINE, keyName.get(), "", NS_CONST_CAST(char *, nativeTitle.get()) );  
    }
    else
        rv = NS_ERROR_FAILURE;

    if (NS_SUCCEEDED(rv)) {
        nsCAutoString thisApp (thisApplication());
        if (NS_SUCCEEDED(rv)) {

            // (3) now that we have added Software\\Clients\\News\\<app name>
            //     add subkeys for each protocol news supports
            nsCString protocolKeyName(keyName + NS_LITERAL_CSTRING("\\Protocols\\"));
            setupDefaultProtocolKey(protocolKeyName.get(), "news", "URL:News Protocol", "-mail");
            setupDefaultProtocolKey(protocolKeyName.get(), "nntp", "URL:NNTP Protocol", "-mail");
            setupDefaultProtocolKey(protocolKeyName.get(), "snews", "URL:Snews Protocol", "-mail");

            // (4) Software\Clients\News\<app name>\shell\open\command value 
            nsCAutoString appKeyName;
            appKeyName.Assign(keyName);
            appKeyName.AppendLiteral("\\shell\\open\\command");
            rv = SetRegistryKey(HKEY_LOCAL_MACHINE, appKeyName.get(), "", (char *)thisApp.get());
                        
            // (5) add a default icon entry to Software\\Clients\\News\\<app name>
                if (NS_SUCCEEDED(rv)) {
                    nsCAutoString iconPath(thisApp);
                    iconPath += ",0";
                 nsCAutoString iconKeyName (keyName);
                    iconKeyName.AppendLiteral("\\DefaultIcon");
                 rv = SetRegistryKey(HKEY_LOCAL_MACHINE, iconKeyName.get(),"", (char *)iconPath.get());
                }
            }            

        // if we got this far, set a flag in the registry so we know we have registered ourself as a default news application
        SetRegistryKey(HKEY_LOCAL_MACHINE, kAppDesktopKey, "registeredAsNewsApp", "1");
                }

    return rv;
}            

/** Sets Mozilla as the default News Client 
 */
nsresult nsMapiRegistryUtils::setDefaultNewsClient()
{
    nsresult rv;
    nsresult mailKeySet=NS_ERROR_FAILURE;

    if (mRestrictedRegAccess) 
      return NS_ERROR_FAILURE;

    rv = saveDefaultNewsClient();

    if (NS_FAILED(saveUserDefaultNewsClient()) || NS_FAILED(rv)) 
      return NS_ERROR_FAILURE;
    
    // make sure we have already registered ourself as a potential news application with the OS:
    registerNewsApp(PR_TRUE);

    // give Software\Clients\News a default value of our application name.
    nsCAutoString appName;
    NS_CopyUnicodeToNative(vendorName(), appName);

    SetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Clients\\News", "", (char *)appName.get());

    // delete the existing news related protocol keys from HKEY_LOCAL_MACHINE\Classes
    recursiveDeleteKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\news"); 
    recursiveDeleteKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\nntp"); 
    recursiveDeleteKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\snews"); 

    // copy our protocol handlers out to HKEY_LOCAL_MACHINE\Classes\<protocol>
    nsCAutoString keyName("Software\\Clients\\News\\");
    keyName.Append(appName);
    keyName.AppendLiteral("\\Protocols\\");

    rv = setProtocolHandler(keyName.get(), "news"); 
    rv = setProtocolHandler(keyName.get(), "snews"); 
    rv = setProtocolHandler(keyName.get(), "nntp"); 

    rv = SetRegistryKey(HKEY_CURRENT_USER, "Software\\Clients\\News", "", (char *)appName.get());

    // send out a system notifcation to let everyone know the default news client has changed
    ::SendMessageTimeout( HWND_BROADCAST,
                          WM_SETTINGCHANGE,
                          0,
                          (LPARAM)MOZ_CLIENT_NEWS_KEY,
                          SMTO_NORMAL|SMTO_ABORTIFHUNG,
                          MOZ_HWND_BROADCAST_MSG_TIMEOUT,
                          NULL);
    return rv;
}

nsresult nsMapiRegistryUtils::setDefaultFeedClient()
{
    return setupDefaultProtocolKey("Software\\Classes\\", "feed", "URL:Feed Protocol", "-mail");
}

nsresult nsMapiRegistryUtils::unsetDefaultFeedClient()
{
    // delete the existing mail related protocol keys from HKEY_LOCAL_MACHINE\Software\Classes
    return recursiveDeleteKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\feed"); 
}

PRBool nsMapiRegistryUtils::IsDefaultFeedClient()
{  
    //first try to get the users default news client
    nsCAutoString result; 
    GetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\feed\\shell\\open\\command", "", result);     

    if (!result.IsEmpty()) {
       nsCAutoString strExtension;
       strExtension.Assign(EXE_EXTENSION);
       ToUpperCase(result);
       PRInt32 index = result.RFind(strExtension.get());
       if (index != kNotFound) 
           result.Truncate(index + strExtension.Length());

       nsCAutoString thisApp (thisApplication()) ;
       return (result == thisApp);
    }

    return PR_FALSE;
}

/** Sets Mozilla as default Mail Client
 */
nsresult nsMapiRegistryUtils::setDefaultMailClient()
{
    nsresult rv = NS_OK;    
    if (mRestrictedRegAccess) 
      return NS_ERROR_FAILURE;
    
    if (!isSmartDll()) {
        if (NS_FAILED(CopyMozMapiToWinSysDir())) 
          return NS_ERROR_FAILURE;
    }

    rv = saveDefaultMailClient();
    if (NS_SUCCEEDED(rv))
      rv = saveUserDefaultMailClient();
    
    NS_ENSURE_SUCCESS(rv, rv);
    
    // make sure we have already registered ourself as a potential mail application with the OS:
    registerMailApp(PR_TRUE);

    // give Software\Clients\Mail a default value of our application name.
    nsCAutoString appName;
    NS_CopyUnicodeToNative(vendorName(), appName);
    SetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Clients\\Mail", "", (char *)appName.get());

    // if we succeeded in setting ourselves as the default mapi client, then 
     // make sure we also set ourselves as the mailto protocol handler for the user...
     nsCAutoString keyName("Software\\Clients\\Mail\\");
     keyName.Append(appName);
     keyName.AppendLiteral("\\Protocols\\");
        
    // delete the existing mail related protocol keys from HKEY_LOCAL_MACHINE\Software\Classes
    recursiveDeleteKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\mailto"); 
    rv = setProtocolHandler(keyName.get(), "mailto");

    rv = SetRegistryKey(HKEY_CURRENT_USER, "Software\\Clients\\Mail", "", (char *)appName.get());

    // We are now the default mail client so set defaultMailHasBeenSet
    if (NS_SUCCEEDED(rv))
      rv = SetRegistryKey(HKEY_LOCAL_MACHINE, kAppDesktopKey, "defaultMailHasBeenSet", "1");

     // now notify anyone listening that there is a new default mail client in town
        ::SendMessageTimeout( HWND_BROADCAST,
                              WM_SETTINGCHANGE,
                              0,
                              (LPARAM)MOZ_CLIENT_MAIL_KEY,
                              SMTO_NORMAL|SMTO_ABORTIFHUNG,
                              MOZ_HWND_BROADCAST_MSG_TIMEOUT,
                              NULL);

    // register the new mapi server
    RegisterServer(CLSID_CMapiImp, "Mozilla MAPI", "MozillaMapi", "MozillaMapi.1");
    return rv;
}

nsresult nsMapiRegistryUtils::unsetDefaultNewsClient() {
    nsresult rv = NS_OK;
    nsresult mailKeySet = NS_ERROR_FAILURE;

    if (mRestrictedRegAccess) 
      return NS_ERROR_FAILURE;
    
    nsCAutoString name ;

    // get the name of the default news client
    GetRegistryKey(HKEY_LOCAL_MACHINE, kAppDesktopKey, "HKEY_LOCAL_MACHINE\\Software\\Clients\\News", name);
    
    // Use vendorName instead of brandname since brandName is product name
    // and has more than just the name of the application
    nsCAutoString appName;
    NS_CopyUnicodeToNative(vendorName(), appName);
    
    // if we are the current default client....
    if (!name.IsEmpty() && !appName.IsEmpty() && name.Equals(appName))  
    {
      // XXX Do we need to do anything here? Maybe we want to clear out name. If we were the previous default news client
      // and the user is trying to unset us as the current default news client, we probably don't want to re-store ourselves
      // again?

      name = "";
    }

    // delete the protocol keys we copied to HKEY_LOCAL_MACHINE\Classes when we were made the default news application

    recursiveDeleteKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\news"); 
    recursiveDeleteKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\nntp"); 
    recursiveDeleteKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\snews"); 

    // set HKEY_LOCAL_MACHINE\Software\Clients\News
    if (!name.IsEmpty()) 
    {
      mailKeySet = SetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Clients\\News", "", (char *)name.get());

      // copy the protocol handlers for the original default news app out to HKEY_LOCAL_MACHINE\Classes\<protocol>
      nsCAutoString keyName("Software\\Clients\\News\\");
      keyName.Append(name);
      keyName.AppendLiteral("\\Protocols\\");

      rv = setProtocolHandler(keyName.get(), "news"); 
      rv = setProtocolHandler(keyName.get(), "snews"); 
      rv = setProtocolHandler(keyName.get(), "nntp"); 
    }
    else {
        mailKeySet = SetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Clients\\News","", "");
    }
    
    // change HKEY_CURRENT_USER\\Software\\Clients\\News 
    nsCAutoString userAppName ;
    GetRegistryKey(HKEY_LOCAL_MACHINE, kAppDesktopKey, "HKEY_CURRENT_USER\\Software\\Clients\\News", userAppName);

    if (!userAppName.IsEmpty()) 
        SetRegistryKey(HKEY_CURRENT_USER, "Software\\Clients\\News", "", (char *)userAppName.get());
    else 
        DeleteRegistryValue(HKEY_CURRENT_USER, "Software\\Clients\\News", "");

    
     // send out a system notifcation to let everyone know the default news client has changed
    ::SendMessageTimeout( HWND_BROADCAST,
                          WM_SETTINGCHANGE,
                          0,
                          (LPARAM)MOZ_CLIENT_NEWS_KEY,
                          SMTO_NORMAL|SMTO_ABORTIFHUNG,
                          MOZ_HWND_BROADCAST_MSG_TIMEOUT,
                          NULL);
    return mailKeySet;
}

/** Removes Mozilla as the default Mail client and restores the previous setting
 */
nsresult nsMapiRegistryUtils::unsetDefaultMailClient() {
    nsresult result = NS_OK;
    nsresult mailKeySet = NS_ERROR_FAILURE;
    
    if (mRestrictedRegAccess) 
      return NS_ERROR_FAILURE;

    if (!isSmartDll()) 
    {
      result = RestoreBackedUpMapiDll();
      NS_ENSURE_SUCCESS(result, result);
    }

    nsCAutoString name ;
    GetRegistryKey(HKEY_LOCAL_MACHINE, kAppDesktopKey, "HKEY_LOCAL_MACHINE\\Software\\Clients\\Mail", name);

    // Use vendorName instead of brandname since brandName is product name
    // and has more than just the name of the application
    nsCAutoString appName;
    NS_CopyUnicodeToNative(vendorName(), appName);

    if (!name.IsEmpty() && !appName.IsEmpty() && name.Equals(appName)) {
        nsCAutoString keyName("HKEY_LOCAL_MACHINE\\Software\\Clients\\Mail\\");
        keyName.Append(appName.get());
        keyName.AppendLiteral("\\Protocols\\mailto\\shell\\open\\command");
        nsCAutoString appPath ;
        GetRegistryKey(HKEY_LOCAL_MACHINE, kAppDesktopKey, keyName.get(), appPath);
        if (!appPath.IsEmpty()) {
            keyName.Assign("Software\\Clients\\Mail\\");
            keyName.Append(appName.get());
            keyName.AppendLiteral("\\Protocols\\mailto\\shell\\open\\command");
            result = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                       keyName.get(), 
                       "", (char *)appPath.get());
            if (NS_SUCCEEDED(result)) {
                char* pathSep = (char *)
                    _mbsrchr((const unsigned char *) appPath.get(), '\\');
                if (pathSep) 
                    appPath.Truncate(pathSep - appPath.get() + 1);
                appPath += "mozMapi32.dll";
                keyName.Assign("Software\\Clients\\Mail\\");
                keyName.Append(appName.get());
                result = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                                        keyName.get(), 
                                        "DLLPath", (char *) appPath.get());
            }
        }
    }

    recursiveDeleteKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\mailto"); 

    if (!name.IsEmpty())  // Restore the previous default app for Software\Clients\Mail
    {
        // copy the protocol handlers for the original default mail app out to HKEY_LOCAL_MACHINE\Classes\<protocol>

        nsCAutoString keyName("Software\\Clients\\Mail\\");
        keyName.Append(name);
        keyName.AppendLiteral("\\Protocols\\");
        setProtocolHandler(keyName.get(), "mailto"); 

        mailKeySet = SetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Clients\\Mail", "", (char *)name.get());
    }
    else
        mailKeySet = SetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Clients\\Mail", "", "");

    // change HKEY_CURRENT_USER\\Software\\Clients\\Mail 
    nsCAutoString userAppName ;
    GetRegistryKey(HKEY_LOCAL_MACHINE, kAppDesktopKey, "HKEY_CURRENT_USER\\Software\\Clients\\Mail", userAppName);
    
    restoreFileExtension("Software\\Classes\\", ".eml");

    if (!userAppName.IsEmpty()) 
        SetRegistryKey(HKEY_CURRENT_USER, "Software\\Clients\\Mail", "", (char *)userAppName.get());
    else 
        DeleteRegistryValue(HKEY_CURRENT_USER, "Software\\Clients\\Mail", "");

    SetRegistryKey(HKEY_LOCAL_MACHINE, kAppDesktopKey,"defaultMailHasBeenSet", "0");

    ::SendMessageTimeout( HWND_BROADCAST,
                          WM_SETTINGCHANGE,
                          0,
                          (LPARAM)MOZ_CLIENT_MAIL_KEY,
                          SMTO_NORMAL|SMTO_ABORTIFHUNG,
                          MOZ_HWND_BROADCAST_MSG_TIMEOUT,
                          NULL);

    UnregisterServer(CLSID_CMapiImp, "MozillaMapi", "MozillaMapi.1");
    return mailKeySet;
}

/** Returns FALSE if showMapiDialog is set to 0.
 *  Returns TRUE otherwise
 *  Also returns TRUE if the Mozilla has been set as the default mail client
 *  and some other application has changed that setting.
 *  This function gets called only if the current app is not the default mail
 *  client
 */
PRBool nsMapiRegistryUtils::getShowDialog() {
    PRBool rv = PR_FALSE;
    nsCAutoString showDialog ;
    GetRegistryKey(HKEY_LOCAL_MACHINE, kAppDesktopKey, 
                                        "showMapiDialog", showDialog);
    // if the user has not selected the checkbox, show dialog 
    if (showDialog.IsEmpty() || showDialog.Equals("1"))
      rv = PR_TRUE;

    if (!rv) 
    {
        // even if the user has selected the checkbox
        // show it if some other application has changed the 
        // default setting.
        nsCAutoString setMailDefault ;
        GetRegistryKey(HKEY_LOCAL_MACHINE,kAppDesktopKey, 
                                       "defaultMailHasBeenSet", setMailDefault);
        if (setMailDefault.Equals("1")) 
        {
            // need to reset the defaultMailHasBeenSet to "0"
            // so that after the dialog is displayed once,
            // we do not keep displaying this dialog after the user has
            // selected the checkbox
            rv = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                                kAppDesktopKey, 
                                "defaultMailHasBeenSet", "0");
            rv = PR_TRUE;
        }
    }
    return rv;
}

nsresult nsMapiRegistryUtils::MakeMapiStringBundle(nsIStringBundle ** aMapiStringBundle)
{
    nsresult rv = NS_OK ;

    if (m_mapiStringBundle)
    {
        *aMapiStringBundle = m_mapiStringBundle ;
        NS_ADDREF(*aMapiStringBundle);
        return rv ;
    }

    nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(
                                     NS_STRINGBUNDLE_CONTRACTID, &rv));
    if (NS_FAILED(rv) || !bundleService) return NS_ERROR_FAILURE;

    rv = bundleService->CreateBundle(
                    MAPI_PROPERTIES_CHROME,
                    getter_AddRefs(m_mapiStringBundle));
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ADDREF(*aMapiStringBundle = m_mapiStringBundle) ;

    return rv ;
}

nsresult nsMapiRegistryUtils::ShowMapiErrorDialog(PRBool aForMail) 
{
    nsresult rv;
    nsCOMPtr<nsIPromptService> promptService(do_GetService(
                  NS_PROMPTSERVICE_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv) && promptService)
    {
        nsCOMPtr<nsIStringBundle> bundle;
        rv = MakeMapiStringBundle (getter_AddRefs (bundle)) ;
        NS_ENSURE_SUCCESS(rv, rv);

        nsXPIDLString dialogTitle;
        const PRUnichar *brandStrings[] = { brandName() };
        NS_NAMED_LITERAL_STRING(dialogTitlePropertyTag, "errorMessageTitle");
        rv = bundle->FormatStringFromName(dialogTitlePropertyTag.get(),
                                          brandStrings, 1,
                                          getter_Copies(dialogTitle));
        NS_ENSURE_SUCCESS(rv, rv);

        nsXPIDLString dialogText;

        NS_NAMED_LITERAL_STRING(dialogTextPropertyTag, "errorMessage");
        NS_NAMED_LITERAL_STRING(dialogTextPropertyTagForNews, "errorMessageNews");


        rv = bundle->FormatStringFromName(aForMail ? dialogTextPropertyTag.get() : dialogTextPropertyTagForNews.get(),
                                          brandStrings, 1,
                                          getter_Copies(dialogText));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = promptService->Alert(nsnull, dialogTitle, dialogText);
    }
    return rv;
}

