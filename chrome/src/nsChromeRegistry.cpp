/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsChromeRegistry.h"
#include "nsChromeRegistryChrome.h"
#include "nsChromeRegistryContent.h"

#include <string.h>

#include "prio.h"
#include "prprf.h"

#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsEscape.h"
#include "nsLayoutCID.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsUnicharUtils.h"

#include "nsCSSStyleSheet.h"
#include "nsIConsoleService.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDocShell.h"
#include "nsIDOMElement.h"
#include "nsIDOMLocation.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDOMWindow.h"
#include "nsIIOService.h"
#include "nsIJARProtocolHandler.h"
#include "nsIObserverService.h"
#include "nsIPresShell.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptError.h"
#include "nsIWindowMediator.h"

nsChromeRegistry* nsChromeRegistry::gChromeRegistry;

////////////////////////////////////////////////////////////////////////////////

void
nsChromeRegistry::LogMessage(const char* aMsg, ...)
{
  nsCOMPtr<nsIConsoleService> console 
    (do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!console)
    return;

  va_list args;
  va_start(args, aMsg);
  char* formatted = PR_vsmprintf(aMsg, args);
  va_end(args);
  if (!formatted)
    return;

  console->LogStringMessage(NS_ConvertUTF8toUTF16(formatted).get());
  PR_smprintf_free(formatted);
}

void
nsChromeRegistry::LogMessageWithContext(nsIURI* aURL, uint32_t aLineNumber, uint32_t flags,
                                        const char* aMsg, ...)
{
  nsresult rv;

  nsCOMPtr<nsIConsoleService> console 
    (do_GetService(NS_CONSOLESERVICE_CONTRACTID));

  nsCOMPtr<nsIScriptError> error
    (do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
  if (!console || !error)
    return;

  va_list args;
  va_start(args, aMsg);
  char* formatted = PR_vsmprintf(aMsg, args);
  va_end(args);
  if (!formatted)
    return;

  nsCString spec;
  if (aURL)
    aURL->GetSpec(spec);

  rv = error->Init(NS_ConvertUTF8toUTF16(formatted),
                   NS_ConvertUTF8toUTF16(spec),
                   EmptyString(),
                   aLineNumber, 0, flags, "chrome registration");
  PR_smprintf_free(formatted);

  if (NS_FAILED(rv))
    return;

  console->LogMessage(error);
}

nsChromeRegistry::~nsChromeRegistry()
{
  gChromeRegistry = nullptr;
}

NS_INTERFACE_MAP_BEGIN(nsChromeRegistry)
  NS_INTERFACE_MAP_ENTRY(nsIChromeRegistry)
  NS_INTERFACE_MAP_ENTRY(nsIXULChromeRegistry)
  NS_INTERFACE_MAP_ENTRY(nsIToolkitChromeRegistry)
#ifdef MOZ_XUL
  NS_INTERFACE_MAP_ENTRY(nsIXULOverlayProvider)
#endif
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIChromeRegistry)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsChromeRegistry)
NS_IMPL_RELEASE(nsChromeRegistry)

////////////////////////////////////////////////////////////////////////////////
// nsIChromeRegistry methods:

already_AddRefed<nsIChromeRegistry>
nsChromeRegistry::GetService()
{
  if (!gChromeRegistry)
  {
    // We don't actually want this ref, we just want the service to
    // initialize if it hasn't already.
    nsCOMPtr<nsIChromeRegistry> reg(
        do_GetService(NS_CHROMEREGISTRY_CONTRACTID));
    if (!gChromeRegistry)
      return nullptr;
  }
  nsCOMPtr<nsIChromeRegistry> registry = gChromeRegistry;
  return registry.forget();
}

nsresult
nsChromeRegistry::Init()
{
  mOverrideTable.Init();

  // This initialization process is fairly complicated and may cause reentrant
  // getservice calls to resolve chrome URIs (especially locale files). We
  // don't want that, so we inform the protocol handler about our existence
  // before we are actually fully initialized.
  gChromeRegistry = this;

  mInitialized = true;

  return NS_OK;
}

nsresult
nsChromeRegistry::GetProviderAndPath(nsIURL* aChromeURL,
                                     nsACString& aProvider, nsACString& aPath)
{
  nsresult rv;

#ifdef DEBUG
  bool isChrome;
  aChromeURL->SchemeIs("chrome", &isChrome);
  NS_ASSERTION(isChrome, "Non-chrome URI?");
#endif

  nsAutoCString path;
  rv = aChromeURL->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  if (path.Length() < 3) {
    LogMessage("Invalid chrome URI: %s", path.get());
    return NS_ERROR_FAILURE;
  }

  path.SetLength(nsUnescapeCount(path.BeginWriting()));
  NS_ASSERTION(path.First() == '/', "Path should always begin with a slash!");

  int32_t slash = path.FindChar('/', 1);
  if (slash == 1) {
    LogMessage("Invalid chrome URI: %s", path.get());
    return NS_ERROR_FAILURE;
  }

  if (slash == -1) {
    aPath.Truncate();
  }
  else {
    if (slash == (int32_t) path.Length() - 1)
      aPath.Truncate();
    else
      aPath.Assign(path.get() + slash + 1, path.Length() - slash - 1);

    --slash;
  }

  aProvider.Assign(path.get() + 1, slash);
  return NS_OK;
}


nsresult
nsChromeRegistry::Canonify(nsIURL* aChromeURL)
{
  NS_NAMED_LITERAL_CSTRING(kSlash, "/");

  nsresult rv;

  nsAutoCString provider, path;
  rv = GetProviderAndPath(aChromeURL, provider, path);
  NS_ENSURE_SUCCESS(rv, rv);

  if (path.IsEmpty()) {
    nsAutoCString package;
    rv = aChromeURL->GetHost(package);
    NS_ENSURE_SUCCESS(rv, rv);

    // we re-use the "path" local string to build a new URL path
    path.Assign(kSlash + provider + kSlash + package);
    if (provider.EqualsLiteral("content")) {
      path.AppendLiteral(".xul");
    }
    else if (provider.EqualsLiteral("locale")) {
      path.AppendLiteral(".dtd");
    }
    else if (provider.EqualsLiteral("skin")) {
      path.AppendLiteral(".css");
    }
    else {
      return NS_ERROR_INVALID_ARG;
    }
    aChromeURL->SetPath(path);
  }
  else {
    // prevent directory traversals ("..")
    // path is already unescaped once, but uris can get unescaped twice
    const char* pos = path.BeginReading();
    const char* end = path.EndReading();
    while (pos < end) {
      switch (*pos) {
        case ':':
          return NS_ERROR_DOM_BAD_URI;
        case '.':
          if (pos[1] == '.')
            return NS_ERROR_DOM_BAD_URI;
          break;
        case '%':
          // chrome: URIs with double-escapes are trying to trick us.
          // watch for %2e, and %25 in case someone triple unescapes
          if (pos[1] == '2' &&
               ( pos[2] == 'e' || pos[2] == 'E' || 
                 pos[2] == '5' ))
            return NS_ERROR_DOM_BAD_URI;
          break;
        case '?':
        case '#':
          pos = end;
          continue;
      }
      ++pos;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::ConvertChromeURL(nsIURI* aChromeURI, nsIURI* *aResult)
{
  nsresult rv;
  NS_ASSERTION(aChromeURI, "null url!");

  if (mOverrideTable.Get(aChromeURI, aResult))
    return NS_OK;

  nsCOMPtr<nsIURL> chromeURL (do_QueryInterface(aChromeURI));
  NS_ENSURE_TRUE(chromeURL, NS_NOINTERFACE);

  nsAutoCString package, provider, path;
  rv = chromeURL->GetHostPort(package);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GetProviderAndPath(chromeURL, provider, path);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIURI* baseURI = GetBaseURIFromPackage(package, provider, path);

  uint32_t flags;
  rv = GetFlagsFromPackage(package, &flags);
  if (NS_FAILED(rv))
    return rv;

  if (flags & PLATFORM_PACKAGE) {
#if defined(XP_WIN) || defined(XP_OS2)
    path.Insert("win/", 0);
#elif defined(XP_MACOSX)
    path.Insert("mac/", 0);
#else
    path.Insert("unix/", 0);
#endif
  }

  if (!baseURI) {
    LogMessage("No chrome package registered for chrome://%s/%s/%s",
               package.get(), provider.get(), path.get());
    return NS_ERROR_FAILURE;
  }

  return NS_NewURI(aResult, path, nullptr, baseURI);
}

////////////////////////////////////////////////////////////////////////

// theme stuff


static void FlushSkinBindingsForWindow(nsIDOMWindow* aWindow)
{
  // Get the DOM document.
  nsCOMPtr<nsIDOMDocument> domDocument;
  aWindow->GetDocument(getter_AddRefs(domDocument));
  if (!domDocument)
    return;

  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);
  if (!document)
    return;

  // Annihilate all XBL bindings.
  document->FlushSkinBindings();
}

// XXXbsmedberg: move this to nsIWindowMediator
NS_IMETHODIMP nsChromeRegistry::RefreshSkins()
{
  nsCOMPtr<nsIWindowMediator> windowMediator
    (do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!windowMediator)
    return NS_OK;

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  windowMediator->GetEnumerator(nullptr, getter_AddRefs(windowEnumerator));
  bool more;
  windowEnumerator->HasMoreElements(&more);
  while (more) {
    nsCOMPtr<nsISupports> protoWindow;
    windowEnumerator->GetNext(getter_AddRefs(protoWindow));
    if (protoWindow) {
      nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(protoWindow);
      if (domWindow)
        FlushSkinBindingsForWindow(domWindow);
    }
    windowEnumerator->HasMoreElements(&more);
  }

  FlushSkinCaches();

  windowMediator->GetEnumerator(nullptr, getter_AddRefs(windowEnumerator));
  windowEnumerator->HasMoreElements(&more);
  while (more) {
    nsCOMPtr<nsISupports> protoWindow;
    windowEnumerator->GetNext(getter_AddRefs(protoWindow));
    if (protoWindow) {
      nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(protoWindow);
      if (domWindow)
        RefreshWindow(domWindow);
    }
    windowEnumerator->HasMoreElements(&more);
  }
   
  return NS_OK;
}

void
nsChromeRegistry::FlushSkinCaches()
{
  nsCOMPtr<nsIObserverService> obsSvc =
    mozilla::services::GetObserverService();
  NS_ASSERTION(obsSvc, "Couldn't get observer service.");

  obsSvc->NotifyObservers(static_cast<nsIChromeRegistry*>(this),
                          NS_CHROME_FLUSH_SKINS_TOPIC, nullptr);
}

static bool IsChromeURI(nsIURI* aURI)
{
    bool isChrome=false;
    if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && isChrome)
        return true;
    return false;
}

// XXXbsmedberg: move this to windowmediator
nsresult nsChromeRegistry::RefreshWindow(nsIDOMWindow* aWindow)
{
  // Deal with our subframes first.
  nsCOMPtr<nsIDOMWindowCollection> frames;
  aWindow->GetFrames(getter_AddRefs(frames));
  uint32_t length;
  frames->GetLength(&length);
  uint32_t j;
  for (j = 0; j < length; j++) {
    nsCOMPtr<nsIDOMWindow> childWin;
    frames->Item(j, getter_AddRefs(childWin));
    RefreshWindow(childWin);
  }

  nsresult rv;
  // Get the DOM document.
  nsCOMPtr<nsIDOMDocument> domDocument;
  aWindow->GetDocument(getter_AddRefs(domDocument));
  if (!domDocument)
    return NS_OK;

  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);
  if (!document)
    return NS_OK;

  // Deal with the agent sheets first.  Have to do all the style sets by hand.
  nsCOMPtr<nsIPresShell> shell = document->GetShell();
  if (shell) {
    // Reload only the chrome URL agent style sheets.
    nsCOMArray<nsIStyleSheet> agentSheets;
    rv = shell->GetAgentStyleSheets(agentSheets);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMArray<nsIStyleSheet> newAgentSheets;
    for (int32_t l = 0; l < agentSheets.Count(); ++l) {
      nsIStyleSheet *sheet = agentSheets[l];

      nsIURI* uri = sheet->GetSheetURI();

      if (IsChromeURI(uri)) {
        // Reload the sheet.
        nsRefPtr<nsCSSStyleSheet> newSheet;
        rv = document->LoadChromeSheetSync(uri, true,
                                           getter_AddRefs(newSheet));
        if (NS_FAILED(rv)) return rv;
        if (newSheet) {
          rv = newAgentSheets.AppendObject(newSheet) ? NS_OK : NS_ERROR_FAILURE;
          if (NS_FAILED(rv)) return rv;
        }
      }
      else {  // Just use the same sheet.
        rv = newAgentSheets.AppendObject(sheet) ? NS_OK : NS_ERROR_FAILURE;
        if (NS_FAILED(rv)) return rv;
      }
    }

    rv = shell->SetAgentStyleSheets(newAgentSheets);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Build an array of nsIURIs of style sheets we need to load.
  nsCOMArray<nsIStyleSheet> oldSheets;
  nsCOMArray<nsIStyleSheet> newSheets;

  int32_t count = document->GetNumberOfStyleSheets();

  // Iterate over the style sheets.
  int32_t i;
  for (i = 0; i < count; i++) {
    // Get the style sheet
    nsIStyleSheet *styleSheet = document->GetStyleSheetAt(i);

    if (!oldSheets.AppendObject(styleSheet)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Iterate over our old sheets and kick off a sync load of the new
  // sheet if and only if it's a chrome URL.
  for (i = 0; i < count; i++) {
    nsRefPtr<nsCSSStyleSheet> sheet = do_QueryObject(oldSheets[i]);
    nsIURI* uri = sheet ? sheet->GetOriginalURI() : nullptr;

    if (uri && IsChromeURI(uri)) {
      // Reload the sheet.
      nsRefPtr<nsCSSStyleSheet> newSheet;
      // XXX what about chrome sheets that have a title or are disabled?  This
      // only works by sheer dumb luck.
      document->LoadChromeSheetSync(uri, false, getter_AddRefs(newSheet));
      // Even if it's null, we put in in there.
      newSheets.AppendObject(newSheet);
    }
    else {
      // Just use the same sheet.
      newSheets.AppendObject(sheet);
    }
  }

  // Now notify the document that multiple sheets have been added and removed.
  document->UpdateStyleSheets(oldSheets, newSheets);
  return NS_OK;
}

void
nsChromeRegistry::FlushAllCaches()
{
  nsCOMPtr<nsIObserverService> obsSvc =
    mozilla::services::GetObserverService();
  NS_ASSERTION(obsSvc, "Couldn't get observer service.");

  obsSvc->NotifyObservers((nsIChromeRegistry*) this,
                          NS_CHROME_FLUSH_TOPIC, nullptr);
}  

// xxxbsmedberg Move me to nsIWindowMediator
NS_IMETHODIMP
nsChromeRegistry::ReloadChrome()
{
  UpdateSelectedLocale();
  FlushAllCaches();
  // Do a reload of all top level windows.
  nsresult rv = NS_OK;

  // Get the window mediator
  nsCOMPtr<nsIWindowMediator> windowMediator
    (do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (windowMediator) {
    nsCOMPtr<nsISimpleEnumerator> windowEnumerator;

    rv = windowMediator->GetEnumerator(nullptr, getter_AddRefs(windowEnumerator));
    if (NS_SUCCEEDED(rv)) {
      // Get each dom window
      bool more;
      rv = windowEnumerator->HasMoreElements(&more);
      if (NS_FAILED(rv)) return rv;
      while (more) {
        nsCOMPtr<nsISupports> protoWindow;
        rv = windowEnumerator->GetNext(getter_AddRefs(protoWindow));
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(protoWindow);
          if (domWindow) {
            nsCOMPtr<nsIDOMLocation> location;
            domWindow->GetLocation(getter_AddRefs(location));
            if (location) {
              rv = location->Reload(false);
              if (NS_FAILED(rv)) return rv;
            }
          }
        }
        rv = windowEnumerator->HasMoreElements(&more);
        if (NS_FAILED(rv)) return rv;
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsChromeRegistry::AllowScriptsForPackage(nsIURI* aChromeURI, bool *aResult)
{
  nsresult rv;
  *aResult = false;

#ifdef DEBUG
  bool isChrome;
  aChromeURI->SchemeIs("chrome", &isChrome);
  NS_ASSERTION(isChrome, "Non-chrome URI passed to AllowScriptsForPackage!");
#endif

  nsCOMPtr<nsIURL> url (do_QueryInterface(aChromeURI));
  NS_ENSURE_TRUE(url, NS_NOINTERFACE);

  nsAutoCString provider, file;
  rv = GetProviderAndPath(url, provider, file);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!provider.EqualsLiteral("skin"))
    *aResult = true;

  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::AllowContentToAccess(nsIURI *aURI, bool *aResult)
{
  nsresult rv;

  *aResult = false;

#ifdef DEBUG
  bool isChrome;
  aURI->SchemeIs("chrome", &isChrome);
  NS_ASSERTION(isChrome, "Non-chrome URI passed to AllowContentToAccess!");
#endif

  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
  if (!url) {
    NS_ERROR("Chrome URL doesn't implement nsIURL.");
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoCString package;
  rv = url->GetHostPort(package);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t flags;
  rv = GetFlagsFromPackage(package, &flags);

  if (NS_SUCCEEDED(rv)) {
    *aResult = !!(flags & CONTENT_ACCESSIBLE);
  }
  return NS_OK;
}

NS_IMETHODIMP_(bool)
nsChromeRegistry::WrappersEnabled(nsIURI *aURI)
{
  nsCOMPtr<nsIURL> chromeURL (do_QueryInterface(aURI));
  if (!chromeURL)
    return false;

  bool isChrome = false;
  nsresult rv = chromeURL->SchemeIs("chrome", &isChrome);
  if (NS_FAILED(rv) || !isChrome)
    return false;

  nsAutoCString package;
  rv = chromeURL->GetHostPort(package);
  if (NS_FAILED(rv))
    return false;

  uint32_t flags;
  rv = GetFlagsFromPackage(package, &flags);
  return NS_SUCCEEDED(rv) && (flags & XPCNATIVEWRAPPERS);
}

already_AddRefed<nsChromeRegistry>
nsChromeRegistry::GetSingleton()
{
  if (gChromeRegistry) {
    nsRefPtr<nsChromeRegistry> registry = gChromeRegistry;
    return registry.forget();
  }

  nsRefPtr<nsChromeRegistry> cr;
  if (GeckoProcessType_Content == XRE_GetProcessType())
    cr = new nsChromeRegistryContent();
  else
    cr = new nsChromeRegistryChrome();

  if (NS_FAILED(cr->Init()))
    return nullptr;

  return cr.forget();
}
