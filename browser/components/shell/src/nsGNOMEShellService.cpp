/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsCOMPtr.h"
#include "nsGNOMEShellService.h"
#include "nsShellService.h"
#include "nsIServiceManager.h"
#include "nsIFile.h"
#include "nsIProperties.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIPrefService.h"
#include "prenv.h"
#include "nsStringAPI.h"
#include "nsIGConfService.h"
#include "nsIGIOService.h"
#include "nsIGSettingsService.h"
#include "nsIStringBundle.h"
#include "nsIOutputStream.h"
#include "nsIProcess.h"
#include "nsNetUtil.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIImageLoadingContent.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "prprf.h"
#if defined(MOZ_WIDGET_GTK)
#include "nsIImageToPixbuf.h"
#endif
#include "nsXULAppAPI.h"

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <limits.h>
#include <stdlib.h>

using namespace mozilla;

struct ProtocolAssociation
{
  const char *name;
  bool essential;
};

struct MimeTypeAssociation
{
  const char *mimeType;
  const char *extensions;
};

static const ProtocolAssociation appProtocols[] = {
  { "http",   true     },
  { "https",  true     },
  { "ftp",    false },
  { "chrome", false }
};

static const MimeTypeAssociation appTypes[] = {
  { "text/html",             "htm html shtml" },
  { "application/xhtml+xml", "xhtml xht"      }
};

// GConf registry key constants
#define DG_BACKGROUND "/desktop/gnome/background"

static const char kDesktopImageKey[] = DG_BACKGROUND "/picture_filename";
static const char kDesktopOptionsKey[] = DG_BACKGROUND "/picture_options";
static const char kDesktopDrawBGKey[] = DG_BACKGROUND "/draw_background";
static const char kDesktopColorKey[] = DG_BACKGROUND "/primary_color";

static const char kDesktopBGSchema[] = "org.gnome.desktop.background";
static const char kDesktopImageGSKey[] = "picture-uri";
static const char kDesktopOptionGSKey[] = "picture-options";
static const char kDesktopDrawBGGSKey[] = "draw-background";
static const char kDesktopColorGSKey[] = "primary-color";

nsresult
nsGNOMEShellService::Init()
{
  nsresult rv;

  // GConf, GSettings or GIO _must_ be available, or we do not allow
  // CreateInstance to succeed.

  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);
  nsCOMPtr<nsIGIOService> giovfs =
    do_GetService(NS_GIOSERVICE_CONTRACTID);
  nsCOMPtr<nsIGSettingsService> gsettings =
    do_GetService(NS_GSETTINGSSERVICE_CONTRACTID);

  if (!gconf && !giovfs && !gsettings)
    return NS_ERROR_NOT_AVAILABLE;

  // Check G_BROKEN_FILENAMES.  If it's set, then filenames in glib use
  // the locale encoding.  If it's not set, they use UTF-8.
  mUseLocaleFilenames = PR_GetEnv("G_BROKEN_FILENAMES") != nullptr;

  if (GetAppPathFromLauncher())
    return NS_OK;

  nsCOMPtr<nsIProperties> dirSvc
    (do_GetService("@mozilla.org/file/directory_service;1"));
  NS_ENSURE_TRUE(dirSvc, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIFile> appPath;
  rv = dirSvc->Get(XRE_EXECUTABLE_FILE, NS_GET_IID(nsIFile),
                   getter_AddRefs(appPath));
  NS_ENSURE_SUCCESS(rv, rv);

  return appPath->GetNativePath(mAppPath);
}

NS_IMPL_ISUPPORTS(nsGNOMEShellService, nsIShellService)

bool
nsGNOMEShellService::GetAppPathFromLauncher()
{
  gchar *tmp;

  const char *launcher = PR_GetEnv("MOZ_APP_LAUNCHER");
  if (!launcher)
    return false;

  if (g_path_is_absolute(launcher)) {
    mAppPath = launcher;
    tmp = g_path_get_basename(launcher);
    gchar *fullpath = g_find_program_in_path(tmp);
    if (fullpath && mAppPath.Equals(fullpath))
      mAppIsInPath = true;
    g_free(fullpath);
  } else {
    tmp = g_find_program_in_path(launcher);
    if (!tmp)
      return false;
    mAppPath = tmp;
    mAppIsInPath = true;
  }

  g_free(tmp);
  return true;
}

bool
nsGNOMEShellService::KeyMatchesAppName(const char *aKeyValue) const
{

  gchar *commandPath;
  if (mUseLocaleFilenames) {
    gchar *nativePath = g_filename_from_utf8(aKeyValue, -1,
                                             nullptr, nullptr, nullptr);
    if (!nativePath) {
      NS_ERROR("Error converting path to filesystem encoding");
      return false;
    }

    commandPath = g_find_program_in_path(nativePath);
    g_free(nativePath);
  } else {
    commandPath = g_find_program_in_path(aKeyValue);
  }

  if (!commandPath)
    return false;

  bool matches = mAppPath.Equals(commandPath);
  g_free(commandPath);
  return matches;
}

bool
nsGNOMEShellService::CheckHandlerMatchesAppName(const nsACString &handler) const
{
  gint argc;
  gchar **argv;
  nsAutoCString command(handler);

  // The string will be something of the form: [/path/to/]browser "%s"
  // We want to remove all of the parameters and get just the binary name.

  if (g_shell_parse_argv(command.get(), &argc, &argv, nullptr) && argc > 0) {
    command.Assign(argv[0]);
    g_strfreev(argv);
  }

  if (!KeyMatchesAppName(command.get()))
    return false; // the handler is set to another app

  return true;
}

NS_IMETHODIMP
nsGNOMEShellService::IsDefaultBrowser(bool aStartupCheck,
                                      bool aForAllTypes,
                                      bool* aIsDefaultBrowser)
{
  *aIsDefaultBrowser = false;
  if (aStartupCheck)
    mCheckedThisSession = true;

  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);
  nsCOMPtr<nsIGIOService> giovfs = do_GetService(NS_GIOSERVICE_CONTRACTID);

  bool enabled;
  nsAutoCString handler;
  nsCOMPtr<nsIGIOMimeApp> gioApp;

  for (unsigned int i = 0; i < ArrayLength(appProtocols); ++i) {
    if (!appProtocols[i].essential)
      continue;

    if (gconf) {
      handler.Truncate();
      gconf->GetAppForProtocol(nsDependentCString(appProtocols[i].name),
                               &enabled, handler);

      if (!CheckHandlerMatchesAppName(handler) || !enabled)
        return NS_OK; // the handler is disabled or set to another app
    }

    if (giovfs) {
      handler.Truncate();
      giovfs->GetAppForURIScheme(nsDependentCString(appProtocols[i].name),
                                 getter_AddRefs(gioApp));
      if (!gioApp)
        return NS_OK;

      gioApp->GetCommand(handler);

      if (!CheckHandlerMatchesAppName(handler))
        return NS_OK; // the handler is set to another app
    }
  }

  *aIsDefaultBrowser = true;

  return NS_OK;
}

NS_IMETHODIMP
nsGNOMEShellService::SetDefaultBrowser(bool aClaimAllTypes,
                                       bool aForAllUsers)
{
#ifdef DEBUG
  if (aForAllUsers)
    NS_WARNING("Setting the default browser for all users is not yet supported");
#endif

  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);
  nsCOMPtr<nsIGIOService> giovfs = do_GetService(NS_GIOSERVICE_CONTRACTID);
  if (gconf) {
    nsAutoCString appKeyValue;
    if (mAppIsInPath) {
      // mAppPath is in the users path, so use only the basename as the launcher
      gchar *tmp = g_path_get_basename(mAppPath.get());
      appKeyValue = tmp;
      g_free(tmp);
    } else {
      appKeyValue = mAppPath;
    }

    appKeyValue.AppendLiteral(" %s");

    for (unsigned int i = 0; i < ArrayLength(appProtocols); ++i) {
      if (appProtocols[i].essential || aClaimAllTypes) {
        gconf->SetAppForProtocol(nsDependentCString(appProtocols[i].name),
                                 appKeyValue);
      }
    }
  }

  if (giovfs) {
    nsresult rv;
    nsCOMPtr<nsIStringBundleService> bundleService =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStringBundle> brandBundle;
    rv = bundleService->CreateBundle(BRAND_PROPERTIES, getter_AddRefs(brandBundle));
    NS_ENSURE_SUCCESS(rv, rv);

    nsString brandShortName;
    brandBundle->GetStringFromName(MOZ_UTF16("brandShortName"),
                                   getter_Copies(brandShortName));

    // use brandShortName as the application id.
    NS_ConvertUTF16toUTF8 id(brandShortName);
    nsCOMPtr<nsIGIOMimeApp> appInfo;
    rv = giovfs->CreateAppFromCommand(mAppPath,
                                      id,
                                      getter_AddRefs(appInfo));
    NS_ENSURE_SUCCESS(rv, rv);

    // set handler for the protocols
    for (unsigned int i = 0; i < ArrayLength(appProtocols); ++i) {
      if (appProtocols[i].essential || aClaimAllTypes) {
        appInfo->SetAsDefaultForURIScheme(nsDependentCString(appProtocols[i].name));
      }
    }

    // set handler for .html and xhtml files and MIME types:
    if (aClaimAllTypes) {
      // Add mime types for html, xhtml extension and set app to just created appinfo.
      for (unsigned int i = 0; i < ArrayLength(appTypes); ++i) {
        appInfo->SetAsDefaultForMimeType(nsDependentCString(appTypes[i].mimeType));
        appInfo->SetAsDefaultForFileExtensions(nsDependentCString(appTypes[i].extensions));
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGNOMEShellService::GetShouldCheckDefaultBrowser(bool* aResult)
{
  // If we've already checked, the browser has been started and this is a 
  // new window open, and we don't want to check again.
  if (mCheckedThisSession) {
    *aResult = false;
    return NS_OK;
  }

  nsCOMPtr<nsIPrefBranch> prefs;
  nsCOMPtr<nsIPrefService> pserve(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (pserve)
    pserve->GetBranch("", getter_AddRefs(prefs));

  if (prefs)
    prefs->GetBoolPref(PREF_CHECKDEFAULTBROWSER, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsGNOMEShellService::SetShouldCheckDefaultBrowser(bool aShouldCheck)
{
  nsCOMPtr<nsIPrefBranch> prefs;
  nsCOMPtr<nsIPrefService> pserve(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (pserve)
    pserve->GetBranch("", getter_AddRefs(prefs));

  if (prefs)
    prefs->SetBoolPref(PREF_CHECKDEFAULTBROWSER, aShouldCheck);

  return NS_OK;
}

NS_IMETHODIMP
nsGNOMEShellService::GetCanSetDesktopBackground(bool* aResult)
{
  // setting desktop background is currently only supported
  // for Gnome or desktops using the same GSettings and GConf keys
  const char* gnomeSession = getenv("GNOME_DESKTOP_SESSION_ID");
  if (gnomeSession) {
    *aResult = true;
  } else {
    *aResult = false;
  }

  return NS_OK;
}

static nsresult
WriteImage(const nsCString& aPath, imgIContainer* aImage)
{
#if !defined(MOZ_WIDGET_GTK)
  return NS_ERROR_NOT_AVAILABLE;
#else
  nsCOMPtr<nsIImageToPixbuf> imgToPixbuf =
    do_GetService("@mozilla.org/widget/image-to-gdk-pixbuf;1");
  if (!imgToPixbuf)
      return NS_ERROR_NOT_AVAILABLE;

  GdkPixbuf* pixbuf = imgToPixbuf->ConvertImageToPixbuf(aImage);
  if (!pixbuf)
      return NS_ERROR_NOT_AVAILABLE;

  gboolean res = gdk_pixbuf_save(pixbuf, aPath.get(), "png", nullptr, nullptr);

  g_object_unref(pixbuf);
  return res ? NS_OK : NS_ERROR_FAILURE;
#endif
}
                 
NS_IMETHODIMP
nsGNOMEShellService::SetDesktopBackground(nsIDOMElement* aElement, 
                                          int32_t aPosition)
{
  nsresult rv;
  nsCOMPtr<nsIImageLoadingContent> imageContent = do_QueryInterface(aElement, &rv);
  if (!imageContent) return rv;

  // get the image container
  nsCOMPtr<imgIRequest> request;
  rv = imageContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                                getter_AddRefs(request));
  if (!request) return rv;
  nsCOMPtr<imgIContainer> container;
  rv = request->GetImage(getter_AddRefs(container));
  if (!container) return rv;

  // Set desktop wallpaper filling style
  nsAutoCString options;
  if (aPosition == BACKGROUND_TILE)
    options.AssignLiteral("wallpaper");
  else if (aPosition == BACKGROUND_STRETCH)
    options.AssignLiteral("stretched");
  else if (aPosition == BACKGROUND_FILL)
    options.AssignLiteral("zoom");
  else if (aPosition == BACKGROUND_FIT)
    options.AssignLiteral("scaled");
  else
    options.AssignLiteral("centered");

  // Write the background file to the home directory.
  nsAutoCString filePath(PR_GetEnv("HOME"));

  // get the product brand name from localized strings
  nsString brandName;
  nsCID bundleCID = NS_STRINGBUNDLESERVICE_CID;
  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(bundleCID));
  if (bundleService) {
    nsCOMPtr<nsIStringBundle> brandBundle;
    rv = bundleService->CreateBundle(BRAND_PROPERTIES,
                                     getter_AddRefs(brandBundle));
    if (NS_SUCCEEDED(rv) && brandBundle) {
      rv = brandBundle->GetStringFromName(MOZ_UTF16("brandShortName"),
                                          getter_Copies(brandName));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // build the file name
  filePath.Append('/');
  filePath.Append(NS_ConvertUTF16toUTF8(brandName));
  filePath.AppendLiteral("_wallpaper.png");

  // write the image to a file in the home dir
  rv = WriteImage(filePath, container);
  NS_ENSURE_SUCCESS(rv, rv);

  // Try GSettings first. If we don't have GSettings or the right schema, fall back
  // to using GConf instead. Note that if GSettings works ok, the changes get
  // mirrored to GConf by the gsettings->gconf bridge in gnome-settings-daemon
  nsCOMPtr<nsIGSettingsService> gsettings = 
    do_GetService(NS_GSETTINGSSERVICE_CONTRACTID);
  if (gsettings) {
    nsCOMPtr<nsIGSettingsCollection> background_settings;
    gsettings->GetCollectionForSchema(
      NS_LITERAL_CSTRING(kDesktopBGSchema), getter_AddRefs(background_settings));
    if (background_settings) {
      gchar *file_uri = g_filename_to_uri(filePath.get(), nullptr, nullptr);
      if (!file_uri)
         return NS_ERROR_FAILURE;

      background_settings->SetString(NS_LITERAL_CSTRING(kDesktopOptionGSKey),
                                     options);

      background_settings->SetString(NS_LITERAL_CSTRING(kDesktopImageGSKey),
                                     nsDependentCString(file_uri));
      g_free(file_uri);
      background_settings->SetBoolean(NS_LITERAL_CSTRING(kDesktopDrawBGGSKey),
                                      true);
      return rv;
    }
  }

  // if the file was written successfully, set it as the system wallpaper
  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);

  if (gconf) {
    gconf->SetString(NS_LITERAL_CSTRING(kDesktopOptionsKey), options);

    // Set the image to an empty string first to force a refresh
    // (since we could be writing a new image on top of an existing
    // Firefox_wallpaper.png and nautilus doesn't monitor the file for changes)
    gconf->SetString(NS_LITERAL_CSTRING(kDesktopImageKey),
                     EmptyCString());

    gconf->SetString(NS_LITERAL_CSTRING(kDesktopImageKey), filePath);
    gconf->SetBool(NS_LITERAL_CSTRING(kDesktopDrawBGKey), true);
  }

  return rv;
}

#define COLOR_16_TO_8_BIT(_c) ((_c) >> 8)
#define COLOR_8_TO_16_BIT(_c) ((_c) << 8 | (_c))

NS_IMETHODIMP
nsGNOMEShellService::GetDesktopBackgroundColor(uint32_t *aColor)
{
  nsCOMPtr<nsIGSettingsService> gsettings = 
    do_GetService(NS_GSETTINGSSERVICE_CONTRACTID);
  nsCOMPtr<nsIGSettingsCollection> background_settings;
  nsAutoCString background;

  if (gsettings) {
    gsettings->GetCollectionForSchema(
      NS_LITERAL_CSTRING(kDesktopBGSchema), getter_AddRefs(background_settings));
    if (background_settings) {
      background_settings->GetString(NS_LITERAL_CSTRING(kDesktopColorGSKey),
                                     background);
    }
  }

  if (!background_settings) {
    nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);
    if (gconf)
      gconf->GetString(NS_LITERAL_CSTRING(kDesktopColorKey), background);
  }

  if (background.IsEmpty()) {
    *aColor = 0;
    return NS_OK;
  }

  GdkColor color;
  gboolean success = gdk_color_parse(background.get(), &color);

  NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);

  *aColor = COLOR_16_TO_8_BIT(color.red) << 16 |
            COLOR_16_TO_8_BIT(color.green) << 8 |
            COLOR_16_TO_8_BIT(color.blue);
  return NS_OK;
}

static void
ColorToCString(uint32_t aColor, nsCString& aResult)
{
  // The #rrrrggggbbbb format is used to match gdk_color_to_string()
  char *buf = aResult.BeginWriting(13);
  if (!buf)
    return;

  uint16_t red = COLOR_8_TO_16_BIT((aColor >> 16) & 0xff);
  uint16_t green = COLOR_8_TO_16_BIT((aColor >> 8) & 0xff);
  uint16_t blue = COLOR_8_TO_16_BIT(aColor & 0xff);

  PR_snprintf(buf, 14, "#%04x%04x%04x", red, green, blue);
}

NS_IMETHODIMP
nsGNOMEShellService::SetDesktopBackgroundColor(uint32_t aColor)
{
  NS_ASSERTION(aColor <= 0xffffff, "aColor has extra bits");
  nsAutoCString colorString;
  ColorToCString(aColor, colorString);

  nsCOMPtr<nsIGSettingsService> gsettings =
    do_GetService(NS_GSETTINGSSERVICE_CONTRACTID);
  if (gsettings) {
    nsCOMPtr<nsIGSettingsCollection> background_settings;
    gsettings->GetCollectionForSchema(
      NS_LITERAL_CSTRING(kDesktopBGSchema), getter_AddRefs(background_settings));
    if (background_settings) {
      background_settings->SetString(NS_LITERAL_CSTRING(kDesktopColorGSKey),
                                     colorString);
      return NS_OK;
    }
  }

  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);

  if (gconf) {
    gconf->SetString(NS_LITERAL_CSTRING(kDesktopColorKey), colorString);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGNOMEShellService::OpenApplication(int32_t aApplication)
{
  nsAutoCString scheme;
  if (aApplication == APPLICATION_MAIL)
    scheme.AssignLiteral("mailto");
  else if (aApplication == APPLICATION_NEWS)
    scheme.AssignLiteral("news");
  else
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIGIOService> giovfs = do_GetService(NS_GIOSERVICE_CONTRACTID);
  if (giovfs) {
    nsCOMPtr<nsIGIOMimeApp> gioApp;
    giovfs->GetAppForURIScheme(scheme, getter_AddRefs(gioApp));
    if (gioApp)
      return gioApp->Launch(EmptyCString());
  }

  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);
  if (!gconf)
    return NS_ERROR_FAILURE;

  bool enabled;
  nsAutoCString appCommand;
  gconf->GetAppForProtocol(scheme, &enabled, appCommand);

  if (!enabled)
    return NS_ERROR_FAILURE;

  // XXX we don't currently handle launching a terminal window.
  // If the handler requires a terminal, bail.
  bool requiresTerminal;
  gconf->HandlerRequiresTerminal(scheme, &requiresTerminal);
  if (requiresTerminal)
    return NS_ERROR_FAILURE;

  // Perform shell argument expansion
  int argc;
  char **argv;
  if (!g_shell_parse_argv(appCommand.get(), &argc, &argv, nullptr))
    return NS_ERROR_FAILURE;

  char **newArgv = new char*[argc + 1];
  int newArgc = 0;

  // Run through the list of arguments.  Copy all of them to the new
  // argv except for %s, which we skip.
  for (int i = 0; i < argc; ++i) {
    if (strcmp(argv[i], "%s") != 0)
      newArgv[newArgc++] = argv[i];
  }

  newArgv[newArgc] = nullptr;

  gboolean err = g_spawn_async(nullptr, newArgv, nullptr, G_SPAWN_SEARCH_PATH,
                               nullptr, nullptr, nullptr, nullptr);

  g_strfreev(argv);
  delete[] newArgv;

  return err ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGNOMEShellService::OpenApplicationWithURI(nsIFile* aApplication, const nsACString& aURI)
{
  nsresult rv;
  nsCOMPtr<nsIProcess> process = 
    do_CreateInstance("@mozilla.org/process/util;1", &rv);
  if (NS_FAILED(rv))
    return rv;
  
  rv = process->Init(aApplication);
  if (NS_FAILED(rv))
    return rv;

  const nsCString spec(aURI);
  const char* specStr = spec.get();
  return process->Run(false, &specStr, 1);
}

NS_IMETHODIMP
nsGNOMEShellService::GetDefaultFeedReader(nsIFile** _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
