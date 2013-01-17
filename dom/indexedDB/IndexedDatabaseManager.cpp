/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IndexedDatabaseManager.h"

#include "mozIApplicationClearPrivateDataParams.h"
#include "nsIAtom.h"
#include "nsIConsoleService.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIFile.h"
#include "nsIFileStorage.h"
#include "nsIObserverService.h"
#include "nsIPrincipal.h"
#include "nsIScriptError.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsISHEntry.h"
#include "nsISimpleEnumerator.h"
#include "nsITimer.h"

#include "mozilla/dom/file/FileService.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/TabContext.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/storage.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsContentUtils.h"
#include "nsCRTGlue.h"
#include "nsDirectoryServiceUtils.h"
#include "nsEventDispatcher.h"
#include "nsScriptSecurityManager.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMPrivate.h"

#include "AsyncConnectionHelper.h"
#include "IDBDatabase.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBKeyRange.h"
#include "OpenDatabaseHelper.h"
#include "TransactionThreadPool.h"

#include "IndexedDatabaseInlines.h"
#include <algorithm>

// The amount of time, in milliseconds, that our IO thread will stay alive
// after the last event it processes.
#define DEFAULT_THREAD_TIMEOUT_MS 30000

// The amount of time, in milliseconds, that we will wait for active database
// transactions on shutdown before aborting them.
#define DEFAULT_SHUTDOWN_TIMER_MS 30000

// Amount of space that IndexedDB databases may use by default in megabytes.
#define DEFAULT_QUOTA_MB 50

// Preference that users can set to override DEFAULT_QUOTA_MB
#define PREF_INDEXEDDB_QUOTA "dom.indexedDB.warningQuota"

// profile-before-change, when we need to shut down IDB
#define PROFILE_BEFORE_CHANGE_OBSERVER_ID "profile-before-change"

USING_INDEXEDDB_NAMESPACE
using namespace mozilla::services;
using namespace mozilla::dom;
using mozilla::Preferences;
using mozilla::dom::file::FileService;
using mozilla::dom::quota::QuotaManager;

static NS_DEFINE_CID(kDOMSOF_CID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

namespace {

int32_t gShutdown = 0;
int32_t gClosed = 0;

// Does not hold a reference.
IndexedDatabaseManager* gInstance = nullptr;

int32_t gIndexedDBQuotaMB = DEFAULT_QUOTA_MB;

bool
GetDatabaseBaseFilename(const nsAString& aFilename,
                        nsAString& aDatabaseBaseFilename)
{
  NS_ASSERTION(!aFilename.IsEmpty(), "Bad argument!");

  NS_NAMED_LITERAL_STRING(sqlite, ".sqlite");
  nsAString::size_type filenameLen = aFilename.Length();
  nsAString::size_type sqliteLen = sqlite.Length();

  if (sqliteLen > filenameLen ||
      Substring(aFilename, filenameLen - sqliteLen, sqliteLen) != sqlite) {
    return false;
  }

  aDatabaseBaseFilename = Substring(aFilename, 0, filenameLen - sqliteLen);

  return true;
}

// Adds all databases in the hash to the given array.
template <class T>
PLDHashOperator
EnumerateToTArray(const nsACString& aKey,
                  nsTArray<IDBDatabase*>* aValue,
                  void* aUserArg)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");
  NS_ASSERTION(aUserArg, "Null pointer!");

  static_cast<nsTArray<T>*>(aUserArg)->AppendElements(*aValue);
  return PL_DHASH_NEXT;
}

bool
PatternMatchesOrigin(const nsACString& aPatternString, const nsACString& aOrigin)
{
  // Aren't we smart!
  return StringBeginsWith(aOrigin, aPatternString);
}

enum MozBrowserPatternFlag
{
  MozBrowser = 0,
  NotMozBrowser,
  IgnoreMozBrowser
};

// Use one of the friendly overloads below.
void
GetOriginPatternString(uint32_t aAppId, MozBrowserPatternFlag aBrowserFlag,
                       const nsACString& aOrigin, nsAutoCString& _retval)
{
  NS_ASSERTION(aAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID,
               "Bad appId!");
  NS_ASSERTION(aOrigin.IsEmpty() || aBrowserFlag != IgnoreMozBrowser,
               "Bad args!");

  if (aOrigin.IsEmpty()) {
    _retval.Truncate();

    _retval.AppendInt(aAppId);
    _retval.Append('+');

    if (aBrowserFlag != IgnoreMozBrowser) {
      if (aBrowserFlag == MozBrowser) {
        _retval.Append('t');
      }
      else {
        _retval.Append('f');
      }
      _retval.Append('+');
    }

    return;
  }

#ifdef DEBUG
  if (aAppId != nsIScriptSecurityManager::NO_APP_ID ||
      aBrowserFlag == MozBrowser) {
    nsAutoCString pattern;
    GetOriginPatternString(aAppId, aBrowserFlag, EmptyCString(), pattern);
    NS_ASSERTION(PatternMatchesOrigin(pattern, aOrigin),
                 "Origin doesn't match parameters!");
  }
#endif

  _retval = aOrigin;
}

void
GetOriginPatternString(uint32_t aAppId, bool aBrowserOnly,
                       const nsACString& aOrigin, nsAutoCString& _retval)
{
  return GetOriginPatternString(aAppId,
                                aBrowserOnly ? MozBrowser : NotMozBrowser,
                                aOrigin, _retval);
}

void
GetOriginPatternStringMaybeIgnoreBrowser(uint32_t aAppId, bool aBrowserOnly,
                                         nsAutoCString& _retval)
{
  return GetOriginPatternString(aAppId,
                                aBrowserOnly ? MozBrowser : IgnoreMozBrowser,
                                EmptyCString(), _retval);
}

template <class ValueType>
class PatternMatchArray : public nsAutoTArray<ValueType, 20>
{
  typedef PatternMatchArray<ValueType> SelfType;

  struct Closure
  {
    Closure(SelfType& aSelf, const nsACString& aPattern)
    : mSelf(aSelf), mPattern(aPattern)
    { }

    SelfType& mSelf;
    const nsACString& mPattern;
  };

public:
  template <class T>
  void
  Find(const T& aHashtable,
       const nsACString& aPattern)
  {
    SelfType::Clear();

    Closure closure(*this, aPattern);
    aHashtable.EnumerateRead(SelfType::Enumerate, &closure);
  }

private:
  static PLDHashOperator
  Enumerate(const nsACString& aKey,
            nsTArray<ValueType>* aValue,
            void* aUserArg)
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
    NS_ASSERTION(aValue, "Null pointer!");
    NS_ASSERTION(aUserArg, "Null pointer!");

    Closure* closure = static_cast<Closure*>(aUserArg);

    if (PatternMatchesOrigin(closure->mPattern, aKey)) {
      closure->mSelf.AppendElements(*aValue);
    }

    return PL_DHASH_NEXT;
  }
};

typedef PatternMatchArray<IDBDatabase*> DatabasePatternMatchArray;

PLDHashOperator
InvalidateAndRemoveFileManagers(
                           const nsACString& aKey,
                           nsAutoPtr<nsTArray<nsRefPtr<FileManager> > >& aValue,
                           void* aUserArg)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");

  const nsACString* pattern =
    static_cast<const nsACString*>(aUserArg);

  if (!pattern || PatternMatchesOrigin(*pattern, aKey)) {
    for (uint32_t i = 0; i < aValue->Length(); i++) {
      nsRefPtr<FileManager>& fileManager = aValue->ElementAt(i);
      fileManager->Invalidate();
    }
    return PL_DHASH_REMOVE;
  }

  return PL_DHASH_NEXT;
}

void
SanitizeOriginString(nsCString& aOrigin)
{
  // We want profiles to be platform-independent so we always need to replace
  // the same characters on every platform. Windows has the most extensive set
  // of illegal characters so we use its FILE_ILLEGAL_CHARACTERS and
  // FILE_PATH_SEPARATOR.
  static const char kReplaceChars[] = CONTROL_CHARACTERS "/:*?\"<>|\\";

#ifdef XP_WIN
  NS_ASSERTION(!strcmp(kReplaceChars,
                       FILE_ILLEGAL_CHARACTERS FILE_PATH_SEPARATOR),
               "Illegal file characters have changed!");
#endif

  aOrigin.ReplaceChar(kReplaceChars, '+');
}

nsresult
GetASCIIOriginFromURI(nsIURI* aURI,
                      uint32_t aAppId,
                      bool aInMozBrowser,
                      nsACString& aOrigin)
{
  NS_ASSERTION(aURI, "Null uri!");

  nsCString origin;
  mozilla::GetExtendedOrigin(aURI, aAppId, aInMozBrowser, origin);

  if (origin.IsEmpty()) {
    NS_WARNING("GetExtendedOrigin returned empty string!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  aOrigin.Assign(origin);
  return NS_OK;
}

nsresult
GetASCIIOriginFromPrincipal(nsIPrincipal* aPrincipal,
                            nsACString& aOrigin)
{
  NS_ASSERTION(aPrincipal, "Don't hand me a null principal!");

  static const char kChromeOrigin[] = "chrome";

  nsCString origin;
  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    origin.AssignLiteral(kChromeOrigin);
  }
  else {
    bool isNullPrincipal;
    nsresult rv = aPrincipal->GetIsNullPrincipal(&isNullPrincipal);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    if (isNullPrincipal) {
      NS_WARNING("IndexedDB not supported from this principal!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    rv = aPrincipal->GetExtendedOrigin(origin);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    if (origin.EqualsLiteral(kChromeOrigin)) {
      NS_WARNING("Non-chrome principal can't use chrome origin!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }

  aOrigin.Assign(origin);
  return NS_OK;
}

} // anonymous namespace

IndexedDatabaseManager::IndexedDatabaseManager()
: mFileMutex("IndexedDatabaseManager.mFileMutex")
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance, "More than one instance!");
}

IndexedDatabaseManager::~IndexedDatabaseManager()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance || gInstance == this, "Different instances!");
  gInstance = nullptr;
}

bool IndexedDatabaseManager::sIsMainProcess = false;

// static
already_AddRefed<IndexedDatabaseManager>
IndexedDatabaseManager::GetOrCreate()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (IsShuttingDown()) {
    NS_ERROR("Calling GetOrCreateInstance() after shutdown!");
    return nullptr;
  }

  nsRefPtr<IndexedDatabaseManager> instance(gInstance);

  if (!instance) {
    sIsMainProcess = XRE_GetProcessType() == GeckoProcessType_Default;

    instance = new IndexedDatabaseManager();

    instance->mLiveDatabases.Init();
    instance->mFileManagers.Init();

    nsresult rv;

    if (sIsMainProcess) {
      nsCOMPtr<nsIFile> dbBaseDirectory;
      rv = NS_GetSpecialDirectory(NS_APP_INDEXEDDB_PARENT_DIR,
                                  getter_AddRefs(dbBaseDirectory));
      if (NS_FAILED(rv)) {
          rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                      getter_AddRefs(dbBaseDirectory));
      }
      NS_ENSURE_SUCCESS(rv, nullptr);

      rv = dbBaseDirectory->Append(NS_LITERAL_STRING("indexedDB"));
      NS_ENSURE_SUCCESS(rv, nullptr);

      rv = dbBaseDirectory->GetPath(instance->mDatabaseBasePath);
      NS_ENSURE_SUCCESS(rv, nullptr);

      // Make a lazy thread for any IO we need (like clearing or enumerating the
      // contents of indexedDB database directories).
      instance->mIOThread =
        new LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS,
                           NS_LITERAL_CSTRING("IndexedDB I/O"),
                           LazyIdleThread::ManualShutdown);

      // Make a timer here to avoid potential failures later. We don't actually
      // initialize the timer until shutdown.
      instance->mShutdownTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
      NS_ENSURE_TRUE(instance->mShutdownTimer, nullptr);
    }

    // Make sure that the quota manager is up.
    NS_ENSURE_TRUE(QuotaManager::GetOrCreate(), nullptr);

    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    NS_ENSURE_TRUE(obs, nullptr);

    // We need this callback to know when to shut down all our threads.
    rv = obs->AddObserver(instance, PROFILE_BEFORE_CHANGE_OBSERVER_ID, false);
    NS_ENSURE_SUCCESS(rv, nullptr);

    if (NS_FAILED(Preferences::AddIntVarCache(&gIndexedDBQuotaMB,
                                              PREF_INDEXEDDB_QUOTA,
                                              DEFAULT_QUOTA_MB))) {
      NS_WARNING("Unable to respond to quota pref changes!");
      gIndexedDBQuotaMB = DEFAULT_QUOTA_MB;
    }

    // The observer service will hold our last reference, don't AddRef here.
    gInstance = instance;
  }

  return instance.forget();
}

// static
IndexedDatabaseManager*
IndexedDatabaseManager::Get()
{
  // Does not return an owning reference.
  return gInstance;
}

// static
IndexedDatabaseManager*
IndexedDatabaseManager::FactoryCreate()
{
  // Returns a raw pointer that carries an owning reference! Lame, but the
  // singleton factory macros force this.
  return GetOrCreate().get();
}

nsresult
IndexedDatabaseManager::GetDirectoryForOrigin(const nsACString& aASCIIOrigin,
                                              nsIFile** aDirectory) const
{
  nsresult rv;
  nsCOMPtr<nsIFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = directory->InitWithPath(GetBaseDirectory());
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString originSanitized(aASCIIOrigin);
  SanitizeOriginString(originSanitized);

  rv = directory->Append(NS_ConvertASCIItoUTF16(originSanitized));
  NS_ENSURE_SUCCESS(rv, rv);

  directory.forget(aDirectory);
  return NS_OK;
}

// static
already_AddRefed<nsIAtom>
IndexedDatabaseManager::GetDatabaseId(const nsACString& aOrigin,
                                      const nsAString& aName)
{
  nsCString str(aOrigin);
  str.Append("*");
  str.Append(NS_ConvertUTF16toUTF8(aName));

  nsCOMPtr<nsIAtom> atom = do_GetAtom(str);
  NS_ENSURE_TRUE(atom, nullptr);

  return atom.forget();
}

// static
nsresult
IndexedDatabaseManager::FireWindowOnError(nsPIDOMWindow* aOwner,
                                          nsEventChainPostVisitor& aVisitor)
{
  NS_ENSURE_TRUE(aVisitor.mDOMEvent, NS_ERROR_UNEXPECTED);
  if (!aOwner) {
    return NS_OK;
  }

  if (aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault) {
    return NS_OK;
  }

  nsString type;
  nsresult rv = aVisitor.mDOMEvent->GetType(type);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!type.EqualsLiteral(ERROR_EVT_STR)) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMEventTarget> eventTarget;
  rv = aVisitor.mDOMEvent->GetTarget(getter_AddRefs(eventTarget));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIIDBRequest> strongRequest = do_QueryInterface(eventTarget);
  IDBRequest* request = static_cast<IDBRequest*>(strongRequest.get());
  NS_ENSURE_TRUE(request, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMDOMError> error;
  rv = request->GetError(getter_AddRefs(error));
  NS_ENSURE_SUCCESS(rv, rv);

  nsString errorName;
  if (error) {
    rv = error->GetName(errorName);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsScriptErrorEvent event(true, NS_LOAD_ERROR);
  request->FillScriptErrorEvent(&event);
  NS_ABORT_IF_FALSE(event.fileName,
                    "FillScriptErrorEvent should give us a non-null string "
                    "for our error's fileName");

  event.errorMsg = errorName.get();

  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(aOwner));
  NS_ASSERTION(sgo, "How can this happen?!");

  nsEventStatus status = nsEventStatus_eIgnore;
  if (NS_FAILED(sgo->HandleScriptError(&event, &status))) {
    NS_WARNING("Failed to dispatch script error event");
    status = nsEventStatus_eIgnore;
  }

  bool preventDefaultCalled = status == nsEventStatus_eConsumeNoDefault;
  if (preventDefaultCalled) {
    return NS_OK;
  }

  // Log an error to the error console.
  nsCOMPtr<nsIScriptError> scriptError =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_FAILED(scriptError->InitWithWindowID(errorName,
                                              nsDependentString(event.fileName),
                                              EmptyString(), event.lineNr,
                                              0, 0,
                                              "IndexedDB",
                                              aOwner->WindowID()))) {
    NS_WARNING("Failed to init script error!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return consoleService->LogMessage(scriptError);
}

// static
bool
IndexedDatabaseManager::TabContextMayAccessOrigin(const TabContext& aContext,
                                                  const nsACString& aOrigin)
{
  NS_ASSERTION(!aOrigin.IsEmpty(), "Empty origin!");

  // If aContext is for a browser element, it's allowed only to access other
  // browser elements.  But if aContext is not for a browser element, it may
  // access both browser and non-browser elements.
  nsAutoCString pattern;
  GetOriginPatternStringMaybeIgnoreBrowser(aContext.OwnOrContainingAppId(),
                                           aContext.IsBrowserElement(),
                                           pattern);

  return PatternMatchesOrigin(pattern, aOrigin);
}

bool
IndexedDatabaseManager::RegisterDatabase(IDBDatabase* aDatabase)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabase, "Null pointer!");

  // Don't allow any new databases to be created after shutdown.
  if (IsShuttingDown()) {
    return false;
  }

  // Add this database to its origin array if it exists, create it otherwise.
  nsTArray<IDBDatabase*>* array;
  if (!mLiveDatabases.Get(aDatabase->Origin(), &array)) {
    nsAutoPtr<nsTArray<IDBDatabase*> > newArray(new nsTArray<IDBDatabase*>());
    mLiveDatabases.Put(aDatabase->Origin(), newArray);
    array = newArray.forget();
  }
  if (!array->AppendElement(aDatabase)) {
    NS_WARNING("Out of memory?");
    return false;
  }

  aDatabase->mRegistered = true;
  return true;
}

void
IndexedDatabaseManager::UnregisterDatabase(IDBDatabase* aDatabase)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabase, "Null pointer!");

  // Remove this database from its origin array, maybe remove the array if it
  // is then empty.
  nsTArray<IDBDatabase*>* array;
  if (mLiveDatabases.Get(aDatabase->Origin(), &array) &&
      array->RemoveElement(aDatabase)) {
    if (array->IsEmpty()) {
      mLiveDatabases.Remove(aDatabase->Origin());
    }
    return;
  }
  NS_ERROR("Didn't know anything about this database!");
}

void
IndexedDatabaseManager::OnUsageCheckComplete(AsyncUsageRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aRunnable, "Null pointer!");
  NS_ASSERTION(!aRunnable->mURI, "Should have been cleared!");
  NS_ASSERTION(!aRunnable->mCallback, "Should have been cleared!");

  if (!mUsageRunnables.RemoveElement(aRunnable)) {
    NS_ERROR("Don't know anything about this runnable!");
  }
}

nsresult
IndexedDatabaseManager::WaitForOpenAllowed(
                                  const OriginOrPatternString& aOriginOrPattern,
                                  nsIAtom* aId,
                                  nsIRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aOriginOrPattern.IsEmpty(), "Empty pattern!");
  NS_ASSERTION(aRunnable, "Null pointer!");

  nsAutoPtr<SynchronizedOp> op(new SynchronizedOp(aOriginOrPattern, aId));

  // See if this runnable needs to wait.
  bool delayed = false;
  for (uint32_t index = mSynchronizedOps.Length(); index > 0; index--) {
    nsAutoPtr<SynchronizedOp>& existingOp = mSynchronizedOps[index - 1];
    if (op->MustWaitFor(*existingOp)) {
      existingOp->DelayRunnable(aRunnable);
      delayed = true;
      break;
    }
  }

  // Otherwise, dispatch it immediately.
  if (!delayed) {
    nsresult rv = NS_DispatchToCurrentThread(aRunnable);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Adding this to the synchronized ops list will block any additional
  // ops from proceeding until this one is done.
  mSynchronizedOps.AppendElement(op.forget());

  return NS_OK;
}

void
IndexedDatabaseManager::AllowNextSynchronizedOp(
                                  const OriginOrPatternString& aOriginOrPattern,
                                  nsIAtom* aId)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aOriginOrPattern.IsEmpty(), "Empty origin/pattern!");

  uint32_t count = mSynchronizedOps.Length();
  for (uint32_t index = 0; index < count; index++) {
    nsAutoPtr<SynchronizedOp>& op = mSynchronizedOps[index];
    if (op->mOriginOrPattern.IsOrigin() == aOriginOrPattern.IsOrigin() &&
        op->mOriginOrPattern == aOriginOrPattern) {
      if (op->mId == aId) {
        NS_ASSERTION(op->mDatabases.IsEmpty(), "How did this happen?");

        op->DispatchDelayedRunnables();

        mSynchronizedOps.RemoveElementAt(index);
        return;
      }

      // If one or the other is for an origin clear, we should have matched
      // solely on origin.
      NS_ASSERTION(op->mId && aId, "Why didn't we match earlier?");
    }
  }

  NS_NOTREACHED("Why didn't we find a SynchronizedOp?");
}

nsresult
IndexedDatabaseManager::AcquireExclusiveAccess(
                                           const nsACString& aPattern,
                                           IDBDatabase* aDatabase,
                                           AsyncConnectionHelper* aHelper,
                                           nsIRunnable* aRunnable,
                                           WaitingOnDatabasesCallback aCallback,
                                           void* aClosure)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aDatabase || aHelper, "Need a helper with a database!");
  NS_ASSERTION(aDatabase || aRunnable, "Need a runnable without a database!");

  // Find the right SynchronizedOp.
  SynchronizedOp* op =
    FindSynchronizedOp(aPattern, aDatabase ? aDatabase->Id() : nullptr);

  NS_ASSERTION(op, "We didn't find a SynchronizedOp?");
  NS_ASSERTION(!op->mHelper, "SynchronizedOp already has a helper?!?");
  NS_ASSERTION(!op->mRunnable, "SynchronizedOp already has a runnable?!?");

  DatabasePatternMatchArray matches;
  matches.Find(mLiveDatabases, aPattern);

  // We need to wait for the databases to go away.
  // Hold on to all database objects that represent the same database file
  // (except the one that is requesting this version change).
  nsTArray<nsRefPtr<IDBDatabase> > liveDatabases;

  if (!matches.IsEmpty()) {
    if (aDatabase) {
      // Grab all databases that are not yet closed but whose database id match
      // the one we're looking for.
      for (uint32_t index = 0; index < matches.Length(); index++) {
        IDBDatabase*& database = matches[index];
        if (!database->IsClosed() &&
            database != aDatabase &&
            database->Id() == aDatabase->Id()) {
          liveDatabases.AppendElement(database);
        }
      }
    }
    else {
      // We want *all* databases, even those that are closed, if we're going to
      // clear the origin.
      liveDatabases.AppendElements(matches);
    }
  }

  op->mHelper = aHelper;
  op->mRunnable = aRunnable;

  if (!liveDatabases.IsEmpty()) {
    NS_ASSERTION(op->mDatabases.IsEmpty(),
                 "How do we already have databases here?");
    op->mDatabases.AppendElements(liveDatabases);

    // Give our callback the databases so it can decide what to do with them.
    aCallback(liveDatabases, aClosure);

    NS_ASSERTION(liveDatabases.IsEmpty(),
                 "Should have done something with the array!");

    if (aDatabase) {
      // Wait for those databases to close.
      return NS_OK;
    }
  }

  // If we're trying to open a database and nothing blocks it, or if we're
  // clearing an origin, then go ahead and schedule the op.
  nsresult rv = RunSynchronizedOp(aDatabase, op);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
bool
IndexedDatabaseManager::IsShuttingDown()
{
  return !!gShutdown;
}

// static
bool
IndexedDatabaseManager::IsClosed()
{
  return !!gClosed;
}

void
IndexedDatabaseManager::AbortCloseDatabasesForWindow(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aWindow, "Null pointer!");

  nsAutoTArray<IDBDatabase*, 50> liveDatabases;
  mLiveDatabases.EnumerateRead(EnumerateToTArray<IDBDatabase*>,
                               &liveDatabases);

  FileService* service = FileService::Get();
  TransactionThreadPool* pool = TransactionThreadPool::Get();

  for (uint32_t index = 0; index < liveDatabases.Length(); index++) {
    IDBDatabase*& database = liveDatabases[index];
    if (database->GetOwner() == aWindow) {
      if (NS_FAILED(database->Close())) {
        NS_WARNING("Failed to close database for dying window!");
      }

      if (service) {
        service->AbortLockedFilesForStorage(database);
      }

      if (pool) {
        pool->AbortTransactionsForDatabase(database);
      }
    }
  }
}

bool
IndexedDatabaseManager::HasOpenTransactions(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aWindow, "Null pointer!");

  nsAutoTArray<IDBDatabase*, 50> liveDatabases;
  mLiveDatabases.EnumerateRead(EnumerateToTArray<IDBDatabase*>,
                               &liveDatabases);
  
  FileService* service = FileService::Get();
  TransactionThreadPool* pool = TransactionThreadPool::Get();
  if (!service && !pool) {
    return false;
  }

  for (uint32_t index = 0; index < liveDatabases.Length(); index++) {
    IDBDatabase*& database = liveDatabases[index];
    if (database->GetOwner() == aWindow &&
        ((service && service->HasLockedFilesForStorage(database)) ||
         (pool && pool->HasTransactionsForDatabase(database)))) {
      return true;
    }
  }
  
  return false;
}

void
IndexedDatabaseManager::OnDatabaseClosed(IDBDatabase* aDatabase)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabase, "Null pointer!");

  // Check through the list of SynchronizedOps to see if any are waiting for
  // this database to close before proceeding.
  SynchronizedOp* op = FindSynchronizedOp(aDatabase->Origin(), aDatabase->Id());
  if (op) {
    // This database is in the scope of this SynchronizedOp.  Remove it
    // from the list if necessary.
    if (op->mDatabases.RemoveElement(aDatabase)) {
      // Now set up the helper if there are no more live databases.
      NS_ASSERTION(op->mHelper || op->mRunnable,
                   "How did we get rid of the helper/runnable before "
                    "removing the last database?");
      if (op->mDatabases.IsEmpty()) {
        // At this point, all databases are closed, so no new transactions
        // can be started.  There may, however, still be outstanding
        // transactions that have not completed.  We need to wait for those
        // before we dispatch the helper.
        if (NS_FAILED(RunSynchronizedOp(aDatabase, op))) {
          NS_WARNING("Failed to run synchronized op!");
        }
      }
    }
  }
}

// static
uint32_t
IndexedDatabaseManager::GetIndexedDBQuotaMB()
{
  return uint32_t(std::max(gIndexedDBQuotaMB, 0));
}

nsresult
IndexedDatabaseManager::EnsureOriginIsInitialized(const nsACString& aOrigin,
                                                  FactoryPrivilege aPrivilege,
                                                  nsIFile** aDirectory)
{
#ifdef DEBUG
  {
    bool correctThread;
    NS_ASSERTION(NS_SUCCEEDED(mIOThread->IsOnCurrentThread(&correctThread)) &&
                 correctThread,
                 "Running on the wrong thread!");
  }
#endif

  nsCOMPtr<nsIFile> directory;
  nsresult rv = GetDirectoryForOrigin(aOrigin, getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = directory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    bool isDirectory;
    rv = directory->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(isDirectory, NS_ERROR_UNEXPECTED);
  }
  else {
    rv = directory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mInitializedOrigins.Contains(aOrigin)) {
    NS_ADDREF(*aDirectory = directory);
    return NS_OK;
  }

  // We need to see if there are any files in the directory already. If they
  // are database files then we need to cleanup stored files (if it's needed)
  // and also initialize the quota.

  nsAutoTArray<nsString, 20> subdirsToProcess;
  nsAutoTArray<nsCOMPtr<nsIFile> , 20> unknownFiles;

  uint64_t usage = 0;

  nsTHashtable<nsStringHashKey> validSubdirs;
  validSubdirs.Init(20);

  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = directory->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) && hasMore) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
    NS_ENSURE_TRUE(file, NS_NOINTERFACE);

    nsString leafName;
    rv = file->GetLeafName(leafName);
    NS_ENSURE_SUCCESS(rv, rv);

    if (StringEndsWith(leafName, NS_LITERAL_STRING(".sqlite-journal"))) {
      continue;
    }

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);

    if (isDirectory) {
      if (!validSubdirs.GetEntry(leafName)) {
        subdirsToProcess.AppendElement(leafName);
      }
      continue;
    }

    nsString dbBaseFilename;
    if (!GetDatabaseBaseFilename(leafName, dbBaseFilename)) {
      unknownFiles.AppendElement(file);
      continue;
    }

    nsCOMPtr<nsIFile> fmDirectory;
    rv = directory->Clone(getter_AddRefs(fmDirectory));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = fmDirectory->Append(dbBaseFilename);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = FileManager::InitDirectory(fmDirectory, file, aOrigin);
    NS_ENSURE_SUCCESS(rv, rv);

    if (aPrivilege != Chrome) {
      uint64_t fileUsage;
      rv = FileManager::GetUsage(fmDirectory, &fileUsage);
      NS_ENSURE_SUCCESS(rv, rv);

      IncrementUsage(&usage, fileUsage);

      int64_t fileSize;
      rv = file->GetFileSize(&fileSize);
      NS_ENSURE_SUCCESS(rv, rv);

      IncrementUsage(&usage, uint64_t(fileSize));
    }

    validSubdirs.PutEntry(dbBaseFilename);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  for (uint32_t i = 0; i < subdirsToProcess.Length(); i++) {
    const nsString& subdir = subdirsToProcess[i];
    if (!validSubdirs.GetEntry(subdir)) {
      NS_WARNING("Unknown subdirectory found!");
      return NS_ERROR_UNEXPECTED;
    }
  }

  for (uint32_t i = 0; i < unknownFiles.Length(); i++) {
    nsCOMPtr<nsIFile>& unknownFile = unknownFiles[i];

    // Some temporary SQLite files could disappear, so we have to check if the
    // unknown file still exists.
    bool exists;
    rv = unknownFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (exists) {
      nsString leafName;
      unknownFile->GetLeafName(leafName);

      // The journal file may exists even after db has been correctly opened.
      if (!StringEndsWith(leafName, NS_LITERAL_STRING(".sqlite-journal"))) {
        NS_WARNING("Unknown file found!");
        return NS_ERROR_UNEXPECTED;
      }
    }
  }

  if (aPrivilege != Chrome) {
    QuotaManager* quotaManager = QuotaManager::Get();
    NS_ASSERTION(quotaManager, "Shouldn't be null!");

    quotaManager->InitQuotaForOrigin(aOrigin, GetIndexedDBQuotaMB(), usage);
  }

  mInitializedOrigins.AppendElement(aOrigin);

  NS_ADDREF(*aDirectory = directory);
  return NS_OK;
}

void
IndexedDatabaseManager::UninitializeOriginsByPattern(
                                                    const nsACString& aPattern)
{
#ifdef DEBUG
  {
    bool correctThread;
    NS_ASSERTION(NS_SUCCEEDED(mIOThread->IsOnCurrentThread(&correctThread)) &&
                 correctThread,
                 "Running on the wrong thread!");
  }
#endif

  for (int32_t i = mInitializedOrigins.Length() - 1; i >= 0; i--) {
    if (PatternMatchesOrigin(aPattern, mInitializedOrigins[i])) {
      mInitializedOrigins.RemoveElementAt(i);
    }
  }
}

// static
nsresult
IndexedDatabaseManager::GetASCIIOriginFromWindow(nsPIDOMWindow* aWindow,
                                                 nsCString& aASCIIOrigin)
{
  NS_ASSERTION(NS_IsMainThread(),
               "We're about to touch a window off the main thread!");

  if (!aWindow) {
    aASCIIOrigin.AssignLiteral("chrome");
    NS_ASSERTION(nsContentUtils::IsCallerChrome(),
                 "Null window but not chrome!");
    return NS_OK;
  }

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(sop, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  NS_ENSURE_TRUE(principal, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsresult rv = GetASCIIOriginFromPrincipal(principal, aASCIIOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

#ifdef DEBUG
//static
bool
IndexedDatabaseManager::IsMainProcess()
{
  NS_ASSERTION(gInstance,
               "IsMainProcess() called before indexedDB has been initialized!");
  NS_ASSERTION((XRE_GetProcessType() == GeckoProcessType_Default) ==
               sIsMainProcess, "XRE_GetProcessType changed its tune!");
  return sIsMainProcess;
}
#endif

already_AddRefed<FileManager>
IndexedDatabaseManager::GetFileManager(const nsACString& aOrigin,
                                       const nsAString& aDatabaseName)
{
  nsTArray<nsRefPtr<FileManager> >* array;
  if (!mFileManagers.Get(aOrigin, &array)) {
    return nullptr;
  }

  for (uint32_t i = 0; i < array->Length(); i++) {
    nsRefPtr<FileManager>& fileManager = array->ElementAt(i);

    if (fileManager->DatabaseName().Equals(aDatabaseName)) {
      nsRefPtr<FileManager> result = fileManager;
      return result.forget();
    }
  }
  
  return nullptr;
}

void
IndexedDatabaseManager::AddFileManager(FileManager* aFileManager)
{
  NS_ASSERTION(aFileManager, "Null file manager!");

  nsTArray<nsRefPtr<FileManager> >* array;
  if (!mFileManagers.Get(aFileManager->Origin(), &array)) {
    array = new nsTArray<nsRefPtr<FileManager> >();
    mFileManagers.Put(aFileManager->Origin(), array);
  }

  array->AppendElement(aFileManager);
}

void
IndexedDatabaseManager::InvalidateFileManagersForPattern(
                                                     const nsACString& aPattern)
{
  NS_ASSERTION(!aPattern.IsEmpty(), "Empty pattern!");
  mFileManagers.Enumerate(InvalidateAndRemoveFileManagers,
                          const_cast<nsACString*>(&aPattern));
}

void
IndexedDatabaseManager::InvalidateFileManager(const nsACString& aOrigin,
                                              const nsAString& aDatabaseName)
{
  nsTArray<nsRefPtr<FileManager> >* array;
  if (!mFileManagers.Get(aOrigin, &array)) {
    return;
  }

  for (uint32_t i = 0; i < array->Length(); i++) {
    nsRefPtr<FileManager> fileManager = array->ElementAt(i);
    if (fileManager->DatabaseName().Equals(aDatabaseName)) {
      fileManager->Invalidate();
      array->RemoveElementAt(i);

      if (array->IsEmpty()) {
        mFileManagers.Remove(aOrigin);
      }

      break;
    }
  }
}

nsresult
IndexedDatabaseManager::AsyncDeleteFile(FileManager* aFileManager,
                                        int64_t aFileId)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG_POINTER(aFileManager);

  // See if we're currently clearing the databases for this origin. If so then
  // we pretend that we've already deleted everything.
  if (IsClearOriginPending(aFileManager->Origin())) {
    return NS_OK;
  }

  nsRefPtr<AsyncDeleteFileRunnable> runnable =
    new AsyncDeleteFileRunnable(aFileManager, aFileId);

  nsresult rv = mIOThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
nsresult
IndexedDatabaseManager::RunSynchronizedOp(IDBDatabase* aDatabase,
                                          SynchronizedOp* aOp)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aOp, "Null pointer!");
  NS_ASSERTION(!aDatabase || aOp->mHelper, "No helper on this op!");
  NS_ASSERTION(aDatabase || aOp->mRunnable, "No runnable on this op!");
  NS_ASSERTION(!aDatabase || aOp->mDatabases.IsEmpty(),
               "This op isn't ready to run!");

  FileService* service = FileService::Get();
  TransactionThreadPool* pool = TransactionThreadPool::Get();

  nsTArray<IDBDatabase*> databases;
  if (aDatabase) {
    if (service || pool) {
      databases.AppendElement(aDatabase);
    }
  }
  else {
    aOp->mDatabases.SwapElements(databases);
  }

  uint32_t waitCount = service && pool && !databases.IsEmpty() ? 2 : 1;

  nsRefPtr<WaitForTransactionsToFinishRunnable> runnable =
    new WaitForTransactionsToFinishRunnable(aOp, waitCount);

  // There's no point in delaying if we don't yet have a transaction thread pool
  // or a file service. Also, if we're not waiting on any databases then we can
  // also run immediately.
  if (!(service || pool) || databases.IsEmpty()) {
    nsresult rv = runnable->Run();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  // Ask each service to call us back when they're done with this database.
  if (service) {
    // Have to copy here in case the pool needs a list too.
    nsTArray<nsCOMPtr<nsIFileStorage> > array;
    array.AppendElements(databases);

    if (!service->WaitForAllStoragesToComplete(array, runnable)) {
      NS_WARNING("Failed to wait for storages to complete!");
      return NS_ERROR_FAILURE;
    }
  }

  if (pool && !pool->WaitForAllDatabasesToComplete(databases, runnable)) {
    NS_WARNING("Failed to wait for databases to complete!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

IndexedDatabaseManager::SynchronizedOp*
IndexedDatabaseManager::FindSynchronizedOp(const nsACString& aPattern,
                                           nsIAtom* aId)
{
  for (uint32_t index = 0; index < mSynchronizedOps.Length(); index++) {
    const nsAutoPtr<SynchronizedOp>& currentOp = mSynchronizedOps[index];
    if (PatternMatchesOrigin(aPattern, currentOp->mOriginOrPattern) &&
        (!currentOp->mId || currentOp->mId == aId)) {
      return currentOp;
    }
  }

  return nullptr;
}

nsresult
IndexedDatabaseManager::ClearDatabasesForApp(uint32_t aAppId, bool aBrowserOnly)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID,
               "Bad appId!");

  // This only works from the main process.
  NS_ENSURE_TRUE(IsMainProcess(), NS_ERROR_NOT_AVAILABLE);

  nsAutoCString pattern;
  GetOriginPatternStringMaybeIgnoreBrowser(aAppId, aBrowserOnly, pattern);

  // If there is a pending or running clear operation for this app, return
  // immediately.
  if (IsClearOriginPending(pattern)) {
    return NS_OK;
  }

  OriginOrPatternString oops = OriginOrPatternString::FromPattern(pattern);

  // Queue up the origin clear runnable.
  nsRefPtr<OriginClearRunnable> runnable = new OriginClearRunnable(oops);

  nsresult rv = WaitForOpenAllowed(oops, nullptr, runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  runnable->AdvanceState();

  // Give the runnable some help by invalidating any databases in the way.
  DatabasePatternMatchArray matches;
  matches.Find(mLiveDatabases, pattern);

  for (uint32_t index = 0; index < matches.Length(); index++) {
    // We need to grab references here to prevent the database from dying while
    // we invalidate it.
    nsRefPtr<IDBDatabase> database = matches[index];
    database->Invalidate();
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS2(IndexedDatabaseManager, nsIIndexedDatabaseManager,
                                           nsIObserver)

NS_IMETHODIMP
IndexedDatabaseManager::GetUsageForURI(
                                     nsIURI* aURI,
                                     nsIIndexedDatabaseUsageCallback* aCallback,
                                     uint32_t aAppId,
                                     bool aInMozBrowserOnly,
                                     uint8_t aOptionalArgCount)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aCallback);

  // This only works from the main process.
  NS_ENSURE_TRUE(IsMainProcess(), NS_ERROR_NOT_AVAILABLE);

  if (!aOptionalArgCount) {
    aAppId = nsIScriptSecurityManager::NO_APP_ID;
  }

  // Figure out which origin we're dealing with.
  nsCString origin;
  nsresult rv = GetASCIIOriginFromURI(aURI, aAppId, aInMozBrowserOnly, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginOrPatternString oops = OriginOrPatternString::FromOrigin(origin);

  nsRefPtr<AsyncUsageRunnable> runnable =
    new AsyncUsageRunnable(aAppId, aInMozBrowserOnly, oops, aURI, aCallback);

  nsRefPtr<AsyncUsageRunnable>* newRunnable =
    mUsageRunnables.AppendElement(runnable);
  NS_ENSURE_TRUE(newRunnable, NS_ERROR_OUT_OF_MEMORY);

  // Otherwise put the computation runnable in the queue.
  rv = WaitForOpenAllowed(oops, nullptr, runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  runnable->AdvanceState();

  return NS_OK;
}

NS_IMETHODIMP
IndexedDatabaseManager::CancelGetUsageForURI(
                                     nsIURI* aURI,
                                     nsIIndexedDatabaseUsageCallback* aCallback,
                                     uint32_t aAppId,
                                     bool aInMozBrowserOnly,
                                     uint8_t aOptionalArgCount)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aCallback);

  // This only works from the main process.
  NS_ENSURE_TRUE(IsMainProcess(), NS_ERROR_NOT_AVAILABLE);

  if (!aOptionalArgCount) {
    aAppId = nsIScriptSecurityManager::NO_APP_ID;
  }

  // See if one of our pending callbacks matches both the URI and the callback
  // given. Cancel an remove it if so.
  for (uint32_t index = 0; index < mUsageRunnables.Length(); index++) {
    nsRefPtr<AsyncUsageRunnable>& runnable = mUsageRunnables[index];

    if (runnable->mAppId == aAppId &&
        runnable->mInMozBrowserOnly == aInMozBrowserOnly) {
      bool equals;
      nsresult rv = runnable->mURI->Equals(aURI, &equals);
      NS_ENSURE_SUCCESS(rv, rv);

      if (equals && SameCOMIdentity(aCallback, runnable->mCallback)) {
        runnable->Cancel();
        break;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
IndexedDatabaseManager::ClearDatabasesForURI(nsIURI* aURI,
                                             uint32_t aAppId,
                                             bool aInMozBrowserOnly,
                                             uint8_t aOptionalArgCount)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG_POINTER(aURI);

  // This only works from the main process.
  NS_ENSURE_TRUE(IsMainProcess(), NS_ERROR_NOT_AVAILABLE);

  if (!aOptionalArgCount) {
    aAppId = nsIScriptSecurityManager::NO_APP_ID;
  }

  // Figure out which origin we're dealing with.
  nsCString origin;
  nsresult rv = GetASCIIOriginFromURI(aURI, aAppId, aInMozBrowserOnly, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString pattern;
  GetOriginPatternString(aAppId, aInMozBrowserOnly, origin, pattern);

  // If there is a pending or running clear operation for this origin, return
  // immediately.
  if (IsClearOriginPending(pattern)) {
    return NS_OK;
  }

  OriginOrPatternString oops = OriginOrPatternString::FromPattern(pattern);

  // Queue up the origin clear runnable.
  nsRefPtr<OriginClearRunnable> runnable = new OriginClearRunnable(oops);

  rv = WaitForOpenAllowed(oops, nullptr, runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  runnable->AdvanceState();

  // Give the runnable some help by invalidating any databases in the way.
  DatabasePatternMatchArray matches;
  matches.Find(mLiveDatabases, pattern);

  for (uint32_t index = 0; index < matches.Length(); index++) {
    // We need to grab references to any live databases here to prevent them
    // from dying while we invalidate them.
    nsRefPtr<IDBDatabase> database = matches[index];
    database->Invalidate();
  }

  // After everything has been invalidated the helper should be dispatched to
  // the end of the event queue.
  return NS_OK;
}

NS_IMETHODIMP
IndexedDatabaseManager::Observe(nsISupports* aSubject,
                                const char* aTopic,
                                const PRUnichar* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!strcmp(aTopic, PROFILE_BEFORE_CHANGE_OBSERVER_ID)) {
    // Setting this flag prevents the service from being recreated and prevents
    // further databases from being created.
    if (PR_ATOMIC_SET(&gShutdown, 1)) {
      NS_ERROR("Shutdown more than once?!");
    }

    if (sIsMainProcess) {
      FileService* service = FileService::Get();
      if (service) {
        // This should only wait for IDB databases (file storages) to complete.
        // Other file storages may still have running locked files.
        // If the necko service (thread pool) gets the shutdown notification
        // first then the sync loop won't be processed at all, otherwise it will
        // lock the main thread until all IDB file storages are finished.

        nsTArray<nsCOMPtr<nsIFileStorage> >
          liveDatabases(mLiveDatabases.Count());
        mLiveDatabases.EnumerateRead(
                                  EnumerateToTArray<nsCOMPtr<nsIFileStorage> >,
                                  &liveDatabases);

        if (!liveDatabases.IsEmpty()) {
          nsRefPtr<WaitForLockedFilesToFinishRunnable> runnable =
            new WaitForLockedFilesToFinishRunnable();

          if (!service->WaitForAllStoragesToComplete(liveDatabases,
                                                     runnable)) {
            NS_WARNING("Failed to wait for databases to complete!");
          }

          nsIThread* thread = NS_GetCurrentThread();
          while (runnable->IsBusy()) {
            if (!NS_ProcessNextEvent(thread)) {
              NS_ERROR("Failed to process next event!");
              break;
            }
          }
        }
      }

      // Make sure to join with our IO thread.
      if (NS_FAILED(mIOThread->Shutdown())) {
        NS_WARNING("Failed to shutdown IO thread!");
      }

      // Kick off the shutdown timer.
      if (NS_FAILED(mShutdownTimer->Init(this, DEFAULT_SHUTDOWN_TIMER_MS,
                                         nsITimer::TYPE_ONE_SHOT))) {
        NS_WARNING("Failed to initialize shutdown timer!");
      }

      // This will spin the event loop while we wait on all the database threads
      // to close. Our timer may fire during that loop.
      TransactionThreadPool::Shutdown();

      // Cancel the timer regardless of whether it actually fired.
      if (NS_FAILED(mShutdownTimer->Cancel())) {
        NS_WARNING("Failed to cancel shutdown timer!");
      }
    }

    mFileManagers.Enumerate(InvalidateAndRemoveFileManagers, nullptr);

    if (PR_ATOMIC_SET(&gClosed, 1)) {
      NS_ERROR("Close more than once?!");
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC)) {
    NS_ASSERTION(sIsMainProcess, "Should only happen in the main process!");

    NS_WARNING("Some database operations are taking longer than expected "
               "during shutdown and will be aborted!");

    // Grab all live databases, for all origins.
    nsAutoTArray<IDBDatabase*, 50> liveDatabases;
    mLiveDatabases.EnumerateRead(EnumerateToTArray<IDBDatabase*>,
                                 &liveDatabases);

    // Invalidate them all.
    if (!liveDatabases.IsEmpty()) {
      uint32_t count = liveDatabases.Length();
      for (uint32_t index = 0; index < count; index++) {
        liveDatabases[index]->Invalidate();
      }
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, TOPIC_WEB_APP_CLEAR_DATA)) {
    nsCOMPtr<mozIApplicationClearPrivateDataParams> params =
      do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(params, NS_ERROR_UNEXPECTED);

    uint32_t appId;
    nsresult rv = params->GetAppId(&appId);
    NS_ENSURE_SUCCESS(rv, rv);

    bool browserOnly;
    rv = params->GetBrowserOnly(&browserOnly);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ClearDatabasesForApp(appId, browserOnly);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_NOTREACHED("Unknown topic!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(IndexedDatabaseManager::OriginClearRunnable,
                              nsIRunnable)

// static
void
IndexedDatabaseManager::
OriginClearRunnable::InvalidateOpenedDatabases(
                                   nsTArray<nsRefPtr<IDBDatabase> >& aDatabases,
                                   void* aClosure)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsTArray<nsRefPtr<IDBDatabase> > databases;
  databases.SwapElements(aDatabases);

  for (uint32_t index = 0; index < databases.Length(); index++) {
    databases[index]->Invalidate();
  }
}

void
IndexedDatabaseManager::
OriginClearRunnable::DeleteFiles(IndexedDatabaseManager* aManager)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aManager, "Don't pass me null!");

  nsresult rv;

  nsCOMPtr<nsIFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = directory->InitWithPath(aManager->GetBaseDirectory());
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsISimpleEnumerator> entries;
  if (NS_FAILED(directory->GetDirectoryEntries(getter_AddRefs(entries))) ||
      !entries) {
    return;
  }

  nsCString originSanitized(mOriginOrPattern);
  SanitizeOriginString(originSanitized);

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) && hasMore) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    NS_ENSURE_SUCCESS_VOID(rv);

    nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
    NS_ASSERTION(file, "Don't know what this is!");

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS_VOID(rv);

    if (!isDirectory) {
      NS_WARNING("Something in the IndexedDB directory that doesn't belong!");
      continue;
    }

    nsString leafName;
    rv = file->GetLeafName(leafName);
    NS_ENSURE_SUCCESS_VOID(rv);

    // Skip databases for other apps.
    if (!PatternMatchesOrigin(originSanitized,
                              NS_ConvertUTF16toUTF8(leafName))) {
      continue;
    }

    if (NS_FAILED(file->Remove(true))) {
      // This should never fail if we've closed all database connections
      // correctly...
      NS_ERROR("Failed to remove directory!");
    }

    QuotaManager* quotaManager = QuotaManager::Get();
    NS_ASSERTION(quotaManager, "Shouldn't be null!");

    quotaManager->RemoveQuotaForPattern(mOriginOrPattern);

    aManager->UninitializeOriginsByPattern(mOriginOrPattern);
  }
}

NS_IMETHODIMP
IndexedDatabaseManager::OriginClearRunnable::Run()
{
  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never fail!");

  switch (mCallbackState) {
    case Pending: {
      NS_NOTREACHED("Should never get here without being dispatched!");
      return NS_ERROR_UNEXPECTED;
    }

    case OpenAllowed: {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

      AdvanceState();

      // Now we have to wait until the thread pool is done with all of the
      // databases we care about.
      nsresult rv = mgr->AcquireExclusiveAccess(mOriginOrPattern, this,
                                                InvalidateOpenedDatabases,
                                                nullptr);
      NS_ENSURE_SUCCESS(rv, rv);

      return NS_OK;
    }

    case IO: {
      NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

      AdvanceState();

      DeleteFiles(mgr);

      // Now dispatch back to the main thread.
      if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
        NS_WARNING("Failed to dispatch to main thread!");
        return NS_ERROR_FAILURE;
      }

      return NS_OK;
    }

    case Complete: {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

      mgr->InvalidateFileManagersForPattern(mOriginOrPattern);

      // Tell the IndexedDatabaseManager that we're done.
      mgr->AllowNextSynchronizedOp(mOriginOrPattern, nullptr);

      return NS_OK;
    }

    default:
      NS_ERROR("Unknown state value!");
      return NS_ERROR_UNEXPECTED;
  }

  NS_NOTREACHED("Should never get here!");
  return NS_ERROR_UNEXPECTED;
}

IndexedDatabaseManager::AsyncUsageRunnable::AsyncUsageRunnable(
                                     uint32_t aAppId,
                                     bool aInMozBrowserOnly,
                                     const OriginOrPatternString& aOrigin,
                                     nsIURI* aURI,
                                     nsIIndexedDatabaseUsageCallback* aCallback)
: mURI(aURI),
  mCallback(aCallback),
  mUsage(0),
  mFileUsage(0),
  mAppId(aAppId),
  mCanceled(0),
  mOrigin(aOrigin),
  mCallbackState(Pending),
  mInMozBrowserOnly(aInMozBrowserOnly)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aURI, "Null pointer!");
  NS_ASSERTION(aOrigin.IsOrigin(), "Expect origin only here!");
  NS_ASSERTION(!aOrigin.IsEmpty(), "Empty origin!");
  NS_ASSERTION(aCallback, "Null pointer!");
}

void
IndexedDatabaseManager::AsyncUsageRunnable::Cancel()
{
  if (PR_ATOMIC_SET(&mCanceled, 1)) {
    NS_ERROR("Canceled more than once?!");
  }
}

nsresult
IndexedDatabaseManager::AsyncUsageRunnable::TakeShortcut()
{
  NS_ASSERTION(mCallbackState == Pending, "Huh?");

  nsresult rv = NS_DispatchToCurrentThread(this);
  NS_ENSURE_SUCCESS(rv, rv);

  mCallbackState = Shortcut;
  return NS_OK;
}

nsresult
IndexedDatabaseManager::AsyncUsageRunnable::RunInternal()
{
  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never fail!");

  if (mCanceled) {
    return NS_OK;
  }

  switch (mCallbackState) {
    case Pending: {
      NS_NOTREACHED("Should never get here without being dispatched!");
      return NS_ERROR_UNEXPECTED;
    }

    case OpenAllowed: {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

      AdvanceState();

      if (NS_FAILED(mgr->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL))) {
        NS_WARNING("Failed to dispatch to the IO thread!");
      }

      return NS_OK;
    }

    case IO: {
      NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

      AdvanceState();

      // Get the directory that contains all the database files we care about.
      nsCOMPtr<nsIFile> directory;
      nsresult rv = mgr->GetDirectoryForOrigin(mOrigin, getter_AddRefs(directory));
      NS_ENSURE_SUCCESS(rv, rv);

      bool exists;
      rv = directory->Exists(&exists);
      NS_ENSURE_SUCCESS(rv, rv);

      // If the directory exists then enumerate all the files inside, adding up the
      // sizes to get the final usage statistic.
      if (exists && !mCanceled) {
        rv = GetUsageForDirectory(directory, &mUsage);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Run dispatches us back to the main thread.
      return NS_OK;
    }

    case Complete: // Fall through
    case Shortcut: {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

      // Call the callback unless we were canceled.
      if (!mCanceled) {
        uint64_t usage = mUsage;
        IncrementUsage(&usage, mFileUsage);
        mCallback->OnUsageResult(mURI, usage, mFileUsage, mAppId,
                                 mInMozBrowserOnly);
      }

      // Clean up.
      mURI = nullptr;
      mCallback = nullptr;

      // And tell the IndexedDatabaseManager that we're done.
      mgr->OnUsageCheckComplete(this);
      if (mCallbackState == Complete) {
        mgr->AllowNextSynchronizedOp(mOrigin, nullptr);
      }

      return NS_OK;
    }

    default:
      NS_ERROR("Unknown state value!");
      return NS_ERROR_UNEXPECTED;
  }

  NS_NOTREACHED("Should never get here!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
IndexedDatabaseManager::AsyncUsageRunnable::GetUsageForDirectory(
                                     nsIFile* aDirectory,
                                     uint64_t* aUsage)
{
  NS_ASSERTION(aDirectory, "Null pointer!");
  NS_ASSERTION(aUsage, "Null pointer!");

  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!entries) {
    return NS_OK;
  }

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) &&
         hasMore && !mCanceled) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> file(do_QueryInterface(entry));
    NS_ASSERTION(file, "Don't know what this is!");

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);

    if (isDirectory) {
      if (aUsage == &mFileUsage) {
        NS_WARNING("Unknown directory found!");
      }
      else {
        rv = GetUsageForDirectory(file, &mFileUsage);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      continue;
    }

    int64_t fileSize;
    rv = file->GetFileSize(&fileSize);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(fileSize >= 0, "Negative size?!");

    IncrementUsage(aUsage, uint64_t(fileSize));
  }
  NS_ENSURE_SUCCESS(rv, rv);
 
  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(IndexedDatabaseManager::AsyncUsageRunnable,
                              nsIRunnable)

NS_IMETHODIMP
IndexedDatabaseManager::AsyncUsageRunnable::Run()
{
  nsresult rv = RunInternal();

  if (!NS_IsMainThread()) {
    if (NS_FAILED(rv)) {
      mUsage = 0;
    }

    if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
      NS_WARNING("Failed to dispatch to main thread!");
    }
  }

  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(
                    IndexedDatabaseManager::WaitForTransactionsToFinishRunnable,
                    nsIRunnable)

NS_IMETHODIMP
IndexedDatabaseManager::WaitForTransactionsToFinishRunnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mOp, "Null op!");
  NS_ASSERTION(mOp->mHelper || mOp->mRunnable, "Nothing to run!");
  NS_ASSERTION(mCountdown, "Wrong countdown!");

  if (--mCountdown) {
    return NS_OK;
  }

  // Don't hold the callback alive longer than necessary.
  nsRefPtr<AsyncConnectionHelper> helper;
  helper.swap(mOp->mHelper);

  nsCOMPtr<nsIRunnable> runnable;
  runnable.swap(mOp->mRunnable);

  mOp = nullptr;

  nsresult rv;

  if (helper && helper->HasTransaction()) {
    // If the helper has a transaction, dispatch it to the transaction
    // threadpool.
    rv = helper->DispatchToTransactionPool();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Otherwise, dispatch it to the IO thread.
    IndexedDatabaseManager* manager = IndexedDatabaseManager::Get();
    NS_ASSERTION(manager, "We should definitely have a manager here");

    nsIEventTarget* target = manager->IOThread();

    rv = helper ?
         helper->Dispatch(target) :
         target->Dispatch(runnable, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // The helper or runnable is responsible for calling
  // IndexedDatabaseManager::AllowNextSynchronizedOp.
  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(
                     IndexedDatabaseManager::WaitForLockedFilesToFinishRunnable,
                     nsIRunnable)

NS_IMETHODIMP
IndexedDatabaseManager::WaitForLockedFilesToFinishRunnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mBusy = false;

  return NS_OK;
}

IndexedDatabaseManager::
SynchronizedOp::SynchronizedOp(const OriginOrPatternString& aOriginOrPattern,
                               nsIAtom* aId)
: mOriginOrPattern(aOriginOrPattern), mId(aId)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  MOZ_COUNT_CTOR(IndexedDatabaseManager::SynchronizedOp);
}

IndexedDatabaseManager::SynchronizedOp::~SynchronizedOp()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  MOZ_COUNT_DTOR(IndexedDatabaseManager::SynchronizedOp);
}

bool
IndexedDatabaseManager::
SynchronizedOp::MustWaitFor(const SynchronizedOp& aExistingOp)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  bool match;

  if (aExistingOp.mOriginOrPattern.IsOrigin()) {
    if (mOriginOrPattern.IsOrigin()) {
      match = aExistingOp.mOriginOrPattern.Equals(mOriginOrPattern);
    }
    else {
      match = PatternMatchesOrigin(mOriginOrPattern, aExistingOp.mOriginOrPattern);
    }
  }
  else if (mOriginOrPattern.IsOrigin()) {
    match = PatternMatchesOrigin(aExistingOp.mOriginOrPattern, mOriginOrPattern);
  }
  else {
    match = PatternMatchesOrigin(mOriginOrPattern, aExistingOp.mOriginOrPattern) ||
            PatternMatchesOrigin(aExistingOp.mOriginOrPattern, mOriginOrPattern);
  }

  // If the origins don't match, the second can proceed.
  if (!match) {
    return false;
  }

  // If the origins and the ids match, the second must wait.
  if (aExistingOp.mId == mId) {
    return true;
  }

  // Waiting is required if either one corresponds to an origin clearing
  // (a null Id).
  if (!aExistingOp.mId || !mId) {
    return true;
  }

  // Otherwise, things for the same origin but different databases can proceed
  // independently.
  return false;
}

void
IndexedDatabaseManager::SynchronizedOp::DelayRunnable(nsIRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mDelayedRunnables.IsEmpty() || !mId,
               "Only ClearOrigin operations can delay multiple runnables!");

  mDelayedRunnables.AppendElement(aRunnable);
}

void
IndexedDatabaseManager::SynchronizedOp::DispatchDelayedRunnables()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mHelper, "Any helper should be gone by now!");

  uint32_t count = mDelayedRunnables.Length();
  for (uint32_t index = 0; index < count; index++) {
    NS_DispatchToCurrentThread(mDelayedRunnables[index]);
  }

  mDelayedRunnables.Clear();
}

NS_IMETHODIMP
IndexedDatabaseManager::InitWindowless(const jsval& aObj, JSContext* aCx)
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_ARG(!JSVAL_IS_PRIMITIVE(aObj));

  JSObject* obj = JSVAL_TO_OBJECT(aObj);

  JSBool hasIndexedDB;
  if (!JS_HasProperty(aCx, obj, "indexedDB", &hasIndexedDB)) {
    return NS_ERROR_FAILURE;
  }

  if (hasIndexedDB) {
    NS_WARNING("Passed object already has an 'indexedDB' property!");
    return NS_ERROR_FAILURE;
  }

  // Instantiating this class will register exception providers so even 
  // in xpcshell we will get typed (dom) exceptions, instead of general
  // exceptions.
  nsCOMPtr<nsIDOMScriptObjectFactory> sof(do_GetService(kDOMSOF_CID));

  JSObject* global = JS_GetGlobalForObject(aCx, obj);
  NS_ASSERTION(global, "What?! No global!");

  nsRefPtr<IDBFactory> factory;
  nsresult rv =
    IDBFactory::Create(aCx, global, nullptr, getter_AddRefs(factory));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_ASSERTION(factory, "This should never fail for chrome!");

  jsval indexedDBVal;
  rv = nsContentUtils::WrapNative(aCx, obj, factory, &indexedDBVal);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!JS_DefineProperty(aCx, obj, "indexedDB", indexedDBVal, nullptr,
                         nullptr, JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  JSObject* keyrangeObj = JS_NewObject(aCx, nullptr, nullptr, nullptr);
  NS_ENSURE_TRUE(keyrangeObj, NS_ERROR_OUT_OF_MEMORY);

  if (!IDBKeyRange::DefineConstructors(aCx, keyrangeObj)) {
    return NS_ERROR_FAILURE;
  }

  if (!JS_DefineProperty(aCx, obj, "IDBKeyRange", OBJECT_TO_JSVAL(keyrangeObj),
                         nullptr, nullptr, JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

IndexedDatabaseManager::
AsyncDeleteFileRunnable::AsyncDeleteFileRunnable(FileManager* aFileManager,
                                                 int64_t aFileId)
: mFileManager(aFileManager), mFileId(aFileId)
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(IndexedDatabaseManager::AsyncDeleteFileRunnable,
                              nsIRunnable)

NS_IMETHODIMP
IndexedDatabaseManager::AsyncDeleteFileRunnable::Run()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIFile> directory = mFileManager->GetDirectory();
  NS_ENSURE_TRUE(directory, NS_ERROR_FAILURE);

  nsCOMPtr<nsIFile> file = mFileManager->GetFileForId(directory, mFileId);
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  nsresult rv;
  int64_t fileSize;

  if (mFileManager->Privilege() != Chrome) {
    rv = file->GetFileSize(&fileSize);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  }

  rv = file->Remove(false);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  if (mFileManager->Privilege() != Chrome) {
    QuotaManager* quotaManager = QuotaManager::Get();
    NS_ASSERTION(quotaManager, "Shouldn't be null!");

    quotaManager->DecreaseUsageForOrigin(mFileManager->Origin(), fileSize);
  }

  directory = mFileManager->GetJournalDirectory();
  NS_ENSURE_TRUE(directory, NS_ERROR_FAILURE);

  file = mFileManager->GetFileForId(directory, mFileId);
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  rv = file->Remove(false);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
