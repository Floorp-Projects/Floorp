/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Johnny Stenback <jst@netscape.com>
 *   Christopher A. Aillon <christopher@aillon.com>
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

/* A namespace class for static layout utilities. */

#include "jscntxt.h"

#include "nsJSUtils.h"
#include "nsCOMPtr.h"
#include "nsAString.h"
#include "nsPrintfCString.h"
#include "nsUnicharUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"
#include "nsIPrefLocalizedString.h"
#include "nsServiceManagerUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsDOMCID.h"
#include "nsContentUtils.h"
#include "nsIContentUtils.h"
#include "nsIXPConnect.h"
#include "nsIContent.h"
#include "mozilla/dom/Element.h"
#include "nsIDocument.h"
#include "nsINodeInfo.h"
#include "nsReadableUtils.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNode.h"
#include "nsIDOM3Node.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMError.h"
#include "nsPIDOMWindow.h"
#include "nsIJSContextStack.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsParserCIID.h"
#include "nsIParser.h"
#include "nsIFragmentContentSink.h"
#include "nsIContentSink.h"
#include "nsIHTMLContentSink.h"
#include "nsIXMLContentSink.h"
#include "nsHTMLParts.h"
#include "nsIParserService.h"
#include "nsIServiceManager.h"
#include "nsIAttribute.h"
#include "nsContentList.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsGkAtoms.h"
#include "nsISupportsPrimitives.h"
#include "imgIDecoderObserver.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "imgILoader.h"
#include "mozilla/IHistory.h"
#include "nsDocShellCID.h"
#include "nsIImageLoadingContent.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadGroup.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsContentPolicyUtils.h"
#include "nsNodeInfoManager.h"
#include "nsIXBLService.h"
#include "nsCRT.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMDocumentEvent.h"
#ifdef MOZ_XTF
#include "nsIXTFService.h"
static NS_DEFINE_CID(kXTFServiceCID, NS_XTFSERVICE_CID);
#endif
#include "nsIMIMEService.h"
#include "nsLWBrkCIID.h"
#include "nsILineBreaker.h"
#include "nsIWordBreaker.h"
#include "jsdbgapi.h"
#include "nsIJSRuntimeService.h"
#include "nsIDOMDocumentXBL.h"
#include "nsBindingManager.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsXBLBinding.h"
#include "nsXBLPrototypeBinding.h"
#include "nsEscape.h"
#include "nsICharsetConverterManager.h"
#include "nsIEventListenerManager.h"
#include "nsAttrName.h"
#include "nsIDOMUserDataHandler.h"
#include "nsContentCreatorFunctions.h"
#include "nsTPtrArray.h"
#include "nsGUIEvent.h"
#include "nsMutationEvent.h"
#include "nsIMEStateManager.h"
#include "nsContentErrors.h"
#include "nsUnicharUtilCIID.h"
#include "nsCompressedCharMap.h"
#include "nsINativeKeyBindings.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsXULPopupManager.h"
#include "nsIPermissionManager.h"
#include "nsIContentPrefService.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIRunnable.h"
#include "nsDOMJSUtils.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValue.h"
#include "nsReferencedElement.h"
#include "nsIUGenCategory.h"
#include "nsIDragService.h"
#include "nsIChannelEventSink.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIInterfaceRequestor.h"
#include "nsIOfflineCacheUpdate.h"
#include "nsCPrefetchService.h"
#include "nsIChromeRegistry.h"
#include "nsIMIMEHeaderParam.h"
#include "nsIDOMXULCommandEvent.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMDragEvent.h"
#include "nsDOMDataTransfer.h"
#include "nsHtml5Module.h"
#include "nsPresContext.h"
#include "nsLayoutStatics.h"
#include "nsLayoutUtils.h"
#include "nsFrameManager.h"
#include "BasicLayers.h"
#include "nsFocusManager.h"
#include "nsTextEditorState.h"
#include "nsIPluginHost.h"
#include "nsICategoryManager.h"
#include "nsAHtml5FragmentParser.h"

#ifdef IBMBIDI
#include "nsIBidiKeyboard.h"
#endif
#include "nsCycleCollectionParticipant.h"

// for ReportToConsole
#include "nsIStringBundle.h"
#include "nsIScriptError.h"
#include "nsIConsoleService.h"

#include "mozAutoDocUpdate.h"
#include "imgICache.h"
#include "jsinterp.h"
#include "jsarray.h"
#include "jsdate.h"
#include "jsregexp.h"
#include "jstypedarray.h"
#include "xpcprivate.h"
#include "nsScriptSecurityManager.h"
#include "nsIChannelPolicy.h"
#include "nsChannelPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsContentDLF.h"
#ifdef MOZ_MEDIA
#include "nsHTMLMediaElement.h"
#endif

using namespace mozilla::dom;
using namespace mozilla::layers;

const char kLoadAsData[] = "loadAsData";

static const char kJSStackContractID[] = "@mozilla.org/js/xpc/ContextStack;1";
static NS_DEFINE_CID(kParserServiceCID, NS_PARSERSERVICE_CID);
static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

nsIDOMScriptObjectFactory *nsContentUtils::sDOMScriptObjectFactory = nsnull;
nsIXPConnect *nsContentUtils::sXPConnect;
nsIScriptSecurityManager *nsContentUtils::sSecurityManager;
nsIThreadJSContextStack *nsContentUtils::sThreadJSContextStack;
nsIParserService *nsContentUtils::sParserService = nsnull;
nsINameSpaceManager *nsContentUtils::sNameSpaceManager;
nsIIOService *nsContentUtils::sIOService;
#ifdef MOZ_XTF
nsIXTFService *nsContentUtils::sXTFService = nsnull;
#endif
nsIPrefBranch2 *nsContentUtils::sPrefBranch = nsnull;
imgILoader *nsContentUtils::sImgLoader;
imgICache *nsContentUtils::sImgCache;
mozilla::IHistory *nsContentUtils::sHistory;
nsIConsoleService *nsContentUtils::sConsoleService;
nsDataHashtable<nsISupportsHashKey, EventNameMapping>* nsContentUtils::sAtomEventTable = nsnull;
nsDataHashtable<nsStringHashKey, EventNameMapping>* nsContentUtils::sStringEventTable = nsnull;
nsCOMArray<nsIAtom>* nsContentUtils::sUserDefinedEvents = nsnull;
nsIStringBundleService *nsContentUtils::sStringBundleService;
nsIStringBundle *nsContentUtils::sStringBundles[PropertiesFile_COUNT];
nsIContentPolicy *nsContentUtils::sContentPolicyService;
PRBool nsContentUtils::sTriedToGetContentPolicy = PR_FALSE;
nsILineBreaker *nsContentUtils::sLineBreaker;
nsIWordBreaker *nsContentUtils::sWordBreaker;
nsIUGenCategory *nsContentUtils::sGenCat;
nsTArray<nsISupports**> *nsContentUtils::sPtrsToPtrsToRelease;
nsIScriptRuntime *nsContentUtils::sScriptRuntimes[NS_STID_ARRAY_UBOUND];
PRInt32 nsContentUtils::sScriptRootCount[NS_STID_ARRAY_UBOUND];
PRUint32 nsContentUtils::sJSGCThingRootCount;
#ifdef IBMBIDI
nsIBidiKeyboard *nsContentUtils::sBidiKeyboard = nsnull;
#endif
PRUint32 nsContentUtils::sScriptBlockerCount = 0;
PRUint32 nsContentUtils::sRemovableScriptBlockerCount = 0;
nsCOMArray<nsIRunnable>* nsContentUtils::sBlockedScriptRunners = nsnull;
PRUint32 nsContentUtils::sRunnersCountAtFirstBlocker = 0;
PRUint32 nsContentUtils::sScriptBlockerCountWhereRunnersPrevented = 0;
nsIInterfaceRequestor* nsContentUtils::sSameOriginChecker = nsnull;

nsIJSRuntimeService *nsAutoGCRoot::sJSRuntimeService;
JSRuntime *nsAutoGCRoot::sJSScriptRuntime;

PRBool nsContentUtils::sIsHandlingKeyBoardEvent = PR_FALSE;

PRBool nsContentUtils::sInitialized = PR_FALSE;

nsRefPtrHashtable<nsPrefObserverHashKey, nsPrefOldCallback>
  *nsContentUtils::sPrefCallbackTable = nsnull;

static PLDHashTable sEventListenerManagersHash;

class EventListenerManagerMapEntry : public PLDHashEntryHdr
{
public:
  EventListenerManagerMapEntry(const void *aKey)
    : mKey(aKey)
  {
  }

  ~EventListenerManagerMapEntry()
  {
    NS_ASSERTION(!mListenerManager, "caller must release and disconnect ELM");
  }

private:
  const void *mKey; // must be first, to look like PLDHashEntryStub

public:
  nsCOMPtr<nsIEventListenerManager> mListenerManager;
};

static PRBool
EventListenerManagerHashInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                                  const void *key)
{
  // Initialize the entry with placement new
  new (entry) EventListenerManagerMapEntry(key);
  return PR_TRUE;
}

static void
EventListenerManagerHashClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  EventListenerManagerMapEntry *lm =
    static_cast<EventListenerManagerMapEntry *>(entry);

  // Let the EventListenerManagerMapEntry clean itself up...
  lm->~EventListenerManagerMapEntry();
}

class nsSameOriginChecker : public nsIChannelEventSink,
                            public nsIInterfaceRequestor
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
};

class nsPrefObserverHashKey : public PLDHashEntryHdr {
public:
  typedef nsPrefObserverHashKey* KeyType;
  typedef const nsPrefObserverHashKey* KeyTypePointer;

  static const nsPrefObserverHashKey* KeyToPointer(nsPrefObserverHashKey *aKey)
  {
    return aKey;
  }

  static PLDHashNumber HashKey(const nsPrefObserverHashKey *aKey)
  {
    PRUint32 strHash = nsCRT::HashCode(aKey->mPref.BeginReading(),
                                       aKey->mPref.Length());
    return PR_ROTATE_LEFT32(strHash, 4) ^
           NS_PTR_TO_UINT32(aKey->mCallback);
  }

  nsPrefObserverHashKey(const char *aPref, PrefChangedFunc aCallback) :
    mPref(aPref), mCallback(aCallback) { }

  nsPrefObserverHashKey(const nsPrefObserverHashKey *aOther) :
    mPref(aOther->mPref), mCallback(aOther->mCallback)
  { }

  PRBool KeyEquals(const nsPrefObserverHashKey *aOther) const
  {
    return mCallback == aOther->mCallback &&
           mPref.Equals(aOther->mPref);
  }

  nsPrefObserverHashKey *GetKey() const
  {
    return const_cast<nsPrefObserverHashKey*>(this);
  }

  enum { ALLOW_MEMMOVE = PR_TRUE };

public:
  nsCString mPref;
  PrefChangedFunc mCallback;
};

// For nsContentUtils::RegisterPrefCallback/UnregisterPrefCallback
class nsPrefOldCallback : public nsIObserver,
                          public nsPrefObserverHashKey
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

public:
  nsPrefOldCallback(const char *aPref, PrefChangedFunc aCallback)
    : nsPrefObserverHashKey(aPref, aCallback) { }

  ~nsPrefOldCallback() {
    nsIPrefBranch2 *prefBranch = nsContentUtils::GetPrefBranch();
    if(prefBranch)
      prefBranch->RemoveObserver(mPref.get(), this);
  }

  void AppendClosure(void *aClosure) {
    mClosures.AppendElement(aClosure);
  }

  void RemoveClosure(void *aClosure) {
    mClosures.RemoveElement(aClosure);
  }

  PRBool HasNoClosures() {
    return mClosures.Length() == 0;
  }

public:
  nsTArray<void *>  mClosures;
};

NS_IMPL_ISUPPORTS1(nsPrefOldCallback, nsIObserver)

NS_IMETHODIMP
nsPrefOldCallback::Observe(nsISupports     *aSubject,
                           const char      *aTopic,
                           const PRUnichar *aData)
{
  NS_ASSERTION(!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID),
               "invalid topic");
  NS_LossyConvertUTF16toASCII data(aData);
  for (PRUint32 i = 0; i < mClosures.Length(); i++) {
    mCallback(data.get(), mClosures.ElementAt(i));
  }

  return NS_OK;
}

struct PrefCacheData {
  void* cacheLocation;
  union {
    PRBool defaultValueBool;
    PRInt32 defaultValueInt;
  };
};

nsTArray<nsAutoPtr<PrefCacheData> >* sPrefCacheData = nsnull;

// static
nsresult
nsContentUtils::Init()
{
  if (sInitialized) {
    NS_WARNING("Init() called twice");

    return NS_OK;
  }

  sPrefCacheData = new nsTArray<nsAutoPtr<PrefCacheData> >();

  // It's ok to not have a pref service.
  CallGetService(NS_PREFSERVICE_CONTRACTID, &sPrefBranch);

  nsresult rv = NS_GetNameSpaceManager(&sNameSpaceManager);
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPConnect* xpconnect = nsXPConnect::GetXPConnect();
  NS_ENSURE_TRUE(xpconnect, NS_ERROR_FAILURE);

  sXPConnect = xpconnect;
  sThreadJSContextStack = xpconnect;

  sSecurityManager = nsScriptSecurityManager::GetScriptSecurityManager();
  if(!sSecurityManager)
    return NS_ERROR_FAILURE;
  NS_ADDREF(sSecurityManager);

  rv = CallGetService(NS_IOSERVICE_CONTRACTID, &sIOService);
  if (NS_FAILED(rv)) {
    // This makes life easier, but we can live without it.

    sIOService = nsnull;
  }

  rv = CallGetService(NS_LBRK_CONTRACTID, &sLineBreaker);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = CallGetService(NS_WBRK_CONTRACTID, &sWordBreaker);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CallGetService(NS_UNICHARCATEGORY_CONTRACTID, &sGenCat);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CallGetService(NS_IHISTORY_CONTRACTID, &sHistory);
  if (NS_FAILED(rv)) {
    NS_RUNTIMEABORT("Cannot get the history service");
    return rv;
  }

  sPtrsToPtrsToRelease = new nsTArray<nsISupports**>();
  if (!sPtrsToPtrsToRelease) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!InitializeEventTable())
    return NS_ERROR_FAILURE;

  if (!sEventListenerManagersHash.ops) {
    static PLDHashTableOps hash_table_ops =
    {
      PL_DHashAllocTable,
      PL_DHashFreeTable,
      PL_DHashVoidPtrKeyStub,
      PL_DHashMatchEntryStub,
      PL_DHashMoveEntryStub,
      EventListenerManagerHashClearEntry,
      PL_DHashFinalizeStub,
      EventListenerManagerHashInitEntry
    };

    if (!PL_DHashTableInit(&sEventListenerManagersHash, &hash_table_ops,
                           nsnull, sizeof(EventListenerManagerMapEntry), 16)) {
      sEventListenerManagersHash.ops = nsnull;

      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  sBlockedScriptRunners = new nsCOMArray<nsIRunnable>;
  NS_ENSURE_TRUE(sBlockedScriptRunners, NS_ERROR_OUT_OF_MEMORY);

  sInitialized = PR_TRUE;

  return NS_OK;
}

bool nsContentUtils::sImgLoaderInitialized;

void
nsContentUtils::InitImgLoader()
{
  sImgLoaderInitialized = true;

  // Ignore failure and just don't load images
  nsresult rv = CallGetService("@mozilla.org/image/loader;1", &sImgLoader);
  if (NS_FAILED(rv)) {
    // no image loading for us.  Oh, well.
    sImgLoader = nsnull;
    sImgCache = nsnull;
  } else {
    if (NS_FAILED(CallGetService("@mozilla.org/image/cache;1", &sImgCache )))
      sImgCache = nsnull;
  }
}

PRBool
nsContentUtils::InitializeEventTable() {
  NS_ASSERTION(!sAtomEventTable, "EventTable already initialized!");
  NS_ASSERTION(!sStringEventTable, "EventTable already initialized!");

  static const EventNameMapping eventArray[] = {
    { nsGkAtoms::onmousedown,                   NS_MOUSE_BUTTON_DOWN, EventNameType_All, NS_MOUSE_EVENT },
    { nsGkAtoms::onmouseup,                     NS_MOUSE_BUTTON_UP, EventNameType_All, NS_MOUSE_EVENT },
    { nsGkAtoms::onclick,                       NS_MOUSE_CLICK, EventNameType_All, NS_MOUSE_EVENT },
    { nsGkAtoms::ondblclick,                    NS_MOUSE_DOUBLECLICK, EventNameType_HTMLXUL, NS_MOUSE_EVENT },
    { nsGkAtoms::onmouseover,                   NS_MOUSE_ENTER_SYNTH, EventNameType_All, NS_MOUSE_EVENT },
    { nsGkAtoms::onmouseout,                    NS_MOUSE_EXIT_SYNTH, EventNameType_All, NS_MOUSE_EVENT },
    { nsGkAtoms::onMozMouseHittest,             NS_MOUSE_MOZHITTEST, EventNameType_None, NS_MOUSE_EVENT },
    { nsGkAtoms::onmousemove,                   NS_MOUSE_MOVE, EventNameType_All, NS_MOUSE_EVENT },
    { nsGkAtoms::oncontextmenu,                 NS_CONTEXTMENU, EventNameType_HTMLXUL, NS_MOUSE_EVENT },

    { nsGkAtoms::onkeydown,                     NS_KEY_DOWN, EventNameType_HTMLXUL, NS_KEY_EVENT },
    { nsGkAtoms::onkeyup,                       NS_KEY_UP, EventNameType_HTMLXUL, NS_KEY_EVENT },
    { nsGkAtoms::onkeypress,                    NS_KEY_PRESS, EventNameType_HTMLXUL, NS_KEY_EVENT },
                                                
    { nsGkAtoms::onfocus,                       NS_FOCUS_CONTENT, EventNameType_HTMLXUL, NS_FOCUS_EVENT },
    { nsGkAtoms::onblur,                        NS_BLUR_CONTENT, EventNameType_HTMLXUL, NS_FOCUS_EVENT },

    { nsGkAtoms::onoffline,                     NS_OFFLINE, EventNameType_HTMLXUL, NS_EVENT },
    { nsGkAtoms::ononline,                      NS_ONLINE, EventNameType_HTMLXUL, NS_EVENT },
    { nsGkAtoms::onsubmit,                      NS_FORM_SUBMIT, EventNameType_HTMLXUL, NS_EVENT },
    { nsGkAtoms::onreset,                       NS_FORM_RESET, EventNameType_HTMLXUL, NS_EVENT },
    { nsGkAtoms::onchange,                      NS_FORM_CHANGE, EventNameType_HTMLXUL, NS_EVENT },
    { nsGkAtoms::onselect,                      NS_FORM_SELECTED, EventNameType_HTMLXUL, NS_EVENT },
    { nsGkAtoms::oninvalid,                     NS_FORM_INVALID, EventNameType_HTMLXUL, NS_EVENT },
    { nsGkAtoms::onload,                        NS_LOAD, EventNameType_All, NS_EVENT },
    { nsGkAtoms::onpopstate,                    NS_POPSTATE, EventNameType_HTMLXUL, NS_EVENT_NULL },
    { nsGkAtoms::onunload,                      NS_PAGE_UNLOAD,
                                                (EventNameType_HTMLXUL | EventNameType_SVGSVG), NS_EVENT },
    { nsGkAtoms::onhashchange,                  NS_HASHCHANGE, EventNameType_HTMLXUL, NS_EVENT },
    { nsGkAtoms::onreadystatechange,            NS_READYSTATECHANGE, EventNameType_HTMLXUL },
    { nsGkAtoms::onbeforeunload,                NS_BEFORE_PAGE_UNLOAD, EventNameType_HTMLXUL, NS_EVENT },
    { nsGkAtoms::onabort,                       NS_IMAGE_ABORT,
                                                (EventNameType_HTMLXUL | EventNameType_SVGSVG), NS_EVENT },
    { nsGkAtoms::onerror,                       NS_LOAD_ERROR,
                                                (EventNameType_HTMLXUL | EventNameType_SVGSVG), NS_EVENT },
    { nsGkAtoms::onbeforescriptexecute,         NS_BEFORE_SCRIPT_EXECUTE, EventNameType_HTMLXUL, NS_EVENT },
    { nsGkAtoms::onafterscriptexecute,          NS_AFTER_SCRIPT_EXECUTE, EventNameType_HTMLXUL, NS_EVENT },

    { nsGkAtoms::onDOMAttrModified,             NS_MUTATION_ATTRMODIFIED, EventNameType_HTMLXUL, NS_MUTATION_EVENT },
    { nsGkAtoms::onDOMCharacterDataModified,    NS_MUTATION_CHARACTERDATAMODIFIED, EventNameType_HTMLXUL, NS_MUTATION_EVENT },
    { nsGkAtoms::onDOMNodeInserted,             NS_MUTATION_NODEINSERTED, EventNameType_HTMLXUL, NS_MUTATION_EVENT },
    { nsGkAtoms::onDOMNodeRemoved,              NS_MUTATION_NODEREMOVED, EventNameType_HTMLXUL, NS_MUTATION_EVENT },
    { nsGkAtoms::onDOMNodeInsertedIntoDocument, NS_MUTATION_NODEINSERTEDINTODOCUMENT, EventNameType_HTMLXUL, NS_MUTATION_EVENT },
    { nsGkAtoms::onDOMNodeRemovedFromDocument,  NS_MUTATION_NODEREMOVEDFROMDOCUMENT, EventNameType_HTMLXUL, NS_MUTATION_EVENT },
    { nsGkAtoms::onDOMSubtreeModified,          NS_MUTATION_SUBTREEMODIFIED, EventNameType_HTMLXUL, NS_MUTATION_EVENT },

    { nsGkAtoms::onDOMActivate,                 NS_UI_ACTIVATE, EventNameType_HTMLXUL, NS_UI_EVENT },
    { nsGkAtoms::onDOMFocusIn,                  NS_UI_FOCUSIN, EventNameType_HTMLXUL, NS_UI_EVENT },
    { nsGkAtoms::onDOMFocusOut,                 NS_UI_FOCUSOUT, EventNameType_HTMLXUL, NS_UI_EVENT },
    { nsGkAtoms::oninput,                       NS_FORM_INPUT, EventNameType_HTMLXUL, NS_UI_EVENT },
                                                
    { nsGkAtoms::onDOMMouseScroll,              NS_MOUSE_SCROLL, EventNameType_HTMLXUL, NS_MOUSE_SCROLL_EVENT },
    { nsGkAtoms::onMozMousePixelScroll,         NS_MOUSE_PIXEL_SCROLL, EventNameType_HTMLXUL, NS_MOUSE_SCROLL_EVENT },
                                                
    { nsGkAtoms::onpageshow,                    NS_PAGE_SHOW, EventNameType_HTML, NS_EVENT },
    { nsGkAtoms::onpagehide,                    NS_PAGE_HIDE, EventNameType_HTML, NS_EVENT },
    { nsGkAtoms::onresize,                      NS_RESIZE_EVENT,
                                                (EventNameType_HTMLXUL | EventNameType_SVGSVG), NS_EVENT },
    { nsGkAtoms::onscroll,                      NS_SCROLL_EVENT,
                                                (EventNameType_HTMLXUL | EventNameType_SVGSVG), NS_EVENT_NULL },
    { nsGkAtoms::oncopy,                        NS_COPY, EventNameType_HTMLXUL, NS_EVENT },
    { nsGkAtoms::oncut,                         NS_CUT, EventNameType_HTMLXUL, NS_EVENT },
    { nsGkAtoms::onpaste,                       NS_PASTE, EventNameType_HTMLXUL, NS_EVENT },
    // XUL specific events
    { nsGkAtoms::ontext,                        NS_TEXT_TEXT, EventNameType_XUL, NS_EVENT_NULL },

    { nsGkAtoms::oncompositionstart,            NS_COMPOSITION_START, EventNameType_XUL, NS_COMPOSITION_EVENT },
    { nsGkAtoms::oncompositionend,              NS_COMPOSITION_END, EventNameType_XUL, NS_COMPOSITION_EVENT },

    { nsGkAtoms::oncommand,                     NS_XUL_COMMAND, EventNameType_XUL, NS_INPUT_EVENT },

    { nsGkAtoms::onclose,                       NS_XUL_CLOSE, EventNameType_XUL, NS_EVENT_NULL},
    { nsGkAtoms::onpopupshowing,                NS_XUL_POPUP_SHOWING, EventNameType_XUL, NS_EVENT_NULL},
    { nsGkAtoms::onpopupshown,                  NS_XUL_POPUP_SHOWN, EventNameType_XUL, NS_EVENT_NULL},
    { nsGkAtoms::onpopuphiding,                 NS_XUL_POPUP_HIDING, EventNameType_XUL, NS_EVENT_NULL},
    { nsGkAtoms::onpopuphidden,                 NS_XUL_POPUP_HIDDEN, EventNameType_XUL, NS_EVENT_NULL},
    { nsGkAtoms::onbroadcast,                   NS_XUL_BROADCAST, EventNameType_XUL, NS_EVENT_NULL},
    { nsGkAtoms::oncommandupdate,               NS_XUL_COMMAND_UPDATE, EventNameType_XUL, NS_EVENT_NULL},

    { nsGkAtoms::ondragenter,                   NS_DRAGDROP_ENTER, EventNameType_HTMLXUL, NS_DRAG_EVENT },
    { nsGkAtoms::ondragover,                    NS_DRAGDROP_OVER_SYNTH, EventNameType_HTMLXUL, NS_DRAG_EVENT },
    { nsGkAtoms::ondragexit,                    NS_DRAGDROP_EXIT_SYNTH, EventNameType_XUL, NS_DRAG_EVENT },
    { nsGkAtoms::ondragdrop,                    NS_DRAGDROP_DRAGDROP, EventNameType_XUL, NS_DRAG_EVENT },
    { nsGkAtoms::ondraggesture,                 NS_DRAGDROP_GESTURE, EventNameType_XUL, NS_DRAG_EVENT },
    { nsGkAtoms::ondrag,                        NS_DRAGDROP_DRAG, EventNameType_HTMLXUL, NS_DRAG_EVENT },
    { nsGkAtoms::ondragend,                     NS_DRAGDROP_END, EventNameType_HTMLXUL, NS_DRAG_EVENT },
    { nsGkAtoms::ondragstart,                   NS_DRAGDROP_START, EventNameType_HTMLXUL, NS_DRAG_EVENT },
    { nsGkAtoms::ondragleave,                   NS_DRAGDROP_LEAVE_SYNTH, EventNameType_HTMLXUL, NS_DRAG_EVENT },
    { nsGkAtoms::ondrop,                        NS_DRAGDROP_DROP, EventNameType_HTMLXUL, NS_DRAG_EVENT },

    { nsGkAtoms::onoverflow,                    NS_SCROLLPORT_OVERFLOW, EventNameType_XUL, NS_EVENT_NULL},
    { nsGkAtoms::onunderflow,                   NS_SCROLLPORT_UNDERFLOW, EventNameType_XUL, NS_EVENT_NULL},
#ifdef MOZ_SVG
    { nsGkAtoms::onSVGLoad,                     NS_SVG_LOAD, EventNameType_None, NS_SVG_EVENT },
    { nsGkAtoms::onSVGUnload,                   NS_SVG_UNLOAD, EventNameType_None, NS_SVG_EVENT },
    { nsGkAtoms::onSVGAbort,                    NS_SVG_ABORT, EventNameType_None, NS_SVG_EVENT },
    { nsGkAtoms::onSVGError,                    NS_SVG_ERROR, EventNameType_None, NS_SVG_EVENT },
    { nsGkAtoms::onSVGResize,                   NS_SVG_RESIZE, EventNameType_None, NS_SVG_EVENT },
    { nsGkAtoms::onSVGScroll,                   NS_SVG_SCROLL, EventNameType_None, NS_SVG_EVENT },

    { nsGkAtoms::onSVGZoom,                     NS_SVG_ZOOM, EventNameType_None, NS_SVGZOOM_EVENT },

    // This is a bit hackish, but SVG's event names are weird.
    { nsGkAtoms::onzoom,                        NS_SVG_ZOOM, EventNameType_SVGSVG, NS_EVENT_NULL },
#endif // MOZ_SVG
#ifdef MOZ_SMIL
    { nsGkAtoms::onbegin,                       NS_SMIL_BEGIN, EventNameType_SMIL, NS_EVENT_NULL },
    { nsGkAtoms::onbeginEvent,                  NS_SMIL_BEGIN, EventNameType_None, NS_SMIL_TIME_EVENT },
    { nsGkAtoms::onend,                         NS_SMIL_END, EventNameType_SMIL, NS_EVENT_NULL },
    { nsGkAtoms::onendEvent,                    NS_SMIL_END, EventNameType_None, NS_SMIL_TIME_EVENT },
    { nsGkAtoms::onrepeat,                      NS_SMIL_REPEAT, EventNameType_SMIL, NS_EVENT_NULL },
    { nsGkAtoms::onrepeatEvent,                 NS_SMIL_REPEAT, EventNameType_None, NS_SMIL_TIME_EVENT },
#endif // MOZ_SMIL
#ifdef MOZ_MEDIA
    { nsGkAtoms::onloadstart,                   NS_LOADSTART, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onprogress,                    NS_PROGRESS, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onsuspend,                     NS_SUSPEND, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onemptied,                     NS_EMPTIED, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onstalled,                     NS_STALLED, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onplay,                        NS_PLAY, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onpause,                       NS_PAUSE, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onloadedmetadata,              NS_LOADEDMETADATA, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onloadeddata,                  NS_LOADEDDATA, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onwaiting,                     NS_WAITING, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onplaying,                     NS_PLAYING, EventNameType_HTML,  NS_EVENT_NULL },
    { nsGkAtoms::oncanplay,                     NS_CANPLAY, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::oncanplaythrough,              NS_CANPLAYTHROUGH, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onseeking,                     NS_SEEKING, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onseeked,                      NS_SEEKED, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::ontimeupdate,                  NS_TIMEUPDATE, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onended,                       NS_ENDED, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onratechange,                  NS_RATECHANGE, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::ondurationchange,              NS_DURATIONCHANGE, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onvolumechange,                NS_VOLUMECHANGE, EventNameType_HTML, NS_EVENT_NULL },
    { nsGkAtoms::onMozAudioAvailable,           NS_MOZAUDIOAVAILABLE, EventNameType_None, NS_EVENT_NULL },
#endif // MOZ_MEDIA
    { nsGkAtoms::onMozAfterPaint,               NS_AFTERPAINT, EventNameType_None, NS_EVENT },
    { nsGkAtoms::onMozBeforePaint,              NS_BEFOREPAINT, EventNameType_None, NS_EVENT_NULL },

    { nsGkAtoms::onMozScrolledAreaChanged,      NS_SCROLLEDAREACHANGED, EventNameType_None, NS_SCROLLAREA_EVENT },

    // Simple gesture events
    { nsGkAtoms::onMozSwipeGesture,             NS_SIMPLE_GESTURE_SWIPE, EventNameType_None, NS_SIMPLE_GESTURE_EVENT },
    { nsGkAtoms::onMozMagnifyGestureStart,      NS_SIMPLE_GESTURE_MAGNIFY_START, EventNameType_None, NS_SIMPLE_GESTURE_EVENT },
    { nsGkAtoms::onMozMagnifyGestureUpdate,     NS_SIMPLE_GESTURE_MAGNIFY_UPDATE, EventNameType_None, NS_SIMPLE_GESTURE_EVENT },
    { nsGkAtoms::onMozMagnifyGesture,           NS_SIMPLE_GESTURE_MAGNIFY, EventNameType_None, NS_SIMPLE_GESTURE_EVENT },
    { nsGkAtoms::onMozRotateGestureStart,       NS_SIMPLE_GESTURE_ROTATE_START, EventNameType_None, NS_SIMPLE_GESTURE_EVENT },
    { nsGkAtoms::onMozRotateGestureUpdate,      NS_SIMPLE_GESTURE_ROTATE_UPDATE, EventNameType_None, NS_SIMPLE_GESTURE_EVENT },
    { nsGkAtoms::onMozRotateGesture,            NS_SIMPLE_GESTURE_ROTATE, EventNameType_None, NS_SIMPLE_GESTURE_EVENT },
    { nsGkAtoms::onMozTapGesture,               NS_SIMPLE_GESTURE_TAP, EventNameType_None, NS_SIMPLE_GESTURE_EVENT },
    { nsGkAtoms::onMozPressTapGesture,          NS_SIMPLE_GESTURE_PRESSTAP, EventNameType_None, NS_SIMPLE_GESTURE_EVENT },

    { nsGkAtoms::onMozTouchDown,                NS_MOZTOUCH_DOWN, EventNameType_None, NS_MOZTOUCH_EVENT },
    { nsGkAtoms::onMozTouchMove,                NS_MOZTOUCH_MOVE, EventNameType_None, NS_MOZTOUCH_EVENT },
    { nsGkAtoms::onMozTouchUp,                  NS_MOZTOUCH_UP, EventNameType_None, NS_MOZTOUCH_EVENT },

    { nsGkAtoms::ontransitionend,               NS_TRANSITION_END, EventNameType_None, NS_TRANSITION_EVENT }
  };

  sAtomEventTable = new nsDataHashtable<nsISupportsHashKey, EventNameMapping>;
  sStringEventTable = new nsDataHashtable<nsStringHashKey, EventNameMapping>;
  sUserDefinedEvents = new nsCOMArray<nsIAtom>(64);

  if (!sAtomEventTable || !sStringEventTable || !sUserDefinedEvents ||
      !sAtomEventTable->Init(int(NS_ARRAY_LENGTH(eventArray) / 0.75) + 1) ||
      !sStringEventTable->Init(int(NS_ARRAY_LENGTH(eventArray) / 0.75) + 1)) {
    delete sAtomEventTable;
    sAtomEventTable = nsnull;
    delete sStringEventTable;
    sStringEventTable = nsnull;
    delete sUserDefinedEvents;
    sUserDefinedEvents = nsnull;
    return PR_FALSE;
  }

  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(eventArray); ++i) {
    if (!sAtomEventTable->Put(eventArray[i].mAtom, eventArray[i]) ||
        !sStringEventTable->Put(Substring(nsDependentAtomString(eventArray[i].mAtom), 2),
                                eventArray[i])) {
      delete sAtomEventTable;
      sAtomEventTable = nsnull;
      delete sStringEventTable;
      sStringEventTable = nsnull;
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

/**
 * Access a cached parser service. Don't addref. We need only one
 * reference to it and this class has that one.
 */
/* static */
nsIParserService*
nsContentUtils::GetParserService()
{
  // XXX: This isn't accessed from several threads, is it?
  if (!sParserService) {
    // Lock, recheck sCachedParserService and aquire if this should be
    // safe for multiple threads.
    nsresult rv = CallGetService(kParserServiceCID, &sParserService);
    if (NS_FAILED(rv)) {
      sParserService = nsnull;
    }
  }

  return sParserService;
}

#ifdef MOZ_XTF
nsIXTFService*
nsContentUtils::GetXTFService()
{
  if (!sXTFService) {
    nsresult rv = CallGetService(kXTFServiceCID, &sXTFService);
    if (NS_FAILED(rv)) {
      sXTFService = nsnull;
    }
  }

  return sXTFService;
}
#endif

#ifdef IBMBIDI
nsIBidiKeyboard*
nsContentUtils::GetBidiKeyboard()
{
  if (!sBidiKeyboard) {
    nsresult rv = CallGetService("@mozilla.org/widget/bidikeyboard;1", &sBidiKeyboard);
    if (NS_FAILED(rv)) {
      sBidiKeyboard = nsnull;
    }
  }
  return sBidiKeyboard;
}
#endif

template <class OutputIterator>
struct NormalizeNewlinesCharTraits {
  public:
    typedef typename OutputIterator::value_type value_type;

  public:
    NormalizeNewlinesCharTraits(OutputIterator& aIterator) : mIterator(aIterator) { }
    void writechar(typename OutputIterator::value_type aChar) {
      *mIterator++ = aChar;
    }

  private:
    OutputIterator mIterator;
};

#ifdef HAVE_CPP_PARTIAL_SPECIALIZATION

template <class CharT>
struct NormalizeNewlinesCharTraits<CharT*> {
  public:
    typedef CharT value_type;

  public:
    NormalizeNewlinesCharTraits(CharT* aCharPtr) : mCharPtr(aCharPtr) { }
    void writechar(CharT aChar) {
      *mCharPtr++ = aChar;
    }

  private:
    CharT* mCharPtr;
};

#else

NS_SPECIALIZE_TEMPLATE
struct NormalizeNewlinesCharTraits<char*> {
  public:
    typedef char value_type;

  public:
    NormalizeNewlinesCharTraits(char* aCharPtr) : mCharPtr(aCharPtr) { }
    void writechar(char aChar) {
      *mCharPtr++ = aChar;
    }

  private:
    char* mCharPtr;
};

NS_SPECIALIZE_TEMPLATE
struct NormalizeNewlinesCharTraits<PRUnichar*> {
  public:
    typedef PRUnichar value_type;

  public:
    NormalizeNewlinesCharTraits(PRUnichar* aCharPtr) : mCharPtr(aCharPtr) { }
    void writechar(PRUnichar aChar) {
      *mCharPtr++ = aChar;
    }

  private:
    PRUnichar* mCharPtr;
};

#endif

template <class OutputIterator>
class CopyNormalizeNewlines
{
  public:
    typedef typename OutputIterator::value_type value_type;

  public:
    CopyNormalizeNewlines(OutputIterator* aDestination,
                          PRBool aLastCharCR=PR_FALSE) :
      mLastCharCR(aLastCharCR),
      mDestination(aDestination),
      mWritten(0)
    { }

    PRUint32 GetCharsWritten() {
      return mWritten;
    }

    PRBool IsLastCharCR() {
      return mLastCharCR;
    }

    void write(const typename OutputIterator::value_type* aSource, PRUint32 aSourceLength) {

      const typename OutputIterator::value_type* done_writing = aSource + aSourceLength;

      // If the last source buffer ended with a CR...
      if (mLastCharCR) {
        // ..and if the next one is a LF, then skip it since
        // we've already written out a newline
        if (aSourceLength && (*aSource == value_type('\n'))) {
          ++aSource;
        }
        mLastCharCR = PR_FALSE;
      }

      PRUint32 num_written = 0;
      while ( aSource < done_writing ) {
        if (*aSource == value_type('\r')) {
          mDestination->writechar('\n');
          ++aSource;
          // If we've reached the end of the buffer, record
          // that we wrote out a CR
          if (aSource == done_writing) {
            mLastCharCR = PR_TRUE;
          }
          // If the next character is a LF, skip it
          else if (*aSource == value_type('\n')) {
            ++aSource;
          }
        }
        else {
          mDestination->writechar(*aSource++);
        }
        ++num_written;
      }

      mWritten += num_written;
    }

  private:
    PRBool mLastCharCR;
    OutputIterator* mDestination;
    PRUint32 mWritten;
};

// static
PRUint32
nsContentUtils::CopyNewlineNormalizedUnicodeTo(const nsAString& aSource,
                                               PRUint32 aSrcOffset,
                                               PRUnichar* aDest,
                                               PRUint32 aLength,
                                               PRBool& aLastCharCR)
{
  typedef NormalizeNewlinesCharTraits<PRUnichar*> sink_traits;

  sink_traits dest_traits(aDest);
  CopyNormalizeNewlines<sink_traits> normalizer(&dest_traits,aLastCharCR);
  nsReadingIterator<PRUnichar> fromBegin, fromEnd;
  copy_string(aSource.BeginReading(fromBegin).advance( PRInt32(aSrcOffset) ),
              aSource.BeginReading(fromEnd).advance( PRInt32(aSrcOffset+aLength) ),
              normalizer);
  aLastCharCR = normalizer.IsLastCharCR();
  return normalizer.GetCharsWritten();
}

// static
PRUint32
nsContentUtils::CopyNewlineNormalizedUnicodeTo(nsReadingIterator<PRUnichar>& aSrcStart, const nsReadingIterator<PRUnichar>& aSrcEnd, nsAString& aDest)
{
  typedef nsWritingIterator<PRUnichar> WritingIterator;
  typedef NormalizeNewlinesCharTraits<WritingIterator> sink_traits;

  WritingIterator iter;
  aDest.BeginWriting(iter);
  sink_traits dest_traits(iter);
  CopyNormalizeNewlines<sink_traits> normalizer(&dest_traits);
  copy_string(aSrcStart, aSrcEnd, normalizer);
  return normalizer.GetCharsWritten();
}

// Replaced by precompiled CCMap (see bug 180266). To update the list
// of characters, see one of files included below. As for the way
// the original list of characters was obtained by Frank Tang, see bug 54467.
// Updated to fix the regression (bug 263411). The list contains
// characters of the following Unicode character classes : Ps, Pi, Po, Pf, Pe.
// (ref.: http://www.w3.org/TR/2004/CR-CSS21-20040225/selector.html#first-letter)
#include "punct_marks.x-ccmap"
DEFINE_X_CCMAP(gPuncCharsCCMapExt, const);

// static
PRBool
nsContentUtils::IsPunctuationMark(PRUint32 aChar)
{
  return CCMAP_HAS_CHAR_EXT(gPuncCharsCCMapExt, aChar);
}

// static
PRBool
nsContentUtils::IsPunctuationMarkAt(const nsTextFragment* aFrag, PRUint32 aOffset)
{
  PRUnichar h = aFrag->CharAt(aOffset);
  if (!IS_SURROGATE(h)) {
    return IsPunctuationMark(h);
  }
  if (NS_IS_HIGH_SURROGATE(h) && aOffset + 1 < aFrag->GetLength()) {
    PRUnichar l = aFrag->CharAt(aOffset + 1);
    if (NS_IS_LOW_SURROGATE(l)) {
      return IsPunctuationMark(SURROGATE_TO_UCS4(h, l));
    }
  }
  return PR_FALSE;
}

// static
PRBool nsContentUtils::IsAlphanumeric(PRUint32 aChar)
{
  nsIUGenCategory::nsUGenCategory cat = sGenCat->Get(aChar);

  return (cat == nsIUGenCategory::kLetter || cat == nsIUGenCategory::kNumber);
}
 
// static
PRBool nsContentUtils::IsAlphanumericAt(const nsTextFragment* aFrag, PRUint32 aOffset)
{
  PRUnichar h = aFrag->CharAt(aOffset);
  if (!IS_SURROGATE(h)) {
    return IsAlphanumeric(h);
  }
  if (NS_IS_HIGH_SURROGATE(h) && aOffset + 1 < aFrag->GetLength()) {
    PRUnichar l = aFrag->CharAt(aOffset + 1);
    if (NS_IS_LOW_SURROGATE(l)) {
      return IsAlphanumeric(SURROGATE_TO_UCS4(h, l));
    }
  }
  return PR_FALSE;
}

/* static */
PRBool
nsContentUtils::IsHTMLWhitespace(PRUnichar aChar)
{
  return aChar == PRUnichar(0x0009) ||
         aChar == PRUnichar(0x000A) ||
         aChar == PRUnichar(0x000C) ||
         aChar == PRUnichar(0x000D) ||
         aChar == PRUnichar(0x0020);
}

/* static */
PRBool
nsContentUtils::ParseIntMarginValue(const nsAString& aString, nsIntMargin& result)
{
  nsAutoString marginStr(aString);
  marginStr.CompressWhitespace(PR_TRUE, PR_TRUE);
  if (marginStr.IsEmpty()) {
    return PR_FALSE;
  }

  PRInt32 start = 0, end = 0;
  for (int count = 0; count < 4; count++) {
    if ((PRUint32)end >= marginStr.Length())
      return PR_FALSE;

    // top, right, bottom, left
    if (count < 3)
      end = Substring(marginStr, start).FindChar(',');
    else
      end = Substring(marginStr, start).Length();

    if (end <= 0)
      return PR_FALSE;

    PRInt32 ec, val = 
      nsString(Substring(marginStr, start, end)).ToInteger(&ec);
    if (NS_FAILED(ec))
      return PR_FALSE;

    switch(count) {
      case 0:
        result.top = val;
      break;
      case 1:
        result.right = val;
      break;
      case 2:
        result.bottom = val;
      break;
      case 3:
        result.left = val;
      break;
    }
    start += end + 1;
  }
  return PR_TRUE;
}

/* static */
void
nsContentUtils::GetOfflineAppManifest(nsIDocument *aDocument, nsIURI **aURI)
{
  Element* docElement = aDocument->GetRootElement();
  if (!docElement) {
    return;
  }

  nsAutoString manifestSpec;
  docElement->GetAttr(kNameSpaceID_None, nsGkAtoms::manifest, manifestSpec);

  // Manifest URIs can't have fragment identifiers.
  if (manifestSpec.IsEmpty() ||
      manifestSpec.FindChar('#') != kNotFound) {
    return;
  }

  nsContentUtils::NewURIWithDocumentCharset(aURI, manifestSpec,
                                            aDocument,
                                            aDocument->GetDocBaseURI());
}

/* static */
PRBool
nsContentUtils::OfflineAppAllowed(nsIURI *aURI)
{
  nsCOMPtr<nsIOfflineCacheUpdateService> updateService =
    do_GetService(NS_OFFLINECACHEUPDATESERVICE_CONTRACTID);
  if (!updateService) {
    return PR_FALSE;
  }

  PRBool allowed;
  nsresult rv = updateService->OfflineAppAllowedForURI(aURI,
                                                       sPrefBranch,
                                                       &allowed);
  return NS_SUCCEEDED(rv) && allowed;
}

/* static */
PRBool
nsContentUtils::OfflineAppAllowed(nsIPrincipal *aPrincipal)
{
  nsCOMPtr<nsIOfflineCacheUpdateService> updateService =
    do_GetService(NS_OFFLINECACHEUPDATESERVICE_CONTRACTID);
  if (!updateService) {
    return PR_FALSE;
  }

  PRBool allowed;
  nsresult rv = updateService->OfflineAppAllowed(aPrincipal,
                                                 sPrefBranch,
                                                 &allowed);
  return NS_SUCCEEDED(rv) && allowed;
}

// static
void
nsContentUtils::Shutdown()
{
  sInitialized = PR_FALSE;

  NS_HTMLParanoidFragmentSinkShutdown();
  NS_XHTMLParanoidFragmentSinkShutdown();

  NS_IF_RELEASE(sContentPolicyService);
  sTriedToGetContentPolicy = PR_FALSE;
  PRUint32 i;
  for (i = 0; i < PropertiesFile_COUNT; ++i)
    NS_IF_RELEASE(sStringBundles[i]);

  // Clean up c-style's observer 
  if (sPrefCallbackTable) {
    delete sPrefCallbackTable;
    sPrefCallbackTable = nsnull;
  }

  delete sPrefCacheData;
  sPrefCacheData = nsnull;

  NS_IF_RELEASE(sStringBundleService);
  NS_IF_RELEASE(sConsoleService);
  NS_IF_RELEASE(sDOMScriptObjectFactory);
  sXPConnect = nsnull;
  sThreadJSContextStack = nsnull;
  NS_IF_RELEASE(sSecurityManager);
  NS_IF_RELEASE(sNameSpaceManager);
  NS_IF_RELEASE(sParserService);
  NS_IF_RELEASE(sIOService);
  NS_IF_RELEASE(sLineBreaker);
  NS_IF_RELEASE(sWordBreaker);
  NS_IF_RELEASE(sGenCat);
#ifdef MOZ_XTF
  NS_IF_RELEASE(sXTFService);
#endif
  NS_IF_RELEASE(sImgLoader);
  NS_IF_RELEASE(sImgCache);
  NS_IF_RELEASE(sHistory);
  NS_IF_RELEASE(sPrefBranch);
#ifdef IBMBIDI
  NS_IF_RELEASE(sBidiKeyboard);
#endif

  delete sAtomEventTable;
  sAtomEventTable = nsnull;
  delete sStringEventTable;
  sStringEventTable = nsnull;
  delete sUserDefinedEvents;
  sUserDefinedEvents = nsnull;

  if (sPtrsToPtrsToRelease) {
    for (i = 0; i < sPtrsToPtrsToRelease->Length(); ++i) {
      nsISupports** ptrToPtr = sPtrsToPtrsToRelease->ElementAt(i);
      NS_RELEASE(*ptrToPtr);
    }
    delete sPtrsToPtrsToRelease;
    sPtrsToPtrsToRelease = nsnull;
  }

  if (sEventListenerManagersHash.ops) {
    NS_ASSERTION(sEventListenerManagersHash.entryCount == 0,
                 "Event listener manager hash not empty at shutdown!");

    // See comment above.

    // However, we have to handle this table differently.  If it still
    // has entries, we want to leak it too, so that we can keep it alive
    // in case any elements are destroyed.  Because if they are, we need
    // their event listener managers to be destroyed too, or otherwise
    // it could leave dangling references in DOMClassInfo's preserved
    // wrapper table.

    if (sEventListenerManagersHash.entryCount == 0) {
      PL_DHashTableFinish(&sEventListenerManagersHash);
      sEventListenerManagersHash.ops = nsnull;
    }
  }

  NS_ASSERTION(!sBlockedScriptRunners ||
               sBlockedScriptRunners->Count() == 0,
               "How'd this happen?");
  delete sBlockedScriptRunners;
  sBlockedScriptRunners = nsnull;

  NS_IF_RELEASE(sSameOriginChecker);
  
  nsAutoGCRoot::Shutdown();

  nsTextEditorState::ShutDown();
}

// static
PRBool
nsContentUtils::IsCallerTrustedForCapability(const char* aCapability)
{
  // The secman really should handle UniversalXPConnect case, since that
  // should include UniversalBrowserRead... doesn't right now, though.
  PRBool hasCap;
  if (NS_FAILED(sSecurityManager->IsCapabilityEnabled(aCapability, &hasCap)))
    return PR_FALSE;
  if (hasCap)
    return PR_TRUE;
    
  if (NS_FAILED(sSecurityManager->IsCapabilityEnabled("UniversalXPConnect",
                                                      &hasCap)))
    return PR_FALSE;
  return hasCap;
}

/**
 * Checks whether two nodes come from the same origin. aTrustedNode is
 * considered 'safe' in that a user can operate on it and that it isn't
 * a js-object that implements nsIDOMNode.
 * Never call this function with the first node provided by script, it
 * must always be known to be a 'real' node!
 */
// static
nsresult
nsContentUtils::CheckSameOrigin(nsINode *aTrustedNode,
                                nsIDOMNode *aUnTrustedNode)
{
  NS_PRECONDITION(aTrustedNode, "There must be a trusted node");

  PRBool isSystem = PR_FALSE;
  sSecurityManager->SubjectPrincipalIsSystem(&isSystem);
  if (isSystem) {
    // we're running as system, grant access to the node.

    return NS_OK;
  }

  /*
   * Get hold of each node's principal
   */
  nsCOMPtr<nsINode> unTrustedNode = do_QueryInterface(aUnTrustedNode);

  // Make sure these are both real nodes
  NS_ENSURE_TRUE(aTrustedNode && unTrustedNode, NS_ERROR_UNEXPECTED);

  nsIPrincipal* trustedPrincipal = aTrustedNode->NodePrincipal();
  nsIPrincipal* unTrustedPrincipal = unTrustedNode->NodePrincipal();

  if (trustedPrincipal == unTrustedPrincipal) {
    return NS_OK;
  }

  PRBool equal;
  // XXXbz should we actually have a Subsumes() check here instead?  Or perhaps
  // a separate method for that, with callers using one or the other?
  if (NS_FAILED(trustedPrincipal->Equals(unTrustedPrincipal, &equal)) ||
      !equal) {
    return NS_ERROR_DOM_PROP_ACCESS_DENIED;
  }

  return NS_OK;
}

// static
PRBool
nsContentUtils::CanCallerAccess(nsIPrincipal* aSubjectPrincipal,
                                nsIPrincipal* aPrincipal)
{
  PRBool subsumes;
  nsresult rv = aSubjectPrincipal->Subsumes(aPrincipal, &subsumes);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  if (subsumes) {
    return PR_TRUE;
  }

  // The subject doesn't subsume aPrincipal.  Allow access only if the subject
  // has either "UniversalXPConnect" (if aPrincipal is system principal) or
  // "UniversalBrowserRead" (in all other cases).
  PRBool isSystem;
  rv = sSecurityManager->IsSystemPrincipal(aPrincipal, &isSystem);
  isSystem = NS_FAILED(rv) || isSystem;
  const char* capability =
    NS_FAILED(rv) || isSystem ? "UniversalXPConnect" : "UniversalBrowserRead";

  return IsCallerTrustedForCapability(capability);
}

// static
PRBool
nsContentUtils::CanCallerAccess(nsIDOMNode *aNode)
{
  // XXXbz why not check the IsCapabilityEnabled thing up front, and not bother
  // with the system principal games?  But really, there should be a simpler
  // API here, dammit.
  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  sSecurityManager->GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));

  if (!subjectPrincipal) {
    // we're running as system, grant access to the node.

    return PR_TRUE;
  }

  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, PR_FALSE);

  return CanCallerAccess(subjectPrincipal, node->NodePrincipal());
}

// static
PRBool
nsContentUtils::CanCallerAccess(nsPIDOMWindow* aWindow)
{
  // XXXbz why not check the IsCapabilityEnabled thing up front, and not bother
  // with the system principal games?  But really, there should be a simpler
  // API here, dammit.
  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  sSecurityManager->GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));

  if (!subjectPrincipal) {
    // we're running as system, grant access to the node.

    return PR_TRUE;
  }

  nsCOMPtr<nsIScriptObjectPrincipal> scriptObject =
    do_QueryInterface(aWindow->IsOuterWindow() ?
                      aWindow->GetCurrentInnerWindow() : aWindow);
  NS_ENSURE_TRUE(scriptObject, PR_FALSE);

  return CanCallerAccess(subjectPrincipal, scriptObject->GetPrincipal());
}

//static
PRBool
nsContentUtils::InProlog(nsINode *aNode)
{
  NS_PRECONDITION(aNode, "missing node to nsContentUtils::InProlog");

  nsINode* parent = aNode->GetNodeParent();
  if (!parent || !parent->IsNodeOfType(nsINode::eDOCUMENT)) {
    return PR_FALSE;
  }

  nsIDocument* doc = static_cast<nsIDocument*>(parent);
  nsIContent* root = doc->GetRootElement();

  return !root || doc->IndexOf(aNode) < doc->IndexOf(root);
}

static JSContext *
GetContextFromDocument(nsIDocument *aDocument, JSObject** aGlobalObject)
{
  nsIScriptGlobalObject *sgo = aDocument->GetScopeObject();
  if (!sgo) {
    // No script global, no context.

    *aGlobalObject = nsnull;

    return nsnull;
  }

  *aGlobalObject = sgo->GetGlobalJSObject();

  nsIScriptContext *scx = sgo->GetContext();
  if (!scx) {
    // No context left in the old scope...

    return nsnull;
  }

  return (JSContext *)scx->GetNativeContext();
}

// static
nsresult
nsContentUtils::GetContextAndScopes(nsIDocument *aOldDocument,
                                    nsIDocument *aNewDocument, JSContext **aCx,
                                    JSObject **aOldScope, JSObject **aNewScope)
{
  *aCx = nsnull;
  *aOldScope = nsnull;
  *aNewScope = nsnull;

  JSObject *newScope = nsnull;
  nsIScriptGlobalObject *newSGO = aNewDocument->GetScopeObject();
  if (!newSGO || !(newScope = newSGO->GetGlobalJSObject())) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(sXPConnect, NS_ERROR_NOT_INITIALIZED);

  // Make sure to get our hands on the right scope object, since
  // GetWrappedNativeOfNativeObject doesn't call PreCreate and hence won't get
  // the right scope if we pass in something bogus.  The right scope lives on
  // the script global of the old document.
  // XXXbz note that if GetWrappedNativeOfNativeObject did call PreCreate it
  // would get the wrong scope (that of the _new_ document), so we should be
  // glad it doesn't!
  JSObject *oldScope = nsnull;
  JSContext *cx = GetContextFromDocument(aOldDocument, &oldScope);

  if (!oldScope) {
    return NS_OK;
  }

  if (!cx) {
    JSObject *dummy;
    cx = GetContextFromDocument(aNewDocument, &dummy);

    if (!cx) {
      // No context reachable from the old or new document, use the
      // calling context, or the safe context if no caller can be
      // found.

      sThreadJSContextStack->Peek(&cx);

      if (!cx) {
        sThreadJSContextStack->GetSafeJSContext(&cx);

        if (!cx) {
          // No safe context reachable, bail.
          NS_WARNING("No context reachable in GetContextAndScopes()!");

          return NS_ERROR_NOT_AVAILABLE;
        }
      }
    }
  }

  *aCx = cx;
  *aOldScope = oldScope;
  *aNewScope = newScope;

  return NS_OK;
}

nsresult
nsContentUtils::ReparentContentWrappersInScope(nsIScriptGlobalObject *aOldScope,
                                               nsIScriptGlobalObject *aNewScope)
{
  JSContext *cx = nsnull;

  // Try really hard to find a context to work on.
  nsIScriptContext *context = aOldScope->GetContext();
  if (context) {
    cx = static_cast<JSContext *>(context->GetNativeContext());
  }

  if (!cx) {
    context = aNewScope->GetContext();
    if (context) {
      cx = static_cast<JSContext *>(context->GetNativeContext());
    }

    if (!cx) {
      sThreadJSContextStack->Peek(&cx);

      if (!cx) {
        sThreadJSContextStack->GetSafeJSContext(&cx);

        if (!cx) {
          // Wow, this is really bad!
          NS_WARNING("No context reachable in ReparentContentWrappers()!");

          return NS_ERROR_NOT_AVAILABLE;
        }
      }
    }
  }

  // Now that we have a context, let's get the global objects from the two
  // scopes and ask XPConnect to do the rest of the work.

  JSObject *oldScopeObj = aOldScope->GetGlobalJSObject();
  JSObject *newScopeObj = aNewScope->GetGlobalJSObject();

  if (!newScopeObj || !oldScopeObj) {
    // We can't really do anything without the JSObjects.

    return NS_ERROR_NOT_AVAILABLE;
  }

  return sXPConnect->MoveWrappers(cx, oldScopeObj, newScopeObj);
}

nsIDocShell *
nsContentUtils::GetDocShellFromCaller()
{
  JSContext *cx = nsnull;
  sThreadJSContextStack->Peek(&cx);

  if (cx) {
    nsIScriptGlobalObject *sgo = nsJSUtils::GetDynamicScriptGlobal(cx);
    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(sgo));

    if (win) {
      return win->GetDocShell();
    }
  }

  return nsnull;
}

nsPIDOMWindow *
nsContentUtils::GetWindowFromCaller()
{
  JSContext *cx = nsnull;
  sThreadJSContextStack->Peek(&cx);

  if (cx) {
    nsCOMPtr<nsPIDOMWindow> win =
      do_QueryInterface(nsJSUtils::GetDynamicScriptGlobal(cx));
    return win;
  }

  return nsnull;
}

nsIDOMDocument *
nsContentUtils::GetDocumentFromCaller()
{
  JSContext *cx = nsnull;
  JSObject *obj = nsnull;
  sXPConnect->GetCaller(&cx, &obj);
  NS_ASSERTION(cx && obj, "Caller ensures something is running");

  nsCOMPtr<nsPIDOMWindow> win =
    do_QueryInterface(nsJSUtils::GetStaticScriptGlobal(cx, obj));
  if (!win) {
    return nsnull;
  }

  return win->GetExtantDocument();
}

nsIDOMDocument *
nsContentUtils::GetDocumentFromContext()
{
  JSContext *cx = nsnull;
  sThreadJSContextStack->Peek(&cx);

  if (cx) {
    nsIScriptGlobalObject *sgo = nsJSUtils::GetDynamicScriptGlobal(cx);

    if (sgo) {
      nsCOMPtr<nsPIDOMWindow> pwin = do_QueryInterface(sgo);
      if (pwin) {
        return pwin->GetExtantDocument();
      }
    }
  }

  return nsnull;
}

PRBool
nsContentUtils::IsCallerChrome()
{
  PRBool is_caller_chrome = PR_FALSE;
  nsresult rv = sSecurityManager->SubjectPrincipalIsSystem(&is_caller_chrome);
  if (NS_FAILED(rv)) {
    return PR_FALSE;
  }

  return is_caller_chrome;
}

PRBool
nsContentUtils::IsCallerTrustedForRead()
{
  return IsCallerTrustedForCapability("UniversalBrowserRead");
}

PRBool
nsContentUtils::IsCallerTrustedForWrite()
{
  return IsCallerTrustedForCapability("UniversalBrowserWrite");
}

// static
nsINode*
nsContentUtils::GetCrossDocParentNode(nsINode* aChild)
{
  NS_PRECONDITION(aChild, "The child is null!");

  nsINode* parent = aChild->GetNodeParent();
  if (parent || !aChild->IsNodeOfType(nsINode::eDOCUMENT))
    return parent;

  nsIDocument* doc = static_cast<nsIDocument*>(aChild);
  nsIDocument* parentDoc = doc->GetParentDocument();
  return parentDoc ? parentDoc->FindContentForSubDocument(doc) : nsnull;
}

// static
PRBool
nsContentUtils::ContentIsDescendantOf(const nsINode* aPossibleDescendant,
                                      const nsINode* aPossibleAncestor)
{
  NS_PRECONDITION(aPossibleDescendant, "The possible descendant is null!");
  NS_PRECONDITION(aPossibleAncestor, "The possible ancestor is null!");

  do {
    if (aPossibleDescendant == aPossibleAncestor)
      return PR_TRUE;
    aPossibleDescendant = aPossibleDescendant->GetNodeParent();
  } while (aPossibleDescendant);

  return PR_FALSE;
}

// static
PRBool
nsContentUtils::ContentIsCrossDocDescendantOf(nsINode* aPossibleDescendant,
                                              nsINode* aPossibleAncestor)
{
  NS_PRECONDITION(aPossibleDescendant, "The possible descendant is null!");
  NS_PRECONDITION(aPossibleAncestor, "The possible ancestor is null!");

  do {
    if (aPossibleDescendant == aPossibleAncestor)
      return PR_TRUE;
    aPossibleDescendant = GetCrossDocParentNode(aPossibleDescendant);
  } while (aPossibleDescendant);

  return PR_FALSE;
}


// static
nsresult
nsContentUtils::GetAncestors(nsINode* aNode,
                             nsTArray<nsINode*>& aArray)
{
  while (aNode) {
    aArray.AppendElement(aNode);
    aNode = aNode->GetNodeParent();
  }
  return NS_OK;
}

// static
nsresult
nsContentUtils::GetAncestorsAndOffsets(nsIDOMNode* aNode,
                                       PRInt32 aOffset,
                                       nsTArray<nsIContent*>* aAncestorNodes,
                                       nsTArray<PRInt32>* aAncestorOffsets)
{
  NS_ENSURE_ARG_POINTER(aNode);

  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));

  if (!content) {
    return NS_ERROR_FAILURE;
  }

  if (!aAncestorNodes->IsEmpty()) {
    NS_WARNING("aAncestorNodes is not empty");
    aAncestorNodes->Clear();
  }

  if (!aAncestorOffsets->IsEmpty()) {
    NS_WARNING("aAncestorOffsets is not empty");
    aAncestorOffsets->Clear();
  }

  // insert the node itself
  aAncestorNodes->AppendElement(content.get());
  aAncestorOffsets->AppendElement(aOffset);

  // insert all the ancestors
  nsIContent* child = content;
  nsIContent* parent = child->GetParent();
  while (parent) {
    aAncestorNodes->AppendElement(parent);
    aAncestorOffsets->AppendElement(parent->IndexOf(child));
    child = parent;
    parent = parent->GetParent();
  }

  return NS_OK;
}

// static
nsresult
nsContentUtils::GetCommonAncestor(nsIDOMNode *aNode,
                                  nsIDOMNode *aOther,
                                  nsIDOMNode** aCommonAncestor)
{
  *aCommonAncestor = nsnull;

  nsCOMPtr<nsINode> node1 = do_QueryInterface(aNode);
  nsCOMPtr<nsINode> node2 = do_QueryInterface(aOther);

  NS_ENSURE_TRUE(node1 && node2, NS_ERROR_UNEXPECTED);

  nsINode* common = GetCommonAncestor(node1, node2);
  NS_ENSURE_TRUE(common, NS_ERROR_NOT_AVAILABLE);

  return CallQueryInterface(common, aCommonAncestor);
}

// static
nsINode*
nsContentUtils::GetCommonAncestor(nsINode* aNode1,
                                  nsINode* aNode2)
{
  if (aNode1 == aNode2) {
    return aNode1;
  }

  // Build the chain of parents
  nsAutoTPtrArray<nsINode, 30> parents1, parents2;
  do {
    parents1.AppendElement(aNode1);
    aNode1 = aNode1->GetNodeParent();
  } while (aNode1);
  do {
    parents2.AppendElement(aNode2);
    aNode2 = aNode2->GetNodeParent();
  } while (aNode2);

  // Find where the parent chain differs
  PRUint32 pos1 = parents1.Length();
  PRUint32 pos2 = parents2.Length();
  nsINode* parent = nsnull;
  PRUint32 len;
  for (len = NS_MIN(pos1, pos2); len > 0; --len) {
    nsINode* child1 = parents1.ElementAt(--pos1);
    nsINode* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      break;
    }
    parent = child1;
  }

  return parent;
}

/* static */
PRInt32
nsContentUtils::ComparePoints(nsINode* aParent1, PRInt32 aOffset1,
                              nsINode* aParent2, PRInt32 aOffset2,
                              PRBool* aDisconnected)
{
  if (aParent1 == aParent2) {
    return aOffset1 < aOffset2 ? -1 :
           aOffset1 > aOffset2 ? 1 :
           0;
  }

  nsAutoTArray<nsINode*, 32> parents1, parents2;
  nsINode* node1 = aParent1;
  nsINode* node2 = aParent2;
  do {
    parents1.AppendElement(node1);
    node1 = node1->GetNodeParent();
  } while (node1);
  do {
    parents2.AppendElement(node2);
    node2 = node2->GetNodeParent();
  } while (node2);

  PRUint32 pos1 = parents1.Length() - 1;
  PRUint32 pos2 = parents2.Length() - 1;
  
  PRBool disconnected = parents1.ElementAt(pos1) != parents2.ElementAt(pos2);
  if (aDisconnected) {
    *aDisconnected = disconnected;
  }
  if (disconnected) {
    NS_ASSERTION(aDisconnected, "unexpected disconnected nodes");
    return 1;
  }

  // Find where the parent chains differ
  nsINode* parent = parents1.ElementAt(pos1);
  PRUint32 len;
  for (len = NS_MIN(pos1, pos2); len > 0; --len) {
    nsINode* child1 = parents1.ElementAt(--pos1);
    nsINode* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      return parent->IndexOf(child1) < parent->IndexOf(child2) ? -1 : 1;
    }
    parent = child1;
  }

  
  // The parent chains never differed, so one of the nodes is an ancestor of
  // the other

  NS_ASSERTION(!pos1 || !pos2,
               "should have run out of parent chain for one of the nodes");

  if (!pos1) {
    nsINode* child2 = parents2.ElementAt(--pos2);
    return aOffset1 <= parent->IndexOf(child2) ? -1 : 1;
  }

  nsINode* child1 = parents1.ElementAt(--pos1);
  return parent->IndexOf(child1) < aOffset2 ? -1 : 1;
}

nsIContent*
nsContentUtils::FindFirstChildWithResolvedTag(nsIContent* aParent,
                                              PRInt32 aNamespace,
                                              nsIAtom* aTag)
{
  nsIDocument* doc;
  if (!aParent || !(doc = aParent->GetOwnerDoc())) {
    return nsnull;
  }
  
  nsBindingManager* bindingManager = doc->BindingManager();

  PRInt32 namespaceID;
  PRUint32 count = aParent->GetChildCount();

  PRUint32 i;

  for (i = 0; i < count; i++) {
    nsIContent *child = aParent->GetChildAt(i);
    nsIAtom* tag =  bindingManager->ResolveTag(child, &namespaceID);
    if (tag == aTag && namespaceID == aNamespace) {
      return child;
    }
  }

  // now look for children in XBL
  nsCOMPtr<nsIDOMNodeList> children;
  bindingManager->GetXBLChildNodesFor(aParent, getter_AddRefs(children));
  if (!children) {
    return nsnull;
  }

  PRUint32 length;
  children->GetLength(&length);
  for (i = 0; i < length; i++) {
    nsCOMPtr<nsIDOMNode> childNode;
    children->Item(i, getter_AddRefs(childNode));
    nsCOMPtr<nsIContent> childContent = do_QueryInterface(childNode);
    nsIAtom* tag = bindingManager->ResolveTag(childContent, &namespaceID);
    if (tag == aTag && namespaceID == aNamespace) {
      return childContent;
    }
  }

  return nsnull;
}

inline PRBool
IsCharInSet(const char* aSet,
            const PRUnichar aChar)
{
  PRUnichar ch;
  while ((ch = *aSet)) {
    if (aChar == PRUnichar(ch)) {
      return PR_TRUE;
    }
    ++aSet;
  }
  return PR_FALSE;
}

/**
 * This method strips leading/trailing chars, in given set, from string.
 */

// static
const nsDependentSubstring
nsContentUtils::TrimCharsInSet(const char* aSet,
                               const nsAString& aValue)
{
  nsAString::const_iterator valueCurrent, valueEnd;

  aValue.BeginReading(valueCurrent);
  aValue.EndReading(valueEnd);

  // Skip characters in the beginning
  while (valueCurrent != valueEnd) {
    if (!IsCharInSet(aSet, *valueCurrent)) {
      break;
    }
    ++valueCurrent;
  }

  if (valueCurrent != valueEnd) {
    for (;;) {
      --valueEnd;
      if (!IsCharInSet(aSet, *valueEnd)) {
        break;
      }
    }
    ++valueEnd; // Step beyond the last character we want in the value.
  }

  // valueEnd should point to the char after the last to copy
  return Substring(valueCurrent, valueEnd);
}

/**
 * This method strips leading and trailing whitespace from a string.
 */

// static
template<PRBool IsWhitespace(PRUnichar)>
const nsDependentSubstring
nsContentUtils::TrimWhitespace(const nsAString& aStr, PRBool aTrimTrailing)
{
  nsAString::const_iterator start, end;

  aStr.BeginReading(start);
  aStr.EndReading(end);

  // Skip whitespace characters in the beginning
  while (start != end && IsWhitespace(*start)) {
    ++start;
  }

  if (aTrimTrailing) {
    // Skip whitespace characters in the end.
    while (end != start) {
      --end;

      if (!IsWhitespace(*end)) {
        // Step back to the last non-whitespace character.
        ++end;

        break;
      }
    }
  }

  // Return a substring for the string w/o leading and/or trailing
  // whitespace

  return Substring(start, end);
}

// Declaring the templates we are going to use avoid linking issues without
// inlining the method. Considering there is not so much spaces checking
// methods we can consider this to be better than inlining.
template
const nsDependentSubstring
nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(const nsAString&, PRBool);
template
const nsDependentSubstring
nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(const nsAString&, PRBool);

static inline void KeyAppendSep(nsACString& aKey)
{
  if (!aKey.IsEmpty()) {
    aKey.Append('>');
  }
}

static inline void KeyAppendString(const nsAString& aString, nsACString& aKey)
{
  KeyAppendSep(aKey);

  // Could escape separator here if collisions happen.  > is not a legal char
  // for a name or type attribute, so we should be safe avoiding that extra work.

  AppendUTF16toUTF8(aString, aKey);
}

static inline void KeyAppendString(const nsACString& aString, nsACString& aKey)
{
  KeyAppendSep(aKey);

  // Could escape separator here if collisions happen.  > is not a legal char
  // for a name or type attribute, so we should be safe avoiding that extra work.

  aKey.Append(aString);
}

static inline void KeyAppendInt(PRInt32 aInt, nsACString& aKey)
{
  KeyAppendSep(aKey);

  aKey.Append(nsPrintfCString("%d", aInt));
}

static inline void KeyAppendAtom(nsIAtom* aAtom, nsACString& aKey)
{
  NS_PRECONDITION(aAtom, "KeyAppendAtom: aAtom can not be null!\n");

  KeyAppendString(nsAtomCString(aAtom), aKey);
}

static inline PRBool IsAutocompleteOff(const nsIContent* aElement)
{
  return aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::autocomplete,
                               NS_LITERAL_STRING("off"), eIgnoreCase);
}

/*static*/ nsresult
nsContentUtils::GenerateStateKey(nsIContent* aContent,
                                 const nsIDocument* aDocument,
                                 nsIStatefulFrame::SpecialStateID aID,
                                 nsACString& aKey)
{
  aKey.Truncate();

  PRUint32 partID = aDocument ? aDocument->GetPartID() : 0;

  // SpecialStateID case - e.g. scrollbars around the content window
  // The key in this case is a special state id
  if (nsIStatefulFrame::eNoID != aID) {
    KeyAppendInt(partID, aKey);  // first append a partID
    KeyAppendInt(aID, aKey);
    return NS_OK;
  }

  // We must have content if we're not using a special state id
  NS_ENSURE_TRUE(aContent, NS_ERROR_FAILURE);

  // Don't capture state for anonymous content
  if (aContent->IsInAnonymousSubtree()) {
    return NS_OK;
  }

  if (IsAutocompleteOff(aContent)) {
    return NS_OK;
  }

  nsCOMPtr<nsIHTMLDocument> htmlDocument(do_QueryInterface(aContent->GetCurrentDoc()));

  KeyAppendInt(partID, aKey);  // first append a partID
  // Make sure we can't possibly collide with an nsIStatefulFrame
  // special id of some sort
  KeyAppendInt(nsIStatefulFrame::eNoID, aKey);
  PRBool generatedUniqueKey = PR_FALSE;

  if (htmlDocument) {
    // Flush our content model so it'll be up to date
    // If this becomes unnecessary and the following line is removed,
    // please also remove the corresponding flush operation from
    // nsHtml5TreeBuilderCppSupplement.h. (Look for "See bug 497861." there.)
    aContent->GetCurrentDoc()->FlushPendingNotifications(Flush_Content);

    nsContentList *htmlForms = htmlDocument->GetForms();
    nsContentList *htmlFormControls = htmlDocument->GetFormControls();

    NS_ENSURE_TRUE(htmlForms && htmlFormControls, NS_ERROR_OUT_OF_MEMORY);

    // If we have a form control and can calculate form information, use that
    // as the key - it is more reliable than just recording position in the
    // DOM.
    // XXXbz Is it, really?  We have bugs on this, I think...
    // Important to have a unique key, and tag/type/name may not be.
    //
    // If the control has a form, the format of the key is:
    // f>type>IndOfFormInDoc>IndOfControlInForm>FormName>name
    // else:
    // d>type>IndOfControlInDoc>name
    //
    // XXX We don't need to use index if name is there
    // XXXbz We don't?  Why not?  I don't follow.
    //
    nsCOMPtr<nsIFormControl> control(do_QueryInterface(aContent));
    if (control && htmlFormControls && htmlForms) {

      // Append the control type
      KeyAppendInt(control->GetType(), aKey);

      // If in a form, add form name / index of form / index in form
      PRInt32 index = -1;
      Element *formElement = control->GetFormElement();
      if (formElement) {
        if (IsAutocompleteOff(formElement)) {
          aKey.Truncate();
          return NS_OK;
        }

        KeyAppendString(NS_LITERAL_CSTRING("f"), aKey);

        // Append the index of the form in the document
        index = htmlForms->IndexOf(formElement, PR_FALSE);
        if (index <= -1) {
          //
          // XXX HACK this uses some state that was dumped into the document
          // specifically to fix bug 138892.  What we are trying to do is *guess*
          // which form this control's state is found in, with the highly likely
          // guess that the highest form parsed so far is the one.
          // This code should not be on trunk, only branch.
          //
          index = htmlDocument->GetNumFormsSynchronous() - 1;
        }
        if (index > -1) {
          KeyAppendInt(index, aKey);

          // Append the index of the control in the form
          nsCOMPtr<nsIForm> form(do_QueryInterface(formElement));
          index = form->IndexOfControl(control);

          if (index > -1) {
            KeyAppendInt(index, aKey);
            generatedUniqueKey = PR_TRUE;
          }
        }

        // Append the form name
        nsAutoString formName;
        formElement->GetAttr(kNameSpaceID_None, nsGkAtoms::name, formName);
        KeyAppendString(formName, aKey);

      } else {

        KeyAppendString(NS_LITERAL_CSTRING("d"), aKey);

        // If not in a form, add index of control in document
        // Less desirable than indexing by form info.

        // Hash by index of control in doc (we are not in a form)
        // These are important as they are unique, and type/name may not be.

        // We have to flush sink notifications at this point to make
        // sure that htmlFormControls is up to date.
        index = htmlFormControls->IndexOf(aContent, PR_TRUE);
        if (index > -1) {
          KeyAppendInt(index, aKey);
          generatedUniqueKey = PR_TRUE;
        }
      }

      // Append the control name
      nsAutoString name;
      aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
      KeyAppendString(name, aKey);
    }
  }

  if (!generatedUniqueKey) {
    // Either we didn't have a form control or we aren't in an HTML document so
    // we can't figure out form info.  First append a character that is not "d"
    // or "f" to disambiguate from the case when we were a form control in an
    // HTML document.
    KeyAppendString(NS_LITERAL_CSTRING("o"), aKey);
    
    // Now start at aContent and append the indices of it and all its ancestors
    // in their containers.  That should at least pin down its position in the
    // DOM...
    nsINode* parent = aContent->GetNodeParent();
    nsINode* content = aContent;
    while (parent) {
      KeyAppendInt(parent->IndexOf(content), aKey);
      content = parent;
      parent = content->GetNodeParent();
    }
  }

  return NS_OK;
}

// static
nsresult
nsContentUtils::NewURIWithDocumentCharset(nsIURI** aResult,
                                          const nsAString& aSpec,
                                          nsIDocument* aDocument,
                                          nsIURI* aBaseURI)
{
  return NS_NewURI(aResult, aSpec,
                   aDocument ? aDocument->GetDocumentCharacterSet().get() : nsnull,
                   aBaseURI, sIOService);
}

// static
PRBool
nsContentUtils::BelongsInForm(nsIDOMHTMLFormElement *aForm,
                              nsIContent *aContent)
{
  NS_PRECONDITION(aForm, "Must have a form");
  NS_PRECONDITION(aContent, "Must have a content node");

  nsCOMPtr<nsIContent> form(do_QueryInterface(aForm));

  if (!form) {
    NS_ERROR("This should not happen, form is not an nsIContent!");

    return PR_TRUE;
  }

  if (form == aContent) {
    // A form does not belong inside itself, so we return false here

    return PR_FALSE;
  }

  nsIContent* content = aContent->GetParent();

  while (content) {
    if (content == form) {
      // aContent is contained within the form so we return true.

      return PR_TRUE;
    }

    if (content->Tag() == nsGkAtoms::form &&
        content->IsHTML()) {
      // The child is contained within a form, but not the right form
      // so we ignore it.

      return PR_FALSE;
    }

    content = content->GetParent();
  }

  if (form->GetChildCount() > 0) {
    // The form is a container but aContent wasn't inside the form,
    // return false

    return PR_FALSE;
  }

  // The form is a leaf and aContent wasn't inside any other form so
  // we check whether the content comes after the form.  If it does,
  // return true.  If it does not, then it couldn't have been inside
  // the form in the HTML.
  if (PositionIsBefore(form, aContent)) {
    // We could be in this form!
    // In the future, we may want to get document.forms, look at the
    // form after aForm, and if aContent is after that form after
    // aForm return false here....
    return PR_TRUE;
  }

  return PR_FALSE;
}

// static
nsresult
nsContentUtils::CheckQName(const nsAString& aQualifiedName,
                           PRBool aNamespaceAware)
{
  nsIParserService *parserService = GetParserService();
  NS_ENSURE_TRUE(parserService, NS_ERROR_FAILURE);

  const PRUnichar *colon;
  return parserService->CheckQName(PromiseFlatString(aQualifiedName),
                                   aNamespaceAware, &colon);
}

//static
nsresult
nsContentUtils::SplitQName(const nsIContent* aNamespaceResolver,
                           const nsAFlatString& aQName,
                           PRInt32 *aNamespace, nsIAtom **aLocalName)
{
  nsIParserService* parserService = GetParserService();
  NS_ENSURE_TRUE(parserService, NS_ERROR_FAILURE);

  const PRUnichar* colon;
  nsresult rv = parserService->CheckQName(aQName, PR_TRUE, &colon);
  NS_ENSURE_SUCCESS(rv, rv);

  if (colon) {
    const PRUnichar* end;
    aQName.EndReading(end);
    nsAutoString nameSpace;
    rv = aNamespaceResolver->LookupNamespaceURI(Substring(aQName.get(), colon),
                                                nameSpace);
    NS_ENSURE_SUCCESS(rv, rv);

    *aNamespace = NameSpaceManager()->GetNameSpaceID(nameSpace);
    if (*aNamespace == kNameSpaceID_Unknown)
      return NS_ERROR_FAILURE;

    *aLocalName = NS_NewAtom(Substring(colon + 1, end));
  }
  else {
    *aNamespace = kNameSpaceID_None;
    *aLocalName = NS_NewAtom(aQName);
  }
  NS_ENSURE_TRUE(aLocalName, NS_ERROR_OUT_OF_MEMORY);
  return NS_OK;
}

// static
nsresult
nsContentUtils::GetNodeInfoFromQName(const nsAString& aNamespaceURI,
                                     const nsAString& aQualifiedName,
                                     nsNodeInfoManager* aNodeInfoManager,
                                     nsINodeInfo** aNodeInfo)
{
  nsIParserService* parserService = GetParserService();
  NS_ENSURE_TRUE(parserService, NS_ERROR_FAILURE);

  const nsAFlatString& qName = PromiseFlatString(aQualifiedName);
  const PRUnichar* colon;
  nsresult rv = parserService->CheckQName(qName, PR_TRUE, &colon);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 nsID;
  sNameSpaceManager->RegisterNameSpace(aNamespaceURI, nsID);
  if (colon) {
    const PRUnichar* end;
    qName.EndReading(end);

    nsCOMPtr<nsIAtom> prefix = do_GetAtom(Substring(qName.get(), colon));

    rv = aNodeInfoManager->GetNodeInfo(Substring(colon + 1, end), prefix,
                                       nsID, aNodeInfo);
  }
  else {
    rv = aNodeInfoManager->GetNodeInfo(aQualifiedName, nsnull, nsID,
                                       aNodeInfo);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return nsContentUtils::IsValidNodeName((*aNodeInfo)->NameAtom(),
                                         (*aNodeInfo)->GetPrefixAtom(),
                                         (*aNodeInfo)->NamespaceID()) ?
         NS_OK : NS_ERROR_DOM_NAMESPACE_ERR;
}

// static
void
nsContentUtils::SplitExpatName(const PRUnichar *aExpatName, nsIAtom **aPrefix,
                               nsIAtom **aLocalName, PRInt32* aNameSpaceID)
{
  /**
   *  Expat can send the following:
   *    localName
   *    namespaceURI<separator>localName
   *    namespaceURI<separator>localName<separator>prefix
   *
   *  and we use 0xFFFF for the <separator>.
   *
   */

  const PRUnichar *uriEnd = nsnull;
  const PRUnichar *nameEnd = nsnull;
  const PRUnichar *pos;
  for (pos = aExpatName; *pos; ++pos) {
    if (*pos == 0xFFFF) {
      if (uriEnd) {
        nameEnd = pos;
      }
      else {
        uriEnd = pos;
      }
    }
  }

  const PRUnichar *nameStart;
  if (uriEnd) {
    if (sNameSpaceManager) {
      sNameSpaceManager->RegisterNameSpace(nsDependentSubstring(aExpatName,
                                                                uriEnd),
                                           *aNameSpaceID);
    }
    else {
      *aNameSpaceID = kNameSpaceID_Unknown;
    }

    nameStart = (uriEnd + 1);
    if (nameEnd)  {
      const PRUnichar *prefixStart = nameEnd + 1;
      *aPrefix = NS_NewAtom(Substring(prefixStart, pos));
    }
    else {
      nameEnd = pos;
      *aPrefix = nsnull;
    }
  }
  else {
    *aNameSpaceID = kNameSpaceID_None;
    nameStart = aExpatName;
    nameEnd = pos;
    *aPrefix = nsnull;
  }
  *aLocalName = NS_NewAtom(Substring(nameStart, nameEnd));
}

// static
nsPresContext*
nsContentUtils::GetContextForContent(const nsIContent* aContent)
{
  nsIDocument* doc = aContent->GetCurrentDoc();
  if (doc) {
    nsIPresShell *presShell = doc->GetShell();
    if (presShell) {
      return presShell->GetPresContext();
    }
  }
  return nsnull;
}

// static
PRBool
nsContentUtils::CanLoadImage(nsIURI* aURI, nsISupports* aContext,
                             nsIDocument* aLoadingDocument,
                             nsIPrincipal* aLoadingPrincipal,
                             PRInt16* aImageBlockingStatus)
{
  NS_PRECONDITION(aURI, "Must have a URI");
  NS_PRECONDITION(aLoadingDocument, "Must have a document");
  NS_PRECONDITION(aLoadingPrincipal, "Must have a loading principal");

  nsresult rv;

  PRUint32 appType = nsIDocShell::APP_TYPE_UNKNOWN;

  {
    nsCOMPtr<nsISupports> container = aLoadingDocument->GetContainer();
    nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
      do_QueryInterface(container);

    if (docShellTreeItem) {
      nsCOMPtr<nsIDocShellTreeItem> root;
      docShellTreeItem->GetRootTreeItem(getter_AddRefs(root));

      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(root));

      if (!docShell || NS_FAILED(docShell->GetAppType(&appType))) {
        appType = nsIDocShell::APP_TYPE_UNKNOWN;
      }
    }
  }

  if (appType != nsIDocShell::APP_TYPE_EDITOR) {
    // Editor apps get special treatment here, editors can load images
    // from anywhere.  This allows editor to insert images from file://
    // into documents that are being edited.
    rv = sSecurityManager->
      CheckLoadURIWithPrincipal(aLoadingPrincipal, aURI,
                                nsIScriptSecurityManager::ALLOW_CHROME);
    if (NS_FAILED(rv)) {
      if (aImageBlockingStatus) {
        // Reject the request itself, not all requests to the relevant
        // server...
        *aImageBlockingStatus = nsIContentPolicy::REJECT_REQUEST;
      }
      return PR_FALSE;
    }
  }

  PRInt16 decision = nsIContentPolicy::ACCEPT;

  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_IMAGE,
                                 aURI,
                                 aLoadingPrincipal,
                                 aContext,
                                 EmptyCString(), //mime guess
                                 nsnull,         //extra
                                 &decision,
                                 GetContentPolicy(),
                                 sSecurityManager);

  if (aImageBlockingStatus) {
    *aImageBlockingStatus =
      NS_FAILED(rv) ? nsIContentPolicy::REJECT_REQUEST : decision;
  }
  return NS_FAILED(rv) ? PR_FALSE : NS_CP_ACCEPTED(decision);
}

// static
PRBool
nsContentUtils::IsImageInCache(nsIURI* aURI)
{
    if (!sImgLoaderInitialized)
        InitImgLoader();

    if (!sImgCache) return PR_FALSE;

    // If something unexpected happened we return false, otherwise if props
    // is set, the image is cached and we return true
    nsCOMPtr<nsIProperties> props;
    nsresult rv = sImgCache->FindEntryProperties(aURI, getter_AddRefs(props));
    return (NS_SUCCEEDED(rv) && props);
}

// static
nsresult
nsContentUtils::LoadImage(nsIURI* aURI, nsIDocument* aLoadingDocument,
                          nsIPrincipal* aLoadingPrincipal, nsIURI* aReferrer,
                          imgIDecoderObserver* aObserver, PRInt32 aLoadFlags,
                          imgIRequest** aRequest)
{
  NS_PRECONDITION(aURI, "Must have a URI");
  NS_PRECONDITION(aLoadingDocument, "Must have a document");
  NS_PRECONDITION(aLoadingPrincipal, "Must have a principal");
  NS_PRECONDITION(aRequest, "Null out param");

  imgILoader* imgLoader = GetImgLoader();
  if (!imgLoader) {
    // nothing we can do here
    return NS_OK;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = aLoadingDocument->GetDocumentLoadGroup();
  NS_ASSERTION(loadGroup, "Could not get loadgroup; onload may fire too early");

  nsIURI *documentURI = aLoadingDocument->GetDocumentURI();

  // check for a Content Security Policy to pass down to the channel that
  // will get created to load the image
  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  if (aLoadingPrincipal) {
    nsresult rv = aLoadingPrincipal->GetCsp(getter_AddRefs(csp));
    NS_ENSURE_SUCCESS(rv, rv);
    if (csp) {
      channelPolicy = do_CreateInstance("@mozilla.org/nschannelpolicy;1");
      channelPolicy->SetContentSecurityPolicy(csp);
      channelPolicy->SetLoadType(nsIContentPolicy::TYPE_IMAGE);
    }
  }
    
  // Make the URI immutable so people won't change it under us
  NS_TryToSetImmutable(aURI);

  // XXXbz using "documentURI" for the initialDocumentURI is not quite
  // right, but the best we can do here...
  return imgLoader->LoadImage(aURI,                 /* uri to load */
                              documentURI,          /* initialDocumentURI */
                              aReferrer,            /* referrer */
                              loadGroup,            /* loadgroup */
                              aObserver,            /* imgIDecoderObserver */
                              aLoadingDocument,     /* uniquification key */
                              aLoadFlags,           /* load flags */
                              nsnull,               /* cache key */
                              nsnull,               /* existing request*/
                              channelPolicy,        /* CSP info */
                              aRequest);
}

// static
already_AddRefed<imgIContainer>
nsContentUtils::GetImageFromContent(nsIImageLoadingContent* aContent,
                                    imgIRequest **aRequest)
{
  if (aRequest) {
    *aRequest = nsnull;
  }

  NS_ENSURE_TRUE(aContent, nsnull);

  nsCOMPtr<imgIRequest> imgRequest;
  aContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                      getter_AddRefs(imgRequest));
  if (!imgRequest) {
    return nsnull;
  }

  nsCOMPtr<imgIContainer> imgContainer;
  imgRequest->GetImage(getter_AddRefs(imgContainer));

  if (!imgContainer) {
    return nsnull;
  }

  if (aRequest) {
    imgRequest.swap(*aRequest);
  }

  return imgContainer.forget();
}

//static
already_AddRefed<imgIRequest>
nsContentUtils::GetStaticRequest(imgIRequest* aRequest)
{
  NS_ENSURE_TRUE(aRequest, nsnull);
  nsCOMPtr<imgIRequest> retval;
  aRequest->GetStaticRequest(getter_AddRefs(retval));
  return retval.forget();
}

// static
PRBool
nsContentUtils::ContentIsDraggable(nsIContent* aContent)
{
  nsCOMPtr<nsIDOMNSHTMLElement> htmlElement = do_QueryInterface(aContent);
  if (htmlElement) {
    PRBool draggable = PR_FALSE;
    htmlElement->GetDraggable(&draggable);
    if (draggable)
      return PR_TRUE;

    if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::draggable,
                              nsGkAtoms::_false, eIgnoreCase))
      return PR_FALSE;
  }

  // special handling for content area image and link dragging
  return IsDraggableImage(aContent) || IsDraggableLink(aContent);
}

// static
PRBool
nsContentUtils::IsDraggableImage(nsIContent* aContent)
{
  NS_PRECONDITION(aContent, "Must have content node to test");

  nsCOMPtr<nsIImageLoadingContent> imageContent(do_QueryInterface(aContent));
  if (!imageContent) {
    return PR_FALSE;
  }

  nsCOMPtr<imgIRequest> imgRequest;
  imageContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                           getter_AddRefs(imgRequest));

  // XXXbz It may be draggable even if the request resulted in an error.  Why?
  // Not sure; that's what the old nsContentAreaDragDrop/nsFrame code did.
  return imgRequest != nsnull;
}

// static
PRBool
nsContentUtils::IsDraggableLink(const nsIContent* aContent) {
  nsCOMPtr<nsIURI> absURI;
  return aContent->IsLink(getter_AddRefs(absURI));
}

// static
nsAdoptingCString
nsContentUtils::GetCharPref(const char *aPref)
{
  nsAdoptingCString result;

  if (sPrefBranch) {
    sPrefBranch->GetCharPref(aPref, getter_Copies(result));
  }

  return result;
}

// static
PRPackedBool
nsContentUtils::GetBoolPref(const char *aPref, PRBool aDefault)
{
  PRBool result;

  if (!sPrefBranch ||
      NS_FAILED(sPrefBranch->GetBoolPref(aPref, &result))) {
    result = aDefault;
  }

  return (PRPackedBool)result;
}

// static
PRInt32
nsContentUtils::GetIntPref(const char *aPref, PRInt32 aDefault)
{
  PRInt32 result;

  if (!sPrefBranch ||
      NS_FAILED(sPrefBranch->GetIntPref(aPref, &result))) {
    result = aDefault;
  }

  return result;
}

// static
nsAdoptingString
nsContentUtils::GetLocalizedStringPref(const char *aPref)
{
  nsAdoptingString result;

  if (sPrefBranch) {
    nsCOMPtr<nsIPrefLocalizedString> prefLocalString;
    sPrefBranch->GetComplexValue(aPref, NS_GET_IID(nsIPrefLocalizedString),
                                 getter_AddRefs(prefLocalString));
    if (prefLocalString) {
      prefLocalString->GetData(getter_Copies(result));
    }
  }

  return result;
}

// static
nsAdoptingString
nsContentUtils::GetStringPref(const char *aPref)
{
  nsAdoptingString result;

  if (sPrefBranch) {
    nsCOMPtr<nsISupportsString> theString;
    sPrefBranch->GetComplexValue(aPref, NS_GET_IID(nsISupportsString),
                                 getter_AddRefs(theString));
    if (theString) {
      theString->ToString(getter_Copies(result));
    }
  }

  return result;
}

// RegisterPrefCallback/UnregisterPrefCallback are for backward compatiblity
// with c-style observers.

// static
void
nsContentUtils::RegisterPrefCallback(const char *aPref,
                                     PrefChangedFunc aCallback,
                                     void * aClosure)
{
  if (sPrefBranch) {
    if (!sPrefCallbackTable) {
      sPrefCallbackTable = 
        new nsRefPtrHashtable<nsPrefObserverHashKey, nsPrefOldCallback>();
      sPrefCallbackTable->Init();
    }

    nsPrefObserverHashKey hashKey(aPref, aCallback);
    nsRefPtr<nsPrefOldCallback> callback;
    sPrefCallbackTable->Get(&hashKey, getter_AddRefs(callback));
    if (callback) {
      callback->AppendClosure(aClosure);
      return;
    }

    callback = new nsPrefOldCallback(aPref, aCallback);
    callback->AppendClosure(aClosure);
    if (NS_SUCCEEDED(sPrefBranch->AddObserver(aPref, callback, PR_FALSE))) {
      sPrefCallbackTable->Put(callback, callback);
    }
  }
}

// static
void
nsContentUtils::UnregisterPrefCallback(const char *aPref,
                                       PrefChangedFunc aCallback,
                                       void * aClosure)
{
  if (sPrefBranch) {
    if (!sPrefCallbackTable) {
      return;
    }

    nsPrefObserverHashKey hashKey(aPref, aCallback);
    nsRefPtr<nsPrefOldCallback> callback;
    sPrefCallbackTable->Get(&hashKey, getter_AddRefs(callback));

    if (callback) {
      callback->RemoveClosure(aClosure);
      if (callback->HasNoClosures()) {
        // Delete the callback since its list of closures is empty.
        sPrefCallbackTable->Remove(callback);
      }
    }
  }
}

static int
BoolVarChanged(const char *aPref, void *aClosure)
{
  PrefCacheData* cache = static_cast<PrefCacheData*>(aClosure);
  *((PRBool*)cache->cacheLocation) =
    nsContentUtils::GetBoolPref(aPref, cache->defaultValueBool);
  
  return 0;
}

void
nsContentUtils::AddBoolPrefVarCache(const char *aPref,
                                    PRBool* aCache,
                                    PRBool aDefault)
{
  *aCache = GetBoolPref(aPref, aDefault);
  PrefCacheData* data = new PrefCacheData;
  data->cacheLocation = aCache;
  data->defaultValueBool = aDefault;
  sPrefCacheData->AppendElement(data);
  RegisterPrefCallback(aPref, BoolVarChanged, data);
}

static int
IntVarChanged(const char *aPref, void *aClosure)
{
  PrefCacheData* cache = static_cast<PrefCacheData*>(aClosure);
  *((PRInt32*)cache->cacheLocation) =
    nsContentUtils::GetIntPref(aPref, cache->defaultValueInt);
  
  return 0;
}

void
nsContentUtils::AddIntPrefVarCache(const char *aPref,
                                   PRInt32* aCache,
                                   PRInt32 aDefault)
{
  *aCache = GetIntPref(aPref, aDefault);
  PrefCacheData* data = new PrefCacheData;
  data->cacheLocation = aCache;
  data->defaultValueInt = aDefault;
  sPrefCacheData->AppendElement(data);
  RegisterPrefCallback(aPref, IntVarChanged, data);
}

PRBool
nsContentUtils::IsSitePermAllow(nsIURI* aURI, const char* aType)
{
  nsCOMPtr<nsIPermissionManager> permMgr =
    do_GetService("@mozilla.org/permissionmanager;1");
  NS_ENSURE_TRUE(permMgr, PR_FALSE);

  PRUint32 perm;
  nsresult rv = permMgr->TestPermission(aURI, aType, &perm);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  
  return perm == nsIPermissionManager::ALLOW_ACTION;
}

static const char *gEventNames[] = {"event"};
static const char *gSVGEventNames[] = {"evt"};
// for b/w compat, the first name to onerror is still 'event', even though it
// is actually the error message.  (pre this code, the other 2 were not avail.)
// XXXmarkh - a quick lxr shows no affected code - should we correct this?
static const char *gOnErrorNames[] = {"event", "source", "lineno"};

// static
void
nsContentUtils::GetEventArgNames(PRInt32 aNameSpaceID,
                                 nsIAtom *aEventName,
                                 PRUint32 *aArgCount,
                                 const char*** aArgArray)
{
#define SET_EVENT_ARG_NAMES(names) \
    *aArgCount = sizeof(names)/sizeof(names[0]); \
    *aArgArray = names;

  // nsJSEventListener is what does the arg magic for onerror, and it does
  // not seem to take the namespace into account.  So we let onerror in all
  // namespaces get the 3 arg names.
  if (aEventName == nsGkAtoms::onerror) {
    SET_EVENT_ARG_NAMES(gOnErrorNames);
  } else if (aNameSpaceID == kNameSpaceID_SVG) {
    SET_EVENT_ARG_NAMES(gSVGEventNames);
  } else {
    SET_EVENT_ARG_NAMES(gEventNames);
  }
}

nsCxPusher::nsCxPusher()
    : mScriptIsRunning(PR_FALSE),
      mPushedSomething(PR_FALSE)
{
}

nsCxPusher::~nsCxPusher()
{
  Pop();
}

static PRBool
IsContextOnStack(nsIJSContextStack *aStack, JSContext *aContext)
{
  JSContext *ctx = nsnull;
  aStack->Peek(&ctx);
  if (!ctx)
    return PR_FALSE;
  if (ctx == aContext)
    return PR_TRUE;

  nsCOMPtr<nsIJSContextStackIterator>
    iterator(do_CreateInstance("@mozilla.org/js/xpc/ContextStackIterator;1"));
  NS_ENSURE_TRUE(iterator, PR_FALSE);

  nsresult rv = iterator->Reset(aStack);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  PRBool done;
  while (NS_SUCCEEDED(iterator->Done(&done)) && !done) {
    rv = iterator->Prev(&ctx);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Broken iterator implementation");

    if (!ctx) {
      continue;
    }

    if (nsJSUtils::GetDynamicScriptContext(ctx) && ctx == aContext)
      return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
nsCxPusher::Push(nsPIDOMEventTarget *aCurrentTarget)
{
  if (mPushedSomething) {
    NS_ERROR("Whaaa! No double pushing with nsCxPusher::Push()!");

    return PR_FALSE;
  }

  NS_ENSURE_TRUE(aCurrentTarget, PR_FALSE);
  nsresult rv;
  nsIScriptContext* scx =
    aCurrentTarget->GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  if (!scx) {
    // The target may have a special JS context for event handlers.
    JSContext* cx = aCurrentTarget->GetJSContextForEventHandlers();
    if (cx) {
      DoPush(cx);
    }

    // Nothing to do here, I guess.  Have to return true so that event firing
    // will still work correctly even if there is no associated JSContext
    return PR_TRUE;
  }

  JSContext* cx = nsnull;

  if (scx) {
    cx = static_cast<JSContext*>(scx->GetNativeContext());
    // Bad, no JSContext from script context!
    NS_ENSURE_TRUE(cx, PR_FALSE);
  }

  // If there's no native context in the script context it must be
  // in the process or being torn down. We don't want to notify the
  // script context about scripts having been evaluated in such a
  // case, calling with a null cx is fine in that case.
  return Push(cx);
}

PRBool
nsCxPusher::RePush(nsPIDOMEventTarget *aCurrentTarget)
{
  if (!mPushedSomething) {
    return Push(aCurrentTarget);
  }

  if (aCurrentTarget) {
    nsresult rv;
    nsIScriptContext* scx =
      aCurrentTarget->GetContextForEventHandlers(&rv);
    if (NS_FAILED(rv)) {
      Pop();
      return PR_FALSE;
    }

    // If we have the same script context and native context is still
    // alive, no need to Pop/Push.
    if (scx && scx == mScx &&
        scx->GetNativeContext()) {
      return PR_TRUE;
    }
  }

  Pop();
  return Push(aCurrentTarget);
}

PRBool
nsCxPusher::Push(JSContext *cx, PRBool aRequiresScriptContext)
{
  if (mPushedSomething) {
    NS_ERROR("Whaaa! No double pushing with nsCxPusher::Push()!");

    return PR_FALSE;
  }

  if (!cx) {
    return PR_FALSE;
  }

  // Hold a strong ref to the nsIScriptContext, just in case
  // XXXbz do we really need to?  If we don't get one of these in Pop(), is
  // that really a problem?  Or do we need to do this to effectively root |cx|?
  mScx = GetScriptContextFromJSContext(cx);
  if (!mScx && aRequiresScriptContext) {
    // Should probably return PR_FALSE. See bug 416916.
    return PR_TRUE;
  }

  return DoPush(cx);
}

PRBool
nsCxPusher::DoPush(JSContext* cx)
{
  nsIThreadJSContextStack* stack = nsContentUtils::ThreadJSContextStack();
  if (!stack) {
    return PR_TRUE;
  }

  if (cx && IsContextOnStack(stack, cx)) {
    // If the context is on the stack, that means that a script
    // is running at the moment in the context.
    mScriptIsRunning = PR_TRUE;
  }

  if (NS_FAILED(stack->Push(cx))) {
    mScriptIsRunning = PR_FALSE;
    mScx = nsnull;
    return PR_FALSE;
  }

  mPushedSomething = PR_TRUE;
#ifdef DEBUG
  mPushedContext = cx;
#endif
  return PR_TRUE;
}

PRBool
nsCxPusher::PushNull()
{
  return DoPush(nsnull);
}

void
nsCxPusher::Pop()
{
  nsIThreadJSContextStack* stack = nsContentUtils::ThreadJSContextStack();
  if (!mPushedSomething || !stack) {
    mScx = nsnull;
    mPushedSomething = PR_FALSE;

    NS_ASSERTION(!mScriptIsRunning, "Huh, this can't be happening, "
                 "mScriptIsRunning can't be set here!");

    return;
  }

  JSContext *unused;
  stack->Pop(&unused);

  NS_ASSERTION(unused == mPushedContext, "Unexpected context popped");

  if (!mScriptIsRunning && mScx) {
    // No JS is running in the context, but executing the event handler might have
    // caused some JS to run. Tell the script context that it's done.

    mScx->ScriptEvaluated(PR_TRUE);
  }

  mScx = nsnull;
  mScriptIsRunning = PR_FALSE;
  mPushedSomething = PR_FALSE;
}

static const char gPropertiesFiles[nsContentUtils::PropertiesFile_COUNT][56] = {
  // Must line up with the enum values in |PropertiesFile| enum.
  "chrome://global/locale/css.properties",
  "chrome://global/locale/xbl.properties",
  "chrome://global/locale/xul.properties",
  "chrome://global/locale/layout_errors.properties",
  "chrome://global/locale/layout/HtmlForm.properties",
  "chrome://global/locale/printing.properties",
  "chrome://global/locale/dom/dom.properties",
#ifdef MOZ_SVG
  "chrome://global/locale/svg/svg.properties",
#endif
  "chrome://branding/locale/brand.properties",
  "chrome://global/locale/commonDialogs.properties"
};

/* static */ nsresult
nsContentUtils::EnsureStringBundle(PropertiesFile aFile)
{
  if (!sStringBundles[aFile]) {
    if (!sStringBundleService) {
      nsresult rv =
        CallGetService(NS_STRINGBUNDLE_CONTRACTID, &sStringBundleService);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    nsIStringBundle *bundle;
    nsresult rv =
      sStringBundleService->CreateBundle(gPropertiesFiles[aFile], &bundle);
    NS_ENSURE_SUCCESS(rv, rv);
    sStringBundles[aFile] = bundle; // transfer ownership
  }
  return NS_OK;
}

/* static */
nsresult nsContentUtils::GetLocalizedString(PropertiesFile aFile,
                                            const char* aKey,
                                            nsXPIDLString& aResult)
{
  nsresult rv = EnsureStringBundle(aFile);
  NS_ENSURE_SUCCESS(rv, rv);
  nsIStringBundle *bundle = sStringBundles[aFile];

  return bundle->GetStringFromName(NS_ConvertASCIItoUTF16(aKey).get(),
                                   getter_Copies(aResult));
}

/* static */
nsresult nsContentUtils::FormatLocalizedString(PropertiesFile aFile,
                                               const char* aKey,
                                               const PRUnichar **aParams,
                                               PRUint32 aParamsLength,
                                               nsXPIDLString& aResult)
{
  nsresult rv = EnsureStringBundle(aFile);
  NS_ENSURE_SUCCESS(rv, rv);
  nsIStringBundle *bundle = sStringBundles[aFile];

  return bundle->FormatStringFromName(NS_ConvertASCIItoUTF16(aKey).get(),
                                      aParams, aParamsLength,
                                      getter_Copies(aResult));
}

/* static */ nsresult
nsContentUtils::ReportToConsole(PropertiesFile aFile,
                                const char *aMessageName,
                                const PRUnichar **aParams,
                                PRUint32 aParamsLength,
                                nsIURI* aURI,
                                const nsAFlatString& aSourceLine,
                                PRUint32 aLineNumber,
                                PRUint32 aColumnNumber,
                                PRUint32 aErrorFlags,
                                const char *aCategory)
{
  NS_ASSERTION((aParams && aParamsLength) || (!aParams && !aParamsLength),
               "Supply either both parameters and their number or no"
               "parameters and 0.");

  nsresult rv;
  if (!sConsoleService) { // only need to bother null-checking here
    rv = CallGetService(NS_CONSOLESERVICE_CONTRACTID, &sConsoleService);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsXPIDLString errorText;
  if (aParams) {
    rv = FormatLocalizedString(aFile, aMessageName, aParams, aParamsLength,
                               errorText);
  }
  else {
    rv = GetLocalizedString(aFile, aMessageName, errorText);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString spec;
  if (aURI)
    aURI->GetSpec(spec);

  nsCOMPtr<nsIScriptError> errorObject =
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = errorObject->Init(errorText.get(),
                         NS_ConvertUTF8toUTF16(spec).get(), // file name
                         aSourceLine.get(),
                         aLineNumber, aColumnNumber,
                         aErrorFlags, aCategory);
  NS_ENSURE_SUCCESS(rv, rv);

  return sConsoleService->LogMessage(errorObject);
}

PRBool
nsContentUtils::IsChromeDoc(nsIDocument *aDocument)
{
  if (!aDocument) {
    return PR_FALSE;
  }
  
  nsCOMPtr<nsIPrincipal> systemPrincipal;
  sSecurityManager->GetSystemPrincipal(getter_AddRefs(systemPrincipal));

  return aDocument->NodePrincipal() == systemPrincipal;
}

PRBool
nsContentUtils::IsChildOfSameType(nsIDocument* aDoc)
{
  nsCOMPtr<nsISupports> container = aDoc->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(container));
  nsCOMPtr<nsIDocShellTreeItem> sameTypeParent;
  if (docShellAsItem) {
    docShellAsItem->GetSameTypeParent(getter_AddRefs(sameTypeParent));
  }
  return sameTypeParent != nsnull;
}

PRBool
nsContentUtils::GetWrapperSafeScriptFilename(nsIDocument *aDocument,
                                             nsIURI *aURI,
                                             nsACString& aScriptURI)
{
  PRBool scriptFileNameModified = PR_FALSE;
  aURI->GetSpec(aScriptURI);

  if (IsChromeDoc(aDocument)) {
    nsCOMPtr<nsIChromeRegistry> chromeReg =
      mozilla::services::GetChromeRegistryService();

    if (!chromeReg) {
      // If we're running w/o a chrome registry we won't modify any
      // script file names.

      return scriptFileNameModified;
    }

    PRBool docWrappersEnabled =
      chromeReg->WrappersEnabled(aDocument->GetDocumentURI());

    PRBool uriWrappersEnabled = chromeReg->WrappersEnabled(aURI);

    nsIURI *docURI = aDocument->GetDocumentURI();

    if (docURI && docWrappersEnabled && !uriWrappersEnabled) {
      // aURI is a script from a URL that doesn't get wrapper
      // automation. aDocument is a chrome document that does get
      // wrapper automation. Prepend the chrome document's URI
      // followed by the string " -> " to the URI of the script we're
      // loading here so that script in that URI gets the same wrapper
      // automation that the chrome document expects.
      nsCAutoString spec;
      docURI->GetSpec(spec);
      spec.AppendASCII(" -> ");
      spec.Append(aScriptURI);

      aScriptURI = spec;

      scriptFileNameModified = PR_TRUE;
    }
  }

  return scriptFileNameModified;
}

// static
PRBool
nsContentUtils::IsInChromeDocshell(nsIDocument *aDocument)
{
  if (!aDocument) {
    return PR_FALSE;
  }

  if (aDocument->GetDisplayDocument()) {
    return IsInChromeDocshell(aDocument->GetDisplayDocument());
  }

  nsCOMPtr<nsISupports> docContainer = aDocument->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryInterface(docContainer));
  PRInt32 itemType = nsIDocShellTreeItem::typeContent;
  if (docShell) {
    docShell->GetItemType(&itemType);
  }

  return itemType == nsIDocShellTreeItem::typeChrome;
}

// static
nsIContentPolicy*
nsContentUtils::GetContentPolicy()
{
  if (!sTriedToGetContentPolicy) {
    CallGetService(NS_CONTENTPOLICY_CONTRACTID, &sContentPolicyService);
    // It's OK to not have a content policy service
    sTriedToGetContentPolicy = PR_TRUE;
  }

  return sContentPolicyService;
}

// static
nsresult
nsAutoGCRoot::AddJSGCRoot(void* aPtr, RootType aRootType, const char* aName)
{
  if (!sJSScriptRuntime) {
    nsresult rv = CallGetService("@mozilla.org/js/xpc/RuntimeService;1",
                                 &sJSRuntimeService);
    NS_ENSURE_TRUE(sJSRuntimeService, rv);

    sJSRuntimeService->GetRuntime(&sJSScriptRuntime);
    if (!sJSScriptRuntime) {
      NS_RELEASE(sJSRuntimeService);
      NS_WARNING("Unable to get JS runtime from JS runtime service");
      return NS_ERROR_FAILURE;
    }
  }

  PRBool ok;
  if (aRootType == RootType_JSVal)
    ok = ::js_AddRootRT(sJSScriptRuntime, (jsval *)aPtr, aName);
  else
    ok = ::js_AddGCThingRootRT(sJSScriptRuntime, (void **)aPtr, aName);
  if (!ok) {
    NS_WARNING("JS_AddNamedRootRT failed");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

/* static */
nsresult
nsAutoGCRoot::RemoveJSGCRoot(void* aPtr, RootType aRootType)
{
  if (!sJSScriptRuntime) {
    NS_NOTREACHED("Trying to remove a JS GC root when none were added");
    return NS_ERROR_UNEXPECTED;
  }

  if (aRootType == RootType_JSVal)
    ::js_RemoveRoot(sJSScriptRuntime, (jsval *)aPtr);
  else
    ::js_RemoveRoot(sJSScriptRuntime, (JSObject **)aPtr);

  return NS_OK;
}

// static
PRBool
nsContentUtils::IsEventAttributeName(nsIAtom* aName, PRInt32 aType)
{
  const PRUnichar* name = aName->GetUTF16String();
  if (name[0] != 'o' || name[1] != 'n')
    return PR_FALSE;

  EventNameMapping mapping;
  return (sAtomEventTable->Get(aName, &mapping) && mapping.mType & aType);
}

// static
PRUint32
nsContentUtils::GetEventId(nsIAtom* aName)
{
  EventNameMapping mapping;
  if (sAtomEventTable->Get(aName, &mapping))
    return mapping.mId;

  return NS_USER_DEFINED_EVENT;
}

nsIAtom*
nsContentUtils::GetEventIdAndAtom(const nsAString& aName,
                                  PRUint32 aEventStruct,
                                  PRUint32* aEventID)
{
  EventNameMapping mapping;
  if (sStringEventTable->Get(aName, &mapping)) {
    *aEventID =
      mapping.mStructType == aEventStruct ? mapping.mId : NS_USER_DEFINED_EVENT;
    return mapping.mAtom;
  }

  // If we have cached lots of user defined event names, clear some of them.
  if (sUserDefinedEvents->Count() > 127) {
    while (sUserDefinedEvents->Count() > 64) {
      nsIAtom* first = sUserDefinedEvents->ObjectAt(0);
      sStringEventTable->Remove(Substring(nsDependentAtomString(first), 2));
      sUserDefinedEvents->RemoveObjectAt(0);
    }
  }

  *aEventID = NS_USER_DEFINED_EVENT;
  nsCOMPtr<nsIAtom> atom = do_GetAtom(NS_LITERAL_STRING("on") + aName);
  sUserDefinedEvents->AppendObject(atom);
  mapping.mAtom = atom;
  mapping.mId = NS_USER_DEFINED_EVENT;
  mapping.mType = EventNameType_None;
  mapping.mStructType = NS_EVENT_NULL;
  sStringEventTable->Put(aName, mapping);
  return mapping.mAtom;
}

static
nsresult GetEventAndTarget(nsIDocument* aDoc, nsISupports* aTarget,
                           const nsAString& aEventName,
                           PRBool aCanBubble, PRBool aCancelable,
                           nsIDOMEvent** aEvent,
                           nsIDOMEventTarget** aTargetOut)
{
  nsCOMPtr<nsIDOMDocumentEvent> docEvent(do_QueryInterface(aDoc));
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(aTarget));
  NS_ENSURE_TRUE(docEvent && target, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv =
    docEvent->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(event));
  NS_ENSURE_TRUE(privateEvent, NS_ERROR_FAILURE);

  rv = event->InitEvent(aEventName, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = privateEvent->SetTrusted(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = privateEvent->SetTarget(target);
  NS_ENSURE_SUCCESS(rv, rv);

  event.forget(aEvent);
  target.forget(aTargetOut);
  return NS_OK;
}

// static
nsresult
nsContentUtils::DispatchTrustedEvent(nsIDocument* aDoc, nsISupports* aTarget,
                                     const nsAString& aEventName,
                                     PRBool aCanBubble, PRBool aCancelable,
                                     PRBool *aDefaultAction)
{
  nsCOMPtr<nsIDOMEvent> event;
  nsCOMPtr<nsIDOMEventTarget> target;
  nsresult rv = GetEventAndTarget(aDoc, aTarget, aEventName, aCanBubble,
                                  aCancelable, getter_AddRefs(event),
                                  getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool dummy;
  return target->DispatchEvent(event, aDefaultAction ? aDefaultAction : &dummy);
}

nsresult
nsContentUtils::DispatchChromeEvent(nsIDocument *aDoc,
                                    nsISupports *aTarget,
                                    const nsAString& aEventName,
                                    PRBool aCanBubble, PRBool aCancelable,
                                    PRBool *aDefaultAction)
{

  nsCOMPtr<nsIDOMEvent> event;
  nsCOMPtr<nsIDOMEventTarget> target;
  nsresult rv = GetEventAndTarget(aDoc, aTarget, aEventName, aCanBubble,
                                  aCancelable, getter_AddRefs(event),
                                  getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(aDoc, "GetEventAndTarget lied?");
  if (!aDoc->GetWindow())
    return NS_ERROR_INVALID_ARG;

  nsPIDOMEventTarget* piTarget = aDoc->GetWindow()->GetChromeEventHandler();
  if (!piTarget)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIFrameLoaderOwner> flo = do_QueryInterface(piTarget);
  if (flo) {
    nsRefPtr<nsFrameLoader> fl = flo->GetFrameLoader();
    if (fl) {
      nsPIDOMEventTarget* t = fl->GetTabChildGlobalAsEventTarget();
      piTarget = t ? t : piTarget;
    }
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  rv = piTarget->DispatchDOMEvent(nsnull, event, nsnull, &status);
  if (aDefaultAction) {
    *aDefaultAction = (status != nsEventStatus_eConsumeNoDefault);
  }
  return rv;
}

/* static */
Element*
nsContentUtils::MatchElementId(nsIContent *aContent, const nsIAtom* aId)
{
  for (nsIContent* cur = aContent;
       cur;
       cur = cur->GetNextNode(aContent)) {
    if (aId == cur->GetID()) {
      return cur->AsElement();
    }
  }

  return nsnull;
}

/* static */
Element *
nsContentUtils::MatchElementId(nsIContent *aContent, const nsAString& aId)
{
  NS_PRECONDITION(!aId.IsEmpty(), "Will match random elements");
  
  // ID attrs are generally stored as atoms, so just atomize this up front
  nsCOMPtr<nsIAtom> id(do_GetAtom(aId));
  if (!id) {
    // OOM, so just bail
    return nsnull;
  }

  return MatchElementId(aContent, id);
}

// Convert the string from the given charset to Unicode.
/* static */
nsresult
nsContentUtils::ConvertStringFromCharset(const nsACString& aCharset,
                                         const nsACString& aInput,
                                         nsAString& aOutput)
{
  if (aCharset.IsEmpty()) {
    // Treat the string as UTF8
    CopyUTF8toUTF16(aInput, aOutput);
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsICharsetConverterManager> ccm =
    do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIUnicodeDecoder> decoder;
  rv = ccm->GetUnicodeDecoder(PromiseFlatCString(aCharset).get(),
                              getter_AddRefs(decoder));
  if (NS_FAILED(rv))
    return rv;

  nsPromiseFlatCString flatInput(aInput);
  PRInt32 srcLen = flatInput.Length();
  PRInt32 dstLen;
  rv = decoder->GetMaxLength(flatInput.get(), srcLen, &dstLen);
  if (NS_FAILED(rv))
    return rv;

  PRUnichar *ustr = (PRUnichar *)nsMemory::Alloc((dstLen + 1) *
                                                 sizeof(PRUnichar));
  if (!ustr)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = decoder->Convert(flatInput.get(), &srcLen, ustr, &dstLen);
  if (NS_SUCCEEDED(rv)) {
    ustr[dstLen] = 0;
    aOutput.Assign(ustr, dstLen);
  }

  nsMemory::Free(ustr);
  return rv;
}

/* static */
PRBool
nsContentUtils::CheckForBOM(const unsigned char* aBuffer, PRUint32 aLength,
                            nsACString& aCharset, PRBool *bigEndian)
{
  PRBool found = PR_TRUE;
  aCharset.Truncate();
  if (aLength >= 3 &&
      aBuffer[0] == 0xEF &&
      aBuffer[1] == 0xBB &&
      aBuffer[2] == 0xBF) {
    aCharset = "UTF-8";
  }
  else if (aLength >= 4 &&
           aBuffer[0] == 0x00 &&
           aBuffer[1] == 0x00 &&
           aBuffer[2] == 0xFE &&
           aBuffer[3] == 0xFF) {
    aCharset = "UTF-32";
    if (bigEndian)
      *bigEndian = PR_TRUE;
  }
  else if (aLength >= 4 &&
           aBuffer[0] == 0xFF &&
           aBuffer[1] == 0xFE &&
           aBuffer[2] == 0x00 &&
           aBuffer[3] == 0x00) {
    aCharset = "UTF-32";
    if (bigEndian)
      *bigEndian = PR_FALSE;
  }
  else if (aLength >= 2 &&
           aBuffer[0] == 0xFE && aBuffer[1] == 0xFF) {
    aCharset = "UTF-16";
    if (bigEndian)
      *bigEndian = PR_TRUE;
  }
  else if (aLength >= 2 &&
           aBuffer[0] == 0xFF && aBuffer[1] == 0xFE) {
    aCharset = "UTF-16";
    if (bigEndian)
      *bigEndian = PR_FALSE;
  } else {
    found = PR_FALSE;
  }

  return found;
}

/* static */
nsIContent*
nsContentUtils::GetReferencedElement(nsIURI* aURI, nsIContent *aFromContent)
{
  nsReferencedElement ref;
  ref.Reset(aFromContent, aURI);
  return ref.get();
}

/* static */
void
nsContentUtils::RegisterShutdownObserver(nsIObserver* aObserver)
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(aObserver, 
                                 NS_XPCOM_SHUTDOWN_OBSERVER_ID, 
                                 PR_FALSE);
  }
}

/* static */
void
nsContentUtils::UnregisterShutdownObserver(nsIObserver* aObserver)
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(aObserver, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }
}

/* static */
PRBool
nsContentUtils::HasNonEmptyAttr(const nsIContent* aContent, PRInt32 aNameSpaceID,
                                nsIAtom* aName)
{
  static nsIContent::AttrValuesArray strings[] = {&nsGkAtoms::_empty, nsnull};
  return aContent->FindAttrValueIn(aNameSpaceID, aName, strings, eCaseMatters)
    == nsIContent::ATTR_VALUE_NO_MATCH;
}

/* static */
PRBool
nsContentUtils::HasMutationListeners(nsINode* aNode,
                                     PRUint32 aType,
                                     nsINode* aTargetForSubtreeModified)
{
  nsIDocument* doc = aNode->GetOwnerDoc();
  if (!doc) {
    return PR_FALSE;
  }

  NS_ASSERTION((aNode->IsNodeOfType(nsINode::eCONTENT) &&
                static_cast<nsIContent*>(aNode)->
                  IsInNativeAnonymousSubtree()) ||
               sScriptBlockerCount == sRemovableScriptBlockerCount,
               "Want to fire mutation events, but it's not safe");

  // global object will be null for documents that don't have windows.
  nsPIDOMWindow* window = doc->GetInnerWindow();
  // This relies on nsEventListenerManager::AddEventListener, which sets
  // all mutation bits when there is a listener for DOMSubtreeModified event.
  if (window && !window->HasMutationListeners(aType)) {
    return PR_FALSE;
  }

  if (aNode->IsNodeOfType(nsINode::eCONTENT) &&
      static_cast<nsIContent*>(aNode)->IsInNativeAnonymousSubtree()) {
    return PR_FALSE;
  }

  doc->MayDispatchMutationEvent(aTargetForSubtreeModified);

  // If we have a window, we can check it for mutation listeners now.
  if (aNode->IsInDoc()) {
    nsCOMPtr<nsPIDOMEventTarget> piTarget(do_QueryInterface(window));
    if (piTarget) {
      nsIEventListenerManager* manager = piTarget->GetListenerManager(PR_FALSE);
      if (manager) {
        PRBool hasListeners = PR_FALSE;
        manager->HasMutationListeners(&hasListeners);
        if (hasListeners) {
          return PR_TRUE;
        }
      }
    }
  }

  // If we have a window, we know a mutation listener is registered, but it
  // might not be in our chain.  If we don't have a window, we might have a
  // mutation listener.  Check quickly to see.
  while (aNode) {
    nsIEventListenerManager* manager = aNode->GetListenerManager(PR_FALSE);
    if (manager) {
      PRBool hasListeners = PR_FALSE;
      manager->HasMutationListeners(&hasListeners);
      if (hasListeners) {
        return PR_TRUE;
      }
    }

    if (aNode->IsNodeOfType(nsINode::eCONTENT)) {
      nsIContent* content = static_cast<nsIContent*>(aNode);
      nsIContent* insertionParent =
        doc->BindingManager()->GetInsertionParent(content);
      if (insertionParent) {
        aNode = insertionParent;
        continue;
      }
    }
    aNode = aNode->GetNodeParent();
  }

  return PR_FALSE;
}

/* static */
void
nsContentUtils::TraverseListenerManager(nsINode *aNode,
                                        nsCycleCollectionTraversalCallback &cb)
{
  if (!sEventListenerManagersHash.ops) {
    // We're already shut down, just return.
    return;
  }

  EventListenerManagerMapEntry *entry =
    static_cast<EventListenerManagerMapEntry *>
               (PL_DHashTableOperate(&sEventListenerManagersHash, aNode,
                                        PL_DHASH_LOOKUP));
  if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "[via hash] mListenerManager");
    cb.NoteXPCOMChild(entry->mListenerManager);
  }
}

nsIEventListenerManager*
nsContentUtils::GetListenerManager(nsINode *aNode,
                                   PRBool aCreateIfNotFound)
{
  if (!aCreateIfNotFound && !aNode->HasFlag(NODE_HAS_LISTENERMANAGER)) {
    return nsnull;
  }
  
  if (!sEventListenerManagersHash.ops) {
    // We're already shut down, don't bother creating an event listener
    // manager.

    return nsnull;
  }

  if (!aCreateIfNotFound) {
    EventListenerManagerMapEntry *entry =
      static_cast<EventListenerManagerMapEntry *>
                 (PL_DHashTableOperate(&sEventListenerManagersHash, aNode,
                                          PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      return entry->mListenerManager;
    }
    return nsnull;
  }

  EventListenerManagerMapEntry *entry =
    static_cast<EventListenerManagerMapEntry *>
               (PL_DHashTableOperate(&sEventListenerManagersHash, aNode,
                                        PL_DHASH_ADD));

  if (!entry) {
    return nsnull;
  }

  if (!entry->mListenerManager) {
    nsresult rv =
      NS_NewEventListenerManager(getter_AddRefs(entry->mListenerManager));

    if (NS_FAILED(rv)) {
      PL_DHashTableRawRemove(&sEventListenerManagersHash, entry);

      return nsnull;
    }

    entry->mListenerManager->SetListenerTarget(aNode);

    aNode->SetFlags(NODE_HAS_LISTENERMANAGER);
  }

  return entry->mListenerManager;
}

/* static */
void
nsContentUtils::RemoveListenerManager(nsINode *aNode)
{
  if (sEventListenerManagersHash.ops) {
    EventListenerManagerMapEntry *entry =
      static_cast<EventListenerManagerMapEntry *>
                 (PL_DHashTableOperate(&sEventListenerManagersHash, aNode,
                                          PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      nsCOMPtr<nsIEventListenerManager> listenerManager;
      listenerManager.swap(entry->mListenerManager);
      // Remove the entry and *then* do operations that could cause further
      // modification of sEventListenerManagersHash.  See bug 334177.
      PL_DHashTableRawRemove(&sEventListenerManagersHash, entry);
      if (listenerManager) {
        listenerManager->Disconnect();
      }
    }
  }
}

/* static */
PRBool
nsContentUtils::IsValidNodeName(nsIAtom *aLocalName, nsIAtom *aPrefix,
                                PRInt32 aNamespaceID)
{
  if (aNamespaceID == kNameSpaceID_Unknown) {
    return PR_FALSE;
  }

  if (!aPrefix) {
    // If the prefix is null, then either the QName must be xmlns or the
    // namespace must not be XMLNS.
    return (aLocalName == nsGkAtoms::xmlns) ==
           (aNamespaceID == kNameSpaceID_XMLNS);
  }

  // If the prefix is non-null then the namespace must not be null.
  if (aNamespaceID == kNameSpaceID_None) {
    return PR_FALSE;
  }

  // If the namespace is the XMLNS namespace then the prefix must be xmlns,
  // but the localname must not be xmlns.
  if (aNamespaceID == kNameSpaceID_XMLNS) {
    return aPrefix == nsGkAtoms::xmlns && aLocalName != nsGkAtoms::xmlns;
  }

  // If the namespace is not the XMLNS namespace then the prefix must not be
  // xmlns.
  // If the namespace is the XML namespace then the prefix can be anything.
  // If the namespace is not the XML namespace then the prefix must not be xml.
  return aPrefix != nsGkAtoms::xmlns &&
         (aNamespaceID == kNameSpaceID_XML || aPrefix != nsGkAtoms::xml);
}

/* static */
nsresult
nsContentUtils::CreateContextualFragment(nsINode* aContextNode,
                                         const nsAString& aFragment,
                                         PRBool aWillOwnFragment,
                                         nsIDOMDocumentFragment** aReturn)
{
  *aReturn = nsnull;
  NS_ENSURE_ARG(aContextNode);

  nsresult rv;

  // If we don't have a document here, we can't get the right security context
  // for compiling event handlers... so just bail out.
  nsCOMPtr<nsIDocument> document = aContextNode->GetOwnerDoc();
  NS_ENSURE_TRUE(document, NS_ERROR_NOT_AVAILABLE);

  PRBool isHTML = document->IsHTML();
#ifdef DEBUG
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(document);
  NS_ASSERTION(!isHTML || htmlDoc, "Should have HTMLDocument here!");
#endif

  if (isHTML && nsHtml5Module::sEnabled) {
    // See if the document has a cached fragment parser. nsHTMLDocument is the
    // only one that should really have one at the moment.
    nsCOMPtr<nsIParser> parser = document->GetFragmentParser();
    if (parser) {
      // Get the parser ready to use.
      parser->Reset();
    }
    else {
      // Create a new parser for this operation.
      parser = nsHtml5Module::NewHtml5Parser();
      if (!parser) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    nsCOMPtr<nsIDOMDocumentFragment> frag;
    rv = NS_NewDocumentFragment(getter_AddRefs(frag), document->NodeInfoManager());
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsCOMPtr<nsIContent> contextAsContent = do_QueryInterface(aContextNode);
    if (contextAsContent && !contextAsContent->IsElement()) {
      contextAsContent = contextAsContent->GetParent();
      if (contextAsContent && !contextAsContent->IsElement()) {
        // can this even happen?
        contextAsContent = nsnull;
      }
    }
    
    nsAHtml5FragmentParser* asFragmentParser =
        static_cast<nsAHtml5FragmentParser*> (parser.get());
    nsCOMPtr<nsIContent> fragment = do_QueryInterface(frag);
    if (contextAsContent &&
        !(nsGkAtoms::html == contextAsContent->Tag() &&
          contextAsContent->IsHTML())) {
      asFragmentParser->ParseHtml5Fragment(aFragment,
                                           fragment,
                                           contextAsContent->Tag(),
                                           contextAsContent->GetNameSpaceID(),
                                           (document->GetCompatibilityMode() ==
                                               eCompatibility_NavQuirks),
                                           PR_FALSE);
    } else {
      asFragmentParser->ParseHtml5Fragment(aFragment,
                                           fragment,
                                           nsGkAtoms::body,
                                           kNameSpaceID_XHTML,
                                           (document->GetCompatibilityMode() ==
                                               eCompatibility_NavQuirks),
                                           PR_FALSE);
    }
  
    frag.swap(*aReturn);
    document->SetFragmentParser(parser);
    return NS_OK;
  }

  nsAutoTArray<nsString, 32> tagStack;
  nsAutoString uriStr, nameStr;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aContextNode);
  // just in case we have a text node
  if (content && !content->IsElement())
    content = content->GetParent();

  while (content && content->IsElement()) {
    nsString& tagName = *tagStack.AppendElement();
    NS_ENSURE_TRUE(&tagName, NS_ERROR_OUT_OF_MEMORY);

    content->NodeInfo()->GetQualifiedName(tagName);

    // see if we need to add xmlns declarations
    PRUint32 count = content->GetAttrCount();
    PRBool setDefaultNamespace = PR_FALSE;
    if (count > 0) {
      PRUint32 index;

      for (index = 0; index < count; index++) {
        const nsAttrName* name = content->GetAttrNameAt(index);
        if (name->NamespaceEquals(kNameSpaceID_XMLNS)) {
          content->GetAttr(kNameSpaceID_XMLNS, name->LocalName(), uriStr);

          // really want something like nsXMLContentSerializer::SerializeAttr
          tagName.Append(NS_LITERAL_STRING(" xmlns")); // space important
          if (name->GetPrefix()) {
            tagName.Append(PRUnichar(':'));
            name->LocalName()->ToString(nameStr);
            tagName.Append(nameStr);
          } else {
            setDefaultNamespace = PR_TRUE;
          }
          tagName.Append(NS_LITERAL_STRING("=\"") + uriStr +
            NS_LITERAL_STRING("\""));
        }
      }
    }

    if (!setDefaultNamespace) {
      nsINodeInfo* info = content->NodeInfo();
      if (!info->GetPrefixAtom() &&
          info->NamespaceID() != kNameSpaceID_None) {
        // We have no namespace prefix, but have a namespace ID.  Push
        // default namespace attr in, so that our kids will be in our
        // namespace.
        info->GetNamespaceURI(uriStr);
        tagName.Append(NS_LITERAL_STRING(" xmlns=\"") + uriStr +
                       NS_LITERAL_STRING("\""));
      }
    }

    content = content->GetParent();
  }

  nsCAutoString contentType;
  nsAutoString buf;
  document->GetContentType(buf);
  LossyCopyUTF16toASCII(buf, contentType);

  // See if the document has a cached fragment parser. nsHTMLDocument is the
  // only one that should really have one at the moment.
  nsCOMPtr<nsIParser> parser = document->GetFragmentParser();
  if (parser) {
    // Get the parser ready to use.
    parser->Reset();
  }
  else {
    // Create a new parser for this operation.
    parser = do_CreateInstance(kCParserCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // See if the parser already has a content sink that we can reuse.
  nsCOMPtr<nsIFragmentContentSink> sink;
  nsCOMPtr<nsIContentSink> contentsink = parser->GetContentSink();
  if (contentsink) {
    // Make sure it's the correct type.
    if (isHTML) {
      nsCOMPtr<nsIHTMLContentSink> htmlsink = do_QueryInterface(contentsink);
      sink = do_QueryInterface(htmlsink);
    }
    else {
      nsCOMPtr<nsIXMLContentSink> xmlsink = do_QueryInterface(contentsink);
      sink = do_QueryInterface(xmlsink);
    }
  }

  if (!sink) {
    // Either there was no cached content sink or it was the wrong type. Make a
    // new one.
    if (isHTML) {
      rv = NS_NewHTMLFragmentContentSink(getter_AddRefs(sink));
    } else {
      rv = NS_NewXMLFragmentContentSink(getter_AddRefs(sink));
    }
    NS_ENSURE_SUCCESS(rv, rv);

    contentsink = do_QueryInterface(sink);
    NS_ASSERTION(contentsink, "Sink doesn't QI to nsIContentSink!");

    parser->SetContentSink(contentsink);
  }

  sink->SetTargetDocument(document);

  nsDTDMode mode = eDTDMode_autodetect;
  switch (document->GetCompatibilityMode()) {
    case eCompatibility_NavQuirks:
      mode = eDTDMode_quirks;
      break;
    case eCompatibility_AlmostStandards:
      mode = eDTDMode_almost_standards;
      break;
    case eCompatibility_FullStandards:
      mode = eDTDMode_full_standards;
      break;
    default:
      NS_NOTREACHED("unknown mode");
      break;
  }

  rv = parser->ParseFragment(aFragment, nsnull, tagStack,
                             !isHTML, contentType, mode);
  if (NS_SUCCEEDED(rv)) {
    rv = sink->GetFragment(aWillOwnFragment, aReturn);
  }

  document->SetFragmentParser(parser);

  return rv;
}

/* static */
nsresult
nsContentUtils::CreateDocument(const nsAString& aNamespaceURI, 
                               const nsAString& aQualifiedName, 
                               nsIDOMDocumentType* aDoctype,
                               nsIURI* aDocumentURI, nsIURI* aBaseURI,
                               nsIPrincipal* aPrincipal,
                               nsIScriptGlobalObject* aEventObject,
                               nsIDOMDocument** aResult)
{
  nsresult rv = NS_NewDOMDocument(aResult, aNamespaceURI, aQualifiedName,
                                  aDoctype, aDocumentURI, aBaseURI, aPrincipal,
                                  PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> document = do_QueryInterface(*aResult);
  document->SetScriptHandlingObject(aEventObject);
  
  // created documents are immediately "complete" (ready to use)
  document->SetReadyStateInternal(nsIDocument::READYSTATE_COMPLETE);
  return NS_OK;
}

/* static */
nsresult
nsContentUtils::SetNodeTextContent(nsIContent* aContent,
                                   const nsAString& aValue,
                                   PRBool aTryReuse)
{
  // Might as well stick a batch around this since we're performing several
  // mutations.
  mozAutoDocUpdate updateBatch(aContent->GetCurrentDoc(),
    UPDATE_CONTENT_MODEL, PR_TRUE);

  PRUint32 childCount = aContent->GetChildCount();

  if (aTryReuse && !aValue.IsEmpty()) {
    PRUint32 removeIndex = 0;

    // i is unsigned, so i >= is always true
    for (PRUint32 i = 0; i < childCount; ++i) {
      nsIContent* child = aContent->GetChildAt(removeIndex);
      if (removeIndex == 0 && child && child->IsNodeOfType(nsINode::eTEXT)) {
        nsresult rv = child->SetText(aValue, PR_TRUE);
        NS_ENSURE_SUCCESS(rv, rv);

        removeIndex = 1;
      }
      else {
        aContent->RemoveChildAt(removeIndex, PR_TRUE);
      }
    }
    
    if (removeIndex == 1) {
      return NS_OK;
    }
  }
  else {
    // i is unsigned, so i >= is always true
    for (PRUint32 i = childCount; i-- != 0; ) {
      aContent->RemoveChildAt(i, PR_TRUE);
    }
  }

  if (aValue.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> textContent;
  nsresult rv = NS_NewTextNode(getter_AddRefs(textContent),
                               aContent->NodeInfo()->NodeInfoManager());
  NS_ENSURE_SUCCESS(rv, rv);

  textContent->SetText(aValue, PR_TRUE);

  return aContent->AppendChildTo(textContent, PR_TRUE);
}

static void AppendNodeTextContentsRecurse(nsINode* aNode, nsAString& aResult)
{
  nsIContent* child;
  PRUint32 i;
  for (i = 0; (child = aNode->GetChildAt(i)); ++i) {
    if (child->IsElement()) {
      AppendNodeTextContentsRecurse(child, aResult);
    }
    else if (child->IsNodeOfType(nsINode::eTEXT)) {
      child->AppendTextTo(aResult);
    }
  }
}

/* static */
void
nsContentUtils::AppendNodeTextContent(nsINode* aNode, PRBool aDeep,
                                      nsAString& aResult)
{
  if (aNode->IsNodeOfType(nsINode::eTEXT)) {
    static_cast<nsIContent*>(aNode)->AppendTextTo(aResult);
  }
  else if (aDeep) {
    AppendNodeTextContentsRecurse(aNode, aResult);
  }
  else {
    nsIContent* child;
    PRUint32 i;
    for (i = 0; (child = aNode->GetChildAt(i)); ++i) {
      if (child->IsNodeOfType(nsINode::eTEXT)) {
        child->AppendTextTo(aResult);
      }
    }
  }
}

PRBool
nsContentUtils::HasNonEmptyTextContent(nsINode* aNode)
{
  nsIContent* child;
  PRUint32 i;
  for (i = 0; (child = aNode->GetChildAt(i)); ++i) {
    if (child->IsNodeOfType(nsINode::eTEXT) &&
        child->TextLength() > 0) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

/* static */
PRBool
nsContentUtils::IsInSameAnonymousTree(const nsINode* aNode,
                                      const nsIContent* aContent)
{
  NS_PRECONDITION(aNode,
                  "Must have a node to work with");
  NS_PRECONDITION(aContent,
                  "Must have a content to work with");
  
  if (!aNode->IsNodeOfType(nsINode::eCONTENT)) {
    /**
     * The root isn't an nsIContent, so it's a document or attribute.  The only
     * nodes in the same anonymous subtree as it will have a null
     * bindingParent.
     *
     * XXXbz strictly speaking, that's not true for attribute nodes.
     */
    return aContent->GetBindingParent() == nsnull;
  }

  return static_cast<const nsIContent*>(aNode)->GetBindingParent() ==
         aContent->GetBindingParent();
 
}

class AnonymousContentDestroyer : public nsRunnable {
public:
  AnonymousContentDestroyer(nsCOMPtr<nsIContent>* aContent) {
    mContent.swap(*aContent);
    mParent = mContent->GetParent();
    mDoc = mContent->GetOwnerDoc();
  }
  NS_IMETHOD Run() {
    mContent->UnbindFromTree();
    return NS_OK;
  }
private:
  nsCOMPtr<nsIContent> mContent;
  // Hold strong refs to the parent content and document so that they
  // don't die unexpectedly
  nsCOMPtr<nsIDocument> mDoc;
  nsCOMPtr<nsIContent> mParent;
};

/* static */
void
nsContentUtils::DestroyAnonymousContent(nsCOMPtr<nsIContent>* aContent)
{
  if (*aContent) {
    AddScriptRunner(new AnonymousContentDestroyer(aContent));
  }
}

/* static */
nsIDOMScriptObjectFactory*
nsContentUtils::GetDOMScriptObjectFactory()
{
  if (!sDOMScriptObjectFactory) {
    static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,
                         NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

    CallGetService(kDOMScriptObjectFactoryCID, &sDOMScriptObjectFactory);
  }

  return sDOMScriptObjectFactory;
}

/* static */
nsresult
nsContentUtils::HoldScriptObject(PRUint32 aLangID, void *aObject)
{
  NS_ASSERTION(aObject, "unexpected null object");
  NS_ASSERTION(aLangID != nsIProgrammingLanguage::JAVASCRIPT,
               "Should use HoldJSObjects.");
  nsresult rv;

  PRUint32 langIndex = NS_STID_INDEX(aLangID);
  nsIScriptRuntime *runtime = sScriptRuntimes[langIndex];
  if (!runtime) {
    nsIDOMScriptObjectFactory *factory = GetDOMScriptObjectFactory();
    NS_ENSURE_TRUE(factory, NS_ERROR_FAILURE);

    rv = factory->GetScriptRuntimeByID(aLangID, &runtime);
    NS_ENSURE_SUCCESS(rv, rv);

    // This makes sScriptRuntimes hold a strong ref.
    sScriptRuntimes[langIndex] = runtime;
  }

  rv = runtime->HoldScriptObject(aObject);
  NS_ENSURE_SUCCESS(rv, rv);

  ++sScriptRootCount[langIndex];
  NS_LOG_ADDREF(sScriptRuntimes[langIndex], sScriptRootCount[langIndex],
                "HoldScriptObject", sizeof(void*));

  return NS_OK;
}

/* static */
void
nsContentUtils::DropScriptObject(PRUint32 aLangID, void *aObject,
                                 void *aClosure)
{
  NS_ASSERTION(aObject, "unexpected null object");
  NS_ASSERTION(aLangID != nsIProgrammingLanguage::JAVASCRIPT,
               "Should use DropJSObjects.");
  PRUint32 langIndex = NS_STID_INDEX(aLangID);
  NS_LOG_RELEASE(sScriptRuntimes[langIndex], sScriptRootCount[langIndex] - 1,
                 "HoldScriptObject");
  sScriptRuntimes[langIndex]->DropScriptObject(aObject);
  if (--sScriptRootCount[langIndex] == 0) {
    NS_RELEASE(sScriptRuntimes[langIndex]);
  }
}

/* static */
nsresult
nsContentUtils::HoldJSObjects(void* aScriptObjectHolder,
                              nsScriptObjectTracer* aTracer)
{
  NS_ENSURE_TRUE(sXPConnect, NS_ERROR_UNEXPECTED);

  nsresult rv = sXPConnect->AddJSHolder(aScriptObjectHolder, aTracer);
  NS_ENSURE_SUCCESS(rv, rv);

  if (sJSGCThingRootCount++ == 0) {
    nsLayoutStatics::AddRef();
  }
  NS_LOG_ADDREF(sXPConnect, sJSGCThingRootCount, "HoldJSObjects",
                sizeof(void*));

  return NS_OK;
}

/* static */
nsresult
nsContentUtils::DropJSObjects(void* aScriptObjectHolder)
{
  NS_LOG_RELEASE(sXPConnect, sJSGCThingRootCount - 1, "HoldJSObjects");
  nsresult rv = sXPConnect->RemoveJSHolder(aScriptObjectHolder);
  if (--sJSGCThingRootCount == 0) {
    nsLayoutStatics::Release();
  }
  return rv;
}

/* static */
PRUint32
nsContentUtils::GetWidgetStatusFromIMEStatus(PRUint32 aState)
{
  switch (aState & nsIContent::IME_STATUS_MASK_ENABLED) {
    case nsIContent::IME_STATUS_DISABLE:
      return nsIWidget::IME_STATUS_DISABLED;
    case nsIContent::IME_STATUS_ENABLE:
      return nsIWidget::IME_STATUS_ENABLED;
    case nsIContent::IME_STATUS_PASSWORD:
      return nsIWidget::IME_STATUS_PASSWORD;
    case nsIContent::IME_STATUS_PLUGIN:
      return nsIWidget::IME_STATUS_PLUGIN;
    default:
      NS_ERROR("The given state doesn't have valid enable state");
      return nsIWidget::IME_STATUS_ENABLED;
  }
}

/* static */
void
nsContentUtils::NotifyInstalledMenuKeyboardListener(PRBool aInstalling)
{
  nsIMEStateManager::OnInstalledMenuKeyboardListener(aInstalling);
}

static PRBool SchemeIs(nsIURI* aURI, const char* aScheme)
{
  nsCOMPtr<nsIURI> baseURI = NS_GetInnermostURI(aURI);
  NS_ENSURE_TRUE(baseURI, PR_FALSE);

  PRBool isScheme = PR_FALSE;
  return NS_SUCCEEDED(baseURI->SchemeIs(aScheme, &isScheme)) && isScheme;
}

/* static */
nsresult
nsContentUtils::CheckSecurityBeforeLoad(nsIURI* aURIToLoad,
                                        nsIPrincipal* aLoadingPrincipal,
                                        PRUint32 aCheckLoadFlags,
                                        PRBool aAllowData,
                                        PRUint32 aContentPolicyType,
                                        nsISupports* aContext,
                                        const nsACString& aMimeGuess,
                                        nsISupports* aExtra)
{
  NS_PRECONDITION(aLoadingPrincipal, "Must have a loading principal here");

  PRBool isSystemPrin = PR_FALSE;
  if (NS_SUCCEEDED(sSecurityManager->IsSystemPrincipal(aLoadingPrincipal,
                                                       &isSystemPrin)) &&
      isSystemPrin) {
    return NS_OK;
  }
  
  // XXXbz do we want to fast-path skin stylesheets loading XBL here somehow?
  // CheckLoadURIWithPrincipal
  nsresult rv = sSecurityManager->
    CheckLoadURIWithPrincipal(aLoadingPrincipal, aURIToLoad, aCheckLoadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // Content Policy
  PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(aContentPolicyType,
                                 aURIToLoad,
                                 aLoadingPrincipal,
                                 aContext,
                                 aMimeGuess,
                                 aExtra,
                                 &shouldLoad,
                                 GetContentPolicy(),
                                 sSecurityManager);
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_CP_REJECTED(shouldLoad)) {
    return NS_ERROR_CONTENT_BLOCKED;
  }

  // Same Origin
  if ((aAllowData && SchemeIs(aURIToLoad, "data")) ||
      ((aCheckLoadFlags & nsIScriptSecurityManager::ALLOW_CHROME) &&
       SchemeIs(aURIToLoad, "chrome"))) {
    return NS_OK;
  }

  return aLoadingPrincipal->CheckMayLoad(aURIToLoad, PR_TRUE);
}

PRBool
nsContentUtils::IsSystemPrincipal(nsIPrincipal* aPrincipal)
{
  PRBool isSystem;
  nsresult rv = sSecurityManager->IsSystemPrincipal(aPrincipal, &isSystem);
  return NS_SUCCEEDED(rv) && isSystem;
}

/* static */
void
nsContentUtils::TriggerLink(nsIContent *aContent, nsPresContext *aPresContext,
                            nsIURI *aLinkURI, const nsString &aTargetSpec,
                            PRBool aClick, PRBool aIsUserTriggered)
{
  NS_ASSERTION(aPresContext, "Need a nsPresContext");
  NS_PRECONDITION(aLinkURI, "No link URI");

  if (aContent->IsEditable()) {
    return;
  }

  nsILinkHandler *handler = aPresContext->GetLinkHandler();
  if (!handler) {
    return;
  }

  if (!aClick) {
    handler->OnOverLink(aContent, aLinkURI, aTargetSpec.get());

    return;
  }

  // Check that this page is allowed to load this URI.
  nsresult proceed = NS_OK;

  if (sSecurityManager) {
    PRUint32 flag =
      aIsUserTriggered ?
      (PRUint32)nsIScriptSecurityManager::STANDARD :
      (PRUint32)nsIScriptSecurityManager::LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT;
    proceed =
      sSecurityManager->CheckLoadURIWithPrincipal(aContent->NodePrincipal(),
                                                  aLinkURI, flag);
  }

  // Only pass off the click event if the script security manager says it's ok.
  if (NS_SUCCEEDED(proceed)) {
    handler->OnLinkClick(aContent, aLinkURI, aTargetSpec.get());
  }
}

/* static */
nsIWidget*
nsContentUtils::GetTopLevelWidget(nsIWidget* aWidget)
{
  if (!aWidget)
    return nsnull;

  return aWidget->GetTopLevelWidget();
}

/* static */
const nsDependentString
nsContentUtils::GetLocalizedEllipsis()
{
  static PRUnichar sBuf[4] = { 0, 0, 0, 0 };
  if (!sBuf[0]) {
    nsAutoString tmp(GetLocalizedStringPref("intl.ellipsis"));
    PRUint32 len = NS_MIN(PRUint32(tmp.Length()),
                          PRUint32(NS_ARRAY_LENGTH(sBuf) - 1));
    CopyUnicodeTo(tmp, 0, sBuf, len);
    if (!sBuf[0])
      sBuf[0] = PRUnichar(0x2026);
  }
  return nsDependentString(sBuf);
}

//static
nsEvent*
nsContentUtils::GetNativeEvent(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aDOMEvent));
  if (!privateEvent)
    return nsnull;
  return privateEvent->GetInternalNSEvent();
}

//static
PRBool
nsContentUtils::DOMEventToNativeKeyEvent(nsIDOMKeyEvent* aKeyEvent,
                                         nsNativeKeyEvent* aNativeEvent,
                                         PRBool aGetCharCode)
{
  nsCOMPtr<nsIDOMNSUIEvent> uievent = do_QueryInterface(aKeyEvent);
  PRBool defaultPrevented;
  uievent->GetPreventDefault(&defaultPrevented);
  if (defaultPrevented)
    return PR_FALSE;

  nsCOMPtr<nsIDOMNSEvent> nsevent = do_QueryInterface(aKeyEvent);
  PRBool trusted = PR_FALSE;
  nsevent->GetIsTrusted(&trusted);
  if (!trusted)
    return PR_FALSE;

  if (aGetCharCode) {
    aKeyEvent->GetCharCode(&aNativeEvent->charCode);
  } else {
    aNativeEvent->charCode = 0;
  }
  aKeyEvent->GetKeyCode(&aNativeEvent->keyCode);
  aKeyEvent->GetAltKey(&aNativeEvent->altKey);
  aKeyEvent->GetCtrlKey(&aNativeEvent->ctrlKey);
  aKeyEvent->GetShiftKey(&aNativeEvent->shiftKey);
  aKeyEvent->GetMetaKey(&aNativeEvent->metaKey);

  aNativeEvent->nativeEvent = GetNativeEvent(aKeyEvent);

  return PR_TRUE;
}

static PRBool
HasASCIIDigit(const nsTArray<nsShortcutCandidate>& aCandidates)
{
  for (PRUint32 i = 0; i < aCandidates.Length(); ++i) {
    PRUint32 ch = aCandidates[i].mCharCode;
    if (ch >= '0' && ch <= '9')
      return PR_TRUE;
  }
  return PR_FALSE;
}

static PRBool
CharsCaseInsensitiveEqual(PRUint32 aChar1, PRUint32 aChar2)
{
  return aChar1 == aChar2 ||
         (IS_IN_BMP(aChar1) && IS_IN_BMP(aChar2) &&
          ToLowerCase(PRUnichar(aChar1)) == ToLowerCase(PRUnichar(aChar2)));
}

static PRBool
IsCaseChangeableChar(PRUint32 aChar)
{
  return IS_IN_BMP(aChar) &&
         ToLowerCase(PRUnichar(aChar)) != ToUpperCase(PRUnichar(aChar));
}

/* static */
void
nsContentUtils::GetAccelKeyCandidates(nsIDOMKeyEvent* aDOMKeyEvent,
                  nsTArray<nsShortcutCandidate>& aCandidates)
{
  NS_PRECONDITION(aCandidates.IsEmpty(), "aCandidates must be empty");

  nsAutoString eventType;
  aDOMKeyEvent->GetType(eventType);
  // Don't process if aDOMKeyEvent is not a keypress event.
  if (!eventType.EqualsLiteral("keypress"))
    return;

  nsKeyEvent* nativeKeyEvent =
    static_cast<nsKeyEvent*>(GetNativeEvent(aDOMKeyEvent));
  if (nativeKeyEvent) {
    NS_ASSERTION(nativeKeyEvent->eventStructType == NS_KEY_EVENT,
                 "wrong type of native event");
    // nsShortcutCandidate::mCharCode is a candidate charCode.
    // nsShoftcutCandidate::mIgnoreShift means the mCharCode should be tried to
    // execute a command with/without shift key state. If this is TRUE, the
    // shifted key state should be ignored. Otherwise, don't ignore the state.
    // the priority of the charCodes are (shift key is not pressed):
    //   0: charCode/PR_FALSE,
    //   1: unshiftedCharCodes[0]/PR_FALSE, 2: unshiftedCharCodes[1]/PR_FALSE...
    // the priority of the charCodes are (shift key is pressed):
    //   0: charCode/PR_FALSE,
    //   1: shiftedCharCodes[0]/PR_FALSE, 2: shiftedCharCodes[0]/PR_TRUE,
    //   3: shiftedCharCodes[1]/PR_FALSE, 4: shiftedCharCodes[1]/PR_TRUE...
    if (nativeKeyEvent->charCode) {
      nsShortcutCandidate key(nativeKeyEvent->charCode, PR_FALSE);
      aCandidates.AppendElement(key);
    }

    PRUint32 len = nativeKeyEvent->alternativeCharCodes.Length();
    if (!nativeKeyEvent->isShift) {
      for (PRUint32 i = 0; i < len; ++i) {
        PRUint32 ch =
          nativeKeyEvent->alternativeCharCodes[i].mUnshiftedCharCode;
        if (!ch || ch == nativeKeyEvent->charCode)
          continue;

        nsShortcutCandidate key(ch, PR_FALSE);
        aCandidates.AppendElement(key);
      }
      // If unshiftedCharCodes doesn't have numeric but shiftedCharCode has it,
      // this keyboard layout is AZERTY or similar layout, probably.
      // In this case, Accel+[0-9] should be accessible without shift key.
      // However, the priority should be lowest.
      if (!HasASCIIDigit(aCandidates)) {
        for (PRUint32 i = 0; i < len; ++i) {
          PRUint32 ch =
            nativeKeyEvent->alternativeCharCodes[i].mShiftedCharCode;
          if (ch >= '0' && ch <= '9') {
            nsShortcutCandidate key(ch, PR_FALSE);
            aCandidates.AppendElement(key);
            break;
          }
        }
      }
    } else {
      for (PRUint32 i = 0; i < len; ++i) {
        PRUint32 ch = nativeKeyEvent->alternativeCharCodes[i].mShiftedCharCode;
        if (!ch)
          continue;

        if (ch != nativeKeyEvent->charCode) {
          nsShortcutCandidate key(ch, PR_FALSE);
          aCandidates.AppendElement(key);
        }

        // If the char is an alphabet, the shift key state should not be
        // ignored. E.g., Ctrl+Shift+C should not execute Ctrl+C.

        // And checking the charCode is same as unshiftedCharCode too.
        // E.g., for Ctrl+Shift+(Plus of Numpad) should not run Ctrl+Plus.
        PRUint32 unshiftCh =
          nativeKeyEvent->alternativeCharCodes[i].mUnshiftedCharCode;
        if (CharsCaseInsensitiveEqual(ch, unshiftCh))
          continue;

        // On the Hebrew keyboard layout on Windows, the unshifted char is a
        // localized character but the shifted char is a Latin alphabet,
        // then, we should not execute without the shift state. See bug 433192.
        if (IsCaseChangeableChar(ch))
          continue;

        // Setting the alternative charCode candidates for retry without shift
        // key state only when the shift key is pressed.
        nsShortcutCandidate key(ch, PR_TRUE);
        aCandidates.AppendElement(key);
      }
    }
  } else {
    PRUint32 charCode;
    aDOMKeyEvent->GetCharCode(&charCode);
    if (charCode) {
      nsShortcutCandidate key(charCode, PR_FALSE);
      aCandidates.AppendElement(key);
    }
  }
}

/* static */
void
nsContentUtils::GetAccessKeyCandidates(nsKeyEvent* aNativeKeyEvent,
                                       nsTArray<PRUint32>& aCandidates)
{
  NS_PRECONDITION(aCandidates.IsEmpty(), "aCandidates must be empty");

  // return the lower cased charCode candidates for access keys.
  // the priority of the charCodes are:
  //   0: charCode, 1: unshiftedCharCodes[0], 2: shiftedCharCodes[0]
  //   3: unshiftedCharCodes[1], 4: shiftedCharCodes[1],...
  if (aNativeKeyEvent->charCode) {
    PRUint32 ch = aNativeKeyEvent->charCode;
    if (IS_IN_BMP(ch))
      ch = ToLowerCase(PRUnichar(ch));
    aCandidates.AppendElement(ch);
  }
  for (PRUint32 i = 0;
       i < aNativeKeyEvent->alternativeCharCodes.Length(); ++i) {
    PRUint32 ch[2] =
      { aNativeKeyEvent->alternativeCharCodes[i].mUnshiftedCharCode,
        aNativeKeyEvent->alternativeCharCodes[i].mShiftedCharCode };
    for (PRUint32 j = 0; j < 2; ++j) {
      if (!ch[j])
        continue;
      if (IS_IN_BMP(ch[j]))
        ch[j] = ToLowerCase(PRUnichar(ch[j]));
      // Don't append the charCode that was already appended.
      if (aCandidates.IndexOf(ch[j]) == aCandidates.NoIndex)
        aCandidates.AppendElement(ch[j]);
    }
  }
  return;
}

/* static */
void
nsContentUtils::AddScriptBlocker()
{
  if (!sScriptBlockerCount) {
    NS_ASSERTION(sRunnersCountAtFirstBlocker == 0,
                 "Should not already have a count");
    sRunnersCountAtFirstBlocker = sBlockedScriptRunners->Count();
  }
  ++sScriptBlockerCount;
}

/* static */
void
nsContentUtils::AddScriptBlockerAndPreventAddingRunners()
{
  AddScriptBlocker();
  if (sScriptBlockerCountWhereRunnersPrevented == 0) {
    sScriptBlockerCountWhereRunnersPrevented = sScriptBlockerCount;
  }
}

/* static */
void
nsContentUtils::RemoveScriptBlocker()
{
  NS_ASSERTION(sScriptBlockerCount != 0, "Negative script blockers");
  --sScriptBlockerCount;
  if (sScriptBlockerCount < sScriptBlockerCountWhereRunnersPrevented) {
    sScriptBlockerCountWhereRunnersPrevented = 0;
  }
  if (sScriptBlockerCount) {
    return;
  }

  PRUint32 firstBlocker = sRunnersCountAtFirstBlocker;
  PRUint32 lastBlocker = (PRUint32)sBlockedScriptRunners->Count();
  sRunnersCountAtFirstBlocker = 0;
  NS_ASSERTION(firstBlocker <= lastBlocker,
               "bad sRunnersCountAtFirstBlocker");

  while (firstBlocker < lastBlocker) {
    nsCOMPtr<nsIRunnable> runnable = (*sBlockedScriptRunners)[firstBlocker];
    sBlockedScriptRunners->RemoveObjectAt(firstBlocker);
    --lastBlocker;

    runnable->Run();
    NS_ASSERTION(lastBlocker == (PRUint32)sBlockedScriptRunners->Count() &&
                 sRunnersCountAtFirstBlocker == 0,
                 "Bad count");
    NS_ASSERTION(!sScriptBlockerCount, "This is really bad");
  }
}

/* static */
PRBool
nsContentUtils::AddScriptRunner(nsIRunnable* aRunnable)
{
  if (!aRunnable) {
    return PR_FALSE;
  }

  if (sScriptBlockerCount) {
    if (sScriptBlockerCountWhereRunnersPrevented > 0) {
      NS_ERROR("Adding a script runner when that is prevented!");
      return PR_FALSE;
    }
    return sBlockedScriptRunners->AppendObject(aRunnable);
  }
  
  nsCOMPtr<nsIRunnable> run = aRunnable;
  run->Run();

  return PR_TRUE;
}

/* 
 * Helper function for nsContentUtils::ProcessViewportInfo.
 *
 * Handles a single key=value pair. If it corresponds to a valid viewport
 * attribute, add it to the document header data. No validation is done on the
 * value itself (this is done at display time).
 */
static void ProcessViewportToken(nsIDocument *aDocument, 
                                 const nsAString &token) {

  /* Iterators. */
  nsAString::const_iterator tip, tail, end;
  token.BeginReading(tip);
  tail = tip;
  token.EndReading(end);

  /* Move tip to the '='. */
  while ((tip != end) && (*tip != '='))
    ++tip;

  /* If we didn't find an '=', punt. */
  if (tip == end)
    return;

  /* Extract the key and value. */
  const nsAString &key =
    nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(Substring(tail, tip),
                                                        PR_TRUE);
  const nsAString &value =
    nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(Substring(++tip, end),
                                                        PR_TRUE);

  /* Check for known keys. If we find a match, insert the appropriate
   * information into the document header. */
  nsCOMPtr<nsIAtom> key_atom = do_GetAtom(key);
  if (key_atom == nsGkAtoms::height)
    aDocument->SetHeaderData(nsGkAtoms::viewport_height, value);
  else if (key_atom == nsGkAtoms::width)
    aDocument->SetHeaderData(nsGkAtoms::viewport_width, value);
  else if (key_atom == nsGkAtoms::initial_scale)
    aDocument->SetHeaderData(nsGkAtoms::viewport_initial_scale, value);
  else if (key_atom == nsGkAtoms::minimum_scale)
    aDocument->SetHeaderData(nsGkAtoms::viewport_minimum_scale, value);
  else if (key_atom == nsGkAtoms::maximum_scale)
    aDocument->SetHeaderData(nsGkAtoms::viewport_maximum_scale, value);
  else if (key_atom == nsGkAtoms::user_scalable)
    aDocument->SetHeaderData(nsGkAtoms::viewport_user_scalable, value);
}

#define IS_SEPARATOR(c) ((c == '=') || (c == ',') || (c == ';') || \
                         (c == '\t') || (c == '\n') || (c == '\r'))

/* static */
nsresult
nsContentUtils::ProcessViewportInfo(nsIDocument *aDocument,
                                    const nsAString &viewportInfo) {

  /* We never fail. */
  nsresult rv = NS_OK;

  /* Iterators. */
  nsAString::const_iterator tip, tail, end;
  viewportInfo.BeginReading(tip);
  tail = tip;
  viewportInfo.EndReading(end);

  /* Read the tip to the first non-separator character. */
  while ((tip != end) && (IS_SEPARATOR(*tip) || nsCRT::IsAsciiSpace(*tip)))
    ++tip;

  /* Read through and find tokens separated by separators. */
  while (tip != end) {

    /* Synchronize tip and tail. */
    tail = tip;

    /* Advance tip past non-separator characters. */
    while ((tip != end) && !IS_SEPARATOR(*tip))
      ++tip;

    /* Allow white spaces that surround the '=' character */
    if ((tip != end) && (*tip == '=')) {
      ++tip;

      while ((tip != end) && nsCRT::IsAsciiSpace(*tip))
        ++tip;

      while ((tip != end) && !(IS_SEPARATOR(*tip) || nsCRT::IsAsciiSpace(*tip)))
        ++tip;
    }

    /* Our token consists of the characters between tail and tip. */
    ProcessViewportToken(aDocument, Substring(tail, tip));

    /* Skip separators. */
    while ((tip != end) && (IS_SEPARATOR(*tip) || nsCRT::IsAsciiSpace(*tip)))
      ++tip;
  }

  return rv;

}

#undef IS_SEPARATOR

/* static */
void
nsContentUtils::HidePopupsInDocument(nsIDocument* aDocument)
{
#ifdef MOZ_XUL
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && aDocument) {
    nsCOMPtr<nsISupports> container = aDocument->GetContainer();
    nsCOMPtr<nsIDocShellTreeItem> docShellToHide = do_QueryInterface(container);
    if (docShellToHide)
      pm->HidePopupsInDocShell(docShellToHide);
  }
#endif
}

/* static */
already_AddRefed<nsIDragSession>
nsContentUtils::GetDragSession()
{
  nsIDragSession* dragSession = nsnull;
  nsCOMPtr<nsIDragService> dragService =
    do_GetService("@mozilla.org/widget/dragservice;1");
  if (dragService)
    dragService->GetCurrentSession(&dragSession);
  return dragSession;
}

/* static */
nsresult
nsContentUtils::SetDataTransferInEvent(nsDragEvent* aDragEvent)
{
  if (aDragEvent->dataTransfer || !NS_IS_TRUSTED_EVENT(aDragEvent))
    return NS_OK;

  // For draggesture and dragstart events, the data transfer object is
  // created before the event fires, so it should already be set. For other
  // drag events, get the object from the drag session.
  NS_ASSERTION(aDragEvent->message != NS_DRAGDROP_GESTURE &&
               aDragEvent->message != NS_DRAGDROP_START,
               "draggesture event created without a dataTransfer");

  nsCOMPtr<nsIDragSession> dragSession = GetDragSession();
  NS_ENSURE_TRUE(dragSession, NS_OK); // no drag in progress

  nsCOMPtr<nsIDOMDataTransfer> initialDataTransfer;
  dragSession->GetDataTransfer(getter_AddRefs(initialDataTransfer));
  if (!initialDataTransfer) {
    // A dataTransfer won't exist when a drag was started by some other
    // means, for instance calling the drag service directly, or a drag
    // from another application. In either case, a new dataTransfer should
    // be created that reflects the data. Pass true to the constructor for
    // the aIsExternal argument, so that only system access is allowed.
    PRUint32 action = 0;
    dragSession->GetDragAction(&action);
    initialDataTransfer =
      new nsDOMDataTransfer(aDragEvent->message, action);
    NS_ENSURE_TRUE(initialDataTransfer, NS_ERROR_OUT_OF_MEMORY);

    // now set it in the drag session so we don't need to create it again
    dragSession->SetDataTransfer(initialDataTransfer);
  }

  // each event should use a clone of the original dataTransfer.
  nsCOMPtr<nsIDOMNSDataTransfer> initialDataTransferNS =
    do_QueryInterface(initialDataTransfer);
  NS_ENSURE_TRUE(initialDataTransferNS, NS_ERROR_FAILURE);
  initialDataTransferNS->Clone(aDragEvent->message, aDragEvent->userCancelled,
                               getter_AddRefs(aDragEvent->dataTransfer));
  NS_ENSURE_TRUE(aDragEvent->dataTransfer, NS_ERROR_OUT_OF_MEMORY);

  // for the dragenter and dragover events, initialize the drop effect
  // from the drop action, which platform specific widget code sets before
  // the event is fired based on the keyboard state.
  if (aDragEvent->message == NS_DRAGDROP_ENTER ||
      aDragEvent->message == NS_DRAGDROP_OVER) {
    nsCOMPtr<nsIDOMNSDataTransfer> newDataTransfer =
      do_QueryInterface(aDragEvent->dataTransfer);
    NS_ENSURE_TRUE(newDataTransfer, NS_ERROR_FAILURE);

    PRUint32 action, effectAllowed;
    dragSession->GetDragAction(&action);
    newDataTransfer->GetEffectAllowedInt(&effectAllowed);
    newDataTransfer->SetDropEffectInt(FilterDropEffect(action, effectAllowed));
  }
  else if (aDragEvent->message == NS_DRAGDROP_DROP ||
           aDragEvent->message == NS_DRAGDROP_DRAGDROP ||
           aDragEvent->message == NS_DRAGDROP_END) {
    // For the drop and dragend events, set the drop effect based on the
    // last value that the dropEffect had. This will have been set in
    // nsEventStateManager::PostHandleEvent for the last dragenter or
    // dragover event.
    nsCOMPtr<nsIDOMNSDataTransfer> newDataTransfer =
      do_QueryInterface(aDragEvent->dataTransfer);
    NS_ENSURE_TRUE(newDataTransfer, NS_ERROR_FAILURE);

    PRUint32 dropEffect;
    initialDataTransferNS->GetDropEffectInt(&dropEffect);
    newDataTransfer->SetDropEffectInt(dropEffect);
  }

  return NS_OK;
}

/* static */
PRUint32
nsContentUtils::FilterDropEffect(PRUint32 aAction, PRUint32 aEffectAllowed)
{
  // It is possible for the drag action to include more than one action, but
  // the widget code which sets the action from the keyboard state should only
  // be including one. If multiple actions were set, we just consider them in
  //  the following order:
  //   copy, link, move
  if (aAction & nsIDragService::DRAGDROP_ACTION_COPY)
    aAction = nsIDragService::DRAGDROP_ACTION_COPY;
  else if (aAction & nsIDragService::DRAGDROP_ACTION_LINK)
    aAction = nsIDragService::DRAGDROP_ACTION_LINK;
  else if (aAction & nsIDragService::DRAGDROP_ACTION_MOVE)
    aAction = nsIDragService::DRAGDROP_ACTION_MOVE;

  // Filter the action based on the effectAllowed. If the effectAllowed
  // doesn't include the action, then that action cannot be done, so adjust
  // the action to something that is allowed. For a copy, adjust to move or
  // link. For a move, adjust to copy or link. For a link, adjust to move or
  // link. Otherwise, use none.
  if (aAction & aEffectAllowed ||
      aEffectAllowed == nsIDragService::DRAGDROP_ACTION_UNINITIALIZED)
    return aAction;
  if (aEffectAllowed & nsIDragService::DRAGDROP_ACTION_MOVE)
    return nsIDragService::DRAGDROP_ACTION_MOVE;
  if (aEffectAllowed & nsIDragService::DRAGDROP_ACTION_COPY)
    return nsIDragService::DRAGDROP_ACTION_COPY;
  if (aEffectAllowed & nsIDragService::DRAGDROP_ACTION_LINK)
    return nsIDragService::DRAGDROP_ACTION_LINK;
  return nsIDragService::DRAGDROP_ACTION_NONE;
}

/* static */
PRBool
nsContentUtils::URIIsLocalFile(nsIURI *aURI)
{
  PRBool isFile;
  nsCOMPtr<nsINetUtil> util = do_QueryInterface(sIOService);

  return util && NS_SUCCEEDED(util->ProtocolHasFlags(aURI,
                                nsIProtocolHandler::URI_IS_LOCAL_FILE,
                                &isFile)) &&
         isFile;
}

/* static */
nsIScriptContext*
nsContentUtils::GetContextForEventHandlers(nsINode* aNode,
                                           nsresult* aRv)
{
  *aRv = NS_OK;
  nsIDocument* ownerDoc = aNode->GetOwnerDoc();
  if (!ownerDoc) {
    *aRv = NS_ERROR_UNEXPECTED;
    return nsnull;
  }

  PRBool hasHadScriptObject = PR_TRUE;
  nsIScriptGlobalObject* sgo =
    ownerDoc->GetScriptHandlingObject(hasHadScriptObject);
  // It is bad if the document doesn't have event handling context,
  // but it used to have one.
  if (!sgo && hasHadScriptObject) {
    *aRv = NS_ERROR_UNEXPECTED;
    return nsnull;
  }

  if (sgo) {
    nsIScriptContext* scx = sgo->GetContext();
    // Bad, no context from script global object!
    if (!scx) {
      *aRv = NS_ERROR_UNEXPECTED;
      return nsnull;
    }
    return scx;
  }

  return nsnull;
}

/* static */
JSContext *
nsContentUtils::GetCurrentJSContext()
{
  JSContext *cx = nsnull;

  sThreadJSContextStack->Peek(&cx);

  return cx;
}

/* static */
void
nsContentUtils::ASCIIToLower(nsAString& aStr)
{
  PRUnichar* iter = aStr.BeginWriting();
  PRUnichar* end = aStr.EndWriting();
  while (iter != end) {
    PRUnichar c = *iter;
    if (c >= 'A' && c <= 'Z') {
      *iter = c + ('a' - 'A');
    }
    ++iter;
  }
}

/* static */
void
nsContentUtils::ASCIIToLower(const nsAString& aSource, nsAString& aDest)
{
  PRUint32 len = aSource.Length();
  aDest.SetLength(len);
  if (aDest.Length() == len) {
    PRUnichar* dest = aDest.BeginWriting();
    const PRUnichar* iter = aSource.BeginReading();
    const PRUnichar* end = aSource.EndReading();
    while (iter != end) {
      PRUnichar c = *iter;
      *dest = (c >= 'A' && c <= 'Z') ?
         c + ('a' - 'A') : c;
      ++iter;
      ++dest;
    }
  }
}

/* static */
void
nsContentUtils::ASCIIToUpper(nsAString& aStr)
{
  PRUnichar* iter = aStr.BeginWriting();
  PRUnichar* end = aStr.EndWriting();
  while (iter != end) {
    PRUnichar c = *iter;
    if (c >= 'a' && c <= 'z') {
      *iter = c + ('A' - 'a');
    }
    ++iter;
  }
}

/* static */
void
nsContentUtils::ASCIIToUpper(const nsAString& aSource, nsAString& aDest)
{
  PRUint32 len = aSource.Length();
  aDest.SetLength(len);
  if (aDest.Length() == len) {
    PRUnichar* dest = aDest.BeginWriting();
    const PRUnichar* iter = aSource.BeginReading();
    const PRUnichar* end = aSource.EndReading();
    while (iter != end) {
      PRUnichar c = *iter;
      *dest = (c >= 'a' && c <= 'z') ?
         c + ('A' - 'a') : c;
      ++iter;
      ++dest;
    }
  }
}

PRBool
nsContentUtils::EqualsIgnoreASCIICase(const nsAString& aStr1,
                                      const nsAString& aStr2)
{
  PRUint32 len = aStr1.Length();
  if (len != aStr2.Length()) {
    return PR_FALSE;
  }

  const PRUnichar* str1 = aStr1.BeginReading();
  const PRUnichar* str2 = aStr2.BeginReading();
  const PRUnichar* end = str1 + len;

  while (str1 < end) {
    PRUnichar c1 = *str1++;
    PRUnichar c2 = *str2++;

    // First check if any bits other than the 0x0020 differs
    if ((c1 ^ c2) & 0xffdf) {
      return PR_FALSE;
    }

    // We know they only differ in the 0x0020 bit.
    // Likely the two chars are the same, so check that first
    if (c1 != c2) {
      // They do differ, but since it's only in the 0x0020 bit, check if it's
      // the same ascii char, but just differing in case
      PRUnichar c1Upper = c1 & 0xffdf;
      if (!('A' <= c1Upper && c1Upper <= 'Z')) {
        return PR_FALSE;
      }
    }
  }

  return PR_TRUE;
}

/* static */
void
nsAutoGCRoot::Shutdown()
{
  NS_IF_RELEASE(sJSRuntimeService);
}

/* static */
nsIInterfaceRequestor*
nsContentUtils::GetSameOriginChecker()
{
  if (!sSameOriginChecker) {
    sSameOriginChecker = new nsSameOriginChecker();
    NS_IF_ADDREF(sSameOriginChecker);
  }
  return sSameOriginChecker;
}


NS_IMPL_ISUPPORTS2(nsSameOriginChecker,
                   nsIChannelEventSink,
                   nsIInterfaceRequestor)

NS_IMETHODIMP
nsSameOriginChecker::AsyncOnChannelRedirect(nsIChannel *aOldChannel,
                                            nsIChannel *aNewChannel,
                                            PRUint32 aFlags,
                                            nsIAsyncVerifyRedirectCallback *cb)
{
  NS_PRECONDITION(aNewChannel, "Redirecting to null channel?");
  if (!nsContentUtils::GetSecurityManager())
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIPrincipal> oldPrincipal;
  nsContentUtils::GetSecurityManager()->
    GetChannelPrincipal(aOldChannel, getter_AddRefs(oldPrincipal));

  nsCOMPtr<nsIURI> newURI;
  aNewChannel->GetURI(getter_AddRefs(newURI));
  nsCOMPtr<nsIURI> newOriginalURI;
  aNewChannel->GetOriginalURI(getter_AddRefs(newOriginalURI));

  NS_ENSURE_STATE(oldPrincipal && newURI && newOriginalURI);

  nsresult rv = oldPrincipal->CheckMayLoad(newURI, PR_FALSE);
  if (NS_SUCCEEDED(rv) && newOriginalURI != newURI) {
    rv = oldPrincipal->CheckMayLoad(newOriginalURI, PR_FALSE);
  }

  if (NS_FAILED(rv))
      return rv;

  cb->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
nsSameOriginChecker::GetInterface(const nsIID & aIID, void **aResult)
{
  return QueryInterface(aIID, aResult);
}

/* static */
nsresult
nsContentUtils::GetASCIIOrigin(nsIPrincipal* aPrincipal, nsCString& aOrigin)
{
  NS_PRECONDITION(aPrincipal, "missing principal");

  aOrigin.Truncate();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  if (uri) {
    return GetASCIIOrigin(uri, aOrigin);
  }

  aOrigin.AssignLiteral("null");

  return NS_OK;
}

/* static */
nsresult
nsContentUtils::GetASCIIOrigin(nsIURI* aURI, nsCString& aOrigin)
{
  NS_PRECONDITION(aURI, "missing uri");

  aOrigin.Truncate();

  nsCOMPtr<nsIURI> uri = NS_GetInnermostURI(aURI);
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  nsCString host;
  nsresult rv = uri->GetAsciiHost(host);

  if (NS_SUCCEEDED(rv) && !host.IsEmpty()) {
    nsCString scheme;
    rv = uri->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    aOrigin = scheme + NS_LITERAL_CSTRING("://") + host;

    // If needed, append the port
    PRInt32 port;
    uri->GetPort(&port);
    if (port != -1) {
      PRInt32 defaultPort = NS_GetDefaultPort(scheme.get());
      if (port != defaultPort) {
        aOrigin.Append(':');
        aOrigin.AppendInt(port);
      }
    }
  }
  else {
    aOrigin.AssignLiteral("null");
  }

  return NS_OK;
}

/* static */
nsresult
nsContentUtils::GetUTFOrigin(nsIPrincipal* aPrincipal, nsString& aOrigin)
{
  NS_PRECONDITION(aPrincipal, "missing principal");

  aOrigin.Truncate();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  if (uri) {
    return GetUTFOrigin(uri, aOrigin);
  }

  aOrigin.AssignLiteral("null");

  return NS_OK;
}

/* static */
nsresult
nsContentUtils::GetUTFOrigin(nsIURI* aURI, nsString& aOrigin)
{
  NS_PRECONDITION(aURI, "missing uri");

  aOrigin.Truncate();

  nsCOMPtr<nsIURI> uri = NS_GetInnermostURI(aURI);
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  nsCString host;
  nsresult rv = uri->GetHost(host);

  if (NS_SUCCEEDED(rv) && !host.IsEmpty()) {
    nsCString scheme;
    rv = uri->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    aOrigin = NS_ConvertUTF8toUTF16(scheme + NS_LITERAL_CSTRING("://") + host);

    // If needed, append the port
    PRInt32 port;
    uri->GetPort(&port);
    if (port != -1) {
      PRInt32 defaultPort = NS_GetDefaultPort(scheme.get());
      if (port != defaultPort) {
        aOrigin.Append(':');
        aOrigin.AppendInt(port);
      }
    }
  }
  else {
    aOrigin.AssignLiteral("null");
  }
  
  return NS_OK;
}

/* static */
already_AddRefed<nsIDocument>
nsContentUtils::GetDocumentFromScriptContext(nsIScriptContext *aScriptContext)
{
  if (!aScriptContext)
    return nsnull;

  nsCOMPtr<nsIDOMWindow> window =
    do_QueryInterface(aScriptContext->GetGlobalObject());
  nsIDocument *doc = nsnull;
  if (window) {
    nsCOMPtr<nsIDOMDocument> domdoc;
    window->GetDocument(getter_AddRefs(domdoc));
    if (domdoc) {
      CallQueryInterface(domdoc, &doc);
    }
  }
  return doc;
}

/* static */
PRBool
nsContentUtils::CheckMayLoad(nsIPrincipal* aPrincipal, nsIChannel* aChannel)
{
  nsCOMPtr<nsIURI> channelURI;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  return NS_SUCCEEDED(aPrincipal->CheckMayLoad(channelURI, PR_FALSE));
}

nsContentTypeParser::nsContentTypeParser(const nsAString& aString)
  : mString(aString), mService(nsnull)
{
  CallGetService("@mozilla.org/network/mime-hdrparam;1", &mService);
}

nsContentTypeParser::~nsContentTypeParser()
{
  NS_IF_RELEASE(mService);
}

nsresult
nsContentTypeParser::GetParameter(const char* aParameterName, nsAString& aResult)
{
  NS_ENSURE_TRUE(mService, NS_ERROR_FAILURE);
  return mService->GetParameter(mString, aParameterName,
                                EmptyCString(), PR_FALSE, nsnull,
                                aResult);
}

/* static */

// If you change this code, change also AllowedToAct() in
// XPCSystemOnlyWrapper.cpp!
PRBool
nsContentUtils::CanAccessNativeAnon()
{
  JSContext* cx = nsnull;
  sThreadJSContextStack->Peek(&cx);
  if (!cx) {
    return PR_TRUE;
  }
  JSStackFrame* fp;
  nsIPrincipal* principal =
    sSecurityManager->GetCxSubjectPrincipalAndFrame(cx, &fp);
  NS_ENSURE_TRUE(principal, PR_FALSE);

  if (!fp) {
    if (!JS_FrameIterator(cx, &fp)) {
      // No code at all is running. So we must be arriving here as the result
      // of C++ code asking us to do something. Allow access.
      return PR_TRUE;
    }

    // Some code is running, we can't make the assumption, as above, but we
    // can't use a native frame, so clear fp.
    fp = nsnull;
  } else if (!JS_IsScriptFrame(cx, fp)) {
    fp = nsnull;
  }

  PRBool privileged;
  if (NS_SUCCEEDED(sSecurityManager->IsSystemPrincipal(principal, &privileged)) &&
      privileged) {
    // Chrome things are allowed to touch us.
    return PR_TRUE;
  }

  // XXX HACK EWW! Allow chrome://global/ access to these things, even
  // if they've been cloned into less privileged contexts.
  static const char prefix[] = "chrome://global/";
  const char *filename;
  if (fp && JS_IsScriptFrame(cx, fp) &&
      (filename = JS_GetFrameScript(cx, fp)->filename) &&
      !strncmp(filename, prefix, NS_ARRAY_LENGTH(prefix) - 1)) {
    return PR_TRUE;
  }

  // Before we throw, check for UniversalXPConnect.
  nsresult rv = sSecurityManager->IsCapabilityEnabled("UniversalXPConnect", &privileged);
  if (NS_SUCCEEDED(rv) && privileged) {
    return PR_TRUE;
  }

  return PR_FALSE;
}

/* static */ nsresult
nsContentUtils::DispatchXULCommand(nsIContent* aTarget,
                                   PRBool aTrusted,
                                   nsIDOMEvent* aSourceEvent,
                                   nsIPresShell* aShell,
                                   PRBool aCtrl,
                                   PRBool aAlt,
                                   PRBool aShift,
                                   PRBool aMeta)
{
  NS_ENSURE_STATE(aTarget);
  nsIDocument* doc = aTarget->GetOwnerDoc();
  nsCOMPtr<nsIDOMDocumentEvent> docEvent = do_QueryInterface(doc);
  NS_ENSURE_STATE(docEvent);
  nsCOMPtr<nsIDOMEvent> event;
  docEvent->CreateEvent(NS_LITERAL_STRING("xulcommandevent"),
                        getter_AddRefs(event));
  nsCOMPtr<nsIDOMXULCommandEvent> xulCommand = do_QueryInterface(event);
  nsCOMPtr<nsIPrivateDOMEvent> pEvent = do_QueryInterface(xulCommand);
  NS_ENSURE_STATE(pEvent);
  nsCOMPtr<nsIDOMAbstractView> view = do_QueryInterface(doc->GetWindow());
  nsresult rv = xulCommand->InitCommandEvent(NS_LITERAL_STRING("command"),
                                             PR_TRUE, PR_TRUE, view,
                                             0, aCtrl, aAlt, aShift, aMeta,
                                             aSourceEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aShell) {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsCOMPtr<nsIPresShell> kungFuDeathGrip = aShell;
    return aShell->HandleDOMEventWithTarget(aTarget, event, &status);
  }

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(aTarget);
  NS_ENSURE_STATE(target);
  PRBool dummy;
  return target->DispatchEvent(event, &dummy);
}

// static
nsresult
nsContentUtils::WrapNative(JSContext *cx, JSObject *scope, nsISupports *native,
                           nsWrapperCache *cache, const nsIID* aIID, jsval *vp,
                           nsIXPConnectJSObjectHolder **aHolder,
                           PRBool aAllowWrapping)
{
  if (!native) {
    NS_ASSERTION(!aHolder || !*aHolder, "*aHolder should be null!");

    *vp = JSVAL_NULL;

    return NS_OK;
  }

  NS_ENSURE_TRUE(sXPConnect && sThreadJSContextStack, NS_ERROR_UNEXPECTED);

  // Keep sXPConnect and sThreadJSContextStack alive. If we're on the main
  // thread then this can be done simply and cheaply by adding a reference to
  // nsLayoutStatics. If we're not on the main thread then we need to add a
  // more expensive reference sXPConnect directly. We have to use manual
  // AddRef and Release calls so don't early-exit from this function after we've
  // added the reference!
  PRBool isMainThread = NS_IsMainThread();

  if (isMainThread) {
    nsLayoutStatics::AddRef();
  }
  else {
    sXPConnect->AddRef();
  }

  JSContext *topJSContext;
  nsresult rv = sThreadJSContextStack->Peek(&topJSContext);
  if (NS_SUCCEEDED(rv)) {
    PRBool push = topJSContext != cx;
    if (push) {
      rv = sThreadJSContextStack->Push(cx);
    }
    if (NS_SUCCEEDED(rv)) {
      rv = sXPConnect->WrapNativeToJSVal(cx, scope, native, cache, aIID,
                                         aAllowWrapping, vp, aHolder);
      if (push) {
        sThreadJSContextStack->Pop(nsnull);
      }
    }
  }

  if (isMainThread) {
    nsLayoutStatics::Release();
  }
  else {
    sXPConnect->Release();
  }

  return rv;
}

void
nsContentUtils::StripNullChars(const nsAString& aInStr, nsAString& aOutStr)
{
  // In common cases where we don't have nulls in the
  // string we can simple simply bypass the checking code.
  PRInt32 firstNullPos = aInStr.FindChar('\0');
  if (firstNullPos == kNotFound) {
    aOutStr.Assign(aInStr);
    return;
  }

  aOutStr.SetCapacity(aInStr.Length() - 1);
  nsAString::const_iterator start, end;
  aInStr.BeginReading(start);
  aInStr.EndReading(end);
  while (start != end) {
    if (*start != '\0')
      aOutStr.Append(*start);
    ++start;
  }
}

namespace {

const unsigned int kCloneStackFrameStackSize = 20;

class CloneStackFrame
{
  friend class CloneStack;

public:
  // These three jsvals must all stick together as they're treated as a jsval
  // array!
  jsval source;
  jsval clone;
  jsval temp;
  js::AutoIdArray ids;
  jsuint index;

private:
  // Only let CloneStack access these.
  CloneStackFrame(JSContext* aCx, jsval aSource, jsval aClone, JSIdArray* aIds)
  : source(aSource), clone(aClone), temp(JSVAL_NULL), ids(aCx, aIds), index(0),
    prevFrame(nsnull),  tvrVals(aCx, 3, &source)
  {
    MOZ_COUNT_CTOR(CloneStackFrame);
  }

  ~CloneStackFrame()
  {
    MOZ_COUNT_DTOR(CloneStackFrame);
  }

  CloneStackFrame* prevFrame;
  js::AutoArrayRooter tvrVals;
};

class CloneStack
{
public:
  CloneStack(JSContext* cx)
  : mCx(cx), mLastFrame(nsnull) {
    mObjectSet.Init();
  }

  ~CloneStack() {
    while (!IsEmpty()) {
      Pop();
    }
  }

  PRBool
  Push(jsval source, jsval clone, JSIdArray* ids) {
    NS_ASSERTION(!JSVAL_IS_PRIMITIVE(source) && !JSVAL_IS_PRIMITIVE(clone),
                 "Must be an object!");
    if (!ids) {
      return PR_FALSE;
    }

    CloneStackFrame* newFrame;
    if (mObjectSet.Count() < kCloneStackFrameStackSize) {
      // If the object can fit in our stack space then use that.
      CloneStackFrame* buf = reinterpret_cast<CloneStackFrame*>(mStackFrames);
      newFrame = new (buf + mObjectSet.Count())
                     CloneStackFrame(mCx, source, clone, ids);
    }
    else {
      // Use the heap.
      newFrame = new CloneStackFrame(mCx, source, clone, ids);
    }

    mObjectSet.PutEntry(JSVAL_TO_OBJECT(source));

    newFrame->prevFrame = mLastFrame;
    mLastFrame = newFrame;

    return PR_TRUE;
  }

  CloneStackFrame*
  Peek() {
    return mLastFrame;
  }

  void
  Pop() {
    if (IsEmpty()) {
      NS_ERROR("Empty stack!");
      return;
    }

    CloneStackFrame* lastFrame = mLastFrame;

    mObjectSet.RemoveEntry(JSVAL_TO_OBJECT(lastFrame->source));
    mLastFrame = lastFrame->prevFrame;

    if (mObjectSet.Count() >= kCloneStackFrameStackSize) {
      // Only delete if this was a heap object.
      delete lastFrame;
    }
    else {
      // Otherwise just run the destructor.
      lastFrame->~CloneStackFrame();
    }
  }

  PRBool
  IsEmpty() {
    NS_ASSERTION((!mLastFrame && !mObjectSet.Count()) ||
                 (mLastFrame && mObjectSet.Count()),
                 "Hashset is out of sync!");
    return mObjectSet.Count() == 0;
  }

  PRBool
  Search(JSObject* obj) {
    return !!mObjectSet.GetEntry(obj);
  }

private:
  JSContext* mCx;
  CloneStackFrame* mLastFrame;
  nsTHashtable<nsVoidPtrHashKey> mObjectSet;

  // Use a char array instead of CloneStackFrame array to prevent the JSAuto*
  // helpers from running until we're ready for them.
  char mStackFrames[kCloneStackFrameStackSize * sizeof(CloneStackFrame)];
};

struct ReparentObjectData {
  ReparentObjectData(JSContext* cx, JSObject* obj)
  : cx(cx), obj(obj), ids(nsnull), index(0) { }

  ~ReparentObjectData() {
    if (ids) {
      JS_DestroyIdArray(cx, ids);
    }
  }

  JSContext* cx;
  JSObject* obj;
  JSIdArray* ids;
  jsint index;
};

inline nsresult
SetPropertyOnValueOrObject(JSContext* cx,
                           jsval val,
                           jsval* rval,
                           JSObject* obj,
                           jsid id)
{
  NS_ASSERTION((rval && !obj) || (!rval && obj), "Can only clone to one dest!");
  if (rval) {
    *rval = val;
    return NS_OK;
  }
  if (!JS_DefinePropertyById(cx, obj, id, val, nsnull, nsnull,
                             JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

inline JSObject*
CreateEmptyObjectOrArray(JSContext* cx,
                         JSObject* obj)
{
  if (JS_IsArrayObject(cx, obj)) {
    jsuint length;
    if (!JS_GetArrayLength(cx, obj, &length)) {
      NS_ERROR("Failed to get array length?!");
      return nsnull;
    }
    return JS_NewArrayObject(cx, length, NULL);
  }
  return JS_NewObject(cx, NULL, NULL, NULL);
}

nsresult
CloneSimpleValues(JSContext* cx,
                  jsval val,
                  jsval* rval,
                  PRBool* wasCloned,
                  JSObject* robj = nsnull,
                  jsid rid = INT_TO_JSID(0))
{
  *wasCloned = PR_TRUE;

  // No cloning necessary for these non-GC'd jsvals.
  if (!JSVAL_IS_GCTHING(val) || JSVAL_IS_NULL(val)) {
    return SetPropertyOnValueOrObject(cx, val, rval, robj, rid);
  }

  // We'll use immutable strings to prevent copying if we can.
  if (JSVAL_IS_STRING(val)) {
    if (!JS_MakeStringImmutable(cx, JSVAL_TO_STRING(val))) {
      return NS_ERROR_FAILURE;
    }
    return SetPropertyOnValueOrObject(cx, val, rval, robj, rid);
  }

  NS_ASSERTION(!JSVAL_IS_PRIMITIVE(val), "Not an object!");
  JSObject* obj = JSVAL_TO_OBJECT(val);

  // Dense arrays of primitives can be cloned quickly.
  JSObject* newArray;
  if (!js_CloneDensePrimitiveArray(cx, obj, &newArray)) {
    return NS_ERROR_FAILURE;
  }
  if (newArray) {
    return SetPropertyOnValueOrObject(cx, OBJECT_TO_JSVAL(newArray), rval, robj,
                                      rid);
  }

  // Date objects.
  if (js_DateIsValid(cx, obj)) {
    jsdouble msec = js_DateGetMsecSinceEpoch(cx, obj);
    JSObject* newDate;
    if (!(msec  && (newDate = js_NewDateObjectMsec(cx, msec)))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    return SetPropertyOnValueOrObject(cx, OBJECT_TO_JSVAL(newDate), rval, robj,
                                      rid);
  }

  // RegExp objects.
  if (js_ObjectIsRegExp(obj)) {
    JSObject* proto;
    if (!js_GetClassPrototype(cx, JS_GetScopeChain(cx), JSProto_RegExp,
                              &proto)) {
      return NS_ERROR_FAILURE;
    }
    JSObject* newRegExp = js_CloneRegExpObject(cx, obj, proto);
    if (!newRegExp) {
      return NS_ERROR_FAILURE;
    }
    return SetPropertyOnValueOrObject(cx, OBJECT_TO_JSVAL(newRegExp), rval,
                                      robj, rid);
  }

  // Typed array objects.
  if (js_IsTypedArray(obj)) {
    js::TypedArray* src = js::TypedArray::fromJSObject(obj);
    JSObject* newTypedArray = js_CreateTypedArrayWithArray(cx, src->type, obj);
    if (!newTypedArray) {
      return NS_ERROR_FAILURE;
    }
    return SetPropertyOnValueOrObject(cx, OBJECT_TO_JSVAL(newTypedArray), rval,
                                      robj, rid);
  }

  // ArrayBuffer objects.
  if (js_IsArrayBuffer(obj)) {
    js::ArrayBuffer* src = js::ArrayBuffer::fromJSObject(obj);
    if (!src) {
      return NS_ERROR_FAILURE;
    }

    JSObject* newBuffer = js_CreateArrayBuffer(cx, src->byteLength);
    if (!newBuffer) {
      return NS_ERROR_FAILURE;
    }
    memcpy(js::ArrayBuffer::fromJSObject(newBuffer)->data, src->data,
           src->byteLength);
    return SetPropertyOnValueOrObject(cx, OBJECT_TO_JSVAL(newBuffer), rval,
                                      robj, rid);
  }

  // Do we support File?
  // Do we support Blob?
  // Do we support FileList?

  // Function objects don't get cloned.
  if (JS_ObjectIsFunction(cx, obj)) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  // Security wrapped objects are not allowed either.
  if (obj->isWrapper() && !obj->getClass()->ext.innerObject)
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  // See if this JSObject is backed by some C++ object. If it is then we assume
  // that it is inappropriate to clone.
  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  nsContentUtils::XPConnect()->
    GetWrappedNativeOfJSObject(cx, obj, getter_AddRefs(wrapper));
  if (wrapper) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  *wasCloned = PR_FALSE;
  return NS_OK;
}

} // anonymous namespace

// static
nsresult
nsContentUtils::CreateStructuredClone(JSContext* cx,
                                      jsval val,
                                      jsval* rval)
{
  JSAutoRequest ar(cx);

  nsCOMPtr<nsIXPConnect> xpconnect(sXPConnect);
  NS_ENSURE_STATE(xpconnect);

  PRBool wasCloned;
  nsresult rv = CloneSimpleValues(cx, val, rval, &wasCloned);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (wasCloned) {
    return NS_OK;
  }

  NS_ASSERTION(JSVAL_IS_OBJECT(val), "Not an object?!");
  JSObject* obj = CreateEmptyObjectOrArray(cx, JSVAL_TO_OBJECT(val));
  if (!obj) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  jsval output = OBJECT_TO_JSVAL(obj);
  js::AutoValueRooter tvr(cx, output);

  CloneStack stack(cx);
  if (!stack.Push(val, OBJECT_TO_JSVAL(obj),
                  JS_Enumerate(cx, JSVAL_TO_OBJECT(val)))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  while (!stack.IsEmpty()) {
    CloneStackFrame* frame = stack.Peek();

    NS_ASSERTION(!!frame->ids &&
                 frame->ids.length() >= frame->index &&
                 !JSVAL_IS_PRIMITIVE(frame->source) &&
                 !JSVAL_IS_PRIMITIVE(frame->clone),
                 "Bad frame state!");

    if (frame->index == frame->ids.length()) {
      // Done cloning this object, pop the frame.
      stack.Pop();
      continue;
    }

    // Get the current id and increment the index.
    jsid id = frame->ids[frame->index++];

    if (!JS_GetPropertyById(cx, JSVAL_TO_OBJECT(frame->source), id,
                            &frame->temp)) {
      return NS_ERROR_FAILURE;
    }

    if (!JSVAL_IS_PRIMITIVE(frame->temp) &&
        stack.Search(JSVAL_TO_OBJECT(frame->temp))) {
      // Spec says to throw this particular exception for cyclical references.
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }

    JSObject* clone = JSVAL_TO_OBJECT(frame->clone);

    PRBool wasCloned;
    nsresult rv = CloneSimpleValues(cx, frame->temp, nsnull, &wasCloned, clone,
                                    id);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (!wasCloned) {
      NS_ASSERTION(JSVAL_IS_OBJECT(frame->temp), "Not an object?!");
      obj = CreateEmptyObjectOrArray(cx, JSVAL_TO_OBJECT(frame->temp));
      if (!obj ||
          !stack.Push(frame->temp, OBJECT_TO_JSVAL(obj),
                      JS_Enumerate(cx, JSVAL_TO_OBJECT(frame->temp)))) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      // Set the new object as a property of the clone. We'll fill it on the
      // next iteration.
      if (!JS_DefinePropertyById(cx, clone, id, OBJECT_TO_JSVAL(obj), nsnull,
                                 nsnull, JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  *rval = output;
  return NS_OK;
}

// static
nsresult
nsContentUtils::ReparentClonedObjectToScope(JSContext* cx,
                                            JSObject* obj,
                                            JSObject* scope)
{
  JSAutoRequest ar(cx);

  scope = JS_GetGlobalForObject(cx, scope);

  nsAutoTArray<ReparentObjectData, 20> objectData;
  objectData.AppendElement(ReparentObjectData(cx, obj));

  while (!objectData.IsEmpty()) {
    ReparentObjectData& data = objectData[objectData.Length() - 1];

    if (!data.ids) {
      NS_ASSERTION(!data.index, "Shouldn't have index here");

      // Typed arrays are special and don't need to be enumerated.
      if (js_IsTypedArray(data.obj)) {
        if (!js_ReparentTypedArrayToScope(cx, data.obj, scope)) {
          return NS_ERROR_FAILURE;
        }

        // No need to enumerate anything here.
        objectData.RemoveElementAt(objectData.Length() - 1);
        continue;
      }

      JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(JS_GET_CLASS(cx, data.obj));
      if (!key) {
        // We should never be reparenting an object that doesn't have a standard
        // proto key.
        return NS_ERROR_FAILURE;
      }

      // Fix the prototype and parent first.
      JSObject* proto;
      if (!js_GetClassPrototype(cx, scope, key, &proto) ||
          !JS_SetPrototype(cx, data.obj, proto) ||
          !JS_SetParent(cx, data.obj, scope)) {
        return NS_ERROR_FAILURE;
      }

      // Primitive arrays don't need to be enumerated either but the proto and
      // parent needed to be fixed above. Now we can just move on.
      if (js_IsDensePrimitiveArray(data.obj)) {
        objectData.RemoveElementAt(objectData.Length() - 1);
        continue;
      }

      // And now enumerate the object's properties.
      if (!(data.ids = JS_Enumerate(cx, data.obj))) {
        return NS_ERROR_FAILURE;
      }
    }

    // If we've gone through all the object's properties then we're done with
    // this frame.
    if (data.index == data.ids->length) {
      objectData.RemoveElementAt(objectData.Length() - 1);
      continue;
    }

    // Get the id and increment!
    jsid id = data.ids->vector[data.index++];

    jsval prop;
    if (!JS_GetPropertyById(cx, data.obj, id, &prop)) {
      return NS_ERROR_FAILURE;
    }

    // Push a new frame if this property is an object.
    if (!JSVAL_IS_PRIMITIVE(prop)) {
      objectData.AppendElement(ReparentObjectData(cx, JSVAL_TO_OBJECT(prop)));
    }
  }

  return NS_OK;
}

struct ClassMatchingInfo {
  nsAttrValue::AtomArray mClasses;
  nsCaseTreatment mCaseTreatment;
};

static PRBool
MatchClassNames(nsIContent* aContent, PRInt32 aNamespaceID, nsIAtom* aAtom,
                void* aData)
{
  // We can't match if there are no class names
  const nsAttrValue* classAttr = aContent->GetClasses();
  if (!classAttr) {
    return PR_FALSE;
  }
  
  // need to match *all* of the classes
  ClassMatchingInfo* info = static_cast<ClassMatchingInfo*>(aData);
  PRUint32 length = info->mClasses.Length();
  if (!length) {
    // If we actually had no classes, don't match.
    return PR_FALSE;
  }
  PRUint32 i;
  for (i = 0; i < length; ++i) {
    if (!classAttr->Contains(info->mClasses[i],
                             info->mCaseTreatment)) {
      return PR_FALSE;
    }
  }
  
  return PR_TRUE;
}

static void
DestroyClassNameArray(void* aData)
{
  ClassMatchingInfo* info = static_cast<ClassMatchingInfo*>(aData);
  delete info;
}

static void*
AllocClassMatchingInfo(nsINode* aRootNode,
                       const nsString* aClasses)
{
  nsAttrValue attrValue;
  attrValue.ParseAtomArray(*aClasses);
  // nsAttrValue::Equals is sensitive to order, so we'll send an array
  ClassMatchingInfo* info = new ClassMatchingInfo;
  NS_ENSURE_TRUE(info, nsnull);

  if (attrValue.Type() == nsAttrValue::eAtomArray) {
    info->mClasses.SwapElements(*(attrValue.GetAtomArrayValue()));
  } else if (attrValue.Type() == nsAttrValue::eAtom) {
    info->mClasses.AppendElement(attrValue.GetAtomValue());
  }

  info->mCaseTreatment =
    aRootNode->GetOwnerDoc() &&
    aRootNode->GetOwnerDoc()->GetCompatibilityMode() == eCompatibility_NavQuirks ?
    eIgnoreCase : eCaseMatters;
  return info;
}

// static

nsresult
nsContentUtils::GetElementsByClassName(nsINode* aRootNode,
                                       const nsAString& aClasses,
                                       nsIDOMNodeList** aReturn)
{
  NS_PRECONDITION(aRootNode, "Must have root node");
  
  nsContentList* elements =
    NS_GetFuncStringContentList(aRootNode, MatchClassNames,
                                DestroyClassNameArray,
                                AllocClassMatchingInfo,
                                aClasses).get();
  NS_ENSURE_TRUE(elements, NS_ERROR_OUT_OF_MEMORY);

  // Transfer ownership
  *aReturn = elements;

  return NS_OK;
}

#ifdef DEBUG
class DebugWrapperTraversalCallback : public nsCycleCollectionTraversalCallback
{
public:
  DebugWrapperTraversalCallback(void* aWrapper) : mFound(PR_FALSE),
                                                  mWrapper(aWrapper)
  {
    mFlags = WANT_ALL_TRACES;
  }

  NS_IMETHOD_(void) DescribeNode(CCNodeType type,
                                 nsrefcnt refcount,
                                 size_t objsz,
                                 const char* objname)
  {
  }
  NS_IMETHOD_(void) NoteXPCOMRoot(nsISupports *root)
  {
  }
  NS_IMETHOD_(void) NoteRoot(PRUint32 langID, void* root,
                             nsCycleCollectionParticipant* helper)
  {
  }
  NS_IMETHOD_(void) NoteScriptChild(PRUint32 langID, void* child)
  {
    if (langID == nsIProgrammingLanguage::JAVASCRIPT) {
      mFound = child == mWrapper;
    }
  }
  NS_IMETHOD_(void) NoteXPCOMChild(nsISupports *child)
  {
  }
  NS_IMETHOD_(void) NoteNativeChild(void* child,
                                    nsCycleCollectionParticipant* helper)
  {
  }

  NS_IMETHOD_(void) NoteNextEdgeName(const char* name)
  {
  }

  PRBool mFound;

private:
  void* mWrapper;
};

static void
DebugWrapperTraceCallback(PRUint32 langID, void *p, void *closure)
{
  DebugWrapperTraversalCallback* callback =
    static_cast<DebugWrapperTraversalCallback*>(closure);
  callback->NoteScriptChild(langID, p);
}

// static
void
nsContentUtils::CheckCCWrapperTraversal(nsISupports* aScriptObjectHolder,
                                        nsWrapperCache* aCache)
{
  nsXPCOMCycleCollectionParticipant* participant;
  CallQueryInterface(aScriptObjectHolder, &participant);

  DebugWrapperTraversalCallback callback(aCache->GetWrapper());

  participant->Traverse(aScriptObjectHolder, callback);
  NS_ASSERTION(callback.mFound,
               "Cycle collection participant didn't traverse to preserved "
               "wrapper! This will probably crash.");

  callback.mFound = PR_FALSE;
  participant->Trace(aScriptObjectHolder, DebugWrapperTraceCallback, &callback);
  NS_ASSERTION(callback.mFound,
               "Cycle collection participant didn't trace preserved wrapper! "
               "This will probably crash.");
}
#endif

mozAutoRemovableBlockerRemover::mozAutoRemovableBlockerRemover(nsIDocument* aDocument MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
{
  MOZILLA_GUARD_OBJECT_NOTIFIER_INIT;
  mNestingLevel = nsContentUtils::GetRemovableScriptBlockerLevel();
  mDocument = aDocument;
  nsISupports* sink = aDocument ? aDocument->GetCurrentContentSink() : nsnull;
  mObserver = do_QueryInterface(sink);
  for (PRUint32 i = 0; i < mNestingLevel; ++i) {
    if (mObserver) {
      mObserver->EndUpdate(mDocument, UPDATE_CONTENT_MODEL);
    }
    nsContentUtils::RemoveRemovableScriptBlocker();
  }

  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(), "killing mutation events");
}

mozAutoRemovableBlockerRemover::~mozAutoRemovableBlockerRemover()
{
  NS_ASSERTION(nsContentUtils::GetRemovableScriptBlockerLevel() == 0,
               "Should have had none");
  for (PRUint32 i = 0; i < mNestingLevel; ++i) {
    nsContentUtils::AddRemovableScriptBlocker();
    if (mObserver) {
      mObserver->BeginUpdate(mDocument, UPDATE_CONTENT_MODEL);
    }
  }
}

// static
PRBool
nsContentUtils::IsFocusedContent(const nsIContent* aContent)
{
  nsFocusManager* fm = nsFocusManager::GetFocusManager();

  return fm && fm->GetFocusedContent() == aContent;
}

bool
nsContentUtils::IsSubDocumentTabbable(nsIContent* aContent)
{
  nsIDocument* doc = aContent->GetCurrentDoc();
  if (!doc) {
    return false;
  }

  // XXXbz should this use GetOwnerDoc() for GetSubDocumentFor?
  // sXBL/XBL2 issue!
  nsIDocument* subDoc = doc->GetSubDocumentFor(aContent);
  if (!subDoc) {
    return false;
  }

  nsCOMPtr<nsISupports> container = subDoc->GetContainer();
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(container);
  if (!docShell) {
    return false;
  }

  nsCOMPtr<nsIContentViewer> contentViewer;
  docShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (!contentViewer) {
    return false;
  }

  nsCOMPtr<nsIContentViewer> zombieViewer;
  contentViewer->GetPreviousViewer(getter_AddRefs(zombieViewer));

  // If there are 2 viewers for the current docshell, that
  // means the current document is a zombie document.
  // Only navigate into the subdocument if it's not a zombie.
  return !zombieViewer;
}

void
nsContentUtils::FlushLayoutForTree(nsIDOMWindow* aWindow)
{
    nsCOMPtr<nsPIDOMWindow> piWin = do_QueryInterface(aWindow);
    if (!piWin)
        return;

    // Note that because FlushPendingNotifications flushes parents, this
    // is O(N^2) in docshell tree depth.  However, the docshell tree is
    // usually pretty shallow.

    nsCOMPtr<nsIDOMDocument> domDoc;
    aWindow->GetDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    if (doc) {
        doc->FlushPendingNotifications(Flush_Layout);
    }

    nsCOMPtr<nsIDocShellTreeNode> node =
        do_QueryInterface(piWin->GetDocShell());
    if (node) {
        PRInt32 i = 0, i_end;
        node->GetChildCount(&i_end);
        for (; i < i_end; ++i) {
            nsCOMPtr<nsIDocShellTreeItem> item;
            node->GetChildAt(i, getter_AddRefs(item));
            nsCOMPtr<nsIDOMWindow> win = do_GetInterface(item);
            if (win) {
                FlushLayoutForTree(win);
            }
        }
    }
}

void nsContentUtils::RemoveNewlines(nsString &aString)
{
  // strip CR/LF and null
  static const char badChars[] = {'\r', '\n', 0};
  aString.StripChars(badChars);
}

void
nsContentUtils::PlatformToDOMLineBreaks(nsString &aString)
{
  if (aString.FindChar(PRUnichar('\r')) != -1) {
    // Windows linebreaks: Map CRLF to LF:
    aString.ReplaceSubstring(NS_LITERAL_STRING("\r\n").get(),
                             NS_LITERAL_STRING("\n").get());

    // Mac linebreaks: Map any remaining CR to LF:
    aString.ReplaceSubstring(NS_LITERAL_STRING("\r").get(),
                             NS_LITERAL_STRING("\n").get());
  }
}

already_AddRefed<LayerManager>
nsContentUtils::LayerManagerForDocument(nsIDocument *aDoc)
{
  nsIDocument* doc = aDoc;
  nsIDocument* displayDoc = doc->GetDisplayDocument();
  if (displayDoc) {
    doc = displayDoc;
  }

  nsIPresShell* shell = doc->GetShell();
  nsCOMPtr<nsISupports> container = doc->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem = do_QueryInterface(container);
  while (!shell && docShellTreeItem) {
    // We may be in a display:none subdocument, or we may not have a presshell
    // created yet.
    // Walk the docshell tree to find the nearest container that has a presshell,
    // and find the root widget from that.
    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(docShellTreeItem);
    nsCOMPtr<nsIPresShell> presShell;
    docShell->GetPresShell(getter_AddRefs(presShell));
    if (presShell) {
      shell = presShell;
    } else {
      nsCOMPtr<nsIDocShellTreeItem> parent;
      docShellTreeItem->GetParent(getter_AddRefs(parent));
      docShellTreeItem = parent;
    }
  }

  if (shell) {
    nsIFrame* rootFrame = shell->FrameManager()->GetRootFrame();
    if (rootFrame) {
      nsIWidget* widget =
        nsLayoutUtils::GetDisplayRootFrame(rootFrame)->GetNearestWidget();
      if (widget) {
        nsRefPtr<LayerManager> manager = widget->GetLayerManager();
        return manager.forget();
      }
    }
  }

  nsRefPtr<LayerManager> manager = new BasicLayerManager();
  return manager.forget();
}


NS_IMPL_ISUPPORTS1(nsIContentUtils, nsIContentUtils)

PRBool
nsIContentUtils::IsSafeToRunScript()
{
  return nsContentUtils::IsSafeToRunScript();
}

PRBool
nsIContentUtils::ParseIntMarginValue(const nsAString& aString, nsIntMargin& result)
{
  return nsContentUtils::ParseIntMarginValue(aString, result);
}

already_AddRefed<nsIDocumentLoaderFactory>
nsIContentUtils::FindInternalContentViewer(const char* aType,
                                           ContentViewerType* aLoaderType)
{
  if (aLoaderType) {
    *aLoaderType = TYPE_UNSUPPORTED;
  }

  // one helper factory, please
  nsCOMPtr<nsICategoryManager> catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
  if (!catMan)
    return NULL;

  nsCOMPtr<nsIDocumentLoaderFactory> docFactory;

  nsXPIDLCString contractID;
  nsresult rv = catMan->GetCategoryEntry("Gecko-Content-Viewers", aType, getter_Copies(contractID));
  if (NS_SUCCEEDED(rv)) {
    docFactory = do_GetService(contractID);
    if (docFactory && aLoaderType) {
      if (contractID.EqualsLiteral(CONTENT_DLF_CONTRACTID))
        *aLoaderType = TYPE_CONTENT;
      else if (contractID.EqualsLiteral(PLUGIN_DLF_CONTRACTID))
        *aLoaderType = TYPE_PLUGIN;
      else
      *aLoaderType = TYPE_UNKNOWN;
    }   
    return docFactory.forget();
  }

#ifdef MOZ_MEDIA
#ifdef MOZ_OGG
  if (nsHTMLMediaElement::IsOggEnabled()) {
    for (unsigned int i = 0; i < NS_ARRAY_LENGTH(nsHTMLMediaElement::gOggTypes); ++i) {
      const char* type = nsHTMLMediaElement::gOggTypes[i];
      if (!strcmp(aType, type)) {
        docFactory = do_GetService("@mozilla.org/content/document-loader-factory;1");
        if (docFactory && aLoaderType) {
          *aLoaderType = TYPE_CONTENT;
        }
        return docFactory.forget();
      }
    }
  }
#endif

#ifdef MOZ_WEBM
  if (nsHTMLMediaElement::IsWebMEnabled()) {
    for (unsigned int i = 0; i < NS_ARRAY_LENGTH(nsHTMLMediaElement::gWebMTypes); ++i) {
      const char* type = nsHTMLMediaElement::gWebMTypes[i];
      if (!strcmp(aType, type)) {
        docFactory = do_GetService("@mozilla.org/content/document-loader-factory;1");
        if (docFactory && aLoaderType) {
          *aLoaderType = TYPE_CONTENT;
        }
        return docFactory.forget();
      }
    }
  }
#endif
#endif // MOZ_MEDIA

  return NULL;
}
