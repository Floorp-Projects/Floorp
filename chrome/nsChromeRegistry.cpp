/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsChromeRegistry.h"
#include "nsChromeRegistryChrome.h"
#include "nsChromeRegistryContent.h"

#include "prprf.h"

#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsQueryObject.h"

#include "mozilla/dom/URL.h"
#include "nsIConsoleService.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMLocation.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDOMWindow.h"
#include "nsIObserverService.h"
#include "nsIPresShell.h"
#include "nsIScriptError.h"
#include "nsIWindowMediator.h"
#include "nsIPrefService.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"

nsChromeRegistry* nsChromeRegistry::gChromeRegistry;

// DO NOT use namespace mozilla; it'll break due to a naming conflict between
// mozilla::TextRange and a TextRange in OSX headers.
using mozilla::StyleSheet;
using mozilla::dom::IsChromeURI;

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
  if (NS_WARN_IF(!aChromeURI)) {
    return NS_ERROR_INVALID_ARG;
  }

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
#if defined(XP_WIN)
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
    return NS_ERROR_FILE_NOT_FOUND;
  }

  return NS_NewURI(aResult, path, nullptr, baseURI);
}

////////////////////////////////////////////////////////////////////////

// theme stuff


static void FlushSkinBindingsForWindow(nsPIDOMWindowOuter* aWindow)
{
  // Get the document.
  nsCOMPtr<nsIDocument> document = aWindow->GetDoc();
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
      nsCOMPtr<nsPIDOMWindowOuter> domWindow = do_QueryInterface(protoWindow);
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
      nsCOMPtr<nsPIDOMWindowOuter> domWindow = do_QueryInterface(protoWindow);
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

// XXXbsmedberg: move this to windowmediator
nsresult nsChromeRegistry::RefreshWindow(nsPIDOMWindowOuter* aWindow)
{
  // Deal with our subframes first.
  nsCOMPtr<nsIDOMWindowCollection> frames = aWindow->GetFrames();
  uint32_t length;
  frames->GetLength(&length);
  uint32_t j;
  for (j = 0; j < length; j++) {
    nsCOMPtr<mozIDOMWindowProxy> childWin;
    frames->Item(j, getter_AddRefs(childWin));
    nsCOMPtr<nsPIDOMWindowOuter> piWindow = nsPIDOMWindowOuter::From(childWin);
    RefreshWindow(piWindow);
  }

  nsresult rv;
  // Get the document.
  nsCOMPtr<nsIDocument> document = aWindow->GetDoc();
  if (!document)
    return NS_OK;

  // Deal with the agent sheets first.  Have to do all the style sets by hand.
  nsCOMPtr<nsIPresShell> shell = document->GetShell();
  if (shell) {
    // Reload only the chrome URL agent style sheets.
    nsTArray<RefPtr<StyleSheet>> agentSheets;
    rv = shell->GetAgentStyleSheets(agentSheets);
    NS_ENSURE_SUCCESS(rv, rv);

    nsTArray<RefPtr<StyleSheet>> newAgentSheets;
    for (StyleSheet* sheet : agentSheets) {
      nsIURI* uri = sheet->GetSheetURI();

      if (IsChromeURI(uri)) {
        // Reload the sheet.
        RefPtr<StyleSheet> newSheet;
        rv = document->LoadChromeSheetSync(uri, true, &newSheet);
        if (NS_FAILED(rv)) return rv;
        if (newSheet) {
          rv = newAgentSheets.AppendElement(newSheet) ? NS_OK : NS_ERROR_FAILURE;
          if (NS_FAILED(rv)) return rv;
        }
      }
      else {  // Just use the same sheet.
        rv = newAgentSheets.AppendElement(sheet) ? NS_OK : NS_ERROR_FAILURE;
        if (NS_FAILED(rv)) return rv;
      }
    }

    rv = shell->SetAgentStyleSheets(newAgentSheets);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  int32_t count = document->GetNumberOfStyleSheets();

  // Build an array of style sheets we need to reload.
  nsTArray<RefPtr<StyleSheet>> oldSheets(count);
  nsTArray<RefPtr<StyleSheet>> newSheets(count);

  // Iterate over the style sheets.
  for (int32_t i = 0; i < count; i++) {
    // Get the style sheet
    StyleSheet* styleSheet = document->GetStyleSheetAt(i);
    oldSheets.AppendElement(styleSheet);
  }

  // Iterate over our old sheets and kick off a sync load of the new
  // sheet if and only if it's a non-inline sheet with a chrome URL.
  for (StyleSheet* sheet : oldSheets) {
    MOZ_ASSERT(sheet, "GetStyleSheetAt shouldn't return nullptr for "
                      "in-range sheet indexes");
    nsIURI* uri = sheet->GetSheetURI();

    if (!sheet->IsInline() && IsChromeURI(uri)) {
      // Reload the sheet.
      RefPtr<StyleSheet> newSheet;
      // XXX what about chrome sheets that have a title or are disabled?  This
      // only works by sheer dumb luck.
      document->LoadChromeSheetSync(uri, false, &newSheet);
      // Even if it's null, we put in in there.
      newSheets.AppendElement(newSheet);
    } else {
      // Just use the same sheet.
      newSheets.AppendElement(sheet);
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
          nsCOMPtr<nsPIDOMWindowOuter> domWindow = do_QueryInterface(protoWindow);
          if (domWindow) {
            nsIDOMLocation* location = domWindow->GetLocation();
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

NS_IMETHODIMP
nsChromeRegistry::CanLoadURLRemotely(nsIURI *aURI, bool *aResult)
{
  nsresult rv;

  *aResult = false;

#ifdef DEBUG
  bool isChrome;
  aURI->SchemeIs("chrome", &isChrome);
  NS_ASSERTION(isChrome, "Non-chrome URI passed to CanLoadURLRemotely!");
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
    *aResult = !!(flags & REMOTE_ALLOWED);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::MustLoadURLRemotely(nsIURI *aURI, bool *aResult)
{
  nsresult rv;

  *aResult = false;

#ifdef DEBUG
  bool isChrome;
  aURI->SchemeIs("chrome", &isChrome);
  NS_ASSERTION(isChrome, "Non-chrome URI passed to MustLoadURLRemotely!");
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
    *aResult = !!(flags & REMOTE_REQUIRED);
  }
  return NS_OK;
}

bool
nsChromeRegistry::GetDirectionForLocale(const nsACString& aLocale)
{
  // first check the intl.uidirection.<locale> preference, and if that is not
  // set, check the same preference but with just the first two characters of
  // the locale. If that isn't set, default to left-to-right.
  nsAutoCString prefString = NS_LITERAL_CSTRING("intl.uidirection.") + aLocale;
  nsCOMPtr<nsIPrefBranch> prefBranch (do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (!prefBranch) {
    return false;
  }

  nsXPIDLCString dir;
  prefBranch->GetCharPref(prefString.get(), getter_Copies(dir));
  if (dir.IsEmpty()) {
    int32_t hyphen = prefString.FindChar('-');
    if (hyphen >= 1) {
      nsAutoCString shortPref(Substring(prefString, 0, hyphen));
      prefBranch->GetCharPref(shortPref.get(), getter_Copies(dir));
    }
  }

  return dir.EqualsLiteral("rtl");
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
    RefPtr<nsChromeRegistry> registry = gChromeRegistry;
    return registry.forget();
  }

  RefPtr<nsChromeRegistry> cr;
  if (GeckoProcessType_Content == XRE_GetProcessType())
    cr = new nsChromeRegistryContent();
  else
    cr = new nsChromeRegistryChrome();

  if (NS_FAILED(cr->Init()))
    return nullptr;

  return cr.forget();
}
