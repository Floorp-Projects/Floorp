/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GaiaChrome.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsChromeRegistry.h"
#include "nsDirectoryServiceDefs.h"
#include "nsLocalFile.h"
#include "nsXULAppAPI.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/Services.h"
#include "mozilla/FileLocation.h"

#define NS_GAIACHROME_CID \
  { 0x83f8f999, 0x6b87, 0x4dd8, { 0xa0, 0x93, 0x72, 0x0b, 0xfb, 0x67, 0x4d, 0x38 } }

using namespace mozilla;

StaticRefPtr<GaiaChrome> gGaiaChrome;

NS_IMPL_ISUPPORTS(GaiaChrome, nsIGaiaChrome)

GaiaChrome::GaiaChrome()
  : mPackageName(NS_LITERAL_CSTRING("gaia"))
  , mAppsDir(NS_LITERAL_STRING("apps"))
  , mDataRoot(NS_LITERAL_STRING("/data/local"))
  , mSystemRoot(NS_LITERAL_STRING("/system/b2g"))
{
  MOZ_ASSERT(NS_IsMainThread());

  GetProfileDir();
  Register();
}

//virtual
GaiaChrome::~GaiaChrome()
{
}

nsresult
GaiaChrome::GetProfileDir()
{
  nsCOMPtr<nsIFile> profDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(profDir));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = profDir->Clone(getter_AddRefs(mProfDir));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
GaiaChrome::ComputeAppsPath(nsIFile* aPath)
{
#if defined(MOZ_MULET)
  aPath->InitWithFile(mProfDir);
#elif defined(MOZ_WIDGET_GONK)
  nsCOMPtr<nsIFile> locationDetection = new nsLocalFile();
  locationDetection->InitWithPath(mSystemRoot);
  locationDetection->Append(mAppsDir);
  bool appsInSystem = EnsureIsDirectory(locationDetection);
  locationDetection->InitWithPath(mDataRoot);
  locationDetection->Append(mAppsDir);
  bool appsInData = EnsureIsDirectory(locationDetection);

  if (!appsInData && !appsInSystem) {
    printf_stderr("!!! NO root directory with apps found\n");
    MOZ_ASSERT(false);
    return NS_ERROR_UNEXPECTED;
  }

  aPath->InitWithPath(appsInData ? mDataRoot : mSystemRoot);
#else
  return NS_ERROR_UNEXPECTED;
#endif

  aPath->Append(mAppsDir);
  aPath->Append(NS_LITERAL_STRING("."));

  nsresult rv = EnsureValidPath(aPath);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

bool
GaiaChrome::EnsureIsDirectory(nsIFile* aPath)
{
  bool isDir = false;
  aPath->IsDirectory(&isDir);
  return isDir;
}

nsresult
GaiaChrome::EnsureValidPath(nsIFile* appsDir)
{
  // Ensure there is a valid "apps/system" directory
  nsCOMPtr<nsIFile> systemAppDir = new nsLocalFile();
  systemAppDir->InitWithFile(appsDir);
  systemAppDir->Append(NS_LITERAL_STRING("system"));

  bool hasSystemAppDir = EnsureIsDirectory(systemAppDir);
  if (!hasSystemAppDir) {
    nsCString path; appsDir->GetNativePath(path);
    // We don't want to continue if the apps path does not exists ...
    printf_stderr("!!! Gaia chrome package is not a directory: %s\n", path.get());
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
GaiaChrome::Register()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsChromeRegistry::gChromeRegistry != nullptr);

  nsCOMPtr<nsIFile> aPath = new nsLocalFile();
  nsresult rv = ComputeAppsPath(aPath);
  NS_ENSURE_SUCCESS(rv, rv);

  FileLocation appsLocation(aPath);
  nsCString uri;
  appsLocation.GetURIString(uri);

  char* argv[2];
  argv[0] = (char*)mPackageName.get();
  argv[1] = (char*)uri.get();

  nsChromeRegistry::ManifestProcessingContext cx(NS_APP_LOCATION, appsLocation);
  nsChromeRegistry::gChromeRegistry->ManifestContent(cx, 0, argv, 0);

  return NS_OK;
}

already_AddRefed<GaiaChrome>
GaiaChrome::FactoryCreate()
{
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!gGaiaChrome) {
    gGaiaChrome = new GaiaChrome();
    ClearOnShutdown(&gGaiaChrome);
  }

  RefPtr<GaiaChrome> service = gGaiaChrome.get();
  return service.forget();
}

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(GaiaChrome,
                                         GaiaChrome::FactoryCreate)

NS_DEFINE_NAMED_CID(NS_GAIACHROME_CID);

static const mozilla::Module::CIDEntry kGaiaChromeCIDs[] = {
  { &kNS_GAIACHROME_CID, false, nullptr, GaiaChromeConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kGaiaChromeContracts[] = {
  { "@mozilla.org/b2g/gaia-chrome;1", &kNS_GAIACHROME_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kGaiaChromeCategories[] = {
  { "profile-after-change", "Gaia Chrome Registration", GAIACHROME_CONTRACTID },
  { nullptr }
};

static const mozilla::Module kGaiaChromeModule = {
  mozilla::Module::kVersion,
  kGaiaChromeCIDs,
  kGaiaChromeContracts,
  kGaiaChromeCategories
};

NSMODULE_DEFN(GaiaChromeModule) = &kGaiaChromeModule;
