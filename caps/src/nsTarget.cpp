/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#include "prlog.h"
#include "nsTarget.h"
#include "nsPrivilegeManager.h"
#include "nsUserTarget.h"
#include "nsUserDialogHelper.h"
#include "xp.h"
#include "xpgetstr.h"
#include "plhash.h"

/* XXXXXXXX Begin oF HACK */

/* XXX: This array must be kept in sync with the allxpstr.h. 
 * The following is hack until we have a design on removal of allxpstr.h
 */
char *capsTargetStrings[] = {
  "low",

  "medium",

  "high",

  "#aaffaa",

  "#ffffaa",

  "#ffaaaa",

  "http://home.netscape.com/eng/mozilla/4.0/handbook/",

  "Reading files stored in your computer",

  "Reading any files stored on hard disks or other storage media connected to your computer.",

  "#FileRead",

  "Modifying files stored in your computer",

  "Modifying any files stored on hard disks or other storage media connected to you computer.",

  "#FileWrite",

  "Deleting files stored in your computer",

  "Deletion of any files stored on hard disks or other storage media connected to your computer.",

  "#FileDelete"

  "Access to impersonate as another application",

  "Access to impersonate as another application",

  "#Impersonator",

  "Access to browser data",

  "Access to browser data that may be considered private, such as a list of web sites visited or the contents of web page forms you may have filled in.",

  "#BrowserRead",

  "Modifying the browser",

  "Modifying the browser in a potentially dangerous way, such as creating windows that may look like they belong to another program or positioning windows anywhere on the screen.",

  "#BrowserWrite",

  "Reading or modifying browser data",

  "Reading or modifying browser data that may be considered private, such as a list of web sites visited or the contents of web forms you may have filled in. Modifications may also include creating windows that look like they belong to another program or positioning windowsanywhere on the screen.",

  "#BrowserAccess",

  "Reading preferences settings",

  "Access to read the current settings of your preferences.",

  "#PrefsRead",

  "Modifying preferences settings",

  "Modifying the current settings of your preferences.",

  "#PrefsWrite",

  "Sending email messages on your behalf",

  "Sending email messages on your behalf",

  "#SendMail",

  "Access to the vendor's portion of your computer's registry of installed software",

  "Most computers store information about installed software, such as version numbers, in a registry file. When you install new software, the installation program sometimes needs to read or change entries in the portion of the"
  "registry that describes the software vendor's products. You should grant "
"this form of access only if you are installing new software from a reliable "
"vendor. The entity that signs the software can access only that entity's "
"portion of the registry.",

  "#RegPrivate",

  "Access to shared information in the computer's registry of installed software",

"Most computers store information about installed software, such as version "
"numbers, in a registry file. This file also includes information shared by "
"all programs installed on your computer, including information about the user "
"or the system. Programs that have access to shared registry information can "
"obtain information about other programs that have the same access. This allows "
"programs that work closely together to get information about each other. "
"You should grant this form of access only if you know that the program "
"requesting it is designed to work with other programs on your hard disk.",


  "#RegStandard",

  "Access to any part of your computer's registry of installed software",

  "Most computers store information about installed software, such as version "
"numbers, in a registry file. System administrators sometimes need to change "
"entries in the registry for software from a variety of vendors. You should "
"grant this form of access only if you are running software provided by your "
"system administrator.",


  "#RegAdmin",

  "Access required to setup and configure your browser",

  "Access to, and modification of, browser data, preferences, files, networking "
"and modem configuration. This access is commonly granted to the main setup "
"program for your browser.",

  "#AccountSetup",

  "Access to the site archive file",

  "Access required to add, modify, or delete site archive files and make "
"arbitrary network connections in the process. This form of access is required "
"only by netcasting applications such as Netscape Netcaster, which request it "
"in combination with several other kinds of access. Applications should not "
"normally request this access by itself, and you should not normally grant it.",


  "#SiteArchive",

  "Displaying text or graphics anywhere on the screen",

  "Displaying HTML text or graphics on any part of the screen, without window "
"borders, toolbars, or menus. Typically granted to invoke canvas mode, screen "
"savers, and so on.",

  "#CanvasAccess",

  "Reading, modification, or deletion of any of your files",

  "This form of access is typically required by a program such as a word "
"processor or a debugger that needs to create, read, modify, or delete files "
"on hard disks or other storage media connected to your computer.",

  "#FileAccess",

  "Uninstall software",

  "Access required for automatic removal of previously installed software.",

  "#Uninstall",

  "Installing and running software on your computer",

  "Installing software on your computer's hard disk. An installation "
"program can also execute or delete any software on your computer. "
"You should not grant this form of access unless you are installing or "
"updating software from a reliable source.",

  "#SoftwareInstall",

  "Installing and running software without warning you",

  "Installing software on your computer's main hard disk without giving you any "
"warning, potentially deleting other files on the hard disk. Any software on the "
"hard disk may be executed in the process. This is an extremely dangerous form "
"of access. It should be granted by system administrators only.",

  "#SilentInstall",

  "Complete access to your computer for java programs",

  "Complete access required by java programs to your computer, such as Java "
"Virtual machine reading, writing, deleting information from your disk, "
"and to send receive and send information to any computer on the Internet.",

  "#AllJavaPermission",

  "Access to all Privileged JavaScript operations",

  "Access to all Privileged JavaScript operations.",

  "#AllJavaScriptPermission",

};

typedef enum CAPS_TARGET {
  CAPS_TARGET_RISK_STR_LOW,
  CAPS_TARGET_RISK_STR_MEDIUM,
  CAPS_TARGET_RISK_STR_HIGH,
  CAPS_TARGET_RISK_COLOR_LOW,
  CAPS_TARGET_RISK_COLOR_MEDIUM,
  CAPS_TARGET_RISK_COLOR_HIGH,
  CAPS_TARGET_HELP_URL,
  CAPS_TARGET_DESC_FILE_READ,
  CAPS_TARGET_DETAIL_DESC_FILE_READ,
  CAPS_TARGET_URL_FILE_READ,
  CAPS_TARGET_DESC_FILE_WRITE,
  CAPS_TARGET_DETAIL_DESC_FILE_WRITE,
  CAPS_TARGET_URL_FILE_WRITE,
  CAPS_TARGET_DESC_FILE_DELETE,
  CAPS_TARGET_DETAIL_DESC_FILE_DELETE,
  CAPS_TARGET_URL_FILE_DELETE,
  CAPS_TARGET_DESC_IMPERSONATOR,
  CAPS_TARGET_DETAIL_DESC_IMPERSONATOR,
  CAPS_TARGET_URL_IMPERSONATOR,
  CAPS_TARGET_DESC_BROWSER_READ,
  CAPS_TARGET_DETAIL_DESC_BROWSER_READ,
  CAPS_TARGET_URL_BROWSER_READ,
  CAPS_TARGET_DESC_BROWSER_WRITE,
  CAPS_TARGET_DETAIL_DESC_BROWSER_WRITE,
  CAPS_TARGET_URL_BROWSER_WRITE,
  CAPS_TARGET_DESC_BROWSER_ACCESS,
  CAPS_TARGET_DETAIL_DESC_BROWSER_ACCESS,
  CAPS_TARGET_URL_BROWSER_ACCESS,
  CAPS_TARGET_DESC_PREFS_READ,
  CAPS_TARGET_DETAIL_DESC_PREFS_READ,
  CAPS_TARGET_URL_PREFS_READ,
  CAPS_TARGET_DESC_PREFS_WRITE,
  CAPS_TARGET_DETAIL_DESC_PREFS_WRITE,
  CAPS_TARGET_URL_PREFS_WRITE,
  CAPS_TARGET_DESC_SEND_MAIL,
  CAPS_TARGET_DETAIL_DESC_SEND_MAIL,
  CAPS_TARGET_URL_SEND_MAIL,
  CAPS_TARGET_DESC_REG_PRIVATE,
  CAPS_TARGET_DETAIL_DESC_REG_PRIVATE,
//  CAPS_TARGET_DETAIL_DESC_REG_PRIVATE_1,
  CAPS_TARGET_URL_REG_PRIVATE,
  CAPS_TARGET_DESC_REG_STANDARD,
  CAPS_TARGET_DETAIL_DESC_REG_STANDARD,
//  CAPS_TARGET_DETAIL_DESC_REG_STANDARD_1,
//  CAPS_TARGET_DETAIL_DESC_REG_STANDARD_2,
  CAPS_TARGET_URL_REG_STANDARD,
  CAPS_TARGET_DESC_REG_ADMIN,
  CAPS_TARGET_DETAIL_DESC_REG_ADMIN,
//  CAPS_TARGET_DETAIL_DESC_REG_ADMIN_1,
  CAPS_TARGET_URL_REG_ADMIN,
  CAPS_TARGET_DESC_ACCOUNT_SETUP,
  CAPS_TARGET_DETAIL_DESC_ACCOUNT_SETUP,
  CAPS_TARGET_URL_ACCOUNT_SETUP,
  CAPS_TARGET_DESC_SAR,
  CAPS_TARGET_DETAIL_DESC_SAR,
//  CAPS_TARGET_DETAIL_DESC_SAR_1,
  CAPS_TARGET_URL_SAR,
  CAPS_TARGET_DESC_CANVAS_ACCESS,
  CAPS_TARGET_DETAIL_DESC_CANVAS_ACCESS,
  CAPS_TARGET_URL_CANVAS_ACCESS,
  CAPS_TARGET_DESC_FILE_ACCESS,
  CAPS_TARGET_DETAIL_DESC_FILE_ACCESS,
  CAPS_TARGET_URL_FILE_ACCESS,
  CAPS_TARGET_DESC_UNINSTALL,
  CAPS_TARGET_DETAIL_DESC_UNINSTALL,
  CAPS_TARGET_URL_UNINSTALL,
  CAPS_TARGET_DESC_SOFTWAREINSTALL,
  CAPS_TARGET_DETAIL_DESC_SOFTWAREINSTALL,
  CAPS_TARGET_URL_SOFTWAREINSTALL,
  CAPS_TARGET_DESC_SILENTINSTALL,
  CAPS_TARGET_DETAIL_DESC_SILENTINSTALL,
  CAPS_TARGET_URL_SILENTINSTALL,
  CAPS_TARGET_DESC_ALL_JAVA_PERMISSION,
  CAPS_TARGET_DETAIL_DESC_ALL_JAVA_PERMISSION,
  CAPS_TARGET_URL_ALL_JAVA_PERMISSION,
  CAPS_TARGET_DESC_ALL_JS_PERMISSION,
  CAPS_TARGET_DETAIL_DESC_ALL_JS_PERMISSION,
  CAPS_TARGET_URL_ALL_JS_PERMISSION,
  CAPS_TARGET_MAXIMUM
}  CAPS_TARGET;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

char* capsGetString(int id) 
{
  PR_ASSERT(id <= CAPS_TARGET_MAXIMUM);
  return capsTargetStrings[id];
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#ifdef XXX
extern int  CAPS_TARGET_RISK_STR_LOW;
extern int  CAPS_TARGET_RISK_STR_MEDIUM;
extern int  CAPS_TARGET_RISK_STR_HIGH;
extern int  CAPS_TARGET_RISK_COLOR_LOW;
extern int  CAPS_TARGET_RISK_COLOR_MEDIUM;
extern int  CAPS_TARGET_RISK_COLOR_HIGH;
extern int  CAPS_TARGET_HELP_URL;
extern int  CAPS_TARGET_DESC_FILE_READ;
extern int  CAPS_TARGET_DETAIL_DESC_FILE_READ;
extern int  CAPS_TARGET_URL_FILE_READ;
extern int  CAPS_TARGET_DESC_FILE_WRITE;
extern int  CAPS_TARGET_DETAIL_DESC_FILE_WRITE;
extern int  CAPS_TARGET_URL_FILE_WRITE;
extern int  CAPS_TARGET_DESC_FILE_DELETE;
extern int  CAPS_TARGET_DETAIL_DESC_FILE_DELETE;
extern int  CAPS_TARGET_URL_FILE_DELETE;
extern int  CAPS_TARGET_DESC_IMPERSONATOR;
extern int  CAPS_TARGET_DETAIL_DESC_IMPERSONATOR;
extern int  CAPS_TARGET_URL_IMPERSONATOR;
extern int  CAPS_TARGET_DESC_BROWSER_READ;
extern int  CAPS_TARGET_DETAIL_DESC_BROWSER_READ;
extern int  CAPS_TARGET_URL_BROWSER_READ;
extern int  CAPS_TARGET_DESC_BROWSER_WRITE;
extern int  CAPS_TARGET_DETAIL_DESC_BROWSER_WRITE;
extern int  CAPS_TARGET_URL_BROWSER_WRITE;
extern int  CAPS_TARGET_DESC_BROWSER_ACCESS;
extern int  CAPS_TARGET_DETAIL_DESC_BROWSER_ACCESS;
extern int  CAPS_TARGET_URL_BROWSER_ACCESS;
extern int  CAPS_TARGET_DESC_PREFS_READ;
extern int  CAPS_TARGET_DETAIL_DESC_PREFS_READ;
extern int  CAPS_TARGET_URL_PREFS_READ;
extern int  CAPS_TARGET_DESC_PREFS_WRITE;
extern int  CAPS_TARGET_DETAIL_DESC_PREFS_WRITE;
extern int  CAPS_TARGET_URL_PREFS_WRITE;
extern int  CAPS_TARGET_DESC_SEND_MAIL;
extern int  CAPS_TARGET_DETAIL_DESC_SEND_MAIL;
extern int  CAPS_TARGET_URL_SEND_MAIL;
extern int  CAPS_TARGET_DESC_REG_PRIVATE;
extern int  CAPS_TARGET_DETAIL_DESC_REG_PRIVATE;
//extern int  CAPS_TARGET_DETAIL_DESC_REG_PRIVATE_1;
extern int  CAPS_TARGET_URL_REG_PRIVATE;
extern int  CAPS_TARGET_DESC_REG_STANDARD;
extern int  CAPS_TARGET_DETAIL_DESC_REG_STANDARD;
//extern int  CAPS_TARGET_DETAIL_DESC_REG_STANDARD_1;
//extern int  CAPS_TARGET_DETAIL_DESC_REG_STANDARD_2;
extern int  CAPS_TARGET_URL_REG_STANDARD;
extern int  CAPS_TARGET_DESC_REG_ADMIN;
extern int  CAPS_TARGET_DETAIL_DESC_REG_ADMIN;
//extern int  CAPS_TARGET_DETAIL_DESC_REG_ADMIN_1;
extern int  CAPS_TARGET_URL_REG_ADMIN;
extern int  CAPS_TARGET_DESC_ACCOUNT_SETUP;
extern int  CAPS_TARGET_DETAIL_DESC_ACCOUNT_SETUP;
extern int  CAPS_TARGET_URL_ACCOUNT_SETUP;
extern int  CAPS_TARGET_DESC_SAR;
extern int  CAPS_TARGET_DETAIL_DESC_SAR;
//extern int  CAPS_TARGET_DETAIL_DESC_SAR_1;
extern int  CAPS_TARGET_URL_SAR;
extern int  CAPS_TARGET_DESC_CANVAS_ACCESS;
extern int  CAPS_TARGET_DETAIL_DESC_CANVAS_ACCESS;
extern int  CAPS_TARGET_URL_CANVAS_ACCESS;
extern int  CAPS_TARGET_DESC_FILE_ACCESS;
extern int  CAPS_TARGET_DETAIL_DESC_FILE_ACCESS;
extern int  CAPS_TARGET_URL_FILE_ACCESS;
extern int  CAPS_TARGET_DESC_UNINSTALL;
extern int  CAPS_TARGET_DETAIL_DESC_UNINSTALL;
extern int  CAPS_TARGET_URL_UNINSTALL;
extern int  CAPS_TARGET_DESC_SOFTWAREINSTALL;
extern int  CAPS_TARGET_DETAIL_DESC_SOFTWAREINSTALL;
extern int  CAPS_TARGET_URL_SOFTWAREINSTALL;
extern int  CAPS_TARGET_DESC_SILENTINSTALL;
extern int  CAPS_TARGET_DETAIL_DESC_SILENTINSTALL;
extern int  CAPS_TARGET_URL_SILENTINSTALL;
extern int  CAPS_TARGET_DESC_ALL_JAVA_PERMISSION;
extern int  CAPS_TARGET_DETAIL_DESC_ALL_JAVA_PERMISSION;
extern int  CAPS_TARGET_URL_ALL_JAVA_PERMISSION;
extern int  CAPS_TARGET_DESC_ALL_JS_PERMISSION;
extern int  CAPS_TARGET_DETAIL_DESC_ALL_JS_PERMISSION;
extern int  CAPS_TARGET_URL_ALL_JS_PERMISSION;
#endif /* XXX */

/* XXXXXXXX END oF HACK */

#define TARGET_STR " Target: "
#define PRIN_STR " Principal: "

static nsHashtable *theTargetRegistry = new nsHashtable();
static nsHashtable *theSystemTargetRegistry = new nsHashtable();
static nsHashtable *theDescToTargetRegistry = new nsHashtable();

static PRBool addToTargetArray(nsHashKey *aKey, void *aData, void* closure);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "jpermission.h"

PR_PUBLIC_API(void)
java_netscape_security_getTargetDetails(const char *charSetName, 
                                        char* targetName, 
                                        char** details, char **risk)
{
    if (!targetName) {
      return; 
    }

    nsTarget *target = nsTarget::getTargetFromDescription(targetName);
    *risk = target->getRisk();
    nsTargetArray *primitiveTargets = target->getFlattenedTargetArray();

    /* Count the length of string buffer to allocate */
    int len=0;
    int extra_len = strlen("<option>") + strlen(" (") + strlen(")");
    int i;
    for (i = primitiveTargets->GetSize(); i-- > 0;) {
      nsTarget *primTarget = (nsTarget *)primitiveTargets->Get(i);
      len += extra_len + strlen(primTarget->getDescription()) + 
             strlen(primTarget->getRisk());
    }
      
    char *desc = new char[len+1];
    desc[0] = '\0';
    for (i = primitiveTargets->GetSize(); i-- > 0;) {
      nsTarget *primTarget = (nsTarget *)primitiveTargets->Get(i);
      XP_STRCAT(desc, "<option>");
      XP_STRCAT(desc, primTarget->getDescription());
      XP_STRCAT(desc, " (");
      XP_STRCAT(desc, primTarget->getRisk());
      XP_STRCAT(desc, ")");
    }
    *details = desc;
    // Should we consider caching the details desc?
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


//
// the code below creates all the system targets -- this will
// occur before any Target methods can execute, so we have
// some confidence the bootstrapping will succeed
//
PRBool CreateSystemTargets(nsPrincipal *sysPrin)
{
  nsUserTarget *userTarg;
  nsTarget *target;
  PRUint32 i=0;
  nsUserTarget *FileReadTarg;
  nsUserTarget *FileWriteTarg;
  nsUserTarget *FileDeleteTarg;
  nsUserTarget *BrowserReadTarg;
  nsUserTarget *BrowserWriteTarg;
  nsUserTarget *UniversalPreferencesReadTarg;
  nsUserTarget *UniversalPreferencesWriteTarg;
  nsUserTarget *SendMailTarg;
  nsUserTarget *RegistryPrivateTarg;
  nsUserTarget *RegistryStandardTarg;
  nsUserTarget *RegistryAdminTarg;
  nsUserTarget *SilentInstallTarg;
  nsUserTarget *SoftwareInstallTarg;
  nsUserTarget *UninstallTarg;
  
  nsUserTarget *UniversalFileAccessTarg;
  nsUserTarget *UniversalBrowserAccessTarg;
  
  nsTarget *ImpersonatorTarg;
  
  int targetRiskHigh = JavaSecUI_targetRiskHigh();
  int targetRiskLow = JavaSecUI_targetRiskLow(); 
  int targetRiskMedium = JavaSecUI_targetRiskMedium();
  char *targetRiskColorHigh = JavaSecUI_getString(CAPS_TARGET_RISK_COLOR_HIGH);
  char *targetRiskColorLow = JavaSecUI_getString(CAPS_TARGET_RISK_COLOR_LOW);
  char *targetRiskColorMedium = JavaSecUI_getString(CAPS_TARGET_RISK_COLOR_MEDIUM);
  
  //
  // targets used by the real browser
  //
  nsTargetArray *targetPtrArray;
  
  FileReadTarg = new nsUserTarget("UniversalFileRead", sysPrin,
                                  targetRiskHigh,
                                  targetRiskColorHigh,
                                  CAPS_TARGET_DESC_FILE_READ,
                                  CAPS_TARGET_DETAIL_DESC_FILE_READ,
                                  CAPS_TARGET_URL_FILE_READ);
  FileReadTarg->registerTarget();
  
  FileWriteTarg = new nsUserTarget("UniversalFileWrite", sysPrin,
                                   targetRiskHigh,
                                   targetRiskColorHigh,
                                   CAPS_TARGET_DESC_FILE_WRITE,
                                   CAPS_TARGET_DETAIL_DESC_FILE_WRITE,
                                   CAPS_TARGET_URL_FILE_WRITE);
  FileWriteTarg->registerTarget();
  
  FileDeleteTarg = new nsUserTarget("UniversalFileDelete", sysPrin,
                                    targetRiskHigh,
                                    targetRiskColorHigh,
                                    CAPS_TARGET_DESC_FILE_DELETE,
                                    CAPS_TARGET_DETAIL_DESC_FILE_DELETE,
                                    CAPS_TARGET_URL_FILE_DELETE);
  FileDeleteTarg->registerTarget();
  
  ImpersonatorTarg = new nsTarget("Impersonator", sysPrin,
                                  targetRiskHigh,
                                  targetRiskColorHigh,
                                  CAPS_TARGET_DESC_IMPERSONATOR,
                                  CAPS_TARGET_DETAIL_DESC_IMPERSONATOR,
                                  CAPS_TARGET_URL_IMPERSONATOR);
  ImpersonatorTarg->registerTarget();
  
  BrowserReadTarg = new nsUserTarget("UniversalBrowserRead", sysPrin,
                                     targetRiskMedium,
                                     targetRiskColorMedium,
                                     CAPS_TARGET_DESC_BROWSER_READ,
                                     CAPS_TARGET_DETAIL_DESC_BROWSER_READ,
                                     CAPS_TARGET_URL_BROWSER_READ);
  BrowserReadTarg->registerTarget();
  
  BrowserWriteTarg = new nsUserTarget("UniversalBrowserWrite", sysPrin,
                                      targetRiskHigh,
                                      targetRiskColorHigh,
                                      CAPS_TARGET_DESC_BROWSER_WRITE,
                                      CAPS_TARGET_DETAIL_DESC_BROWSER_WRITE,
                                      CAPS_TARGET_URL_BROWSER_WRITE);
  BrowserWriteTarg->registerTarget();
  
  UniversalPreferencesReadTarg = new nsUserTarget("UniversalPreferencesRead", 
                                                  sysPrin,
                                                  targetRiskMedium,
                                                  targetRiskColorMedium,
                                                  CAPS_TARGET_DESC_PREFS_READ,
                                                  CAPS_TARGET_DETAIL_DESC_PREFS_READ,
                                                  CAPS_TARGET_URL_PREFS_READ);
  UniversalPreferencesReadTarg->registerTarget();
  
  UniversalPreferencesWriteTarg = new nsUserTarget("UniversalPreferencesWrite", 
                                                   sysPrin,
                                                   targetRiskHigh,
                                                   targetRiskColorHigh,
                                                   CAPS_TARGET_DESC_PREFS_WRITE,
                                                   CAPS_TARGET_DETAIL_DESC_PREFS_WRITE,
                                                   CAPS_TARGET_URL_PREFS_WRITE);
  UniversalPreferencesWriteTarg->registerTarget();
  
  SendMailTarg = new nsUserTarget("UniversalSendMail", sysPrin,
                                  targetRiskMedium,
                                  targetRiskColorMedium,
                                  CAPS_TARGET_DESC_SEND_MAIL,
                                  CAPS_TARGET_DETAIL_DESC_SEND_MAIL,
                                  CAPS_TARGET_URL_SEND_MAIL);
  SendMailTarg->registerTarget();
  
  RegistryPrivateTarg = new nsUserTarget("PrivateRegistryAccess", sysPrin,
                                         targetRiskLow,
                                         targetRiskColorLow,
                                         CAPS_TARGET_DESC_REG_PRIVATE,
                                         CAPS_TARGET_DETAIL_DESC_REG_PRIVATE,
                                         CAPS_TARGET_URL_REG_PRIVATE);
  RegistryPrivateTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(1, 1);
  i = 0;
  targetPtrArray->Set(i++, (void *)RegistryPrivateTarg);
  RegistryStandardTarg = new nsUserTarget("StandardRegistryAccess", sysPrin,
                                          targetRiskMedium,
                                          targetRiskColorMedium,
                                          CAPS_TARGET_DESC_REG_STANDARD,
                                          CAPS_TARGET_DETAIL_DESC_REG_STANDARD,
                                          CAPS_TARGET_URL_REG_STANDARD,
                                          targetPtrArray);
  RegistryStandardTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(1, 1);
  i = 0;
  targetPtrArray->Set(i++, (void *)RegistryStandardTarg);
  RegistryAdminTarg = new nsUserTarget("AdministratorRegistryAccess", 
                                       sysPrin,
                                       targetRiskHigh,
                                       targetRiskColorHigh,
                                       CAPS_TARGET_DESC_REG_ADMIN,
                                       CAPS_TARGET_DETAIL_DESC_REG_ADMIN,
                                       CAPS_TARGET_URL_REG_ADMIN,
                                       targetPtrArray);
  RegistryAdminTarg->registerTarget();
  
  UninstallTarg = new nsUserTarget("Uninstall", 
                                    sysPrin,
                                    targetRiskHigh,
			                        targetRiskColorHigh,
                                    CAPS_TARGET_DESC_UNINSTALL,
                                    CAPS_TARGET_DETAIL_DESC_UNINSTALL,
                                    CAPS_TARGET_URL_UNINSTALL);
  UninstallTarg->registerTarget();

  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(4,1);
  i=0;
  targetPtrArray->Set(i++, (void *)RegistryStandardTarg);
  targetPtrArray->Set(i++, (void *)UniversalPreferencesReadTarg);
  targetPtrArray->Set(i++, (void *)FileReadTarg);
  targetPtrArray->Set(i++, (void *)UninstallTarg);
  SoftwareInstallTarg = new nsUserTarget("SoftwareInstall", 
                                        sysPrin,
                                        targetRiskHigh,
			                            targetRiskColorHigh,
                                        CAPS_TARGET_DESC_SOFTWAREINSTALL,
                                        CAPS_TARGET_DETAIL_DESC_SOFTWAREINSTALL,
                                        CAPS_TARGET_URL_SOFTWAREINSTALL,
                                        targetPtrArray);
  SoftwareInstallTarg->registerTarget();

  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(1, 1);
  i = 0;
  targetPtrArray->Set(i++, (void *)SoftwareInstallTarg);
  SilentInstallTarg = new nsUserTarget("SilentInstall",
                                        sysPrin,
                                        targetRiskHigh,
			                            targetRiskColorHigh,
                                        CAPS_TARGET_DESC_SILENTINSTALL,
                                        CAPS_TARGET_DETAIL_DESC_SILENTINSTALL,
                                        CAPS_TARGET_URL_SILENTINSTALL,
                                        targetPtrArray);
  SilentInstallTarg->registerTarget();

  //
  // targets used by Constellation group
  //
  // Create a user target that protects cache APIs
  nsTarget *SiteArchiveTarget = new nsUserTarget("SiteArchiveTarget",    
                                                 sysPrin,
                                                 targetRiskHigh,
                                                 targetRiskColorHigh,
                                                 CAPS_TARGET_DESC_SAR,
                                                 CAPS_TARGET_DETAIL_DESC_SAR,
                                                 CAPS_TARGET_URL_SAR);
  SiteArchiveTarget->registerTarget();
  

  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(1, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)BrowserWriteTarg);
  userTarg = new nsUserTarget("CanvasAccess", sysPrin,
                              targetRiskHigh,
                              targetRiskColorHigh,
                              CAPS_TARGET_DESC_CANVAS_ACCESS,
                              CAPS_TARGET_DETAIL_DESC_CANVAS_ACCESS,
                              CAPS_TARGET_URL_CANVAS_ACCESS, 
                              targetPtrArray);
  userTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(3, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)FileReadTarg);
  targetPtrArray->Set(i++, (void *)FileWriteTarg);
  targetPtrArray->Set(i++, (void *)FileDeleteTarg);
  UniversalFileAccessTarg = new nsUserTarget("UniversalFileAccess", 
                                             sysPrin,
                                             targetRiskHigh,
                                             targetRiskColorHigh,
                                             CAPS_TARGET_DESC_FILE_ACCESS,
                                             CAPS_TARGET_DETAIL_DESC_FILE_ACCESS,
                                             CAPS_TARGET_URL_FILE_ACCESS,
                                             targetPtrArray);
  UniversalFileAccessTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(2, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)BrowserReadTarg);
  targetPtrArray->Set(i++, (void *)BrowserWriteTarg);
  UniversalBrowserAccessTarg = 
    new nsUserTarget("UniversalBrowserAccess",
                     sysPrin,
                     targetRiskHigh,
                     targetRiskColorHigh,
                     CAPS_TARGET_DESC_BROWSER_ACCESS,
                     CAPS_TARGET_DETAIL_DESC_BROWSER_ACCESS,
                     CAPS_TARGET_URL_BROWSER_ACCESS,
                     targetPtrArray);
  UniversalBrowserAccessTarg->registerTarget();
  
  // a macro Target for PE
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(7, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)BrowserReadTarg);
  targetPtrArray->Set(i++, (void *)BrowserWriteTarg);
  targetPtrArray->Set(i++, (void *)UniversalPreferencesReadTarg);
  targetPtrArray->Set(i++, (void *)UniversalPreferencesWriteTarg);
  targetPtrArray->Set(i++, (void *)UniversalFileAccessTarg);
  userTarg = new nsUserTarget("AccountSetup", sysPrin,
                              targetRiskHigh,
                              targetRiskColorHigh,
                              CAPS_TARGET_DESC_ACCOUNT_SETUP,
                              CAPS_TARGET_DETAIL_DESC_ACCOUNT_SETUP,
                              CAPS_TARGET_URL_ACCOUNT_SETUP,
                              targetPtrArray);
  userTarg->registerTarget();
  

  /* Permission to All privileges in Java */
  target = new nsUserTarget("AllJavaPermission", sysPrin, 
                            targetRiskHigh,
                            targetRiskColorHigh,
                            CAPS_TARGET_DESC_ALL_JAVA_PERMISSION,
                            CAPS_TARGET_DETAIL_DESC_ALL_JAVA_PERMISSION,
                            CAPS_TARGET_URL_ALL_JAVA_PERMISSION);
  target->registerTarget();

  /* Permission to All privileges in Java */
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(7, 1);
  i=0;
  /* The following list of JS targets came from lm_taint.c */
  targetPtrArray->Set(i++, (void *)BrowserReadTarg);
  targetPtrArray->Set(i++, (void *)BrowserWriteTarg);
  targetPtrArray->Set(i++, (void *)SendMailTarg);
  targetPtrArray->Set(i++, (void *)FileReadTarg);
  targetPtrArray->Set(i++, (void *)FileWriteTarg);
  targetPtrArray->Set(i++, (void *)UniversalPreferencesReadTarg);
  targetPtrArray->Set(i++, (void *)UniversalPreferencesWriteTarg);
  target = new nsUserTarget("AllJavaScriptPermission", sysPrin, 
                            targetRiskHigh,
                            targetRiskColorHigh,
                            CAPS_TARGET_DESC_ALL_JS_PERMISSION,
                            CAPS_TARGET_DETAIL_DESC_ALL_JS_PERMISSION,
                            CAPS_TARGET_URL_ALL_JS_PERMISSION,
                            targetPtrArray);
  target->registerTarget();

  return PR_TRUE;
}


//
// 			PUBLIC METHODS 
//

nsTarget::nsTarget(char *name, 
                   nsPrincipal *prin, 
                   PRInt32 risk, 
                   char *riskColor, 
                   int desc_id, 
                   int detail_desc_id,
                   int help_url_id, 
                   nsTargetArray* targetArray)
{
  char *description = NULL;
  char *detailDescription = NULL;
  char *url = NULL;
  if (desc_id)
    description = JavaSecUI_getString(desc_id);
  if (detail_desc_id)
    detailDescription = JavaSecUI_getString(detail_desc_id);
  if (help_url_id)
    url = JavaSecUI_getHelpURL(help_url_id);

  init(name, prin, targetArray, risk, riskColor, description, 
       detailDescription, url);

  /* init method makes a copy of all its arguments. Thus we need
   * to free the URL, which was allocated by JavaSecUI_getHelpURL.
   */
  XP_FREE(url);
}


nsTarget::~nsTarget(void)
{
  if (itsName) 
    delete []itsName;
  if (itsRiskColorStr) 
    delete []itsRiskColorStr;
  if (itsDescriptionStr) 
    delete []itsDescriptionStr;
  if (itsDetailDescriptionStr) 
    delete []itsDetailDescriptionStr;
  if (itsURLStr) 
    delete []itsURLStr;
  if (itsTargetArray) 
    delete []itsTargetArray;
  if (itsString) 
    delete []itsString;
  if (itsExpandedTargetArray) 
    delete []itsExpandedTargetArray;
}

nsTarget * nsTarget::registerTarget()
{
  return registerTarget(NULL);
}

nsTarget * nsTarget::registerTarget(void *context)
{
  nsTarget *targ;

  nsCaps_lock();

  //
  // security concern: Hashtable currently calls the
  // equals() method on objects already stored in the hash
  // table.  This is good, because it means an intruder can't
  // hack a subclass of Target with its own equals
  // function and get it registered.
  //
  // it's extremely important that Hashtable continues to work
  // this way.
  //
  TargetKey targKey(this);
  if (!theTargetRegistry) {
    theTargetRegistry = new nsHashtable();
  }
  targ = (nsTarget *) theTargetRegistry->Get(&targKey);
	    
  //
  // if the target is already registered, just return this one
  // without registering it.
  //
  if (targ != NULL) {
    PR_ASSERT(this == targ);
    nsCaps_unlock();
    return targ;
  }

  nsPrivilegeManager *mgr = nsPrivilegeManager::getPrivilegeManager();
  if ((mgr != NULL) && (context != NULL) && 
      (!mgr->checkMatchPrincipal(context, itsPrincipal, 1))) {
    nsCaps_unlock();
    return NULL;
  }

  //
  // otherwise, add the target to the registry
  //
  // TODO: make sure the caller has the given principal -- you
  // shouldn't be allowed to register a target under a principal
  // you don't own.
  //
  theTargetRegistry->Put(&targKey, this); // hash table will "canonicalize" name


  if (!theSystemTargetRegistry) {
    theSystemTargetRegistry = new nsHashtable();
  }

  if (itsPrincipal->equals(nsPrivilegeManager::getSystemPrincipal())) {
    IntegerKey ikey(PL_HashString(itsName));
    theSystemTargetRegistry->Put(&ikey, this); 
  }

  // The following hash table is used by the Admin UI. It finds
  // the actual target given a description

  IntegerKey ikey(itsDescriptionHash);
  if (!theDescToTargetRegistry) {
    theDescToTargetRegistry = new nsHashtable();
  }
  theDescToTargetRegistry->Put(&ikey, this);

  itsRegistered = PR_TRUE;
	    
  nsCaps_unlock();
  return this;
}

nsTarget * nsTarget::findTarget(nsTarget *target)
{
  TargetKey targKey(target);
  return (nsTarget *) theTargetRegistry->Get(&targKey);
}

nsTarget * nsTarget::findTarget(char *name)
{
  IntegerKey ikey(PL_HashString(name));
  return (nsTarget *)theSystemTargetRegistry->Get(&ikey);
}

nsTarget * nsTarget::findTarget(char *name, nsPrincipal *prin)
{
  if (prin->equals(nsPrivilegeManager::getSystemPrincipal())) {
    return findTarget(name);
  }
  /* name and principal combination uniquely identfies a target */
  nsTarget* targ = new nsTarget((char *)name, prin, 
                                nsRiskType_HighRisk,
                                JavaSecUI_getString(CAPS_TARGET_RISK_COLOR_HIGH),
                                (char*)NULL, (char*)NULL, (char*)NULL, 
                                (nsTargetArray*)NULL);
  nsTarget* ret_val = findTarget(targ);
  delete targ;
  return ret_val;
}

nsPrivilege * nsTarget::checkPrivilegeEnabled(nsPrincipalArray* prinArray, void *data)
{
  return nsPrivilege::findPrivilege(nsPermissionState_Blank, nsDurationState_Session);
}

nsPrivilege * nsTarget::checkPrivilegeEnabled(nsPrincipalArray* prinArray)
{
  return checkPrivilegeEnabled(prinArray, NULL);
}

nsPrivilege * nsTarget::checkPrivilegeEnabled(nsPrincipal *p, void *data)
{
  return nsPrivilege::findPrivilege(nsPermissionState_Blank, nsDurationState_Session);
}

nsPrivilege * nsTarget::enablePrivilege(nsPrincipal *prin, void *data)
{
  if (itsPrincipal->equals(prin)) 
    return nsPrivilege::findPrivilege(nsPermissionState_Allowed, nsDurationState_Session);
  return nsPrivilege::findPrivilege(nsPermissionState_Blank, nsDurationState_Session);
}

nsPrivilege * nsTarget::getPrincipalPrivilege(nsPrincipal *prin, void *data)
{
  return nsPrivilege::findPrivilege(nsPermissionState_Blank, nsDurationState_Session);
}

nsTargetArray* nsTarget::getFlattenedTargetArray(void)
{
  if (itsExpandedTargetArray != NULL)
    return itsExpandedTargetArray;  

  // We must populate the cached value of the Expansion

  nsHashtable *targetHash = new nsHashtable();
  nsTargetArray *expandedTargetArray = new nsTargetArray();

  getFlattenedTargets(targetHash, expandedTargetArray);
  targetHash->Enumerate(addToTargetArray);

  delete targetHash;

  itsExpandedTargetArray = expandedTargetArray; 
  // expandedTargetArray->FreeExtra();
  
  return itsExpandedTargetArray;
}

void nsTarget::getFlattenedTargets(nsHashtable *targHash, 
                                   nsTargetArray *expandedTargetArray) 
{
  nsTarget *target;
  void * data;

  TargetKey targKey(this);
  data = (void *)targHash->Get(&targKey);
  if (data != NULL) {
    return; // We've already added this node
  }

  targHash->Put(&targKey, expandedTargetArray);

  if (itsTargetArray == NULL) {
    return;
  }

  for (PRUint32 i=itsTargetArray->GetSize(); i-- > 0; ) {
    target = (nsTarget *)itsTargetArray->Get(i);
    target->getFlattenedTargets(targHash, expandedTargetArray);
  }
}

static PRBool addToTargetArray(nsHashKey *aKey, void *aData, void* closure) 
{
  TargetKey *targetKey = (TargetKey *) aKey;
  nsTarget *target = targetKey->itsTarget;
  nsTargetArray *targetArray = (nsTargetArray *) aData;
  if (targetArray->Add((void *)target) >= 0)
    return PR_TRUE;
  return PR_FALSE;
}


nsTargetArray* nsTarget::getAllRegisteredTargets(void)
{
  PR_ASSERT(PR_FALSE);
  /* XXX: fix it. We need to walk hashtable and generate a target array.
  nsTargetArray *targArray = new nsTargetArray();
  theTargetRegistry->Enumerate(addToTargetArray);
  */
  return NULL;
}

char * nsTarget::getRisk(void)
{
  return JavaSecUI_targetRiskStr(itsRisk);
}

char * nsTarget::getRiskColor(void)
{
  return itsRiskColorStr;
}

char * nsTarget::getDescription(void)
{
  return itsDescriptionStr;
}

char * nsTarget::getDetailDescription(void)
{
  return itsDetailDescriptionStr;
}

nsTarget * nsTarget::getTargetFromDescription(char *a)
{
  IntegerKey ikey(PL_HashString(a));
  return (nsTarget *) theDescToTargetRegistry->Get(&ikey);
}

char * nsTarget::getHelpURL(void)
{
  return itsURLStr;
}

char * nsTarget::getDetailedInfo(void *a)
{
  return "";
}

nsPrincipal * nsTarget::getPrincipal(void)
{
  return itsPrincipal;
}

char * nsTarget::getName(void)
{
  return itsName;
}

PRBool nsTarget::equals(nsTarget *obj)
{
  PRBool bSameName, bSamePrin;

  if (obj == this) return PR_TRUE;

  bSameName = ((strcmp(itsName, obj->itsName) == 0) ? PR_TRUE : PR_FALSE);

  if (itsPrincipal == NULL)
    bSamePrin = ((obj->itsPrincipal == NULL) ? PR_TRUE : PR_FALSE);
  else
    bSamePrin = itsPrincipal->equals(obj->itsPrincipal);

  return (bSameName && bSamePrin) ? PR_TRUE : PR_FALSE;
}

PRInt32 nsTarget::hashCode(void) 
{
  return PL_HashString(itsName) + 
    ((itsPrincipal != NULL) ? itsPrincipal->hashCode() :0);
}

char * nsTarget::toString(void) 
{
  if (itsString != NULL)
    return itsString;
  const char * prinStr = (itsPrincipal != NULL)? itsPrincipal->toString() : "<none>";
  char * itsString = new char [strlen(TARGET_STR) + strlen(itsName) + 
                               strlen(PRIN_STR) + strlen(prinStr) + 1];
  XP_STRCPY(itsString, TARGET_STR); 
  XP_STRCAT(itsString, itsName); 
  XP_STRCAT(itsString, PRIN_STR); 
  XP_STRCAT(itsString, prinStr); 
  return itsString;
}

PRBool nsTarget::isRegistered(void) 
{
  return itsRegistered;
}

//
// 			PRIVATE METHODS 
//

void nsTarget::init(char *name, nsPrincipal *prin, nsTargetArray* targetArray, 
                    PRInt32 risk, char *riskColor, char *description, 
                    char *detailDescription, char *url)
{
  PR_ASSERT(name != NULL);
  PR_ASSERT(prin != NULL);

  itsName = new char[strlen(name) + 1];
  XP_STRCPY(itsName, name);

  itsPrincipal = prin;
  itsRegistered = PR_FALSE;

  itsRisk = risk;
  if (riskColor) {
    itsRiskColorStr = new char[strlen(riskColor) + 1];
    XP_STRCPY(itsRiskColorStr, riskColor);
  } else {
    itsRiskColorStr = 
      XP_STRDUP(JavaSecUI_getString(CAPS_TARGET_RISK_COLOR_HIGH));
  }

  if (description == NULL)
    description = name;
  itsDescriptionStr = new char[strlen(description) + 1];
  XP_STRCPY(itsDescriptionStr, description);

  if (detailDescription == NULL)
    detailDescription = itsDescriptionStr;

  itsDetailDescriptionStr = new char[strlen(detailDescription) + 1];
  XP_STRCPY(itsDetailDescriptionStr, detailDescription);

  if (url != NULL) {
    itsURLStr = new char[strlen(url) + 1];
    XP_STRCPY(itsURLStr, url);
  } else {
    itsURLStr = NULL;
  }

  itsTargetArray = NULL;
  itsString = NULL;
  itsDescriptionHash = PL_HashString(itsDescriptionStr);
  itsExpandedTargetArray = NULL;

  if (targetArray == NULL)
    return;

  for (PRUint32 i=targetArray->GetSize(); i-- > 0;) {
    nsTarget *target = (nsTarget *)targetArray->Get(i);

    if (target != NULL)
    {
	PR_ASSERT(target->itsPrincipal == prin);
	if (target->itsRisk > itsRisk) {
	  itsRisk = target->itsRisk;
	}
    }
  }
  //  itsTargetArray = new nsTargetArray();
  //  itsTargetArray->CopyArray(targetArray);

  itsTargetArray = targetArray;
}


static PRBool initialize(void) 
{
  return PR_TRUE;
}

PRBool nsTarget::theInited = initialize();
