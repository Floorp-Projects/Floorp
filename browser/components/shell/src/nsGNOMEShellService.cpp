/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Shell Service.
 *
 * The Initial Developer of the Original Code is mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCOMPtr.h"
#include "nsGNOMEShellService.h"
#include "nsShellService.h"
#include "nsIServiceManager.h"
#include "nsIPrefService.h"
#include "nsICmdLineService.h"
#include "prenv.h"
#include "nsString.h"
#include "nsIGConfService.h"
#include "nsIGnomeVFSService.h"
#include "nsIStringBundle.h"
#include "gfxIImageFrame.h"
#include "nsIOutputStream.h"
#include "nsNetUtil.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIImageLoadingContent.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "nsColor.h"

#include <glib.h>
#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <limits.h>
#include <stdlib.h>

struct ProtocolAssociation
{
  const char *name;
  PRBool essential;
};

struct MimeTypeAssociation
{
  const char *mimeType;
  const char *extensions;
};

static const ProtocolAssociation appProtocols[] = {
  { "http",   PR_TRUE  },
  { "https",  PR_TRUE  },
  { "ftp",    PR_FALSE },
  { "gopher", PR_FALSE },
  { "chrome", PR_FALSE }
};

static const MimeTypeAssociation appTypes[] = {
  { "text/html",             "htm html shtml" },
  { "application/xhtml+xml", "xhtml xht"      }
};

static const char kDocumentIconPath[] = "firefox-document.png";

// GConf registry key constants
#define DG_BACKGROUND "/desktop/gnome/background"

static const char kDesktopImageKey[] = DG_BACKGROUND "/picture_filename";
static const char kDesktopOptionsKey[] = DG_BACKGROUND "/picture_options";
static const char kDesktopDrawBGKey[] = DG_BACKGROUND "/draw_background";
static const char kDesktopColorKey[] = DG_BACKGROUND "/primary_color";

nsresult
nsGNOMEShellService::Init()
{
  // GConf and GnomeVFS _must_ be available, or we do not allow
  // CreateInstance to succeed.

  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);
  nsCOMPtr<nsIGnomeVFSService> vfs =
    do_GetService(NS_GNOMEVFSSERVICE_CONTRACTID);

  if (!gconf || !vfs)
    return NS_ERROR_NOT_AVAILABLE;

  // Check G_BROKEN_FILENAMES.  If it's set, then filenames in glib use
  // the locale encoding.  If it's not set, they use UTF-8.
  mUseLocaleFilenames = PR_GetEnv("G_BROKEN_FILENAMES") != nsnull;

  // Get the path we were launched from.
  nsCOMPtr<nsICmdLineService> cmdService =
    do_GetService("@mozilla.org/appshell/commandLineService;1");
  if (!cmdService)
    return NS_ERROR_NOT_AVAILABLE;

  nsXPIDLCString programName;
  cmdService->GetProgramName(getter_Copies(programName));

  // Make sure we have an absolute pathname.
  if (programName[0] != '/') {
    // First search PATH if we were just launched as 'firefox-bin'.
    // If we were launched as './firefox-bin', this will just return
    // the original string.

    gchar *appPath = g_find_program_in_path(programName.get());

    // Now resolve it.
    char resolvedPath[PATH_MAX] = "";
    if (realpath(appPath, resolvedPath)) {
      mAppPath.Assign(resolvedPath);
    }

    g_free(appPath);
  } else {
    mAppPath.Assign(programName);
  }

  // strip "-bin" off of the binary name
  if (StringEndsWith(mAppPath, NS_LITERAL_CSTRING("-bin")))
    mAppPath.SetLength(mAppPath.Length() - 4);

  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsGNOMEShellService, nsIShellService)

PRBool
nsGNOMEShellService::KeyMatchesAppName(const char *aKeyValue) const
{

  gchar *commandPath;
  if (mUseLocaleFilenames) {
    gchar *nativePath = g_filename_from_utf8(aKeyValue, -1, NULL, NULL, NULL);
    if (!nativePath) {
      NS_ERROR("Error converting path to filesystem encoding");
      return PR_FALSE;
    }

    commandPath = g_find_program_in_path(nativePath);
    g_free(nativePath);
  } else {
    commandPath = g_find_program_in_path(aKeyValue);
  }

  if (!commandPath)
    return PR_FALSE;

  PRBool matches = mAppPath.Equals(commandPath);
  g_free(commandPath);
  return matches;
}

NS_IMETHODIMP
nsGNOMEShellService::IsDefaultBrowser(PRBool aStartupCheck,
                                      PRBool* aIsDefaultBrowser)
{
  *aIsDefaultBrowser = PR_FALSE;
  if (aStartupCheck)
    mCheckedThisSession = PR_TRUE;

  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);

  PRBool enabled;
  nsCAutoString handler;

  for (unsigned int i = 0; i < NS_ARRAY_LENGTH(appProtocols); ++i) {
    if (!appProtocols[i].essential)
      continue;

    handler.Truncate();
    gconf->GetAppForProtocol(nsDependentCString(appProtocols[i].name),
                             &enabled, handler);

    // The string will be something of the form: [/path/to/]browser "%s"
    // We want to remove all of the parameters and get just the binary name.

    gint argc;
    gchar **argv;

    if (g_shell_parse_argv(handler.get(), &argc, &argv, NULL) && argc > 0) {
      handler.Assign(argv[0]);
      g_strfreev(argv);
    }

    if (!KeyMatchesAppName(handler.get()) || !enabled)
      return NS_OK; // the handler is disabled or set to another app
  }

  *aIsDefaultBrowser = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
nsGNOMEShellService::SetDefaultBrowser(PRBool aClaimAllTypes,
                                       PRBool aForAllUsers)
{
#ifdef DEBUG
  if (aForAllUsers)
    NS_WARNING("Setting the default browser for all users is not yet supported");
#endif

  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);

  nsCAutoString schemeList;
  nsCAutoString appKeyValue(mAppPath + NS_LITERAL_CSTRING(" \"%s\""));
  unsigned int i;

  for (i = 0; i < NS_ARRAY_LENGTH(appProtocols); ++i) {
    schemeList.Append(nsDependentCString(appProtocols[i].name)
                      + NS_LITERAL_CSTRING(","));

    if (appProtocols[i].essential || aClaimAllTypes) {
      gconf->SetAppForProtocol(nsDependentCString(appProtocols[i].name),
                               appKeyValue);
    }
  }

  if (aClaimAllTypes) {
    nsCOMPtr<nsIGnomeVFSService> vfs =
      do_GetService(NS_GNOMEVFSSERVICE_CONTRACTID);

    nsCOMPtr<nsIStringBundleService> bundleService =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    NS_ENSURE_TRUE(bundleService, NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<nsIStringBundle> brandBundle;
    bundleService->CreateBundle(BRAND_PROPERTIES, getter_AddRefs(brandBundle));
    NS_ENSURE_TRUE(brandBundle, NS_ERROR_FAILURE);

    nsXPIDLString brandShortName, brandFullName;
    brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                   getter_Copies(brandShortName));
    brandBundle->GetStringFromName(NS_LITERAL_STRING("brandFullName").get(),
                                   getter_Copies(brandFullName));

    // use brandShortName as the application id.
    NS_ConvertUTF16toUTF8 id(brandShortName);

    vfs->SetAppStringKey(id, nsIGnomeVFSService::APP_KEY_COMMAND, mAppPath);
    vfs->SetAppStringKey(id, nsIGnomeVFSService::APP_KEY_NAME,
                         NS_ConvertUTF16toUTF8(brandFullName));

    // We don't want to be the default handler for "file:", but we do
    // want Nautilus to know that we support file: if the MIME type is
    // one that we can handle.

    schemeList.Append("file");

    vfs->SetAppStringKey(id, nsIGnomeVFSService::APP_KEY_SUPPORTED_URI_SCHEMES,
                         schemeList);

    vfs->SetAppStringKey(id, nsIGnomeVFSService::APP_KEY_EXPECTS_URIS,
                         NS_LITERAL_CSTRING("true"));

    vfs->SetAppBoolKey(id, nsIGnomeVFSService::APP_KEY_CAN_OPEN_MULTIPLE,
                       PR_FALSE);

    vfs->SetAppBoolKey(id, nsIGnomeVFSService::APP_KEY_REQUIRES_TERMINAL,
                       PR_FALSE);

    // Copy icons/document.png to ~/.icons/firefox-document.png
    nsCAutoString iconFilePath(mAppPath);
    PRInt32 lastSlash = iconFilePath.RFindChar(PRUnichar('/'));
    if (lastSlash == -1) {
      NS_ERROR("no slash in executable path?");
    } else {
      iconFilePath.Truncate(lastSlash);
      nsCOMPtr<nsILocalFile> iconFile;
      NS_NewNativeLocalFile(iconFilePath, PR_FALSE, getter_AddRefs(iconFile));
      if (iconFile) {
        iconFile->AppendRelativeNativePath(NS_LITERAL_CSTRING("icons/document.png"));

        nsCOMPtr<nsILocalFile> userIconPath;
        NS_NewNativeLocalFile(nsDependentCString(PR_GetEnv("HOME")), PR_FALSE,
                              getter_AddRefs(userIconPath));
        if (userIconPath) {
          userIconPath->AppendNative(NS_LITERAL_CSTRING(".icons"));
          iconFile->CopyToNative(userIconPath,
                                 nsDependentCString(kDocumentIconPath));
        }
      }
    }

    for (i = 0; i < NS_ARRAY_LENGTH(appTypes); ++i) {
      vfs->AddMimeType(id, nsDependentCString(appTypes[i].mimeType));
      vfs->SetMimeExtensions(nsDependentCString(appTypes[i].mimeType),
                             nsDependentCString(appTypes[i].extensions));
      vfs->SetAppForMimeType(nsDependentCString(appTypes[i].mimeType), id);
      vfs->SetIconForMimeType(nsDependentCString(appTypes[i].mimeType),
                              NS_LITERAL_CSTRING(kDocumentIconPath));
    }

    vfs->SyncAppRegistry();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGNOMEShellService::GetShouldCheckDefaultBrowser(PRBool* aResult)
{
  // If we've already checked, the browser has been started and this is a 
  // new window open, and we don't want to check again.
  if (mCheckedThisSession) {
    *aResult = PR_FALSE;
    return NS_OK;
  }

  nsCOMPtr<nsIPrefBranch> prefs;
  nsCOMPtr<nsIPrefService> pserve(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (pserve)
    pserve->GetBranch("", getter_AddRefs(prefs));

  prefs->GetBoolPref(PREF_CHECKDEFAULTBROWSER, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsGNOMEShellService::SetShouldCheckDefaultBrowser(PRBool aShouldCheck)
{
  nsCOMPtr<nsIPrefBranch> prefs;
  nsCOMPtr<nsIPrefService> pserve(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (pserve)
    pserve->GetBranch("", getter_AddRefs(prefs));

  prefs->SetBoolPref(PREF_CHECKDEFAULTBROWSER, aShouldCheck);

  return NS_OK;
}

static nsresult
WriteImage(const nsCString& aPath, gfxIImageFrame* aImage)
{
  PRInt32 width, height;
  aImage->GetWidth(&width);
  aImage->GetHeight(&height);

  PRInt32 format;
  aImage->GetFormat(&format);

  aImage->LockImageData();

  PRUint32 bytesPerRow;
  aImage->GetImageBytesPerRow(&bytesPerRow);

  PRUint32 bpp = bytesPerRow / width * 8;

  // XXX If bpp is not 24, we will need to do something else, like
  // allocate a new pixbuf and copy the data in ourselves.
  if (bpp != 24)
    return NS_ERROR_FAILURE;

  PRUint8 *bits;
  PRUint32 length;

  aImage->GetImageData(&bits, &length);
  if (!bits) return NS_ERROR_FAILURE;

  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data(bits,
                                               GDK_COLORSPACE_RGB,
                                               PR_FALSE,
                                               8,
                                               width,
                                               height,
                                               bytesPerRow,
                                               NULL,
                                               NULL);

  GdkPixbuf *alphaPixbuf = nsnull;

  if (format == gfxIFormats::RGB_A1 || format == gfxIFormats::RGB_A8) {
    aImage->LockAlphaData();

    PRUint32 alphaBytesPerRow, alphaDepth, alphaLength;
    aImage->GetAlphaBytesPerRow(&alphaBytesPerRow);

#if 0
    if (format == gfxIFormats::RGB_A1)
      alphaDepth = 1;
    else
      alphaDepth = 8;
#endif
    switch (format) {
    case gfxIFormats::RGB_A1:
      alphaDepth = 1;
      break;
    case gfxIFormats::RGB_A8:
      alphaDepth = 8;
      break;
    default:
      // not reached
      return NS_ERROR_UNEXPECTED;
    }

    PRUint8 *alphaBits;
    aImage->GetAlphaData(&alphaBits, &alphaLength);

    alphaPixbuf = gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0);

    // Run through alphaBits and copy the alpha mask into the pixbuf's
    // alpha channel.
    PRUint8 *maskRow = alphaBits;
    PRUint8 *pixbufRow = gdk_pixbuf_get_pixels(alphaPixbuf);

    gint pixbufRowStride = gdk_pixbuf_get_rowstride(alphaPixbuf);
    gint pixbufChannels = gdk_pixbuf_get_n_channels(alphaPixbuf);

    for (PRInt32 y = 0; y < height; ++y) {
      PRUint8 *pixbufPixel = pixbufRow;
      PRUint8 *maskPixel = maskRow;

      // If using 1-bit alpha, we must expand it to 8-bit
      PRUint32 bitPos = 7;

      for (PRInt32 x = 0; x < width; ++x) {
        if (alphaDepth == 1) {
          pixbufPixel[pixbufChannels - 1] = ((*maskPixel >> bitPos) & 1) ? 255 : 0;
          if (bitPos-- == 0) { // wrapped around, move forward a byte
            ++maskPixel;
            bitPos = 7;
          }
        } else {
          pixbufPixel[pixbufChannels - 1] = *maskPixel++;
        }

        pixbufPixel += pixbufChannels;
      }

      pixbufRow += pixbufRowStride;
      maskRow += alphaBytesPerRow;
    }
  }

  gboolean res = gdk_pixbuf_save(alphaPixbuf ? alphaPixbuf : pixbuf,
                                 aPath.get(), "png", NULL, NULL);

  if (alphaPixbuf) {
    aImage->UnlockAlphaData();
    g_object_unref(alphaPixbuf);
  }

  aImage->UnlockImageData();
  g_object_unref(pixbuf);
  return res ? NS_OK : NS_ERROR_FAILURE;
}
                 
NS_IMETHODIMP
nsGNOMEShellService::SetDesktopBackground(nsIDOMElement* aElement, 
                                          PRInt32 aPosition)
{
  nsresult rv;
  nsCOMPtr<gfxIImageFrame> gfxFrame;

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

  // get the current frame, which holds the image data
  container->GetCurrentFrame(getter_AddRefs(gfxFrame));

  if (!gfxFrame)
    return NS_ERROR_FAILURE;

  // Write the background file to the home directory.
  nsCAutoString filePath(PR_GetEnv("HOME"));

  // get the product brand name from localized strings
  nsXPIDLString brandName;
  nsCID bundleCID = NS_STRINGBUNDLESERVICE_CID;
  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(bundleCID));
  if (bundleService) {
    nsCOMPtr<nsIStringBundle> brandBundle;
    rv = bundleService->CreateBundle(BRAND_PROPERTIES,
                                     getter_AddRefs(brandBundle));
    if (NS_SUCCEEDED(rv) && brandBundle) {
      rv = brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                          getter_Copies(brandName));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // build the file name
  filePath.Append(NS_LITERAL_CSTRING("/") +
                  NS_ConvertUTF16toUTF8(brandName) +
                  NS_LITERAL_CSTRING("_wallpaper.png"));
                  
  // write the image to a file in the home dir
  rv = WriteImage(filePath, gfxFrame);

  // if the file was written successfully, set it as the system wallpaper
  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);

  nsCAutoString options;
  if (aPosition == BACKGROUND_TILE)
    options.Assign("wallpaper");
  else if (aPosition == BACKGROUND_STRETCH)
    options.Assign("stretched");
  else
    options.Assign("centered");

  gconf->SetString(NS_LITERAL_CSTRING(kDesktopOptionsKey), options);

  // Set the image to an empty string first to force a refresh
  // (since we could be writing a new image on top of an existing
  // Firefox_wallpaper.png and nautilus doesn't monitor the file for changes)
  gconf->SetString(NS_LITERAL_CSTRING(kDesktopImageKey),
                   NS_LITERAL_CSTRING(""));

  gconf->SetString(NS_LITERAL_CSTRING(kDesktopImageKey), filePath);
  gconf->SetBool(NS_LITERAL_CSTRING(kDesktopDrawBGKey), PR_TRUE);

  return rv;
}

NS_IMETHODIMP
nsGNOMEShellService::GetDesktopBackgroundColor(PRUint32 *aColor)
{
  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);

  nsCAutoString background;
  gconf->GetString(NS_LITERAL_CSTRING(kDesktopColorKey), background);

  if (background.IsEmpty()) {
    *aColor = 0;
    return NS_OK;
  }

  // Chop off the leading '#' character
  background.Cut(0, 1);

  nscolor rgb;
  if (!NS_ASCIIHexToRGB(background, &rgb))
    return NS_ERROR_FAILURE;

  // The result must be in RGB order with the high 8 bits zero.
  *aColor = (NS_GET_R(rgb) << 16 | NS_GET_G(rgb) << 8  | NS_GET_B(rgb));
  return NS_OK;
}

NS_IMETHODIMP
nsGNOMEShellService::SetDesktopBackgroundColor(PRUint32 aColor)
{
  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);

  unsigned char red = (aColor >> 16);
  unsigned char green = (aColor >> 8) & 0xff;
  unsigned char blue = aColor & 0xff;

  nsCAutoString colorString;
  NS_RGBToASCIIHex(NS_RGB(red, green, blue), colorString);

  gconf->SetString(NS_LITERAL_CSTRING(kDesktopColorKey), colorString);

  return NS_OK;
}

NS_IMETHODIMP
nsGNOMEShellService::OpenPreferredApplication(PRInt32 aApplication)
{
  nsCAutoString scheme;
  if (aApplication == APPLICATION_MAIL)
    scheme.Assign("mailto");
  else if (aApplication == APPLICATION_NEWS)
    scheme.Assign("news");
  else
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);

  PRBool enabled;
  nsCAutoString appCommand;
  gconf->GetAppForProtocol(scheme, &enabled, appCommand);

  if (!enabled)
    return NS_ERROR_FAILURE;

  // XXX we don't currently handle launching a terminal window.
  // If the handler requires a terminal, bail.
  PRBool requiresTerminal;
  gconf->HandlerRequiresTerminal(scheme, &requiresTerminal);
  if (requiresTerminal)
    return NS_ERROR_FAILURE;

  // Perform shell argument expansion
  int argc;
  char **argv;
  if (!g_shell_parse_argv(appCommand.get(), &argc, &argv, NULL))
    return NS_ERROR_FAILURE;

  char **newArgv = new char*[argc + 1];
  int newArgc = 0;

  // Run through the list of arguments.  Copy all of them to the new
  // argv except for %s, which we skip.
  for (int i = 0; i < argc; ++i) {
    if (strcmp(argv[i], "%s") != 0)
      newArgv[newArgc++] = argv[i];
  }

  newArgv[newArgc] = nsnull;

  gboolean err = g_spawn_async(NULL, newArgv, NULL, G_SPAWN_SEARCH_PATH,
                               NULL, NULL, NULL, NULL);

  g_strfreev(argv);
  delete[] newArgv;

  return err ? NS_OK : NS_ERROR_FAILURE;
}
