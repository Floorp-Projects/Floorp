/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
   Version 1.0 (the "License"); you may not use this file except in
   compliance with the License. You may obtain a copy of the License at
   http://www.mozilla.org/NPL/ 

   Software distributed under the License is distributed on an "AS IS" basis,
   WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
   for the specific language governing rights and limitations under the License. 

   The Original Code is Mozilla Communicator client code, released March 31, 1998. 

   The Initial Developer of the Original Code is Netscape Communications Corporation.
   Portions created by Netscape are Copyright (C) 1998 Netscape Communications Corporation.
   All Rights Reserved.
 */
#include "nsIDefaultBrowser.h"

#include "stdafx.h"
#include "dialog.h"

class nsWinDefaultBrowser;

static nsWinDefaultBrowser *pSingleton = NULL;

// Implementation of the abstract (XP) "default browser" interface for Windows.
class nsWinDefaultBrowser : public nsIDefaultBrowser {
    NS_DECL_ISUPPORTS

  public:
    // Functions to be implemented in this class.
    NS_IMETHOD_(PRBool) IsDefaultBrowser();
    NS_IMETHOD_(int)    DisplayDialog();
    NS_IMETHOD GetPreferences( Prefs &prefs );
    NS_IMETHOD SetPreferences( const Prefs &prefs );
    NS_IMETHOD_(PRBool) IsHandling( Thing thing );
    NS_IMETHOD StartHandling( Thing extOrProtocol );
    NS_IMETHOD StopHandling( Thing extOrProtocol );
    NS_IMETHOD HandlePerPreferences();

  private:
    // Constructor (private; use nsIDefaultBrowser::GetInterface).
    nsWinDefaultBrowser();

    // Destructor clears singleton pointer.
    virtual ~nsWinDefaultBrowser() {
        pSingleton = NULL;
    }

    friend class nsIDefaultBrowser;
};

// Standard implementation of AddRef/Release/QueryInterface.
NS_IMPL_ISUPPORTS( nsWinDefaultBrowser, NS_IDEFAULTBROWSER_IID );

// Minimal ctor.
nsWinDefaultBrowser::nsWinDefaultBrowser() {
    NS_INIT_REFCNT();
}

/* nsWinDefaultBrowser::IsDefaultBrowser
 *
 * For now, use old logic.
 */
PRBool nsWinDefaultBrowser::IsDefaultBrowser() {
    //PRBool result = !theApp.m_OwnedAndLostList.NonemptyLostIgnoredIntersection();
    PRBool result = TRUE;

    // Get user desktop integration preferences.
    Prefs prefs;
    if ( this->GetPreferences( prefs ) == NS_OK ) {
        // Check that all desired options are still in effect.
        if ( prefs.bHandleShortcuts ) {
            if ( ( prefs.bHandleHTTP && !IsHandling(kHTTP) )
                 || ( prefs.bHandleHTTPS && !IsHandling(kHTTPS) )
                 || ( prefs.bHandleFTP && !IsHandling(kFTP) ) ) {
                result = FALSE;
            }
        }
        // Next, check files.
        if ( result && prefs.bHandleFiles ) {
            if ( ( prefs.bHandleHTML && !IsHandling(kHTML) )
                 || ( prefs.bHandleJPEG && !IsHandling(kJPEG) )
                 || ( prefs.bHandleGIF && !IsHandling(kGIF) )
                 || ( prefs.bHandleJS && !IsHandling(kJS) )
                 || ( prefs.bHandleXBM && !IsHandling(kXBM) )
                 || ( prefs.bHandleTXT && !IsHandling(kTXT) ) ) {
                result = FALSE;
            }
        }
        // Lastly, check Active Desktop stuff.
        if ( result && prefs.bIntegrateWithActiveDesktop ) {
            if ( ( prefs.bUseInternetKeywords && !IsHandling(kInternetKeywords) )
                 || ( prefs.bUseNetcenterSearch && !IsHandling(kNetcenterSearch) )
                 || ( prefs.bDisableActiveDesktop && !IsHandling(kActiveDesktop) ) ) {
                result = FALSE;
            }
        }
    } else {
        // Error getting preferences, return FALSE so we
        // have chance to rectify the error.
        result = FALSE;
    }

    return result;
}

/* nsWinDefaultBrowser::DisplayDialog
 *
 * Display dialog and return the user response.
 */
int nsWinDefaultBrowser::DisplayDialog() {
    CDefaultBrowserDlg dialog(this);
    int result = dialog.DoModal();
    return result;
}


nsIDefaultBrowser *nsIDefaultBrowser::GetInterface() {
    // If singleton hasn't been created, do it now.
    if ( !pSingleton ) {
        pSingleton = new nsWinDefaultBrowser;
    }
    pSingleton->AddRef();
    return pSingleton;
}

/* getPRBoolFromRegistry
 *
 * Utility to fetch registry value as PRBool.
 */
static PRBool getPRBoolFromRegistry( HKEY key, const char* valueName ) {
    DWORD value = FALSE;
    DWORD len = sizeof value;
    DWORD type;
    ::RegQueryValueEx( key, valueName, NULL, &type, (LPBYTE)&value, &len );
    return !!value;
}

/* putPRBoolIntoRegistry
 *
 * Utility to set PRBool registry value.
 */
static void putPRBoolIntoRegistry( HKEY key, const char* valueName, PRBool value ) {
    ::RegSetValueEx( key, valueName, NULL, REG_DWORD, (LPBYTE)&value, sizeof value );
}

/* getRegistryPrefsKeyName
 *
 * Utility to obtain name of registry key for desktop preferences.
 */
static CString getRegistryPrefsKeyName() {
    // Build registry key name.
	CString keyName;
	keyName.LoadString(IDS_NETHELP_REGISTRY); // Software\Netscape\Netscape Navigator
    keyName += "Desktop Integration Preferences";

    // Return it.
    return keyName;
}

/* getRegistryPrefsKey
 *
 * Utility to open desktop preferences registry key.
 */
static HKEY getRegistryPrefsKey() {
    // Try to open registry key.
    HKEY key = NULL;
	::RegOpenKey( HKEY_CURRENT_USER, getRegistryPrefsKeyName(), &key );

    return key;
}

/* nsWinDefaultBrowser::GetPreferences
 *
 * Returns Prefs structure filled in with user's preferences.
 */
nsresult nsWinDefaultBrowser::GetPreferences( Prefs &prefs ) {
    nsresult result = NS_ERROR_FAILURE;

    // Try to open registry key.
    HKEY key = getRegistryPrefsKey();

    // See if registry key exists.
    if ( !key ) {
        // Probably not created yet.  Populate key with default values.
        prefs.bHandleFiles                = TRUE;
        prefs.bHandleShortcuts            = TRUE;
        prefs.bIntegrateWithActiveDesktop = TRUE;
        prefs.bHandleHTTP                 = TRUE;
        prefs.bHandleHTTPS                = TRUE;
        prefs.bHandleFTP                  = TRUE;
        prefs.bHandleHTML                 = TRUE;
        prefs.bHandleJPEG                 = TRUE;
        prefs.bHandleGIF                  = TRUE;
        prefs.bHandleJS                   = TRUE;
        prefs.bHandleXBM                  = TRUE;
        prefs.bHandleTXT                  = TRUE;
        prefs.bUseInternetKeywords        = TRUE;
        prefs.bUseNetcenterSearch         = TRUE;
        prefs.bDisableActiveDesktop       = TRUE;
        result = this->SetPreferences( prefs );
    } else {
        // Get each registry value and copy to prefs structure.
        prefs.bHandleFiles                = getPRBoolFromRegistry( key, "bHandleFiles" );
        prefs.bHandleShortcuts            = getPRBoolFromRegistry( key, "bHandleShortcuts" );
        prefs.bIntegrateWithActiveDesktop = getPRBoolFromRegistry( key, "bIntegrateWithActiveDesktop" );
        prefs.bHandleHTTP                 = getPRBoolFromRegistry( key, "bHandleHTTP" );
        prefs.bHandleHTTPS                = getPRBoolFromRegistry( key, "bHandleHTTPS" );
        prefs.bHandleFTP                  = getPRBoolFromRegistry( key, "bHandleFTP" );
        prefs.bHandleHTML                 = getPRBoolFromRegistry( key, "bHandleHTML" );
        prefs.bHandleJPEG                 = getPRBoolFromRegistry( key, "bHandleJPEG" );
        prefs.bHandleGIF                  = getPRBoolFromRegistry( key, "bHandleGIF" );
        prefs.bHandleJS                   = getPRBoolFromRegistry( key, "bHandleJS" );
        prefs.bHandleXBM                  = getPRBoolFromRegistry( key, "bHandleXBM" );
        prefs.bHandleTXT                  = getPRBoolFromRegistry( key, "bHandleTXT" );
        prefs.bUseInternetKeywords        = getPRBoolFromRegistry( key, "bUseInternetKeywords" );
        prefs.bUseNetcenterSearch         = getPRBoolFromRegistry( key, "bUseNetcenterSearch" );
        prefs.bDisableActiveDesktop       = getPRBoolFromRegistry( key, "bDisableActiveDesktop" );
        result = NS_OK;
        ::RegCloseKey( key );
    }

    return result;
}

/* nsWinDefaultBrowser::SetPreferences
 *
 * Sets user preferences from Prefs structure contents.
 */
nsresult nsWinDefaultBrowser::SetPreferences( const Prefs &prefs ) {
    nsresult result = NS_ERROR_FAILURE;

    HKEY key = getRegistryPrefsKey();

    // If key not created yet, do so now.
    if ( !key ) {
        ::RegCreateKey( HKEY_CURRENT_USER, getRegistryPrefsKeyName(), &key );
    }
    
    // If everything's hunky-dory, populate key with values.
    if ( key ) {
        putPRBoolIntoRegistry( key, "bHandleFiles", prefs.bHandleFiles );
        putPRBoolIntoRegistry( key, "bHandleShortcuts", prefs.bHandleShortcuts );
        putPRBoolIntoRegistry( key, "bIntegrateWithActiveDesktop", prefs.bIntegrateWithActiveDesktop );
        putPRBoolIntoRegistry( key, "bHandleHTTP", prefs.bHandleHTTP );
        putPRBoolIntoRegistry( key, "bHandleHTTPS", prefs.bHandleHTTPS );
        putPRBoolIntoRegistry( key, "bHandleFTP", prefs.bHandleFTP );
        putPRBoolIntoRegistry( key, "bHandleHTML", prefs.bHandleHTML );
        putPRBoolIntoRegistry( key, "bHandleJPEG", prefs.bHandleJPEG );
        putPRBoolIntoRegistry( key, "bHandleGIF", prefs.bHandleGIF );
        putPRBoolIntoRegistry( key, "bHandleJS", prefs.bHandleJS );
        putPRBoolIntoRegistry( key, "bHandleXBM", prefs.bHandleXBM );
        putPRBoolIntoRegistry( key, "bHandleTXT", prefs.bHandleTXT );
        putPRBoolIntoRegistry( key, "bUseInternetKeywords", prefs.bUseInternetKeywords );
        putPRBoolIntoRegistry( key, "bUseNetcenterSearch", prefs.bUseNetcenterSearch );
        putPRBoolIntoRegistry( key, "bDisableActiveDesktop", prefs.bDisableActiveDesktop );
        result = NS_OK;
    }

    return result;
}

/* Registry keys
 *
 * These are all the registry keys/values we twiddle with in this code.
 */
struct RegistryEntry {
    HKEY        baseKey;   // e.g., HKEY_CURRENT_USER
    CString     keyName;   // Key name.
    CString     valueName; // Value name (can be empty, which implies NULL).
    CString     setting;   // What we set it to.

    RegistryEntry( HKEY baseKey, const char* keyName, const char* valueName, const char* setting )
        : baseKey( baseKey ), keyName( keyName ), valueName( valueName ), setting( setting ) {
    }

    PRBool   isAlreadySet() const;
    nsresult set();
    nsresult reset();
    CString  currentSetting() const;

    const char* valueNameArg() const;

    CString  savedValueName() const;
};

const char* RegistryEntry::valueNameArg() const {
    const char *result = valueName;
    if ( valueName.IsEmpty() )
        result = NULL;              
    return result;
}

CString RegistryEntry::savedValueName() const {
    CString result = valueName;
    if ( result.IsEmpty() ) {
        result = "pre-mozilla";
    } else {
        result += " pre-mozilla";
    }
    return result;
}

static CString thisApplication();

struct ProtocolRegistryEntry : public RegistryEntry {
    CString protocol;
    ProtocolRegistryEntry( const char* protocol, const char* setting )
        : RegistryEntry( HKEY_LOCAL_MACHINE, "", "", setting ),
          protocol( protocol ) {
        keyName = "Software\\Classes\\";
        keyName += protocol;
        keyName += "\\shell\\open\\command";
    }
    nsresult set();
    nsresult reset();
    CString  currentSetting() {
        // Special case NetscapeMarkup.
        if ( protocol == "NetscapeMarkup" ) {
            return thisApplication();
        } else {
            return RegistryEntry::currentSetting();
        }
    }
};

static RegistryEntry
    findOnTheInternet( HKEY_CURRENT_USER,
                       "Software\\Microsoft\\Internet Explorer\\Main",
                       "Search Page",
                       "http://home.netscape.com/home/winsearch.html" ),

    internetKeywords1( HKEY_CURRENT_USER,
                       "Software\\Microsoft\\Internet Explorer\\SearchURL",
                       "",
                       "http://keyword.netscape.com/keyword/%s" ),

    internetKeywords2( HKEY_LOCAL_MACHINE,
                       "Software\\Microsoft\\Windows\\CurrentVersion\\URL\\DefaultPrefix",
                       "",
                       "http://keyword.netscape.com/keyword/" );

// Special case for this one.
static struct ActiveDesktopRegistryEntry : public RegistryEntry {
    ActiveDesktopRegistryEntry()
        : RegistryEntry( HKEY_CURRENT_USER,
                         "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer",
                         "ShellState",
                         "" ) {
    }
    PRBool   isAlreadySet() const;
    nsresult set();
    nsresult reset();
} activeDesktop;
    
/* Registryentry::isAlreadySet
 *
 * Tests whether registry entry already has desired setting.
 */
PRBool RegistryEntry::isAlreadySet() const {
    PRBool result = FALSE;

    CString current = currentSetting();

    result = ( current == setting );

    return result;
}

PRBool ActiveDesktopRegistryEntry::isAlreadySet() const {
    PRBool result = FALSE;
    HKEY   key;
    LONG   rc = ::RegOpenKey( baseKey, keyName, &key );
    if ( rc == ERROR_SUCCESS ) {
        char buffer[4096];
        DWORD len = sizeof buffer;
        rc = ::RegQueryValueEx( key, valueNameArg(), NULL, NULL, (LPBYTE)buffer, &len );
        if ( rc == ERROR_SUCCESS ) {
            if ( !( buffer[4] & 0x40 ) ) {
                  result = TRUE;
            }
        }
        ::RegCloseKey( key );
    }

    return result;
}

/* Registryentry::set
 *
 * Gives registry entry the desired setting.
 */
nsresult RegistryEntry::set() {
    nsresult result = NS_ERROR_FAILURE;
    HKEY   key;
    LONG   rc = ::RegOpenKey( baseKey, keyName, &key );
    if ( rc == ERROR_SUCCESS ) {
        char buffer[4096];
        DWORD len = sizeof buffer;
        rc = ::RegQueryValueEx( key, valueNameArg(), NULL, NULL, (LPBYTE)buffer, &len );
        if ( rc == ERROR_SUCCESS ) {
            if ( XP_STRCMP( setting, buffer ) != 0 ) {
                rc = ::RegSetValueEx( key, valueNameArg(), NULL, REG_SZ, (LPBYTE)(const char*)setting, XP_STRLEN(setting) );
                if ( rc == ERROR_SUCCESS ) {
                    // Save old value.
                    rc = ::RegSetValueEx( key, savedValueName(), NULL, REG_SZ, (LPBYTE)buffer, len );
                    result = NS_OK;
                }
            } else {
                // Already has desired setting.
                result = NS_OK;
            }
        }
        ::RegCloseKey( key );
    }

    return result;
}

nsresult ProtocolRegistryEntry::set() {
    // First, set main keyName to proper setting.
    nsresult result = RegistryEntry::set();

    // If that worked, change DDE stuff.
    if ( result == NS_OK ) {
        CString ddeKey = "Software\\Classes\\";
        ddeKey += protocol;
        ddeKey += "\\shell\\open\\ddeexec";
        RegistryEntry entry( HKEY_LOCAL_MACHINE, ddeKey, NULL, "\"%1\"" );
        result = entry.set();
        if ( result == NS_OK ) {
            entry.keyName = ddeKey + "\\Application";
            entry.setting = "NSShell";
            result = entry.set();
            if ( result == NS_OK ) {
                entry.keyName = ddeKey + "\\Topic";
                entry.setting = "WWW_OpenURL";
                result = entry.set();
            }
        }
        // DDE isn't crucial, so ignore errors.
        result = NS_OK;
    }

    return result;
}
                 
nsresult ActiveDesktopRegistryEntry::set() {
    nsresult result = NS_ERROR_FAILURE;
    HKEY   key;
    LONG   rc = ::RegOpenKey( baseKey, keyName, &key );
    if ( rc == ERROR_SUCCESS ) {
        char buffer[4096];
        DWORD len = sizeof buffer;
        rc = ::RegQueryValueEx( key, valueNameArg(), NULL, NULL, (LPBYTE)buffer, &len );
        if ( rc == ERROR_SUCCESS ) {
            if ( buffer[4] & 0x40 ) {
                buffer[4] &= ~0x40; // Turn off Active Desktop bit.
                rc = ::RegSetValueEx( key, valueNameArg(), NULL, REG_BINARY, (LPBYTE)buffer, len );
                if ( rc == ERROR_SUCCESS ) {
                    // Note fact that on reset() we must turn it back on.
                    rc = ::RegSetValueEx( key, savedValueName(), NULL, REG_BINARY, (LPBYTE)"\x40", 1 );
                    result = NS_OK;
                }
            } else {
                // Already has desired setting.
                result = NS_OK;
            }
        }
        ::RegCloseKey( key );
    }

    return result;
}

/* Registryentry::reset
 *
 * Restores registry entry to its value prior to us giving it our desired setting.
 */
nsresult RegistryEntry::reset() {
    nsresult result = NS_ERROR_FAILURE;
    HKEY   key;
    LONG   rc = ::RegOpenKey( baseKey, keyName, &key );
    if ( rc == ERROR_SUCCESS ) {
        char buffer[4096];
        DWORD len = sizeof buffer;
        rc = ::RegQueryValueEx( key, valueNameArg(), NULL, NULL, (LPBYTE)buffer, &len );
        if ( rc == ERROR_SUCCESS ) {
            if ( XP_STRCMP( setting, buffer ) == 0 ) {
                // Get previous value.
                len = sizeof buffer;
                rc = ::RegQueryValueEx( key, savedValueName(), NULL, NULL, (LPBYTE)buffer, &len );
                if ( rc == ERROR_SUCCESS ) {
                    rc = ::RegSetValueEx( key, valueNameArg(), NULL, REG_SZ, (LPBYTE)buffer, len );
                    result = NS_OK;
                } else {
                    // No saved value?  Reset to null and hope for the best.
                    rc = ::RegSetValueEx( key, valueNameArg(), NULL, REG_SZ, (LPBYTE)"", 0 );
                    result = NS_OK;
                }
            } else {
                // Wasn't set by us.
                result = NS_OK;
            }
            // Clean up "pre-mozilla" value.
            rc = ::RegDeleteValue( key, savedValueName() );
        }
        ::RegCloseKey( key );
    }

    return result;
}

nsresult ProtocolRegistryEntry::reset() {
    // First, reset main keyName.
    nsresult result = RegistryEntry::reset();

    // If that worked, change DDE stuff.
    if ( result == NS_OK ) {
        CString ddeKey = "Software\\Classes\\";
        ddeKey += protocol;
        ddeKey += "\\shell\\open\\ddeexec";
        RegistryEntry entry( HKEY_LOCAL_MACHINE, ddeKey, NULL, "%1" );
        result = entry.reset();
        if ( result == NS_OK ) {
            entry.keyName = ddeKey + "\\Application";
            entry.setting = "NSShell";
            result = entry.reset();
            if ( result == NS_OK ) {
                entry.keyName = ddeKey + "\\Topic";
                entry.setting = "WWW_OpenURL";
                result = entry.reset();
            }
        }
    }

    return result;
}

nsresult ActiveDesktopRegistryEntry::reset() {
    nsresult result = NS_ERROR_FAILURE;
    HKEY   key;
    LONG   rc = ::RegOpenKey( baseKey, keyName, &key );
    if ( rc == ERROR_SUCCESS ) {
        char buffer[4096];
        DWORD len = sizeof buffer;
        rc = ::RegQueryValueEx( key, valueNameArg(), NULL, NULL, (LPBYTE)buffer, &len );
        if ( rc == ERROR_SUCCESS ) {
            if ( !( buffer[4] & 0x40 ) ) {
                // It's off now, see if we turned it off.
                char preMozilla;
                DWORD len2 = sizeof preMozilla;
                rc = ::RegQueryValueEx( key, savedValueName(), NULL, NULL, (LPBYTE)&preMozilla, &len2 );
                if ( rc == ERROR_SUCCESS ) {
                    // Previously was on, so turn it back on now.
                    buffer[4] |= 0x40;
                    rc = ::RegSetValueEx( key, valueNameArg(), NULL, REG_BINARY, (LPBYTE)buffer, len );
                    result = NS_OK;
                } else {
                    // No saved value means we didn't toggle it; so leave it off.
                    result = NS_OK;
                }
            } else {
                // Couldn't have been turned off by us; reset is nop.
                result = NS_OK;
            }
            // Clear saved value.
            rc = ::RegDeleteValue( key, savedValueName() );
        }
        ::RegCloseKey( key );
    }

    return result;
}

/* RegistryEntry::currentSetting
 *
 * Return current setting for this registry entry.
 */
CString RegistryEntry::currentSetting() const {
    CString result;

    HKEY   key;
    LONG   rc = ::RegOpenKey( baseKey, keyName, &key );
    if ( rc == ERROR_SUCCESS ) {
        char buffer[4096];
        DWORD len = sizeof buffer;
        rc = ::RegQueryValueEx( key, valueNameArg(), NULL, NULL, (LPBYTE)buffer, &len );
        if ( rc == ERROR_SUCCESS ) {
            result = buffer;
        }
        ::RegCloseKey( key );
    }

    return result;
}
 

/* handlerForProtocol
 *
 * Returns name of application handling a given Internet shortcut protocol.
 */
static CString handlerForProtocol( const char *protocol ) {
    ProtocolRegistryEntry entry( protocol, NULL );

    CString result = entry.currentSetting();

    return result;
}

/* handlerForExtension
 *
 * Returns name of application handling a given file extension.
 */
static CString handlerForExtension( const char *ext ) {
    CString result;

    // Get file type for given extension.
    CString extKeyName = "Software\\Classes\\";
    extKeyName += ext;
    RegistryEntry entry( HKEY_LOCAL_MACHINE, extKeyName, NULL, NULL );
    CString fileType = entry.currentSetting();

    if ( !fileType.IsEmpty() ) {
        // Now get application for that type.
        result = handlerForProtocol( fileType );
    }

    return result;
}

/* areInternetKeywordsActivated/isNetcenterSearchActivated
 *
 * Returns whether Mozilla has set the registry its way for a given feature.
 */
static PRBool areInternetKeywordsActivated() {
    PRBool result = internetKeywords1.isAlreadySet() && internetKeywords2.isAlreadySet();

    return result;
}

static PRBool isNetcenterSearchActivated() {
    PRBool result = findOnTheInternet.isAlreadySet();

    return result;
}

/* activateInternetKeywords/activateNetcenterSearch
 *
 * Set registry so these things are in effect.
 */
static nsresult activateInternetKeywords() {
    nsresult result = internetKeywords1.set();

    if ( result == NS_OK ) {
        result = internetKeywords2.set();
    }

    ASSERT( result == NS_OK );

    return result;
}

static nsresult activateNetcenterSearch() {
    nsresult result = findOnTheInternet.set();

    return result;
}

/* deactivateInternetKeywords/deactivateNetcenterSearch
 *
 * Set registry so these things are in effect.
 */
static nsresult deactivateInternetKeywords() {
    nsresult result = internetKeywords1.reset();

    if ( result == NS_OK ) {
        result =  internetKeywords2.reset();
    }

    ASSERT( result == NS_OK );

    return result;
}

static nsresult deactivateNetcenterSearch() {
    nsresult result = findOnTheInternet.reset();

    ASSERT( result == NS_OK );

    return result;
}

/* isActiveDesktopDisabled
 *
 * Returns whether the Active Desktop has been disabled.
 */

static PRBool isActiveDesktopDisabled() {
    PRBool result = activeDesktop.isAlreadySet();

    return result;
}

/* resetActiveDesktop
 *
 * Set registry entries to reset Active Desktop.
 */
static nsresult resetActiveDesktop() {
    nsresult result = activeDesktop.reset();

    ASSERT( result == NS_OK );

    return result;
}

/* disableActiveDesktop
 *
 * Set registry entries to disable Active Desktop.
 */
static nsresult disableActiveDesktop() {
    nsresult result = activeDesktop.set();

    ASSERT( result == NS_OK );

    return result;
}

static const char shortcutSuffix[] = " -h \"%1\"";

/* thisApplication
 *
 * Returns the (fully-qualified) name of this executable (plus the suffix).
 */
static CString thisApplication() {
    static CString result;

    if ( result.IsEmpty() ) {
        char buffer[MAX_PATH] = { 0 };
    	DWORD len = ::GetModuleFileName( theApp.m_hInstance, buffer, sizeof buffer );
        len = ::GetShortPathName( buffer, buffer, sizeof buffer );
    
        result = buffer;
        result += shortcutSuffix;
        result.MakeUpper();
    }

    return result;
}

/* File extensions for each file type. */
static const char * const HTMLExtensions[] = { ".htm", ".html", 0 };
static const char * const JPEGExtensions[] = { ".jpg", ".jpeg", ".jpe", ".jfif", ".pjpeg", ".pjp", 0 };
static const char * const GIFExtensions[]  = { ".gif", 0 };
static const char * const JSExtensions[]   = { ".js", 0 };
static const char * const XBMExtensions[]  = { ".xbm", 0 };
static const char * const TXTExtensions[]  = { ".txt", 0 };

/* isHandlingExtensions
 *
 * Return whether this application is handling each of a set of extensions.
 */
PRBool isHandlingExtensions( const char * const ext[] ) {
    PRBool result = TRUE;
    for ( int i = 0; result && ext[i]; i++ ) {
        CString handler = handlerForExtension( ext[i] );
        result = ( handler.IsEmpty() || (handler.MakeUpper(), handler) == thisApplication() );
    }
    return result;
}

/* nsWinDefaultBrowser::IsHandling
 *
 * Returns whether Mozilla is handling a given thing.
 */
PRBool nsWinDefaultBrowser::IsHandling( Thing thing ) {
    PRBool result = TRUE;

    switch ( thing ) {
        case kHTTP:
            result = ( handlerForProtocol("http") == thisApplication() );
            break;

        case kHTTPS:
            result = ( handlerForProtocol("https") == thisApplication() );
            break;

        case kFTP:
            result = ( handlerForProtocol("ftp") == thisApplication() );
            break;

        case kHTML:
            result = isHandlingExtensions( HTMLExtensions );
            break;

        case kJPEG:
            result = isHandlingExtensions( JPEGExtensions );
            break;
    
        case kGIF:
            result = isHandlingExtensions( GIFExtensions );
            break;
    
        case kJS:
            result = isHandlingExtensions( JSExtensions );
            break;
    
        case kXBM:
            result = isHandlingExtensions( XBMExtensions );
            break;
    
        case kTXT:
            result = isHandlingExtensions( TXTExtensions );
            break;

        case kInternetKeywords:
            result = areInternetKeywordsActivated();
            break;

        case kNetcenterSearch:
            result = isNetcenterSearchActivated();
            break;

        case kActiveDesktop:
            result = isActiveDesktopDisabled();
            break;

        default:
            // we certainly aren't handling thing we don't even know about!
            result = FALSE;
            break;
    }

    return result;
}

/* startHandlingProtocol
 *
 * Set registry so that this application handles argument protocol.
 */
static nsresult startHandlingProtocol( const char *protocol ) {
    nsresult result = NS_ERROR_FAILURE;

    ProtocolRegistryEntry entry( protocol, thisApplication() );

    result = entry.set();

    ASSERT( result == NS_OK );

    return result;
}

/* stopHandlingProtocol
 *
 * Set registry so that this application doesn't handle the argument protocol.
 */
static nsresult stopHandlingProtocol( const char *protocol ) {
    nsresult result = NS_ERROR_FAILURE;

    ProtocolRegistryEntry entry( protocol, thisApplication() );

    result = entry.reset();

    ASSERT( result == NS_OK );

    return result;
}

/* setupNetscapeMarkup
 *
 * Set up registry entries for "NetscapeMarkup" file type (our replacement for "htmlfile").
 */
static void setupNetscapeMarkup() {
    //TBD
}

/* startHandlingExtension
 *
 * Set up this application to handle the given file extension.
 */
static nsresult startHandlingExtension( const char *ext ) {
    nsresult result = NS_ERROR_FAILURE;

    // Handle .htm/.html differently.
    CString strExt = ext;
    CString keyName = "Software\\Classes\\";
    keyName += ext;
    if ( strExt == ".htm" || strExt == ".html" ) {
        // Give these extensions type "NetscapeMarkup".
        RegistryEntry entry( HKEY_LOCAL_MACHINE,
                             keyName,
                             "",
                             "NetscapeMarkup" );

        setupNetscapeMarkup();

        result = entry.set();

        ASSERT( result == NS_OK );
    } else {
        // Get file type.
        RegistryEntry extEntry( HKEY_LOCAL_MACHINE,
                                keyName,
                                "",
                                "" );
        CString fileType = extEntry.currentSetting();
        if ( !fileType.IsEmpty() ) {
            ProtocolRegistryEntry entry( fileType, thisApplication() );

            result = entry.set();

            ASSERT( result == NS_OK );
        } else {
            // Extension missing from registry; ignore it.
            result = NS_OK;
        }
    }

    return result;
}

/* stopHandlingExtension
 *
 * Restore registry so this application isn't handling the give extension.
 */
static nsresult stopHandlingExtension( const char *ext ) {
    nsresult result = NS_ERROR_FAILURE;

    // Handle .htm/.html differently.
    CString strExt = ext;
    CString keyName = "Software\\Classes\\";
    keyName += ext;
    if ( strExt == ".htm" || strExt == ".html" ) {
        // Reset type of .htm/.html files.
        RegistryEntry entry( HKEY_LOCAL_MACHINE,
                             keyName,
                             "",
                             "NetscapeMarkup" );

        result = entry.reset();

        ASSERT( result == NS_OK );
    } else {
        // Get file type.
        RegistryEntry extEntry( HKEY_LOCAL_MACHINE,
                                keyName,
                                "",
                                "" );
        CString fileType = extEntry.currentSetting();
        if ( !fileType.IsEmpty() ) {
            ProtocolRegistryEntry entry( fileType, thisApplication() );

            result = entry.reset();

            ASSERT( result == NS_OK );
        } else {
            // Extension missing from registry; ignore it.
            result = NS_OK;
        }
    }

    return result;
}

/* startHandlingExtensions
 *
 * Set up this application to handle the argument set of file extensions.
 */
static nsresult startHandlingExtensions( const char *const ext[] ) {
    nsresult result = NS_OK;

    for ( int i = 0; ext[i] && result == NS_OK; i++ ) {
        result = startHandlingExtension( ext[i] );
    }

    return result;
}

/* stopHandlingExtensions
 *
 * Restore registry for argument set of file extensions.
 */
static nsresult stopHandlingExtensions( const char *const ext[] ) {
    nsresult result = NS_OK;

    for ( int i = 0; ext[i] && result == NS_OK; i++ ) {
        result = stopHandlingExtension( ext[i] );
    }

    return result;
}

/* nsWinDefaultBrowser::StartHandling
 *
 * Does necessary stuff to have Mozilla take over handling of a given thing.
 */
nsresult nsWinDefaultBrowser::StartHandling( Thing extOrProtocol ) {
    nsresult result = NS_ERROR_FAILURE;

    switch ( extOrProtocol ) {
        case kHTTP:
            result = startHandlingProtocol( "http" );
            break;
    
        case kHTTPS:
            result = startHandlingProtocol( "https" );
            break;
    
        case kFTP:
            result = startHandlingProtocol( "ftp" );
            break;

        case kHTML:
            result = startHandlingExtensions( HTMLExtensions );
            break;

        case kJPEG:
            result = startHandlingExtensions( JPEGExtensions );
            break;
    
        case kGIF:
            result = startHandlingExtensions( GIFExtensions );
            break;
    
        case kJS:
            result = startHandlingExtensions( JSExtensions );
            break;
    
        case kXBM:
            result = startHandlingExtensions( XBMExtensions );
            break;
    
        case kTXT:
            result = startHandlingExtensions( TXTExtensions );
            break;

        case kInternetKeywords:
            result = activateInternetKeywords();
            break;

        case kNetcenterSearch:
            result = activateNetcenterSearch();
            break;

        case kActiveDesktop:
            result = disableActiveDesktop();
            break;

        default:
            break;
    }

    return result;
}

/* nsWinDefaultBrowser::StopHandling
 *
 * Vice versa
 */
nsresult nsWinDefaultBrowser::StopHandling( Thing extOrProtocol ) {
    nsresult result = NS_ERROR_FAILURE;

    switch ( extOrProtocol ) {
        case kHTTP:
            result = stopHandlingProtocol( "http" );
            break;
    
        case kHTTPS:
            result = stopHandlingProtocol( "https" );
            break;
    
        case kFTP:
            result = stopHandlingProtocol( "ftp" );
            break;

        case kHTML:
            result = stopHandlingExtensions( HTMLExtensions );
            break;

        case kJPEG:
            result = stopHandlingExtensions( JPEGExtensions );
            break;
    
        case kGIF:
            result = stopHandlingExtensions( GIFExtensions );
            break;
    
        case kJS:
            result = stopHandlingExtensions( JSExtensions );
            break;
    
        case kXBM:
            result = stopHandlingExtensions( XBMExtensions );
            break;
    
        case kTXT:
            result = stopHandlingExtensions( TXTExtensions );
            break;

        case kInternetKeywords:
            result = deactivateInternetKeywords();
            break;

        case kNetcenterSearch:
            result = deactivateNetcenterSearch();
            break;

        case kActiveDesktop:
            result = resetActiveDesktop();
            break;

        default:
            break;
    }

    return result;
}

/* nsWinDefaultBrowser::HandlePerPreferences
 *
 * Get preferences and start handling everything selected.
 */
nsresult nsWinDefaultBrowser::HandlePerPreferences() {
    nsresult result = NS_OK;

    Prefs prefs;
    result = GetPreferences( prefs );

    ASSERT( result == NS_OK );

    // Don't apply bogus preferences!
    if ( result == NS_OK ) {
        if ( prefs.bHandleFiles && prefs.bHandleHTML ) {
            result = StartHandling( kHTML );
        } else {
            result = StopHandling( kHTML );
        }
        ASSERT( result == NS_OK );
        if ( prefs.bHandleFiles && prefs.bHandleJPEG ) {
            result = StartHandling( kJPEG );
        } else {
            result = StopHandling( kJPEG );
        }
        ASSERT( result == NS_OK );
        if ( prefs.bHandleFiles && prefs.bHandleGIF ) {
            result = StartHandling( kGIF );
        } else {
            result = StopHandling( kGIF );
        }
        ASSERT( result == NS_OK );
        if ( prefs.bHandleFiles && prefs.bHandleJS ) {
            result = StartHandling( kJS );
        } else {
            result = StopHandling( kJS );
        }
        ASSERT( result == NS_OK );
        if ( prefs.bHandleFiles && prefs.bHandleXBM ) {
            result = StartHandling( kXBM );
        } else {
            result = StopHandling( kXBM );
        }
        ASSERT( result == NS_OK );
        if ( prefs.bHandleFiles && prefs.bHandleTXT ) {
            result = StartHandling( kTXT );
        } else {
            result = StopHandling( kTXT );
        }
        ASSERT( result == NS_OK );
        if ( prefs.bHandleShortcuts && prefs.bHandleHTTP ) {
            result = StartHandling( kHTTP );
        } else {
            result = StopHandling( kHTTP );
        }
        ASSERT( result == NS_OK );
        if ( prefs.bHandleShortcuts && prefs.bHandleHTTPS ) {
            result = StartHandling( kHTTPS );
        } else {
            result = StopHandling( kHTTPS );
        }
        ASSERT( result == NS_OK );
        if ( prefs.bHandleShortcuts && prefs.bHandleFTP ) {
            result = StartHandling( kFTP );
        } else {
            result = StopHandling( kFTP );
        }
        ASSERT( result == NS_OK );
        if ( prefs.bIntegrateWithActiveDesktop && prefs.bUseInternetKeywords ) {
            result = StartHandling( kInternetKeywords );
        } else {
            result = StopHandling( kInternetKeywords );
        }
        ASSERT( result == NS_OK );
        if ( prefs.bIntegrateWithActiveDesktop && prefs.bUseNetcenterSearch ) {
            result = StartHandling( kNetcenterSearch );
        } else {
            result = StopHandling( kNetcenterSearch );
        }
        ASSERT( result == NS_OK );
        if ( prefs.bIntegrateWithActiveDesktop && prefs.bDisableActiveDesktop ) {
            result = StartHandling( kActiveDesktop );
        } else {
            result = StopHandling( kActiveDesktop );
        }
        ASSERT( result == NS_OK );
    }

    return result;
}

