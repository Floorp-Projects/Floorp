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
#include <mbstring.h>

#define EXE_EXTENSION ".exe" 
#define MOZ_HWND_BROADCAST_MSG_TIMEOUT 5000
#define MOZ_CLIENT_MAIL_KEY "Software\\Clients\\Mail"

nsMapiRegistryUtils::nsMapiRegistryUtils()
{
   m_mapiStringBundle = nsnull ;
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

void nsMapiRegistryUtils::getVarValue(const PRUnichar * varName, nsAutoString & result)
{
    nsresult rv;
    nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(
                                         NS_STRINGBUNDLE_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv) && bundleService) {
        nsCOMPtr<nsIStringBundle> brandBundle;
        rv = bundleService->CreateBundle(
                    "chrome://global/locale/brand.properties",
                    getter_AddRefs(brandBundle));
        if (NS_SUCCEEDED(rv)) {
            nsXPIDLString value;
            rv = brandBundle->GetStringFromName(
                       varName,
                       getter_Copies(value));
            if (NS_SUCCEEDED(rv))
                result = value;
        }
    }
}

const PRUnichar * nsMapiRegistryUtils::brandName() 
{
    if (m_brand.IsEmpty())
        getVarValue(NS_LITERAL_STRING("brandShortName").get(), m_brand);
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



PRBool nsMapiRegistryUtils::IsDefaultMailClient()
{
    if (!isSmartDll() && !isMozDll()) 
        return PR_FALSE;
    //first try to get the users default mail client
    nsCAutoString name; 
    GetRegistryKey(HKEY_CURRENT_USER, "Software\\Clients\\Mail", "", name);
    //if that fails then get the machine's default client
    if(name.IsEmpty()){
        GetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Clients\\Mail", "", name);
    }

    if (!name.IsEmpty()) {
         nsCAutoString keyName("Software\\Clients\\Mail\\");
         keyName += name.get(); 
         keyName += "\\protocols\\mailto\\shell\\open\\command";

         nsCAutoString result;
         GetRegistryKey(HKEY_LOCAL_MACHINE, keyName.get(), "", result);
         if (!result.IsEmpty()) {
             nsCAutoString strExtension;
             strExtension.Assign(EXE_EXTENSION);
             ToUpperCase(result);
             ToUpperCase(strExtension);
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

nsresult nsMapiRegistryUtils::saveDefaultMailClient()
{
    nsresult rv;
    nsCAutoString name ;
    GetRegistryKey(HKEY_LOCAL_MACHINE,"Software\\Clients\\Mail", "", name);
    if (!name.IsEmpty()) {
        rv = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                            "Software\\Mozilla\\Desktop", 
                            "HKEY_LOCAL_MACHINE\\Software\\Clients\\Mail", 
                            (char *)name.get());
        if (NS_SUCCEEDED(rv)) {
            nsCAutoString keyName("Software\\Clients\\Mail\\");
            keyName += name.get(); 
            keyName += "\\protocols\\mailto\\shell\\open\\command";
            nsCAutoString appPath ;
            GetRegistryKey(HKEY_LOCAL_MACHINE, keyName.get(), "", appPath);
            if (!appPath.IsEmpty()) {
                nsCAutoString stringName("HKEY_LOCAL_MACHINE\\");
                stringName += keyName.get();
                rv = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                                  "Software\\Mozilla\\Desktop", 
                                  stringName.get(), (char *)appPath.get());
            }
        }
    }
    else
        rv = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                            "Software\\Mozilla\\Desktop", 
                            "HKEY_LOCAL_MACHINE\\Software\\Clients\\Mail", 
                            "");
    return rv;
} 

nsresult nsMapiRegistryUtils::saveUserDefaultMailClient()
{
    nsresult rv;
    nsCAutoString name ;
    GetRegistryKey(HKEY_CURRENT_USER,"Software\\Clients\\Mail", "", name);
    if (!name.IsEmpty()) {
        rv = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                            "Software\\Mozilla\\Desktop", 
                            "HKEY_CURRENT_USER\\Software\\Clients\\Mail", 
                            (char *)name.get());
    }
    else {
        rv = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                            "Software\\Mozilla\\Desktop", 
                            "HKEY_CURRENT_USER\\Software\\Clients\\Mail", 
                            "");
   }
   return rv;
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
    filePath.Append("\\Mapi32_moz_bak.dll");

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
                            "Software\\Mozilla\\Desktop", 
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
                            "Software\\Mozilla\\Desktop", 
                            "Mapi_backup_dll");
    return rv;
}

nsresult nsMapiRegistryUtils::setMailtoProtocolHandler()
{
    // make sure mailto urls go through our application if we are the default
    // mail application...
    nsCAutoString appName;
    NS_CopyUnicodeToNative(vendorName(), appName);

    nsCAutoString mailAppPath(thisApplication());
    mailAppPath += " -compose %1";

    nsresult rv = SetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\mailto\\shell\\open\\command", "", (char *)mailAppPath.get());
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString iconPath(thisApplication());
    iconPath += ",0";
    return SetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\mailto\\DefaultIcon", "", (char *)iconPath.get());
}

nsresult nsMapiRegistryUtils::setNewsProtocolHandler()
{
    // make sure news and snews urls go through our application if we are the default
    // mail application...
    nsCAutoString appName;
    NS_CopyUnicodeToNative(vendorName(), appName);

    nsCAutoString mailAppPath(thisApplication());
    mailAppPath += " -mail %1";

    nsresult rv = SetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\news\\shell\\open\\command", "", (char *)mailAppPath.get());
    NS_ENSURE_SUCCESS(rv, rv);
    rv = SetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\snews\\shell\\open\\command", "", (char *)mailAppPath.get());
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString iconPath(thisApplication());
    iconPath += ",0";
    rv = SetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\news\\DefaultIcon", "", (char *)iconPath.get());
    NS_ENSURE_SUCCESS(rv, rv);

    return SetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Classes\\snews\\DefaultIcon", "", (char *)iconPath.get());
}

/** Sets Mozilla as default Mail Client
 */
nsresult nsMapiRegistryUtils::setDefaultMailClient()
{
    nsresult rv;
    nsresult mailKeySet=NS_ERROR_FAILURE;
    if (verifyRestrictedAccess()) return NS_ERROR_FAILURE;
    if (!isSmartDll()) {
        if (NS_FAILED(CopyMozMapiToWinSysDir())) return NS_ERROR_FAILURE;
    }
    rv = saveDefaultMailClient();
    if (NS_FAILED(saveUserDefaultMailClient()) ||
        NS_FAILED(rv)) return NS_ERROR_FAILURE;
    nsCAutoString keyName("Software\\Clients\\Mail\\");

    nsCAutoString appName;
    NS_CopyUnicodeToNative(vendorName(), appName);
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
        rv = SetRegistryKey(HKEY_LOCAL_MACHINE, 
             keyName.get(), 
             "", NS_CONST_CAST(char *, nativeTitle.get()) ) ; 
    }
    else
        rv = NS_ERROR_FAILURE;
    if (NS_SUCCEEDED(rv)) {
        nsCAutoString thisApp (thisApplication()) ;
        if (NS_FAILED(rv)) return rv ;

        nsCAutoString dllPath (thisApp) ;
        char* pathSep = (char *) _mbsrchr((const unsigned char *) thisApp.get(), '\\');
        if (pathSep) 
            dllPath.Truncate(pathSep - thisApp.get() + 1);
        dllPath += "mozMapi32.dll";
        rv = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                            keyName.get(), "DLLPath", 
                            (char *)dllPath.get());
        if (NS_SUCCEEDED(rv)) {
            keyName.Append("\\protocols\\mailto");
            rv = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                                keyName.get(), 
                                "", "URL:MailTo Protocol");
            if (NS_SUCCEEDED(rv)) {
                nsCAutoString appPath (thisApp);
                appPath += " \"%1\"";
                keyName.Append("\\shell\\open\\command");
                rv = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                       keyName.get(), 
                       "", (char *)appPath.get());
                if (NS_SUCCEEDED(rv)) {
                    rv = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                                        "Software\\Clients\\Mail", 
                                        "", (char *)appName.get());
                }
                if (NS_SUCCEEDED(rv)) {
                    nsCAutoString mailAppPath(thisApp);
                    mailAppPath += " -mail";
                    nsCAutoString appKeyName ("Software\\Clients\\Mail\\");
                    appKeyName.Append(appName.get());
                    appKeyName.Append("\\shell\\open\\command");
                    rv = SetRegistryKey(HKEY_LOCAL_MACHINE,
                                        appKeyName.get(),
                                        "", (char *)mailAppPath.get());
                }
                if (NS_SUCCEEDED(rv)) {
                    nsCAutoString iconPath(thisApp);
                    iconPath += ",0";
                    nsCAutoString iconKeyName ("Software\\Clients\\Mail\\");
                    iconKeyName.Append(appName.get());
                    iconKeyName.Append("\\DefaultIcon");
                    mailKeySet = SetRegistryKey(HKEY_LOCAL_MACHINE,
                                        iconKeyName.get(),
                                        "", (char *)iconPath.get());
                }
            }            
        }
    }

    if (NS_SUCCEEDED(mailKeySet)) {

#ifdef MOZ_THUNDERBIRD
        // XXX: Test this out in thunderbird first. Hopefully we can just remove this ifdef and let seamonkey
        // register the same keys if the feedback is good. 

        // if we succeeded in setting ourselves as the default mapi client, then 
        // make sure we also set ourselves as the mailto protocol handler for the user...
        rv = setMailtoProtocolHandler(); 
        
        // and set our selves as the default news protocol handler
        // rv = setNewsProtocolHandler(); // XXX news is not ready to be turned on yet. 
#endif

        // ignore the error returned by setNewsProtocolHandler and setMailtoProtocolHandler

        nsresult desktopKeySet = SetRegistryKey(HKEY_CURRENT_USER, 
                                                "Software\\Clients\\Mail",
                                                "", (char *)appName.get());
        if (NS_SUCCEEDED(desktopKeySet)) {
            desktopKeySet = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                                           "Software\\Mozilla\\Desktop", 
                                           "defaultMailHasBeenSet", "1");
        }
        ::SendMessageTimeout( HWND_BROADCAST,
                              WM_SETTINGCHANGE,
                              0,
                              (LPARAM)MOZ_CLIENT_MAIL_KEY,
                              SMTO_NORMAL|SMTO_ABORTIFHUNG,
                              MOZ_HWND_BROADCAST_MSG_TIMEOUT,
                              NULL);
        RegisterServer(CLSID_CMapiImp, "Mozilla MAPI", "MozillaMapi", "MozillaMapi.1");
        return desktopKeySet;
    }
    
    return mailKeySet;
}

/** Removes Mozilla as the default Mail client and restores the previous setting
 */
nsresult nsMapiRegistryUtils::unsetDefaultMailClient() {
    nsresult result = NS_OK;
    nsresult mailKeySet = NS_ERROR_FAILURE;
    if (verifyRestrictedAccess()) return NS_ERROR_FAILURE;
    if (!isSmartDll()) {
        if (NS_FAILED(RestoreBackedUpMapiDll())) return NS_ERROR_FAILURE;
    }
    nsCAutoString name ;
    GetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Mozilla\\Desktop", 
                                      "HKEY_LOCAL_MACHINE\\Software\\Clients\\Mail", name);
    // Use vendorName instead of brandname since brandName is product name
    // and has more than just the name of the application
    nsCAutoString appName;
    NS_CopyUnicodeToNative(vendorName(), appName);

    if (!name.IsEmpty() && !appName.IsEmpty() && name.Equals(appName)) {
        nsCAutoString keyName("HKEY_LOCAL_MACHINE\\Software\\Clients\\Mail\\");
        keyName.Append(appName.get());
        keyName.Append("\\protocols\\mailto\\shell\\open\\command");
        nsCAutoString appPath ;
        GetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Mozilla\\Desktop", 
                        keyName.get(), appPath);
        if (!appPath.IsEmpty()) {
            keyName.Assign("Software\\Clients\\Mail\\");
            keyName.Append(appName.get());
            keyName.Append("\\protocols\\mailto\\shell\\open\\command");
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
    if (!name.IsEmpty()) {
        if (NS_SUCCEEDED(result)) {
            mailKeySet = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                                "Software\\Clients\\Mail", 
                                "", (char *)name.get());
        }
    }
    else
        mailKeySet = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                                "Software\\Clients\\Mail", 
                                "", "");

    if (NS_SUCCEEDED(mailKeySet)) {
        nsCAutoString userAppName ;
        GetRegistryKey(HKEY_LOCAL_MACHINE, 
                                  "Software\\Mozilla\\Desktop", 
                                  "HKEY_CURRENT_USER\\Software\\Clients\\Mail", userAppName);
        nsresult desktopKeySet = NS_OK;
        if (!userAppName.IsEmpty()) {
            desktopKeySet = SetRegistryKey(HKEY_CURRENT_USER, 
                                           "Software\\Clients\\Mail", 
                                           "", (char *)userAppName.get());
        }
        else {
            DeleteRegistryValue(HKEY_CURRENT_USER, "Software\\Clients\\Mail", "");
        }
        if (NS_SUCCEEDED(desktopKeySet)) {
            desktopKeySet = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                                           "Software\\Mozilla\\Desktop", 
                                           "defaultMailHasBeenSet", "0");
        }
        ::SendMessageTimeout( HWND_BROADCAST,
                              WM_SETTINGCHANGE,
                              0,
                              (LPARAM)MOZ_CLIENT_MAIL_KEY,
                              SMTO_NORMAL|SMTO_ABORTIFHUNG,
                              MOZ_HWND_BROADCAST_MSG_TIMEOUT,
                              NULL);
        UnregisterServer(CLSID_CMapiImp, "MozillaMapi", "MozillaMapi.1");
        return desktopKeySet;
    }
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
    GetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Mozilla\\Desktop", 
                                        "showMapiDialog", showDialog);
    // if the user has not selected the checkbox, show dialog 
    if (showDialog.IsEmpty() || showDialog.Equals("1"))
            rv = PR_TRUE;

    if (!rv) {
        // even if the user has selected the checkbox
        // show it if some other application has changed the 
        // default setting.
        nsCAutoString setMailDefault ;
        GetRegistryKey(HKEY_LOCAL_MACHINE,"Software\\Mozilla\\Desktop", 
                                       "defaultMailHasBeenSet", setMailDefault);
        if (setMailDefault.Equals("1")) {
            // need to reset the defaultMailHasBeenSet to "0"
            // so that after the dialog is displayed once,
            // we do not keep displaying this dialog after the user has
            // selected the checkbox
            rv = SetRegistryKey(HKEY_LOCAL_MACHINE, 
                                "Software\\Mozilla\\Desktop", 
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
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    NS_ADDREF(*aMapiStringBundle = m_mapiStringBundle) ;

    return rv ;
}

nsresult nsMapiRegistryUtils::ShowMapiErrorDialog() 
{
    nsresult rv;
    nsCOMPtr<nsIPromptService> promptService(do_GetService(
                  "@mozilla.org/embedcomp/prompt-service;1", &rv));
    if (NS_SUCCEEDED(rv) && promptService)
    {
        nsCOMPtr<nsIStringBundle> bundle;
        rv = MakeMapiStringBundle (getter_AddRefs (bundle)) ;
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        nsXPIDLString dialogTitle;
        const PRUnichar *brandStrings[] = { brandName() };
        NS_NAMED_LITERAL_STRING(dialogTitlePropertyTag, "errorMessageTitle");
        rv = bundle->FormatStringFromName(dialogTitlePropertyTag.get(),
                                          brandStrings, 1,
                                          getter_Copies(dialogTitle));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        nsXPIDLString dialogText;
        NS_NAMED_LITERAL_STRING(dialogTextPropertyTag, "errorMessage");
        rv = bundle->FormatStringFromName(dialogTextPropertyTag.get(),
                                          brandStrings, 1,
                                          getter_Copies(dialogText));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        rv = promptService->Alert(nsnull, dialogTitle,
                                  dialogText);
    }
    return rv;
}

