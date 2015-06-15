//
//  main.m
//  XulRunner
//
//  Created by Ted Mielczarek on 2/9/15.
//  Copyright (c) 2015 Mozilla. All rights reserved.
//

#import <UIKit/UIKit.h>

#include "mozilla-config.h"
#define XPCOM_GLUE 1
#include "nsXULAppAPI.h"
#include "nsXPCOMGlue.h"
#include "nsXREAppData.h"
#include "mozilla/AppData.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"

static const nsXREAppData sAppData = {
  sizeof(nsXREAppData),
  nullptr, // directory
  "Mozilla",
  "Browser",
  nullptr,
  "38.0a1",
  "201502090123",
  "browser@mozilla.org",
  nullptr, // copyright
  0,
  nullptr, // xreDirectory
  "38.0a1",
  "*",
  "https://crash-reports.mozilla.com/submit",
  nullptr,
  "Firefox"
};

XRE_GetFileFromPathType XRE_GetFileFromPath;
XRE_CreateAppDataType XRE_CreateAppData;
XRE_FreeAppDataType XRE_FreeAppData;
XRE_mainType XRE_main;

static const nsDynamicFunctionLoad kXULFuncs[] = {
    { "XRE_GetFileFromPath", (NSFuncPtr*) &XRE_GetFileFromPath },
    { "XRE_CreateAppData", (NSFuncPtr*) &XRE_CreateAppData },
    { "XRE_FreeAppData", (NSFuncPtr*) &XRE_FreeAppData },
    { "XRE_main", (NSFuncPtr*) &XRE_main },
    { nullptr, nullptr }
};

const int MAXPATHLEN = 1024;
const char* XPCOM_DLL = "XUL";

int main(int argc, char * argv[]) {
  char exeDir[MAXPATHLEN];
  NSString* bundlePath = [[NSBundle mainBundle] bundlePath];
  strncpy(exeDir, [bundlePath UTF8String], MAXPATHLEN);
  strcat(exeDir, "/Frameworks/");
  strncat(exeDir, XPCOM_DLL, MAXPATHLEN - strlen(exeDir));

  nsresult rv = XPCOMGlueStartup(exeDir);
  if (NS_FAILED(rv)) {
    printf("Couldn't load XPCOM (0x%08x) from %s\n", rv, exeDir);
    return 255;
  }

  rv = XPCOMGlueLoadXULFunctions(kXULFuncs);
  if (NS_FAILED(rv)) {
    printf("Couldn't load XRE functions.\n");
    return 255;
  }

  mozilla::ScopedAppData appData(&sAppData);

  nsCOMPtr<nsIFile> greDir;
  rv = NS_NewNativeLocalFile(nsDependentCString([bundlePath UTF8String]), true,
                             getter_AddRefs(greDir));
  if (NS_FAILED(rv)) {
    printf("Couldn't find the application directory.\n");
    return 255;
  }
    
  nsCOMPtr<nsIFile> appSubdir;
  greDir->Clone(getter_AddRefs(appSubdir));
  greDir->Append(NS_LITERAL_STRING("Frameworks"));
  appSubdir->Append(NS_LITERAL_STRING("browser"));

  mozilla::SetStrongPtr(appData.directory, static_cast<nsIFile*>(appSubdir.get()));
  greDir.forget(&appData.xreDirectory);

  int result = XRE_main(argc, argv, &appData, 0);
  return result;
}
