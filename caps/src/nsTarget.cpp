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

extern int CAPS_TARGET_RISK_COLOR_HIGH;
extern int CAPS_TARGET_RISK_COLOR_LOW;
extern int CAPS_TARGET_RISK_COLOR_MEDIUM;
extern int CAPS_TARGET_DESC_THREAD_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_THREAD_ACCESS;
extern int CAPS_TARGET_URL_THREAD_ACCESS;
extern int CAPS_TARGET_DESC_THREAD_GROUP_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_THREAD_GROUP_ACCESS;
extern int CAPS_TARGET_URL_THREAD_GROUP_ACCESS;
extern int CAPS_TARGET_DESC_EXEC_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_EXEC_ACCESS;
extern int CAPS_TARGET_URL_EXEC_ACCESS;
extern int CAPS_TARGET_DESC_EXIT_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_EXIT_ACCESS;
extern int CAPS_TARGET_URL_EXIT_ACCESS;
extern int CAPS_TARGET_DESC_LINK_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_LINK_ACCESS;
extern int CAPS_TARGET_URL_LINK_ACCESS;
extern int CAPS_TARGET_DESC_PROPERTY_WRITE;
extern int CAPS_TARGET_DETAIL_DESC_PROPERTY_WRITE;
extern int CAPS_TARGET_URL_PROPERTY_WRITE;
extern int CAPS_TARGET_DESC_PROPERTY_READ;
extern int CAPS_TARGET_DETAIL_DESC_PROPERTY_READ;
extern int CAPS_TARGET_URL_PROPERTY_READ;
extern int CAPS_TARGET_DESC_FILE_READ;
extern int CAPS_TARGET_DETAIL_DESC_FILE_READ;
extern int CAPS_TARGET_URL_FILE_READ;
extern int CAPS_TARGET_DESC_FILE_WRITE;
extern int CAPS_TARGET_DETAIL_DESC_FILE_WRITE;
extern int CAPS_TARGET_URL_FILE_WRITE;
extern int CAPS_TARGET_DESC_FILE_DELETE;
extern int CAPS_TARGET_DETAIL_DESC_FILE_DELETE;
extern int CAPS_TARGET_URL_FILE_DELETE;
extern int CAPS_TARGET_DESC_FD_READ;
extern int CAPS_TARGET_DETAIL_DESC_FD_READ;
extern int CAPS_TARGET_URL_FD_READ;
extern int CAPS_TARGET_DESC_FD_WRITE;
extern int CAPS_TARGET_DETAIL_DESC_FD_WRITE;
extern int CAPS_TARGET_URL_FD_WRITE;
extern int CAPS_TARGET_DESC_LISTEN;
extern int CAPS_TARGET_DETAIL_DESC_LISTEN;
extern int CAPS_TARGET_URL_LISTEN;
extern int CAPS_TARGET_DESC_ACCEPT;
extern int CAPS_TARGET_DETAIL_DESC_ACCEPT;
extern int CAPS_TARGET_URL_ACCEPT;
extern int CAPS_TARGET_DESC_MULTICAST;
extern int CAPS_TARGET_DETAIL_DESC_MULTICAST;
extern int CAPS_TARGET_URL_MULTICAST;
extern int CAPS_TARGET_DESC_TOP_LEVEL_WINDOW;
extern int CAPS_TARGET_DETAIL_DESC_TOP_LEVEL_WINDOW;
extern int CAPS_TARGET_URL_TOP_LEVEL_WINDOW;
extern int CAPS_TARGET_DESC_DIALOG_MODALITY;
extern int CAPS_TARGET_DETAIL_DESC_DIALOG_MODALITY;
extern int CAPS_TARGET_URL_DIALOG_MODALITY;
extern int CAPS_TARGET_DESC_PACKAGE_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_PACKAGE_ACCESS;
extern int CAPS_TARGET_URL_PACKAGE_ACCESS;
extern int CAPS_TARGET_DESC_PACKAGE_DEFINITION;
extern int CAPS_TARGET_DETAIL_DESC_PACKAGE_DEFINITION;
extern int CAPS_TARGET_URL_PACKAGE_DEFINITION;
extern int CAPS_TARGET_DESC_SET_FACTORY;
extern int CAPS_TARGET_DETAIL_DESC_SET_FACTORY;
extern int CAPS_TARGET_URL_SET_FACTORY;
extern int CAPS_TARGET_DESC_MEMBER_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_MEMBER_ACCESS;
extern int CAPS_TARGET_URL_MEMBER_ACCESS;
extern int CAPS_TARGET_DESC_PRINT_JOB_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_PRINT_JOB_ACCESS;
extern int CAPS_TARGET_URL_PRINT_JOB_ACCESS;
extern int CAPS_TARGET_DESC_SYSTEM_CLIPBOARD_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_SYSTEM_CLIPBOARD_ACCESS;
extern int CAPS_TARGET_URL_SYSTEM_CLIPBOARD_ACCESS;
extern int CAPS_TARGET_DESC_AWT_EVENT_QUEUE_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_AWT_EVENT_QUEUE_ACCESS;
extern int CAPS_TARGET_URL_AWT_EVENT_QUEUE_ACCESS;
extern int CAPS_TARGET_DESC_SECURITY_PROVIDER;
extern int CAPS_TARGET_DETAIL_DESC_SECURITY_PROVIDER;
extern int CAPS_TARGET_URL_SECURITY_PROVIDER;
extern int CAPS_TARGET_DESC_CREATE_SECURITY_MANAGER;
extern int CAPS_TARGET_DETAIL_DESC_CREATE_SECURITY_MANAGER;
extern int CAPS_TARGET_URL_CREATE_SECURITY_MANAGER;
extern int CAPS_TARGET_DESC_IMPERSONATOR;
extern int CAPS_TARGET_DETAIL_DESC_IMPERSONATOR;
extern int CAPS_TARGET_URL_IMPERSONATOR;
extern int CAPS_TARGET_DESC_BROWSER_READ;
extern int CAPS_TARGET_DETAIL_DESC_BROWSER_READ;
extern int CAPS_TARGET_URL_BROWSER_READ;
extern int CAPS_TARGET_DESC_BROWSER_WRITE;
extern int CAPS_TARGET_DETAIL_DESC_BROWSER_WRITE;
extern int CAPS_TARGET_URL_BROWSER_WRITE;
extern int CAPS_TARGET_DESC_PREFS_READ;
extern int CAPS_TARGET_DETAIL_DESC_PREFS_READ;
extern int CAPS_TARGET_URL_PREFS_READ;
extern int CAPS_TARGET_DESC_PREFS_WRITE;
extern int CAPS_TARGET_DETAIL_DESC_PREFS_WRITE;
extern int CAPS_TARGET_URL_PREFS_WRITE;
extern int CAPS_TARGET_DESC_SEND_MAIL;
extern int CAPS_TARGET_DETAIL_DESC_SEND_MAIL;
extern int CAPS_TARGET_URL_SEND_MAIL;
extern int CAPS_TARGET_DESC_REG_PRIVATE;
extern int CAPS_TARGET_DETAIL_DESC_REG_PRIVATE;
extern int CAPS_TARGET_URL_REG_PRIVATE;
extern int CAPS_TARGET_DESC_REG_STANDARD;
extern int CAPS_TARGET_DETAIL_DESC_REG_STANDARD;
extern int CAPS_TARGET_URL_REG_STANDARD;
extern int CAPS_TARGET_DESC_REG_ADMIN;
extern int CAPS_TARGET_DETAIL_DESC_REG_ADMIN;
extern int CAPS_TARGET_URL_REG_ADMIN;
extern int CAPS_TARGET_DESC_UNINSTALL;
extern int CAPS_TARGET_DETAIL_DESC_UNINSTALL;
extern int CAPS_TARGET_URL_UNINSTALL;
extern int CAPS_TARGET_DESC_SOFTWAREINSTALL;
extern int CAPS_TARGET_DETAIL_DESC_SOFTWAREINSTALL;
extern int CAPS_TARGET_URL_SOFTWAREINSTALL;
extern int CAPS_TARGET_DESC_SILENTINSTALL;
extern int CAPS_TARGET_DETAIL_DESC_SILENTINSTALL;
extern int CAPS_TARGET_URL_SILENTINSTALL;
extern int CAPS_TARGET_DESC_CONNECT;
extern int CAPS_TARGET_DETAIL_DESC_CONNECT;
extern int CAPS_TARGET_URL_CONNECT;
extern int CAPS_TARGET_DESC_CLIENT_AUTH;
extern int CAPS_TARGET_DETAIL_DESC_CLIENT_AUTH;
extern int CAPS_TARGET_URL_CLIENT_AUTH;
extern int CAPS_TARGET_DESC_REDIRECT;
extern int CAPS_TARGET_DETAIL_DESC_REDIRECT;
extern int CAPS_TARGET_URL_REDIRECT;
extern int CAPS_TARGET_DESC_CONNECT_WITH_REDIRECT;
extern int CAPS_TARGET_DETAIL_DESC_CONNECT_WITH_REDIRECT;
extern int CAPS_TARGET_URL_CONNECT_WITH_REDIRECT;
extern int CAPS_TARGET_DESC_CODEBASE_ENV;
extern int CAPS_TARGET_DETAIL_DESC_CODEBASE_ENV;
extern int CAPS_TARGET_URL_CODEBASE_ENV;
extern int CAPS_TARGET_DESC_SUPER_USER;
extern int CAPS_TARGET_DETAIL_DESC_SUPER_USER;
extern int CAPS_TARGET_URL_SUPER_USER;
extern int CAPS_TARGET_DESC_SAR;
extern int CAPS_TARGET_DETAIL_DESC_SAR;
extern int CAPS_TARGET_URL_SAR;
extern int CAPS_TARGET_DESC_30_CAPABILITIES;
extern int CAPS_TARGET_DETAIL_DESC_30_CAPABILITIES;
extern int CAPS_TARGET_URL_30_CAPABILITIES;
extern int CAPS_TARGET_DESC_MARIMBA;
extern int CAPS_TARGET_DETAIL_DESC_MARIMBA;
extern int CAPS_TARGET_URL_MARIMBA;
extern int CAPS_TARGET_DESC_MARIMBA;
extern int CAPS_TARGET_DETAIL_DESC_MARIMBA;
extern int CAPS_TARGET_URL_MARIMBA;
extern int CAPS_TARGET_DESC_IIOP;
extern int CAPS_TARGET_DETAIL_DESC_IIOP;
extern int CAPS_TARGET_URL_IIOP;
extern int CAPS_TARGET_DESC_DEBUGGER;
extern int CAPS_TARGET_DETAIL_DESC_DEBUGGER;
extern int CAPS_TARGET_URL_DEBUGGER;
extern int CAPS_TARGET_DESC_CANVAS_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_CANVAS_ACCESS;
extern int CAPS_TARGET_URL_CANVAS_ACCESS;
extern int CAPS_TARGET_DESC_FILE_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_FILE_ACCESS;
extern int CAPS_TARGET_URL_FILE_ACCESS;
extern int CAPS_TARGET_DESC_BROWSER_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_BROWSER_ACCESS;
extern int CAPS_TARGET_URL_BROWSER_ACCESS;
extern int CAPS_TARGET_DESC_LIMITED_FILE_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_LIMITED_FILE_ACCESS;
extern int CAPS_TARGET_URL_LIMITED_FILE_ACCESS;
extern int CAPS_TARGET_DESC_GAMES_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_GAMES_ACCESS;
extern int CAPS_TARGET_URL_GAMES_ACCESS;
extern int CAPS_TARGET_DESC_WORD_PROCESSOR_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_WORD_PROCESSOR_ACCESS;
extern int CAPS_TARGET_URL_WORD_PROCESSOR_ACCESS;
extern int CAPS_TARGET_DESC_SPREADSHEET_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_SPREADSHEET_ACCESS;
extern int CAPS_TARGET_URL_SPREADSHEET_ACCESS;
extern int CAPS_TARGET_DESC_PRESENTATION_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_PRESENTATION_ACCESS;
extern int CAPS_TARGET_URL_PRESENTATION_ACCESS;
extern int CAPS_TARGET_DESC_DATABASE_ACCESS;
extern int CAPS_TARGET_DETAIL_DESC_DATABASE_ACCESS;
extern int CAPS_TARGET_URL_DATABASE_ACCESS;
extern int CAPS_TARGET_DESC_TERMINAL_EMULATOR;
extern int CAPS_TARGET_DETAIL_DESC_TERMINAL_EMULATOR;
extern int CAPS_TARGET_URL_TERMINAL_EMULATOR;
extern int CAPS_TARGET_DESC_JAR_PACKAGER;
extern int CAPS_TARGET_DETAIL_DESC_JAR_PACKAGER;
extern int CAPS_TARGET_URL_JAR_PACKAGER;
extern int CAPS_TARGET_DESC_ACCOUNT_SETUP;
extern int CAPS_TARGET_DETAIL_DESC_ACCOUNT_SETUP;
extern int CAPS_TARGET_URL_ACCOUNT_SETUP;
extern int CAPS_TARGET_DESC_CONSTELLATION;
extern int CAPS_TARGET_DETAIL_DESC_CONSTELLATION;
extern int CAPS_TARGET_URL_CONSTELLATION;

extern int CAPS_TARGET_DESC_ALL_JAVA_PERMISSION;
extern int CAPS_TARGET_DETAIL_DESC_ALL_JAVA_PERMISSION;
extern int CAPS_TARGET_URL_ALL_JAVA_PERMISSION;

extern int CAPS_TARGET_RISK_COLOR_HIGH;
extern int CAPS_TARGET_RISK_COLOR_HIGH;

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
java_netscape_security_getTargetDetails(const char *charSetName, char* targetName, 
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
  nsUserTarget *ThreadAccessTarg;
  nsUserTarget *ThreadGroupAccessTarg;
  nsUserTarget *ExecAccessTarg;
  nsUserTarget *ExitAccessTarg;
  nsUserTarget *LinkAccessTarg;
  nsUserTarget *PropertyWriteTarg;
  nsUserTarget *PropertyReadTarg;
  nsUserTarget *FileReadTarg;
  nsUserTarget *FileWriteTarg;
  nsUserTarget *FileDeleteTarg;
  nsUserTarget *ListenTarg;
  nsUserTarget *AcceptTarg;
  nsUserTarget *ConnectTarg;
  nsTarget *RedirectTarg;
  nsUserTarget *ConnectWithRedirectTarg;
  nsUserTarget *MulticastTarg;
  nsUserTarget *TopLevelWindowTarg;
  nsUserTarget *DialogModalityTarg;
  nsTarget *PackageAccessTarg;
  nsTarget *PackageDefinitionTarg;
  nsUserTarget *SetFactoryTarg;
  nsTarget *MemberAccessTarg;
  nsUserTarget *PrintJobAccessTarg;
  nsUserTarget *SystemClipboardAccessTarg;
  nsUserTarget *AwtEventQueueAccessTarg;
  nsTarget *SecurityProviderTarg;
  nsTarget *CreateSecurityManagerTarg;
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
  
  nsUserTarget *LimitedFileAccessTarg;        
  nsUserTarget *UniversalFileAccessTarg;
  nsUserTarget *UniversalBrowserAccessTarg;
  
  nsTarget *ImpersonatorTarg;
  nsUserTarget *FdReadTarg;
  nsUserTarget *FdWriteTarg;
  nsTarget *CodebaseEnvTarg;
  
  nsUserTarget *ClientAuthTarg;

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
  
  ThreadAccessTarg = new nsUserTarget("UniversalThreadAccess", sysPrin,
                                      targetRiskHigh,
                                      targetRiskColorHigh,
                                      JavaSecUI_getString(CAPS_TARGET_DESC_THREAD_ACCESS),
                                      JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_THREAD_ACCESS),
                                      JavaSecUI_getHelpURL(CAPS_TARGET_URL_THREAD_ACCESS)
                                      );
  ThreadAccessTarg->registerTarget();
  
  ThreadGroupAccessTarg = new nsUserTarget("UniversalThreadGroupAccess", 
                                           sysPrin,
                                           targetRiskHigh,
                                           targetRiskColorHigh,
                                           JavaSecUI_getString(CAPS_TARGET_DESC_THREAD_GROUP_ACCESS),
                                           JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_THREAD_GROUP_ACCESS),
                                           JavaSecUI_getHelpURL(CAPS_TARGET_URL_THREAD_GROUP_ACCESS));
  ThreadGroupAccessTarg->registerTarget();
  
  ExecAccessTarg = new nsUserTarget("UniversalExecAccess", sysPrin,
                                    targetRiskHigh,
                                    targetRiskColorHigh,
                                    JavaSecUI_getString(CAPS_TARGET_DESC_EXEC_ACCESS),
                                    JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_EXEC_ACCESS),
                                    JavaSecUI_getHelpURL(CAPS_TARGET_URL_EXEC_ACCESS));
  ExecAccessTarg->registerTarget();
  
  ExitAccessTarg = new nsUserTarget("UniversalExitAccess", sysPrin,
                                    targetRiskHigh,
                                    targetRiskColorHigh,
                                    JavaSecUI_getString(CAPS_TARGET_DESC_EXIT_ACCESS),
                                    JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_EXIT_ACCESS),
                                    JavaSecUI_getHelpURL(CAPS_TARGET_URL_EXIT_ACCESS));
  ExitAccessTarg->registerTarget();
  
  LinkAccessTarg = new nsUserTarget("UniversalLinkAccess", sysPrin,
                                    targetRiskHigh,
                                    targetRiskColorHigh,
                                    JavaSecUI_getString(CAPS_TARGET_DESC_LINK_ACCESS),
                                    JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_LINK_ACCESS),
                                    JavaSecUI_getHelpURL(CAPS_TARGET_URL_LINK_ACCESS));
  LinkAccessTarg->registerTarget();
  
  PropertyWriteTarg = new nsUserTarget("UniversalPropertyWrite", sysPrin,
                                       targetRiskHigh,
                                       targetRiskColorHigh,
                                       JavaSecUI_getString(CAPS_TARGET_DESC_PROPERTY_WRITE),
                                       JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_PROPERTY_WRITE),
                                       JavaSecUI_getHelpURL(CAPS_TARGET_URL_PROPERTY_WRITE));
  PropertyWriteTarg->registerTarget();
  
  PropertyReadTarg = new nsUserTarget("UniversalPropertyRead", sysPrin,
                                      targetRiskLow,
                                      targetRiskColorLow,
                                      JavaSecUI_getString(CAPS_TARGET_DESC_PROPERTY_READ),
                                      JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_PROPERTY_READ),
                                      JavaSecUI_getHelpURL(CAPS_TARGET_URL_PROPERTY_READ));
  PropertyReadTarg->registerTarget();
  
  FileReadTarg = new nsUserTarget("UniversalFileRead", sysPrin,
                                  targetRiskHigh,
                                  targetRiskColorHigh,
                                  JavaSecUI_getString(CAPS_TARGET_DESC_FILE_READ),
                                  JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_FILE_READ),
                                  JavaSecUI_getHelpURL(CAPS_TARGET_URL_FILE_READ));
  FileReadTarg->registerTarget();
  
  FileWriteTarg = new nsUserTarget("UniversalFileWrite", sysPrin,
                                   targetRiskHigh,
                                   targetRiskColorHigh,
                                   JavaSecUI_getString(CAPS_TARGET_DESC_FILE_WRITE),
                                   JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_FILE_WRITE),
                                   JavaSecUI_getHelpURL(CAPS_TARGET_URL_FILE_WRITE));
  FileWriteTarg->registerTarget();
  
  FileDeleteTarg = new nsUserTarget("UniversalFileDelete", sysPrin,
                                    targetRiskHigh,
                                    targetRiskColorHigh,
                                    JavaSecUI_getString(CAPS_TARGET_DESC_FILE_DELETE),
                                    JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_FILE_DELETE),
                                    JavaSecUI_getHelpURL(CAPS_TARGET_URL_FILE_DELETE));
  FileDeleteTarg->registerTarget();
  
  FdReadTarg = new nsUserTarget("UniversalFdRead", sysPrin,
                                targetRiskHigh,
                                targetRiskColorHigh,
                                JavaSecUI_getString(CAPS_TARGET_DESC_FD_READ),
                                JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_FD_READ),
                                JavaSecUI_getHelpURL(CAPS_TARGET_URL_FD_READ));
  FdReadTarg->registerTarget();
  
  FdWriteTarg = new nsUserTarget("UniversalFdWrite", sysPrin,
                                 targetRiskHigh,
                                 targetRiskColorHigh,
                                 JavaSecUI_getString(CAPS_TARGET_DESC_FD_WRITE),
                                 JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_FD_WRITE),
                                 JavaSecUI_getHelpURL(CAPS_TARGET_URL_FD_WRITE));
  FdWriteTarg->registerTarget();
  
  ListenTarg = new nsUserTarget("UniversalListen", sysPrin,
                                targetRiskHigh,
                                targetRiskColorHigh,
                                JavaSecUI_getString(CAPS_TARGET_DESC_LISTEN),
                                JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_LISTEN),
                                JavaSecUI_getHelpURL(CAPS_TARGET_URL_LISTEN));
  ListenTarg->registerTarget();
  
  AcceptTarg = new nsUserTarget("UniversalAccept", sysPrin,
                                targetRiskHigh,
                                targetRiskColorHigh,
                                JavaSecUI_getString(CAPS_TARGET_DESC_ACCEPT),
                                JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_ACCEPT),
                                JavaSecUI_getHelpURL(CAPS_TARGET_URL_ACCEPT));
  AcceptTarg->registerTarget();
  
  MulticastTarg = new nsUserTarget("UniversalMulticast", sysPrin,
                                   targetRiskHigh,
                                   targetRiskColorHigh,
                                   JavaSecUI_getString(CAPS_TARGET_DESC_MULTICAST),
                                   JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_MULTICAST),
                                   JavaSecUI_getHelpURL(CAPS_TARGET_URL_MULTICAST));
  MulticastTarg->registerTarget();
  
  TopLevelWindowTarg = new nsUserTarget("UniversalTopLevelWindow", sysPrin,
                                        targetRiskHigh,
                                        targetRiskColorHigh,
                                        JavaSecUI_getString(CAPS_TARGET_DESC_TOP_LEVEL_WINDOW),
                                        JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_TOP_LEVEL_WINDOW),
                                        JavaSecUI_getHelpURL(CAPS_TARGET_URL_TOP_LEVEL_WINDOW));
  TopLevelWindowTarg->registerTarget();
  
  DialogModalityTarg = new nsUserTarget("UniversalDialogModality", sysPrin,
                                        targetRiskMedium,
                                        targetRiskColorMedium,
                                        JavaSecUI_getString(CAPS_TARGET_DESC_DIALOG_MODALITY),
                                        JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_DIALOG_MODALITY),
                                        JavaSecUI_getHelpURL(CAPS_TARGET_URL_DIALOG_MODALITY));
  DialogModalityTarg->registerTarget();
  
  PackageAccessTarg = new nsTarget("UniversalPackageAccess", sysPrin,
                                   targetRiskHigh,
                                   targetRiskColorHigh,
                                   JavaSecUI_getString(CAPS_TARGET_DESC_PACKAGE_ACCESS),
                                   JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_PACKAGE_ACCESS),
                                   JavaSecUI_getHelpURL(CAPS_TARGET_URL_PACKAGE_ACCESS));
  PackageAccessTarg->registerTarget();
  
  PackageDefinitionTarg = new nsTarget("UniversalPackageDefinition", 
                                       sysPrin,
                                       targetRiskHigh,
                                       targetRiskColorHigh,
                                       JavaSecUI_getString(CAPS_TARGET_DESC_PACKAGE_DEFINITION),
                                       JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_PACKAGE_DEFINITION),
                                       JavaSecUI_getHelpURL(CAPS_TARGET_URL_PACKAGE_DEFINITION));
  PackageDefinitionTarg->registerTarget();
  
  SetFactoryTarg = new nsUserTarget("UniversalSetFactory", 
                                    sysPrin,
                                    targetRiskHigh,
                                    targetRiskColorHigh,
                                    JavaSecUI_getString(CAPS_TARGET_DESC_SET_FACTORY),
                                    JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_SET_FACTORY),
                                    JavaSecUI_getHelpURL(CAPS_TARGET_URL_SET_FACTORY));
  SetFactoryTarg->registerTarget();
  
  MemberAccessTarg = new nsTarget("UniversalMemberAccess", 
                                  sysPrin,
                                  targetRiskHigh,
                                  targetRiskColorHigh,
                                  JavaSecUI_getString(CAPS_TARGET_DESC_MEMBER_ACCESS),
                                  JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_MEMBER_ACCESS),
                                  JavaSecUI_getHelpURL(CAPS_TARGET_URL_MEMBER_ACCESS));
  MemberAccessTarg->registerTarget();
  
  PrintJobAccessTarg = new nsUserTarget("UniversalPrintJobAccess",
                                        sysPrin,
                                        targetRiskLow,
                                        targetRiskColorLow,
                                        JavaSecUI_getString(CAPS_TARGET_DESC_PRINT_JOB_ACCESS),
                                        JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_PRINT_JOB_ACCESS),
                                        JavaSecUI_getHelpURL(CAPS_TARGET_URL_PRINT_JOB_ACCESS));
  PrintJobAccessTarg->registerTarget();
  
  SystemClipboardAccessTarg = new nsUserTarget("UniversalSystemClipboardAccess", 
                                               sysPrin,
                                               targetRiskHigh,
                                               targetRiskColorHigh,
                                               JavaSecUI_getString(CAPS_TARGET_DESC_SYSTEM_CLIPBOARD_ACCESS),
                                               JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_SYSTEM_CLIPBOARD_ACCESS),
                                               JavaSecUI_getHelpURL(CAPS_TARGET_URL_SYSTEM_CLIPBOARD_ACCESS));
  SystemClipboardAccessTarg->registerTarget();
  
  AwtEventQueueAccessTarg = new nsUserTarget("UniversalAwtEventQueueAccess", 
                                             sysPrin,
                                             targetRiskHigh,
                                             targetRiskColorHigh,
                                             JavaSecUI_getString(CAPS_TARGET_DESC_AWT_EVENT_QUEUE_ACCESS),
                                             JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_AWT_EVENT_QUEUE_ACCESS),
                                             JavaSecUI_getHelpURL(CAPS_TARGET_URL_AWT_EVENT_QUEUE_ACCESS));
  AwtEventQueueAccessTarg->registerTarget();
  
  SecurityProviderTarg = new nsTarget("UniversalSecurityProvider",
                                      sysPrin,
                                      targetRiskHigh,
                                      targetRiskColorHigh,
                                      JavaSecUI_getString(CAPS_TARGET_DESC_SECURITY_PROVIDER),
                                      JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_SECURITY_PROVIDER),
                                      JavaSecUI_getHelpURL(CAPS_TARGET_URL_SECURITY_PROVIDER));
  SecurityProviderTarg->registerTarget();
  
  CreateSecurityManagerTarg = new nsTarget("CreateSecurityManager",
                                           sysPrin,
                                           targetRiskHigh,
                                           targetRiskColorHigh,
                                           JavaSecUI_getString(CAPS_TARGET_DESC_CREATE_SECURITY_MANAGER),
                                           JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_CREATE_SECURITY_MANAGER),
                                           JavaSecUI_getHelpURL(CAPS_TARGET_URL_CREATE_SECURITY_MANAGER));
  CreateSecurityManagerTarg->registerTarget();
  
  ImpersonatorTarg = new nsTarget("Impersonator", sysPrin,
                                  targetRiskHigh,
                                  targetRiskColorHigh,
                                  JavaSecUI_getString(CAPS_TARGET_DESC_IMPERSONATOR),
                                  JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_IMPERSONATOR),
                                  JavaSecUI_getHelpURL(CAPS_TARGET_URL_IMPERSONATOR));
  ImpersonatorTarg->registerTarget();
  
  BrowserReadTarg = new nsUserTarget("UniversalBrowserRead", sysPrin,
                                     targetRiskMedium,
                                     targetRiskColorMedium,
                                     JavaSecUI_getString(CAPS_TARGET_DESC_BROWSER_READ),
                                     JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_BROWSER_READ),
                                     JavaSecUI_getHelpURL(CAPS_TARGET_URL_BROWSER_READ));
  BrowserReadTarg->registerTarget();
  
  BrowserWriteTarg = new nsUserTarget("UniversalBrowserWrite", sysPrin,
                                      targetRiskHigh,
                                      targetRiskColorHigh,
                                      JavaSecUI_getString(CAPS_TARGET_DESC_BROWSER_WRITE),
                                      JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_BROWSER_WRITE),
                                      JavaSecUI_getHelpURL(CAPS_TARGET_URL_BROWSER_WRITE));
  BrowserWriteTarg->registerTarget();
  
  UniversalPreferencesReadTarg = new nsUserTarget("UniversalPreferencesRead", 
                                                  sysPrin,
                                                  targetRiskMedium,
                                                  targetRiskColorMedium,
                                                  JavaSecUI_getString(CAPS_TARGET_DESC_PREFS_READ),
                                                  JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_PREFS_READ),
                                                  JavaSecUI_getHelpURL(CAPS_TARGET_URL_PREFS_READ));
  UniversalPreferencesReadTarg->registerTarget();
  
  UniversalPreferencesWriteTarg = new nsUserTarget("UniversalPreferencesWrite", 
                                                   sysPrin,
                                                   targetRiskHigh,
                                                   targetRiskColorHigh,
                                                   JavaSecUI_getString(CAPS_TARGET_DESC_PREFS_WRITE),
                                                   JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_PREFS_WRITE),
                                                   JavaSecUI_getHelpURL(CAPS_TARGET_URL_PREFS_WRITE));
  UniversalPreferencesWriteTarg->registerTarget();
  
  SendMailTarg = new nsUserTarget("UniversalSendMail", sysPrin,
                                  targetRiskMedium,
                                  targetRiskColorMedium,
                                  JavaSecUI_getString(CAPS_TARGET_DESC_SEND_MAIL),
                                  JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_SEND_MAIL),
                                  JavaSecUI_getHelpURL(CAPS_TARGET_URL_SEND_MAIL));
  SendMailTarg->registerTarget();
  
  RegistryPrivateTarg = new nsUserTarget("PrivateRegistryAccess", sysPrin,
                                         targetRiskLow,
                                         targetRiskColorLow,
                                         JavaSecUI_getString(CAPS_TARGET_DESC_REG_PRIVATE),
                                         JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_REG_PRIVATE),
                                         JavaSecUI_getHelpURL(CAPS_TARGET_URL_REG_PRIVATE));
  RegistryPrivateTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(1, 1);
  i = 0;
  targetPtrArray->Set(i++, (void *)RegistryPrivateTarg);
  RegistryStandardTarg = new nsUserTarget("StandardRegistryAccess", sysPrin,
                                          targetRiskMedium,
                                          targetRiskColorMedium,
                                          JavaSecUI_getString(CAPS_TARGET_DESC_REG_STANDARD),
                                          JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_REG_STANDARD),
                                          JavaSecUI_getHelpURL(CAPS_TARGET_URL_REG_STANDARD),
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
                                       JavaSecUI_getString(CAPS_TARGET_DESC_REG_ADMIN),
                                       JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_REG_ADMIN),
                                       JavaSecUI_getHelpURL(CAPS_TARGET_URL_REG_ADMIN),
                                       targetPtrArray);
  RegistryAdminTarg->registerTarget();
  
  UninstallTarg = new nsUserTarget("Uninstall", 
                                    sysPrin,
                                    targetRiskHigh,
			                        targetRiskColorHigh,
                                    JavaSecUI_getString(CAPS_TARGET_DESC_UNINSTALL),
                                    JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_UNINSTALL),
                                    JavaSecUI_getString(CAPS_TARGET_URL_UNINSTALL));
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
                                        JavaSecUI_getString(CAPS_TARGET_DESC_SOFTWAREINSTALL),
                                        JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_SOFTWAREINSTALL),
                                        JavaSecUI_getString(CAPS_TARGET_URL_SOFTWAREINSTALL),
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
                                        JavaSecUI_getString(CAPS_TARGET_DESC_SILENTINSTALL),
                                        JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_SILENTINSTALL),
                                        JavaSecUI_getString(CAPS_TARGET_URL_SILENTINSTALL),
                                        targetPtrArray);
  SilentInstallTarg->registerTarget();

  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(2, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)FdReadTarg);
  targetPtrArray->Set(i++, (void *)FdWriteTarg);
  ConnectTarg = new nsUserTarget("UniversalConnect", sysPrin,
                                 targetRiskHigh,
                                 targetRiskColorHigh,
                                 JavaSecUI_getString(CAPS_TARGET_DESC_CONNECT),
                                 JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_CONNECT),
                                 JavaSecUI_getHelpURL(CAPS_TARGET_URL_CONNECT),
                                 targetPtrArray);
  ConnectTarg->registerTarget();
  
  ClientAuthTarg = new nsUserTarget("ClientAuth", sysPrin,
                                    targetRiskMedium,
                                    targetRiskColorMedium,
                                    JavaSecUI_getString(CAPS_TARGET_DESC_CLIENT_AUTH),
                                    JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_CLIENT_AUTH),
                                    JavaSecUI_getHelpURL(CAPS_TARGET_URL_CLIENT_AUTH));
  ClientAuthTarg->registerTarget();
  
  RedirectTarg = new nsTarget("UniversalRedirect", sysPrin,
                              targetRiskHigh,
                              targetRiskColorHigh,
                              JavaSecUI_getString(CAPS_TARGET_DESC_REDIRECT),
                              JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_REDIRECT),
                              JavaSecUI_getHelpURL(CAPS_TARGET_URL_REDIRECT));
  RedirectTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(2, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)ConnectTarg);
  targetPtrArray->Set(i++, (void *)RedirectTarg);
  ConnectWithRedirectTarg = new nsUserTarget("UniversalConnectWithRedirect", 
                                             sysPrin,
                                             targetRiskHigh,
                                             targetRiskColorHigh,
                                             JavaSecUI_getString(CAPS_TARGET_DESC_CONNECT_WITH_REDIRECT),
                                             JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_CONNECT_WITH_REDIRECT),
                                             JavaSecUI_getHelpURL(CAPS_TARGET_URL_CONNECT_WITH_REDIRECT),
                                             targetPtrArray);
  ConnectWithRedirectTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(2, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)FdReadTarg);
  targetPtrArray->Set(i++, (void *)FdWriteTarg);
  CodebaseEnvTarg = new nsTarget("CodebaseEnvironment", sysPrin,
                                 targetRiskLow,
                                 targetRiskColorLow,
                                 JavaSecUI_getString(CAPS_TARGET_DESC_CODEBASE_ENV),
                                 JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_CODEBASE_ENV),
                                 JavaSecUI_getHelpURL(CAPS_TARGET_URL_CODEBASE_ENV),
                                 targetPtrArray);
  CodebaseEnvTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(31, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)ThreadAccessTarg);
  targetPtrArray->Set(i++, (void *)ThreadGroupAccessTarg);
  targetPtrArray->Set(i++, (void *)ExecAccessTarg);
  targetPtrArray->Set(i++, (void *)ExitAccessTarg);
  targetPtrArray->Set(i++, (void *)LinkAccessTarg);
  targetPtrArray->Set(i++, (void *)PropertyWriteTarg);
  targetPtrArray->Set(i++, (void *)PropertyReadTarg);
  targetPtrArray->Set(i++, (void *)FileReadTarg);
  targetPtrArray->Set(i++, (void *)FileWriteTarg);
  targetPtrArray->Set(i++, (void *)FileDeleteTarg);
  targetPtrArray->Set(i++, (void *)FdReadTarg);
  targetPtrArray->Set(i++, (void *)FdWriteTarg);
  targetPtrArray->Set(i++, (void *)ListenTarg);
  targetPtrArray->Set(i++, (void *)AcceptTarg);
  targetPtrArray->Set(i++, (void *)ConnectTarg);
  targetPtrArray->Set(i++, (void *)MulticastTarg);
  targetPtrArray->Set(i++, (void *)TopLevelWindowTarg);
  targetPtrArray->Set(i++, (void *)PackageAccessTarg);
  targetPtrArray->Set(i++, (void *)PackageDefinitionTarg);
  targetPtrArray->Set(i++, (void *)SetFactoryTarg);
  targetPtrArray->Set(i++, (void *)MemberAccessTarg);
  targetPtrArray->Set(i++, (void *)PrintJobAccessTarg);
  targetPtrArray->Set(i++, (void *)SystemClipboardAccessTarg);
  targetPtrArray->Set(i++, (void *)AwtEventQueueAccessTarg);
  targetPtrArray->Set(i++, (void *)SecurityProviderTarg);
  targetPtrArray->Set(i++, (void *)CreateSecurityManagerTarg);
  targetPtrArray->Set(i++, (void *)ImpersonatorTarg);
  targetPtrArray->Set(i++, (void *)BrowserReadTarg);
  targetPtrArray->Set(i++, (void *)BrowserWriteTarg);
  targetPtrArray->Set(i++, (void *)SendMailTarg);
  targetPtrArray->Set(i++, (void *)CodebaseEnvTarg);
  target = new nsTarget("SuperUser", sysPrin, 
                        targetRiskHigh,
                        targetRiskColorHigh,
                        JavaSecUI_getString(CAPS_TARGET_DESC_SUPER_USER),
                        JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_SUPER_USER),
                        JavaSecUI_getHelpURL(CAPS_TARGET_URL_SUPER_USER), 
                        targetPtrArray);
  target->registerTarget();
  
  //
  // targets used by Constellation group
  //
  // Create a user target that protects cache APIs
  nsTarget *SiteArchiveTarget = new nsUserTarget("SiteArchiveTarget",    
                                                 sysPrin,
                                                 targetRiskHigh,
                                                 targetRiskColorHigh,
                                                 JavaSecUI_getString(CAPS_TARGET_DESC_SAR),
                                                 JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_SAR),
                                                 JavaSecUI_getHelpURL(CAPS_TARGET_URL_SAR));
  SiteArchiveTarget->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(11, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)ThreadAccessTarg);
  targetPtrArray->Set(i++, (void *)ThreadGroupAccessTarg);
  targetPtrArray->Set(i++, (void *)LinkAccessTarg);
  targetPtrArray->Set(i++, (void *)PropertyWriteTarg);
  targetPtrArray->Set(i++, (void *)PropertyReadTarg);
  targetPtrArray->Set(i++, (void *)ListenTarg);
  targetPtrArray->Set(i++, (void *)AcceptTarg);
  targetPtrArray->Set(i++, (void *)ConnectTarg);
  targetPtrArray->Set(i++, (void *)TopLevelWindowTarg);
  targetPtrArray->Set(i++, (void *)PackageAccessTarg);
  targetPtrArray->Set(i++, (void *)PackageDefinitionTarg);
  target = new nsUserTarget("30Capabilities", sysPrin, 
                            targetRiskHigh,
                            targetRiskColorHigh,
                            JavaSecUI_getString(CAPS_TARGET_DESC_30_CAPABILITIES),
                            JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_30_CAPABILITIES),
                            JavaSecUI_getHelpURL(CAPS_TARGET_URL_30_CAPABILITIES), 
                            targetPtrArray);
  target->registerTarget();
  //
  // targets used by Marimba
  //
  
  // access to this target only enables file operations below the
  // root of the castanet channel cache.
  nsTarget *mappTarget = new nsTarget("MarimbaAppContextTarget", sysPrin, 
                                      targetRiskMedium,
                                      targetRiskColorMedium,
                                      JavaSecUI_getString(CAPS_TARGET_DESC_MARIMBA),
                                      JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_MARIMBA),
                                      JavaSecUI_getHelpURL(CAPS_TARGET_URL_MARIMBA));
  mappTarget->registerTarget();
  
  //
  // Internal target used by Marimba code 
  //
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(10, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)FileReadTarg);
  targetPtrArray->Set(i++, (void *)FileWriteTarg);
  targetPtrArray->Set(i++, (void *)FileDeleteTarg);
  targetPtrArray->Set(i++, (void *)PropertyReadTarg);
  targetPtrArray->Set(i++, (void *)ConnectTarg);
  targetPtrArray->Set(i++, (void *)TopLevelWindowTarg);
  targetPtrArray->Set(i++, (void *)PackageAccessTarg);
  targetPtrArray->Set(i++, (void *)ThreadAccessTarg);
  targetPtrArray->Set(i++, (void *)ThreadGroupAccessTarg);
  targetPtrArray->Set(i++, (void *)mappTarget);
  target = new nsUserTarget("MarimbaInternalTarget", sysPrin, 
                            targetRiskHigh,
                            targetRiskColorHigh,
                            JavaSecUI_getString(CAPS_TARGET_DESC_MARIMBA),
                            JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_MARIMBA),
                            JavaSecUI_getHelpURL(CAPS_TARGET_URL_MARIMBA), 
                            targetPtrArray);
  target->registerTarget();
  
  //
  // internal target used by the netscape IIOP runtime
  //
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(4, 1);
  i = 0;
  targetPtrArray->Set(i++, (void *)ListenTarg);
  targetPtrArray->Set(i++, (void *)AcceptTarg);
  targetPtrArray->Set(i++, (void *)ConnectTarg);
  targetPtrArray->Set(i++, (void *)CodebaseEnvTarg);
  nsTarget *iiopTarget = new nsUserTarget("IIOPRuntime", sysPrin,
                                       targetRiskHigh,
                                       targetRiskColorHigh,
                                       JavaSecUI_getString(CAPS_TARGET_DESC_IIOP),
                                       JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_IIOP),
                                       JavaSecUI_getHelpURL(CAPS_TARGET_URL_IIOP),
                                       targetPtrArray);
  iiopTarget->registerTarget();
  
  
  //
  // targets used for internal testing/debugging
  //
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(10, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)ExecAccessTarg);
  targetPtrArray->Set(i++, (void *)PropertyWriteTarg);
  targetPtrArray->Set(i++, (void *)PropertyReadTarg);
  targetPtrArray->Set(i++, (void *)FileReadTarg);
  targetPtrArray->Set(i++, (void *)ListenTarg);
  targetPtrArray->Set(i++, (void *)AcceptTarg);
  targetPtrArray->Set(i++, (void *)ConnectTarg);
  targetPtrArray->Set(i++, (void *)ThreadAccessTarg);
  targetPtrArray->Set(i++, (void *)ThreadGroupAccessTarg);
  targetPtrArray->Set(i++, (void *)SetFactoryTarg);
  userTarg = new nsUserTarget("Debugger", sysPrin,
                              targetRiskHigh,
                              targetRiskColorHigh,
                              JavaSecUI_getString(CAPS_TARGET_DESC_DEBUGGER),
                              JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_DEBUGGER),
                              JavaSecUI_getHelpURL(CAPS_TARGET_URL_DEBUGGER), 
                              targetPtrArray);
  userTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(1, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)BrowserWriteTarg);
  userTarg = new nsUserTarget("CanvasAccess", sysPrin,
                              targetRiskHigh,
                              targetRiskColorHigh,
                              JavaSecUI_getString(CAPS_TARGET_DESC_CANVAS_ACCESS),
                              JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_CANVAS_ACCESS),
                              JavaSecUI_getHelpURL(CAPS_TARGET_URL_CANVAS_ACCESS), 
                              targetPtrArray);
  userTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(5, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)LinkAccessTarg);
  targetPtrArray->Set(i++, (void *)PropertyReadTarg);
  targetPtrArray->Set(i++, (void *)FileReadTarg);
  targetPtrArray->Set(i++, (void *)FileWriteTarg);
  targetPtrArray->Set(i++, (void *)FileDeleteTarg);
  UniversalFileAccessTarg = new nsUserTarget("UniversalFileAccess", 
                                             sysPrin,
                                             targetRiskHigh,
                                             targetRiskColorHigh,
                                             JavaSecUI_getString(CAPS_TARGET_DESC_FILE_ACCESS),
                                             JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_FILE_ACCESS),
                                             JavaSecUI_getHelpURL(CAPS_TARGET_URL_FILE_ACCESS),
                                             targetPtrArray);
  UniversalFileAccessTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(2, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)BrowserReadTarg);
  targetPtrArray->Set(i++, (void *)BrowserWriteTarg);
  UniversalBrowserAccessTarg = new nsUserTarget("UniversalBrowserAccess",
                                                sysPrin,
                                                targetRiskHigh,
                                                targetRiskColorHigh,
                                                JavaSecUI_getString(CAPS_TARGET_DESC_BROWSER_ACCESS),
                                                JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_BROWSER_ACCESS),
                                                JavaSecUI_getHelpURL(CAPS_TARGET_URL_BROWSER_ACCESS),
                                                targetPtrArray);
  UniversalBrowserAccessTarg->registerTarget();
  
  LimitedFileAccessTarg = new nsUserTarget("LimitedFileAccess", sysPrin,
                                         targetRiskLow,
                                         targetRiskColorLow,
                                         JavaSecUI_getString(CAPS_TARGET_DESC_LIMITED_FILE_ACCESS),
                                         JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_LIMITED_FILE_ACCESS),
                                         JavaSecUI_getHelpURL(CAPS_TARGET_URL_LIMITED_FILE_ACCESS));
  LimitedFileAccessTarg->registerTarget();
			
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(1, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)RegistryPrivateTarg);
  userTarg = new nsUserTarget("GamesAccess", sysPrin,
                              targetRiskLow,
                              targetRiskColorLow,
                              JavaSecUI_getString(CAPS_TARGET_DESC_GAMES_ACCESS),
                              JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_GAMES_ACCESS),
                              JavaSecUI_getHelpURL(CAPS_TARGET_URL_GAMES_ACCESS), 
                              targetPtrArray);
  userTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(4, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)UniversalFileAccessTarg);
  targetPtrArray->Set(i++, (void *)RegistryStandardTarg);
  targetPtrArray->Set(i++, (void *)PrintJobAccessTarg);
  targetPtrArray->Set(i++, (void *)SystemClipboardAccessTarg);
  userTarg = new nsUserTarget("WordProcessorAccess", sysPrin,
                              targetRiskHigh,
                              targetRiskColorHigh,
                              JavaSecUI_getString(CAPS_TARGET_DESC_WORD_PROCESSOR_ACCESS),
                              JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_WORD_PROCESSOR_ACCESS),
                              JavaSecUI_getHelpURL(CAPS_TARGET_URL_WORD_PROCESSOR_ACCESS), 
                              targetPtrArray);
  userTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(4, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)UniversalFileAccessTarg);
  targetPtrArray->Set(i++, (void *)RegistryStandardTarg);
  targetPtrArray->Set(i++, (void *)PrintJobAccessTarg);
  targetPtrArray->Set(i++, (void *)SystemClipboardAccessTarg);
  userTarg = new nsUserTarget("SpreadsheetAccess", sysPrin,
                              targetRiskHigh,
                              targetRiskColorHigh,
                              JavaSecUI_getString(CAPS_TARGET_DESC_SPREADSHEET_ACCESS),
                              JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_SPREADSHEET_ACCESS),
                              JavaSecUI_getHelpURL(CAPS_TARGET_URL_SPREADSHEET_ACCESS), 
                              targetPtrArray);
  userTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(4, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)UniversalFileAccessTarg);
  targetPtrArray->Set(i++, (void *)RegistryStandardTarg);
  targetPtrArray->Set(i++, (void *)PrintJobAccessTarg);
  targetPtrArray->Set(i++, (void *)SystemClipboardAccessTarg);
  userTarg = new nsUserTarget("PresentationAccess", sysPrin,
                              targetRiskHigh,
                              targetRiskColorHigh,
                              JavaSecUI_getString(CAPS_TARGET_DESC_PRESENTATION_ACCESS),
                              JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_PRESENTATION_ACCESS),
                              JavaSecUI_getHelpURL(CAPS_TARGET_URL_PRESENTATION_ACCESS), 
                              targetPtrArray);
  userTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(4, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)UniversalFileAccessTarg);
  targetPtrArray->Set(i++, (void *)RegistryStandardTarg);
  targetPtrArray->Set(i++, (void *)PrintJobAccessTarg);
  targetPtrArray->Set(i++, (void *)SystemClipboardAccessTarg);
  userTarg = new nsUserTarget("DatabaseAccess", sysPrin,
                              targetRiskHigh,
                              targetRiskColorHigh,
                              JavaSecUI_getString(CAPS_TARGET_DESC_DATABASE_ACCESS),
                              JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_DATABASE_ACCESS),
                              JavaSecUI_getHelpURL(CAPS_TARGET_URL_DATABASE_ACCESS), 
                              targetPtrArray);
  userTarg->registerTarget();
  
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(7, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)LinkAccessTarg);
  targetPtrArray->Set(i++, (void *)PropertyReadTarg);
  targetPtrArray->Set(i++, (void *)ListenTarg);
  targetPtrArray->Set(i++, (void *)AcceptTarg);
  targetPtrArray->Set(i++, (void *)ConnectTarg);
  targetPtrArray->Set(i++, (void *)PrintJobAccessTarg);
  targetPtrArray->Set(i++, (void *)SystemClipboardAccessTarg);
  userTarg = new nsUserTarget("TerminalEmulator", sysPrin,
                              targetRiskHigh,
                              targetRiskColorHigh,
                              JavaSecUI_getString(CAPS_TARGET_DESC_TERMINAL_EMULATOR),
                              JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_TERMINAL_EMULATOR),
                              JavaSecUI_getHelpURL(CAPS_TARGET_URL_TERMINAL_EMULATOR),
                              targetPtrArray);
  userTarg->registerTarget();
  
  
  // JAR packager Target
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(3, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)UniversalFileAccessTarg);
  targetPtrArray->Set(i++, (void *)RegistryStandardTarg);
  targetPtrArray->Set(i++, (void *)TopLevelWindowTarg);
  userTarg = new nsUserTarget("JARPackager", sysPrin,
                              targetRiskHigh,
                              targetRiskColorHigh,
                              JavaSecUI_getString(CAPS_TARGET_DESC_JAR_PACKAGER),
                              JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_JAR_PACKAGER),
                              JavaSecUI_getHelpURL(CAPS_TARGET_URL_JAR_PACKAGER),
                              targetPtrArray);
  userTarg->registerTarget();
  
  // a macro Target for PE
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(7, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)BrowserReadTarg);
  targetPtrArray->Set(i++, (void *)BrowserWriteTarg);
  targetPtrArray->Set(i++, (void *)UniversalPreferencesReadTarg);
  targetPtrArray->Set(i++, (void *)UniversalPreferencesWriteTarg);
  targetPtrArray->Set(i++, (void *)TopLevelWindowTarg);
  targetPtrArray->Set(i++, (void *)ConnectTarg);
  targetPtrArray->Set(i++, (void *)UniversalFileAccessTarg);
  userTarg = new nsUserTarget("AccountSetup", sysPrin,
                              targetRiskHigh,
                              targetRiskColorHigh,
                              JavaSecUI_getString(CAPS_TARGET_DESC_ACCOUNT_SETUP),
                              JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_ACCOUNT_SETUP),
                              JavaSecUI_getHelpURL(CAPS_TARGET_URL_ACCOUNT_SETUP),
                              targetPtrArray);
  userTarg->registerTarget();
  
  
  // Netcaster Target
  targetPtrArray = new nsTargetArray();
  targetPtrArray->SetSize(12, 1);
  i=0;
  targetPtrArray->Set(i++, (void *)BrowserReadTarg);
  targetPtrArray->Set(i++, (void *)BrowserWriteTarg);
  targetPtrArray->Set(i++, (void *)FileReadTarg);
  targetPtrArray->Set(i++, (void *)FileWriteTarg);
  targetPtrArray->Set(i++, (void *)SiteArchiveTarget);
  targetPtrArray->Set(i++, (void *)UniversalFileAccessTarg);
  targetPtrArray->Set(i++, (void *)UniversalPreferencesReadTarg);
  targetPtrArray->Set(i++, (void *)UniversalPreferencesWriteTarg);
  targetPtrArray->Set(i++, (void *)ConnectWithRedirectTarg);
  targetPtrArray->Set(i++, (void *)ThreadAccessTarg);
  targetPtrArray->Set(i++, (void *)ThreadGroupAccessTarg);
  targetPtrArray->Set(i++, (void *)LinkAccessTarg);
  target = new nsUserTarget("Netcaster", sysPrin, 
                            targetRiskHigh,
                            targetRiskColorHigh,
                            JavaSecUI_getString(CAPS_TARGET_DESC_CONSTELLATION),
                            JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_CONSTELLATION),
                            JavaSecUI_getHelpURL(CAPS_TARGET_URL_CONSTELLATION), 
                            targetPtrArray);
  target->registerTarget();


  /* Permission to All privileges in Java */
  target = new nsUserTarget("AllJavaPermission", sysPrin, 
                            targetRiskHigh,
                            targetRiskColorHigh,
                            JavaSecUI_getString(CAPS_TARGET_DESC_ALL_JAVA_PERMISSION),
                            JavaSecUI_getString(CAPS_TARGET_DETAIL_DESC_ALL_JAVA_PERMISSION),
                            JavaSecUI_getHelpURL(CAPS_TARGET_URL_ALL_JAVA_PERMISSION));
  target->registerTarget();

  return PR_TRUE;
}


//
// 			PUBLIC METHODS 
//

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
  nsTarget * targ = new nsTarget((char *)name, prin);
  return findTarget(targ);
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
  /* XXX: The following needs to be i18n */
  if (itsRisk <= nsRiskType_LowRisk) {
    return "low";
  }
  if (itsRisk <= nsRiskType_MediumRisk) {
    return "medium";
  }
  return "high";
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
  char * prinStr = (itsPrincipal != NULL)? itsPrincipal->toString() : "<none>";
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
    PR_ASSERT(target->itsPrincipal == prin);
    if (target->itsRisk > itsRisk) {
      itsRisk = target->itsRisk;
    }
  }
  //  itsTargetArray = new nsTargetArray();
  //  itsTargetArray->CopyArray(targetArray);

  itsTargetArray = targetArray;
}


static PRBool initialize(void) 
{
  /* XXX:
     We need to implement the static initializer that creates all the 
     system targets
  */

  return PR_TRUE;
}

PRBool nsTarget::theInited = initialize();
