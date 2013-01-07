/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/unused.h"
#include "nsPermissionManager.h"
#include "nsPermission.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsCOMArray.h"
#include "nsArrayEnumerator.h"
#include "nsTArray.h"
#include "nsReadableUtils.h"
#include "nsILineInputStream.h"
#include "nsIIDNService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "prprf.h"
#include "mozilla/storage.h"
#include "mozilla/Attributes.h"
#include "nsXULAppAPI.h"
#include "nsIPrincipal.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsIAppsService.h"
#include "mozIApplication.h"

static nsPermissionManager *gPermissionManager = nullptr;

using mozilla::dom::ContentParent;
using mozilla::dom::ContentChild;
using mozilla::unused; // ha!

static bool
IsChildProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Content;
}

/**
 * @returns The child process object, or if we are not in the child
 *          process, nullptr.
 */
static ContentChild*
ChildProcess()
{
  if (IsChildProcess()) {
    ContentChild* cpc = ContentChild::GetSingleton();
    if (!cpc)
      NS_RUNTIMEABORT("Content Process is NULL!");
    return cpc;
  }

  return nullptr;
}


#define ENSURE_NOT_CHILD_PROCESS_(onError) \
  PR_BEGIN_MACRO \
  if (IsChildProcess()) { \
    NS_ERROR("Cannot perform action in content process!"); \
    onError \
  } \
  PR_END_MACRO

#define ENSURE_NOT_CHILD_PROCESS \
  ENSURE_NOT_CHILD_PROCESS_({ return NS_ERROR_NOT_AVAILABLE; })

#define ENSURE_NOT_CHILD_PROCESS_NORET \
  ENSURE_NOT_CHILD_PROCESS_()

////////////////////////////////////////////////////////////////////////////////

namespace {

nsresult
GetPrincipal(const nsACString& aHost, uint32_t aAppId, bool aIsInBrowserElement,
             nsIPrincipal** aPrincipal)
{
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  NS_ENSURE_TRUE(secMan, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> uri;
  // NOTE: we use "http://" as a protocal but we will just use the host so it
  // doesn't really matter.
  NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING("http://") + aHost);

  return secMan->GetAppCodebasePrincipal(uri, aAppId, aIsInBrowserElement, aPrincipal);
}

nsresult
GetPrincipal(nsIURI* aURI, nsIPrincipal** aPrincipal)
{
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  NS_ENSURE_TRUE(secMan, NS_ERROR_FAILURE);

  return secMan->GetNoAppCodebasePrincipal(aURI, aPrincipal);
}

nsresult
GetPrincipal(const nsACString& aHost, nsIPrincipal** aPrincipal)
{
  return GetPrincipal(aHost, nsIScriptSecurityManager::NO_APP_ID, false, aPrincipal);
}

nsresult
GetHostForPrincipal(nsIPrincipal* aPrincipal, nsACString& aHost)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  uri = NS_GetInnermostURI(uri);
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

  rv = uri->GetAsciiHost(aHost);
  if (NS_FAILED(rv) || aHost.IsEmpty()) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

class AppUninstallObserver MOZ_FINAL : public nsIObserver {
public:
  NS_DECL_ISUPPORTS

  // nsIObserver implementation.
  NS_IMETHODIMP
  Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *data)
  {
    MOZ_ASSERT(!nsCRT::strcmp(aTopic, "webapps-uninstall"));

    nsCOMPtr<nsIAppsService> appsService = do_GetService("@mozilla.org/AppsService;1");
    nsCOMPtr<mozIApplication> app;

    appsService->GetAppFromObserverMessage(nsAutoString(data), getter_AddRefs(app));
    NS_ENSURE_TRUE(app, NS_ERROR_UNEXPECTED);

    uint32_t appId;
    app->GetLocalId(&appId);
    MOZ_ASSERT(appId != nsIScriptSecurityManager::NO_APP_ID);

    nsCOMPtr<nsIPermissionManager> permManager = do_GetService("@mozilla.org/permissionmanager;1");
    return permManager->RemovePermissionsForApp(appId);
  }
};

NS_IMPL_ISUPPORTS1(AppUninstallObserver, nsIObserver)

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////

nsPermissionManager::PermissionKey::PermissionKey(nsIPrincipal* aPrincipal)
{
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(GetHostForPrincipal(aPrincipal, mHost)));
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(aPrincipal->GetAppId(&mAppId)));
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(aPrincipal->GetIsInBrowserElement(&mIsInBrowserElement)));
}

/**
 * Simple callback used by |AsyncClose| to trigger a treatment once
 * the database is closed.
 *
 * Note: Beware that, if you hold onto a |CloseDatabaseListener| from a
 * |nsPermissionManager|, this will create a cycle.
 *
 * Note: Once the callback has been called this DeleteFromMozHostListener cannot
 * be reused.
 */
class CloseDatabaseListener MOZ_FINAL : public mozIStorageCompletionCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGECOMPLETIONCALLBACK
  /**
   * @param aManager The owning manager.
   * @param aRebuildOnSuccess If |true|, reinitialize the database once
   * it has been closed. Otherwise, do nothing such.
   */
  CloseDatabaseListener(nsPermissionManager* aManager,
                        bool aRebuildOnSuccess);

protected:
  nsRefPtr<nsPermissionManager> mManager;
  bool mRebuildOnSuccess;
};

NS_IMPL_ISUPPORTS1(CloseDatabaseListener, mozIStorageCompletionCallback)

CloseDatabaseListener::CloseDatabaseListener(nsPermissionManager* aManager,
                                             bool aRebuildOnSuccess)
  : mManager(aManager)
  , mRebuildOnSuccess(aRebuildOnSuccess)
{
}

NS_IMETHODIMP
CloseDatabaseListener::Complete()
{
  // Help breaking cycles
  nsRefPtr<nsPermissionManager> manager = mManager.forget();
  if (mRebuildOnSuccess && !manager->mIsShuttingDown) {
    return manager->InitDB(true);
  }
  return NS_OK;
}


/**
 * Simple callback used by |RemoveAllInternal| to trigger closing
 * the database and reinitializing it.
 *
 * Note: Beware that, if you hold onto a |DeleteFromMozHostListener| from a
 * |nsPermissionManager|, this will create a cycle.
 *
 * Note: Once the callback has been called this DeleteFromMozHostListener cannot
 * be reused.
 */
class DeleteFromMozHostListener MOZ_FINAL : public mozIStorageStatementCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK

  /**
   * @param aManager The owning manager.
   */
  DeleteFromMozHostListener(nsPermissionManager* aManager);

protected:
  nsRefPtr<nsPermissionManager> mManager;
};

NS_IMPL_ISUPPORTS1(DeleteFromMozHostListener, mozIStorageStatementCallback)

DeleteFromMozHostListener::
DeleteFromMozHostListener(nsPermissionManager* aManager)
  : mManager(aManager)
{
}

NS_IMETHODIMP DeleteFromMozHostListener::HandleResult(mozIStorageResultSet *)
{
  MOZ_NOT_REACHED("Should not get any results");
  return NS_OK;
}

NS_IMETHODIMP DeleteFromMozHostListener::HandleError(mozIStorageError *)
{
  // Errors are handled in |HandleCompletion|
  return NS_OK;
}

NS_IMETHODIMP DeleteFromMozHostListener::HandleCompletion(uint16_t aReason)
{
  // Help breaking cycles
  nsRefPtr<nsPermissionManager> manager = mManager.forget();

  if (aReason == REASON_ERROR) {
    manager->CloseDB(true);
  }

  return NS_OK;
}

/* static */ void
nsPermissionManager::AppUninstallObserverInit()
{
  nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1");
  observerService->AddObserver(new AppUninstallObserver(), "webapps-uninstall", /* holdsWeak= */ false);
}

////////////////////////////////////////////////////////////////////////////////
// nsPermissionManager Implementation

static const char kPermissionsFileName[] = "permissions.sqlite";
#define HOSTS_SCHEMA_VERSION 3

static const char kHostpermFileName[] = "hostperm.1";

static const char kPermissionChangeNotification[] = PERM_CHANGE_NOTIFICATION;

NS_IMPL_ISUPPORTS3(nsPermissionManager, nsIPermissionManager, nsIObserver, nsISupportsWeakReference)

nsPermissionManager::nsPermissionManager()
 : mLargestID(0)
 , mIsShuttingDown(false)
{
}

nsPermissionManager::~nsPermissionManager()
{
  RemoveAllFromMemory();
  gPermissionManager = nullptr;
}

// static
nsIPermissionManager*
nsPermissionManager::GetXPCOMSingleton()
{
  if (gPermissionManager) {
    NS_ADDREF(gPermissionManager);
    return gPermissionManager;
  }

  // Create a new singleton nsPermissionManager.
  // We AddRef only once since XPCOM has rules about the ordering of module
  // teardowns - by the time our module destructor is called, it's too late to
  // Release our members, since GC cycles have already been completed and
  // would result in serious leaks.
  // See bug 209571.
  gPermissionManager = new nsPermissionManager();
  if (gPermissionManager) {
    NS_ADDREF(gPermissionManager);
    if (NS_FAILED(gPermissionManager->Init())) {
      NS_RELEASE(gPermissionManager);
    }
  }

  return gPermissionManager;
}

nsresult
nsPermissionManager::Init()
{
  nsresult rv;

  mPermissionTable.Init();

  mObserverService = do_GetService("@mozilla.org/observer-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    mObserverService->AddObserver(this, "profile-before-change", true);
    mObserverService->AddObserver(this, "profile-do-change", true);
  }

  if (IsChildProcess()) {
    // Get the permissions from the parent process
    InfallibleTArray<IPC::Permission> perms;
    ChildProcess()->SendReadPermissions(&perms);

    for (uint32_t i = 0; i < perms.Length(); i++) {
      const IPC::Permission &perm = perms[i];

      nsCOMPtr<nsIPrincipal> principal;
      rv = GetPrincipal(perm.host, perm.appId, perm.isInBrowserElement, getter_AddRefs(principal));
      NS_ENSURE_SUCCESS(rv, rv);

      AddInternal(principal, perm.type, perm.capability, 0, perm.expireType,
                  perm.expireTime, eNotify, eNoDBOperation);
    }

    // Stop here; we don't need the DB in the child process
    return NS_OK;
  }

  // ignore failure here, since it's non-fatal (we can run fine without
  // persistent storage - e.g. if there's no profile).
  // XXX should we tell the user about this?
  InitDB(false);

  return NS_OK;
}

nsresult
nsPermissionManager::InitDB(bool aRemoveFile)
{
  nsCOMPtr<nsIFile> permissionsFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_PERMISSION_PARENT_DIR, getter_AddRefs(permissionsFile));
  if (NS_FAILED(rv)) {
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(permissionsFile));
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

  rv = permissionsFile->AppendNative(NS_LITERAL_CSTRING(kPermissionsFileName));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aRemoveFile) {
    bool exists = false;
    rv = permissionsFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if (exists) {
      rv = permissionsFile->Remove(false);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  nsCOMPtr<mozIStorageService> storage = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  if (!storage)
    return NS_ERROR_UNEXPECTED;

  // cache a connection to the hosts database
  rv = storage->OpenDatabase(permissionsFile, getter_AddRefs(mDBConn));
  NS_ENSURE_SUCCESS(rv, rv);

  bool ready;
  mDBConn->GetConnectionReady(&ready);
  if (!ready) {
    // delete and try again
    rv = permissionsFile->Remove(false);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = storage->OpenDatabase(permissionsFile, getter_AddRefs(mDBConn));
    NS_ENSURE_SUCCESS(rv, rv);

    mDBConn->GetConnectionReady(&ready);
    if (!ready)
      return NS_ERROR_UNEXPECTED;
  }

  bool tableExists = false;
  mDBConn->TableExists(NS_LITERAL_CSTRING("moz_hosts"), &tableExists);
  if (!tableExists) {
    rv = CreateTable();
    NS_ENSURE_SUCCESS(rv, rv);

  } else {
    // table already exists; check the schema version before reading
    int32_t dbSchemaVersion;
    rv = mDBConn->GetSchemaVersion(&dbSchemaVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    switch (dbSchemaVersion) {
    // upgrading.
    // every time you increment the database schema, you need to implement
    // the upgrading code from the previous version to the new one.
    // fall through to current version

    case 1:
      {
        // previous non-expiry version of database.  Upgrade it by adding the
        // expiration columns
        rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
              "ALTER TABLE moz_hosts ADD expireType INTEGER"));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
              "ALTER TABLE moz_hosts ADD expireTime INTEGER"));
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // fall through to the next upgrade

    // TODO: we want to make default version as version 2 in order to fix bug 784875.
    case 0:
    case 2:
      {
        // Add appId/isInBrowserElement fields.
        rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
              "ALTER TABLE moz_hosts ADD appId INTEGER"));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
              "ALTER TABLE moz_hosts ADD isInBrowserElement INTEGER"));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mDBConn->SetSchemaVersion(HOSTS_SCHEMA_VERSION);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // fall through to the next upgrade

    // current version.
    case HOSTS_SCHEMA_VERSION:
      break;

    // downgrading.
    // if columns have been added to the table, we can still use the ones we
    // understand safely. if columns have been deleted or altered, just
    // blow away the table and start from scratch! if you change the way
    // a column is interpreted, make sure you also change its name so this
    // check will catch it.
    default:
      {
        // check if all the expected columns exist
        nsCOMPtr<mozIStorageStatement> stmt;
        rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
          "SELECT host, type, permission, expireType, expireTime, appId, isInBrowserElement FROM moz_hosts"),
          getter_AddRefs(stmt));
        if (NS_SUCCEEDED(rv))
          break;

        // our columns aren't there - drop the table!
        rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DROP TABLE moz_hosts"));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = CreateTable();
        NS_ENSURE_SUCCESS(rv, rv);
      }
      break;
    }
  }

  // make operations on the table asynchronous, for performance
  mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("PRAGMA synchronous = OFF"));

  // cache frequently used statements (for insertion, deletion, and updating)
  rv = mDBConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "INSERT INTO moz_hosts "
    "(id, host, type, permission, expireType, expireTime, appId, isInBrowserElement) "
    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)"), getter_AddRefs(mStmtInsert));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_hosts "
    "WHERE id = ?1"), getter_AddRefs(mStmtDelete));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_hosts "
    "SET permission = ?2, expireType= ?3, expireTime = ?4 WHERE id = ?1"),
    getter_AddRefs(mStmtUpdate));
  NS_ENSURE_SUCCESS(rv, rv);

  // check whether to import or just read in the db
  if (tableExists)
    return Read();

  return Import();
}

// sets the schema version and creates the moz_hosts table.
nsresult
nsPermissionManager::CreateTable()
{
  // set the schema version, before creating the table
  nsresult rv = mDBConn->SetSchemaVersion(HOSTS_SCHEMA_VERSION);
  if (NS_FAILED(rv)) return rv;

  // create the table
  // SQL also lives in automation.py.in. If you change this SQL change that
  // one too.
  return mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE moz_hosts ("
      " id INTEGER PRIMARY KEY"
      ",host TEXT"
      ",type TEXT"
      ",permission INTEGER"
      ",expireType INTEGER"
      ",expireTime INTEGER"
      ",appId INTEGER"
      ",isInBrowserElement INTEGER"
    ")"));
}

NS_IMETHODIMP
nsPermissionManager::Add(nsIURI     *aURI,
                         const char *aType,
                         uint32_t    aPermission,
                         uint32_t    aExpireType,
                         int64_t     aExpireTime)
{
  NS_ENSURE_ARG_POINTER(aURI);

  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = GetPrincipal(aURI, getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  return AddFromPrincipal(principal, aType, aPermission, aExpireType, aExpireTime);
}

NS_IMETHODIMP
nsPermissionManager::AddFromPrincipal(nsIPrincipal* aPrincipal,
                                      const char* aType, uint32_t aPermission,
                                      uint32_t aExpireType, int64_t aExpireTime)
{
  ENSURE_NOT_CHILD_PROCESS;
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_ARG_POINTER(aType);
  NS_ENSURE_TRUE(aExpireType == nsIPermissionManager::EXPIRE_NEVER ||
                 aExpireType == nsIPermissionManager::EXPIRE_TIME ||
                 aExpireType == nsIPermissionManager::EXPIRE_SESSION,
                 NS_ERROR_INVALID_ARG);

  // Skip addition if the permission is already expired.
  if (aExpireType == nsIPermissionManager::EXPIRE_TIME &&
      aExpireTime <= (PR_Now() / 1000)) {
    return NS_OK;
  }

  // We don't add the system principal because it actually has no URI and we
  // always allow action for them.
  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    return NS_OK;
  }

  return AddInternal(aPrincipal, nsDependentCString(aType), aPermission, 0,
                     aExpireType, aExpireTime, eNotify, eWriteToDB);
}

nsresult
nsPermissionManager::AddInternal(nsIPrincipal* aPrincipal,
                                 const nsAFlatCString &aType,
                                 uint32_t              aPermission,
                                 int64_t               aID,
                                 uint32_t              aExpireType,
                                 int64_t               aExpireTime,
                                 NotifyOperationType   aNotifyOperation,
                                 DBOperationType       aDBOperation)
{
  nsAutoCString host;
  nsresult rv = GetHostForPrincipal(aPrincipal, host);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!IsChildProcess()) {
    uint32_t appId;
    rv = aPrincipal->GetAppId(&appId);
    NS_ENSURE_SUCCESS(rv, rv);

    bool isInBrowserElement;
    rv = aPrincipal->GetIsInBrowserElement(&isInBrowserElement);
    NS_ENSURE_SUCCESS(rv, rv);

    IPC::Permission permission(host, appId, isInBrowserElement, aType,
                               aPermission, aExpireType, aExpireTime);

    nsTArray<ContentParent*> cplist;
    ContentParent::GetAll(cplist);
    for (uint32_t i = 0; i < cplist.Length(); ++i) {
      ContentParent* cp = cplist[i];
      if (cp->NeedsPermissionsUpdate())
        unused << cp->SendAddPermission(permission);
    }
  }

  // look up the type index
  int32_t typeIndex = GetTypeIndex(aType.get(), true);
  NS_ENSURE_TRUE(typeIndex != -1, NS_ERROR_OUT_OF_MEMORY);

  // When an entry already exists, PutEntry will return that, instead
  // of adding a new one
  nsRefPtr<PermissionKey> key = new PermissionKey(aPrincipal);
  PermissionHashKey* entry = mPermissionTable.PutEntry(key);
  if (!entry) return NS_ERROR_FAILURE;
  if (!entry->GetKey()) {
    mPermissionTable.RawRemoveEntry(entry);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // figure out the transaction type, and get any existing permission value
  OperationType op;
  int32_t index = entry->GetPermissionIndex(typeIndex);
  if (index == -1) {
    if (aPermission == nsIPermissionManager::UNKNOWN_ACTION)
      op = eOperationNone;
    else
      op = eOperationAdding;

  } else {
    PermissionEntry oldPermissionEntry = entry->GetPermissions()[index];

    // remove the permission if the permission is UNKNOWN, update the
    // permission if its value or expire type have changed OR if the time has
    // changed and the expire type is time, otherwise, don't modify.  There's
    // no need to modify a permission that doesn't expire with time when the
    // only thing changed is the expire time.
    if (aPermission == oldPermissionEntry.mPermission &&
        aExpireType == oldPermissionEntry.mExpireType &&
        (aExpireType != nsIPermissionManager::EXPIRE_TIME ||
         aExpireTime == oldPermissionEntry.mExpireTime))
      op = eOperationNone;
    else if (aPermission == nsIPermissionManager::UNKNOWN_ACTION)
      op = eOperationRemoving;
    else
      op = eOperationChanging;
  }

  // do the work for adding, deleting, or changing a permission:
  // update the in-memory list, write to the db, and notify consumers.
  int64_t id;
  switch (op) {
  case eOperationNone:
    {
      // nothing to do
      return NS_OK;
    }

  case eOperationAdding:
    {
      if (aDBOperation == eWriteToDB) {
        // we'll be writing to the database - generate a known unique id
        id = ++mLargestID;
      } else {
        // we're reading from the database - use the id already assigned
        id = aID;
      }

      entry->GetPermissions().AppendElement(PermissionEntry(id, typeIndex, aPermission, aExpireType, aExpireTime));

      if (aDBOperation == eWriteToDB && aExpireType != nsIPermissionManager::EXPIRE_SESSION) {
        uint32_t appId;
        rv = aPrincipal->GetAppId(&appId);
        NS_ENSURE_SUCCESS(rv, rv);

        bool isInBrowserElement;
        rv = aPrincipal->GetIsInBrowserElement(&isInBrowserElement);
        NS_ENSURE_SUCCESS(rv, rv);

        UpdateDB(op, mStmtInsert, id, host, aType, aPermission, aExpireType, aExpireTime, appId, isInBrowserElement);
      }

      if (aNotifyOperation == eNotify) {
        NotifyObserversWithPermission(host,
                                      entry->GetKey()->mAppId,
                                      entry->GetKey()->mIsInBrowserElement,
                                      mTypeArray[typeIndex],
                                      aPermission,
                                      aExpireType,
                                      aExpireTime,
                                      NS_LITERAL_STRING("added").get());
      }

      break;
    }

  case eOperationRemoving:
    {
      PermissionEntry oldPermissionEntry = entry->GetPermissions()[index];
      id = oldPermissionEntry.mID;
      entry->GetPermissions().RemoveElementAt(index);

      if (aDBOperation == eWriteToDB)
        // We care only about the id here so we pass dummy values for all other
        // parameters.
        UpdateDB(op, mStmtDelete, id, EmptyCString(), EmptyCString(), 0,
                 nsIPermissionManager::EXPIRE_NEVER, 0, 0, false);

      if (aNotifyOperation == eNotify) {
        NotifyObserversWithPermission(host,
                                      entry->GetKey()->mAppId,
                                      entry->GetKey()->mIsInBrowserElement,
                                      mTypeArray[typeIndex],
                                      oldPermissionEntry.mPermission,
                                      oldPermissionEntry.mExpireType,
                                      oldPermissionEntry.mExpireTime,
                                      NS_LITERAL_STRING("deleted").get());
      }

      // If there are no more permissions stored for that entry, clear it.
      if (entry->GetPermissions().IsEmpty()) {
        mPermissionTable.RawRemoveEntry(entry);
      }

      break;
    }

  case eOperationChanging:
    {
      id = entry->GetPermissions()[index].mID;

      // If the new expireType is EXPIRE_SESSION, then we have to keep a
      // copy of the previous permission value. This cached value will be
      // used when restoring the permissions of an app.
      if (entry->GetPermissions()[index].mExpireType != nsIPermissionManager::EXPIRE_SESSION &&
          aExpireType == nsIPermissionManager::EXPIRE_SESSION) {
        entry->GetPermissions()[index].mNonSessionPermission = entry->GetPermissions()[index].mPermission;
      }

      entry->GetPermissions()[index].mPermission = aPermission;

      if (aDBOperation == eWriteToDB && aExpireType != nsIPermissionManager::EXPIRE_SESSION)
        // We care only about the id, the permission and expireType/expireTime here.
        // We pass dummy values for all other parameters.
        UpdateDB(op, mStmtUpdate, id, EmptyCString(), EmptyCString(),
                 aPermission, aExpireType, aExpireTime, 0, false);

      if (aNotifyOperation == eNotify) {
        NotifyObserversWithPermission(host,
                                      entry->GetKey()->mAppId,
                                      entry->GetKey()->mIsInBrowserElement,
                                      mTypeArray[typeIndex],
                                      aPermission,
                                      aExpireType,
                                      aExpireTime,
                                      NS_LITERAL_STRING("changed").get());
      }

      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::Remove(const nsACString &aHost,
                            const char       *aType)
{
  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = GetPrincipal(aHost, getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  return RemoveFromPrincipal(principal, aType);
}

NS_IMETHODIMP
nsPermissionManager::RemoveFromPrincipal(nsIPrincipal* aPrincipal,
                                         const char* aType)
{
  ENSURE_NOT_CHILD_PROCESS;
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_ARG_POINTER(aType);

  // System principals are never added to the database, no need to remove them.
  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    return NS_OK;
  }

  // AddInternal() handles removal, just let it do the work
  return AddInternal(aPrincipal,
                     nsDependentCString(aType),
                     nsIPermissionManager::UNKNOWN_ACTION,
                     0,
                     nsIPermissionManager::EXPIRE_NEVER,
                     0,
                     eNotify,
                     eWriteToDB);
}

NS_IMETHODIMP
nsPermissionManager::RemoveAll()
{
  ENSURE_NOT_CHILD_PROCESS;
  return RemoveAllInternal(true);
}

void
nsPermissionManager::CloseDB(bool aRebuildOnSuccess)
{
  // Null the statements, this will finalize them.
  mStmtInsert = nullptr;
  mStmtDelete = nullptr;
  mStmtUpdate = nullptr;
  if (mDBConn) {
    mozIStorageCompletionCallback* cb = new CloseDatabaseListener(this,
           aRebuildOnSuccess);
    mozilla::DebugOnly<nsresult> rv = mDBConn->AsyncClose(cb);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    mDBConn = nullptr; // Avoid race conditions
  }
}

nsresult
nsPermissionManager::RemoveAllInternal(bool aNotifyObservers)
{
  // Remove from memory and notify immediately. Since the in-memory
  // database is authoritative, we do not need confirmation from the
  // on-disk database to notify observers.
  RemoveAllFromMemory();
  if (aNotifyObservers) {
    NotifyObservers(nullptr, NS_LITERAL_STRING("cleared").get());
  }

  // clear the db
  if (mDBConn) {
    nsCOMPtr<mozIStorageAsyncStatement> removeStmt;
    nsresult rv = mDBConn->
      CreateAsyncStatement(NS_LITERAL_CSTRING(
         "DELETE FROM moz_hosts"
      ), getter_AddRefs(removeStmt));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    if (!removeStmt) {
      return NS_ERROR_UNEXPECTED;
    }
    nsCOMPtr<mozIStoragePendingStatement> pending;
    mozIStorageStatementCallback* cb = new DeleteFromMozHostListener(this);
    rv = removeStmt->ExecuteAsync(cb, getter_AddRefs(pending));
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::TestExactPermission(nsIURI     *aURI,
                                         const char *aType,
                                         uint32_t   *aPermission)
{
  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = GetPrincipal(aURI, getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  return TestExactPermissionFromPrincipal(principal, aType, aPermission);
}

NS_IMETHODIMP
nsPermissionManager::TestExactPermissionFromPrincipal(nsIPrincipal* aPrincipal,
                                                      const char* aType,
                                                      uint32_t* aPermission)
{
  NS_ENSURE_ARG_POINTER(aPrincipal);

  // System principals do not have URI so we can't try to get
  // retro-compatibility here.
  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    *aPermission = nsIPermissionManager::ALLOW_ACTION;
    return NS_OK;
  }

  return CommonTestPermission(aPrincipal, aType, aPermission, true);
}

NS_IMETHODIMP
nsPermissionManager::TestPermission(nsIURI     *aURI,
                                    const char *aType,
                                    uint32_t   *aPermission)
{
  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = GetPrincipal(aURI, getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  return TestPermissionFromPrincipal(principal, aType, aPermission);
}

NS_IMETHODIMP
nsPermissionManager::TestPermissionFromWindow(nsIDOMWindow* aWindow,
                                              const char* aType,
                                              uint32_t* aPermission)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(window, NS_NOINTERFACE);

  nsPIDOMWindow* innerWindow = window->IsInnerWindow() ?
    window.get() :
    window->GetCurrentInnerWindow();

  // Get the document for security check
  nsCOMPtr<nsIDocument> document = innerWindow->GetExtantDoc();
  NS_ENSURE_TRUE(document, NS_NOINTERFACE);

  nsCOMPtr<nsIPrincipal> principal = document->NodePrincipal();
  return TestPermissionFromPrincipal(principal, aType, aPermission);
}

NS_IMETHODIMP
nsPermissionManager::TestPermissionFromPrincipal(nsIPrincipal* aPrincipal,
                                                 const char* aType,
                                                 uint32_t* aPermission)
{
  NS_ENSURE_ARG_POINTER(aPrincipal);

  // System principals do not have URI so we can't try to get
  // retro-compatibility here.
  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    *aPermission = nsIPermissionManager::ALLOW_ACTION;
    return NS_OK;
  }

  return CommonTestPermission(aPrincipal, aType, aPermission, false);
}

nsresult
nsPermissionManager::CommonTestPermission(nsIPrincipal* aPrincipal,
                                          const char *aType,
                                          uint32_t   *aPermission,
                                          bool        aExactHostMatch)
{
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_ARG_POINTER(aType);

  // set the default
  *aPermission = nsIPermissionManager::UNKNOWN_ACTION;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString host;
  rv = GetHostForPrincipal(aPrincipal, host);

  // No host doesn't mean an error. Just return the default. Unless this is
  // a file uri. In that case use a magic host.
  if (NS_FAILED(rv)) {
    bool isFile;
    rv = uri->SchemeIs("file", &isFile);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isFile) {
      host.AssignLiteral("<file>");
    }
    else {
      return NS_OK;
    }
  }

  int32_t typeIndex = GetTypeIndex(aType, false);
  // If type == -1, the type isn't known,
  // so just return NS_OK
  if (typeIndex == -1) return NS_OK;

  uint32_t appId;
  rv = aPrincipal->GetAppId(&appId);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isInBrowserElement;
  rv = aPrincipal->GetIsInBrowserElement(&isInBrowserElement);
  NS_ENSURE_SUCCESS(rv, rv);

  PermissionHashKey* entry = GetPermissionHashKey(host, appId, isInBrowserElement,
                                                  typeIndex, aExactHostMatch);
  if (entry) {
    *aPermission = entry->GetPermission(typeIndex).mPermission;
  }

  return NS_OK;
}

// Returns PermissionHashKey for a given { host, appId, isInBrowserElement } tuple.
// This is not simply using PermissionKey because we will walk-up domains in
// case of |host| contains sub-domains.
// Returns null if nothing found.
// Also accepts host on the format "<foo>". This will perform an exact match
// lookup as the string doesn't contain any dots.
nsPermissionManager::PermissionHashKey*
nsPermissionManager::GetPermissionHashKey(const nsACString& aHost,
                                          uint32_t aAppId,
                                          bool aIsInBrowserElement,
                                          uint32_t aType,
                                          bool aExactHostMatch)
{
  uint32_t offset = 0;
  PermissionHashKey* entry;
  int64_t now = PR_Now() / 1000;

  do {
    nsRefPtr<PermissionKey> key = new PermissionKey(Substring(aHost, offset), aAppId, aIsInBrowserElement);
    entry = mPermissionTable.GetEntry(key);

    if (entry) {
      PermissionEntry permEntry = entry->GetPermission(aType);

      // if the entry is expired, remove and keep looking for others.
      if (permEntry.mExpireType == nsIPermissionManager::EXPIRE_TIME &&
          permEntry.mExpireTime <= now) {
        nsCOMPtr<nsIPrincipal> principal;
        if (NS_FAILED(GetPrincipal(aHost, aAppId, aIsInBrowserElement, getter_AddRefs(principal)))) {
          return nullptr;
        }

        RemoveFromPrincipal(principal, mTypeArray[aType].get());
      } else if (permEntry.mPermission != nsIPermissionManager::UNKNOWN_ACTION) {
        break;
      }

      // reset entry, to be able to return null on failure
      entry = nullptr;
    }
    if (aExactHostMatch)
      break; // do not try super domains

    offset = aHost.FindChar('.', offset) + 1;

  // walk up the domaintree (we stop as soon as we find a match,
  // which will be the most specific domain we have an entry for).
  } while (offset > 0);
  return entry;
}

// helper struct for passing arguments into hash enumeration callback.
struct nsGetEnumeratorData
{
  nsGetEnumeratorData(nsCOMArray<nsIPermission> *aArray, const nsTArray<nsCString> *aTypes)
   : array(aArray)
   , types(aTypes) {}

  nsCOMArray<nsIPermission> *array;
  const nsTArray<nsCString> *types;
};

static PLDHashOperator
AddPermissionsToList(nsPermissionManager::PermissionHashKey* entry, void *arg)
{
  nsGetEnumeratorData *data = static_cast<nsGetEnumeratorData *>(arg);

  for (uint32_t i = 0; i < entry->GetPermissions().Length(); ++i) {
    nsPermissionManager::PermissionEntry& permEntry = entry->GetPermissions()[i];

    nsPermission *perm = new nsPermission(entry->GetKey()->mHost,
                                          entry->GetKey()->mAppId,
                                          entry->GetKey()->mIsInBrowserElement,
                                          data->types->ElementAt(permEntry.mType),
                                          permEntry.mPermission,
                                          permEntry.mExpireType,
                                          permEntry.mExpireTime);

    data->array->AppendObject(perm);
  }

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP nsPermissionManager::GetEnumerator(nsISimpleEnumerator **aEnum)
{
  // roll an nsCOMArray of all our permissions, then hand out an enumerator
  nsCOMArray<nsIPermission> array;
  nsGetEnumeratorData data(&array, &mTypeArray);

  mPermissionTable.EnumerateEntries(AddPermissionsToList, &data);

  return NS_NewArrayEnumerator(aEnum, array);
}

NS_IMETHODIMP nsPermissionManager::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  ENSURE_NOT_CHILD_PROCESS;

  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    // The profile is about to change,
    // or is going away because the application is shutting down.
    mIsShuttingDown = true;
    if (!nsCRT::strcmp(someData, NS_LITERAL_STRING("shutdown-cleanse").get())) {
      // Clear the permissions file and close the db asynchronously
      RemoveAllInternal(false);
    } else {
      RemoveAllFromMemory();
      CloseDB(false);
    }
  }
  else if (!nsCRT::strcmp(aTopic, "profile-do-change")) {
    // the profile has already changed; init the db from the new location
    InitDB(false);
  }

  return NS_OK;
}

PLDHashOperator
nsPermissionManager::GetPermissionsForApp(nsPermissionManager::PermissionHashKey* entry, void* arg)
{
  GetPermissionsForAppStruct* data = static_cast<GetPermissionsForAppStruct*>(arg);

  for (uint32_t i = 0; i < entry->GetPermissions().Length(); ++i) {
    nsPermissionManager::PermissionEntry& permEntry = entry->GetPermissions()[i];

    if (entry->GetKey()->mAppId != data->appId) {
      continue;
    }

    data->permissions.AppendObject(new nsPermission(entry->GetKey()->mHost,
                                                    entry->GetKey()->mAppId,
                                                    entry->GetKey()->mIsInBrowserElement,
                                                    gPermissionManager->mTypeArray.ElementAt(permEntry.mType),
                                                    permEntry.mPermission,
                                                    permEntry.mExpireType,
                                                    permEntry.mExpireTime));
  }

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsPermissionManager::RemovePermissionsForApp(uint32_t aAppId)
{
  ENSURE_NOT_CHILD_PROCESS;
  NS_ENSURE_ARG(aAppId != nsIScriptSecurityManager::NO_APP_ID);

  // We begin by removing all the permissions from the DB.
  // After clearing the DB, we call AddInternal() to make sure that all
  // processes are aware of this change and the representation of the DB in
  // memory is updated.
  // We have to get all permissions associated with an application and then
  // remove those because doing so in EnumerateEntries() would fail because
  // we might happen to actually delete entries from the list.

  nsAutoCString sql;
  sql.AppendLiteral("DELETE FROM moz_hosts WHERE appId=");
  sql.AppendInt(aAppId);

  nsCOMPtr<mozIStorageAsyncStatement> removeStmt;
  nsresult rv = mDBConn->CreateAsyncStatement(sql, getter_AddRefs(removeStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStoragePendingStatement> pending;
  rv = removeStmt->ExecuteAsync(nullptr, getter_AddRefs(pending));
  NS_ENSURE_SUCCESS(rv, rv);

  GetPermissionsForAppStruct data(aAppId);
  mPermissionTable.EnumerateEntries(GetPermissionsForApp, &data);

  for (int32_t i=0; i<data.permissions.Count(); ++i) {
    nsAutoCString host;
    bool isInBrowserElement;
    nsAutoCString type;

    data.permissions[i]->GetHost(host);
    data.permissions[i]->GetIsInBrowserElement(&isInBrowserElement);
    data.permissions[i]->GetType(type);

    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(GetPrincipal(host, aAppId, isInBrowserElement,
                               getter_AddRefs(principal)))) {
      NS_ERROR("GetPrincipal() failed!");
      continue;
    }

    AddInternal(principal,
                type,
                nsIPermissionManager::UNKNOWN_ACTION,
                0,
                nsIPermissionManager::EXPIRE_NEVER,
                0,
                nsPermissionManager::eNotify,
                nsPermissionManager::eNoDBOperation);
  }

  return NS_OK;
}

PLDHashOperator
nsPermissionManager::RemoveExpiredPermissionsForAppEnumerator(
  nsPermissionManager::PermissionHashKey* entry, void* arg)
{
  uint32_t* appId = static_cast<uint32_t*>(arg);

  for (uint32_t i = 0; i < entry->GetPermissions().Length(); ++i) {
    if (entry->GetKey()->mAppId != *appId) {
      continue;
    }

    nsPermissionManager::PermissionEntry& permEntry = entry->GetPermissions()[i];
    if (permEntry.mExpireType == nsIPermissionManager::EXPIRE_SESSION) {
      PermissionEntry oldPermissionEntry = entry->GetPermissions()[i];

      entry->GetPermissions().RemoveElementAt(i);

      gPermissionManager->NotifyObserversWithPermission(entry->GetKey()->mHost,
                                                        entry->GetKey()->mAppId,
                                                        entry->GetKey()->mIsInBrowserElement,
                                                        gPermissionManager->mTypeArray.ElementAt(oldPermissionEntry.mType),
                                                        oldPermissionEntry.mPermission,
                                                        oldPermissionEntry.mExpireType,
                                                        oldPermissionEntry.mExpireTime,
                                                        NS_LITERAL_STRING("deleted").get());
      --i;
      continue;
    }

    if (permEntry.mNonSessionPermission != permEntry.mPermission) {
      permEntry.mPermission = permEntry.mNonSessionPermission;

      gPermissionManager->NotifyObserversWithPermission(entry->GetKey()->mHost,
                                                        entry->GetKey()->mAppId,
                                                        entry->GetKey()->mIsInBrowserElement,
                                                        gPermissionManager->mTypeArray.ElementAt(permEntry.mType),
                                                        permEntry.mPermission,
                                                        permEntry.mExpireType,
                                                        permEntry.mExpireTime,
                                                        NS_LITERAL_STRING("changed").get());
    }
  }

  return PL_DHASH_NEXT;
}

nsresult
nsPermissionManager::RemoveExpiredPermissionsForApp(uint32_t aAppId)
{
  ENSURE_NOT_CHILD_PROCESS;

  if (aAppId != nsIScriptSecurityManager::NO_APP_ID) {
    mPermissionTable.EnumerateEntries(RemoveExpiredPermissionsForAppEnumerator, &aAppId);
  }

  return NS_OK;
}

//*****************************************************************************
//*** nsPermissionManager private methods
//*****************************************************************************

nsresult
nsPermissionManager::RemoveAllFromMemory()
{
  mLargestID = 0;
  mTypeArray.Clear();
  mPermissionTable.Clear();

  return NS_OK;
}

// Returns -1 on failure
int32_t
nsPermissionManager::GetTypeIndex(const char *aType,
                                  bool        aAdd)
{
  for (uint32_t i = 0; i < mTypeArray.Length(); ++i)
    if (mTypeArray[i].Equals(aType))
      return i;

  if (!aAdd) {
    // Not found, but that is ok - we were just looking.
    return -1;
  }

  // This type was not registered before.
  // append it to the array, without copy-constructing the string
  nsCString *elem = mTypeArray.AppendElement();
  if (!elem)
    return -1;

  elem->Assign(aType);
  return mTypeArray.Length() - 1;
}

// wrapper function for mangling (host,type,perm,expireType,expireTime)
// set into an nsIPermission.
void
nsPermissionManager::NotifyObserversWithPermission(const nsACString &aHost,
                                                   uint32_t          aAppId,
                                                   bool              aIsInBrowserElement,
                                                   const nsCString  &aType,
                                                   uint32_t          aPermission,
                                                   uint32_t          aExpireType,
                                                   int64_t           aExpireTime,
                                                   const PRUnichar  *aData)
{
  nsCOMPtr<nsIPermission> permission =
    new nsPermission(aHost, aAppId, aIsInBrowserElement, aType, aPermission,
                     aExpireType, aExpireTime);
  if (permission)
    NotifyObservers(permission, aData);
}

// notify observers that the permission list changed. there are four possible
// values for aData:
// "deleted" means a permission was deleted. aPermission is the deleted permission.
// "added"   means a permission was added. aPermission is the added permission.
// "changed" means a permission was altered. aPermission is the new permission.
// "cleared" means the entire permission list was cleared. aPermission is null.
void
nsPermissionManager::NotifyObservers(nsIPermission   *aPermission,
                                     const PRUnichar *aData)
{
  if (mObserverService)
    mObserverService->NotifyObservers(aPermission,
                                      kPermissionChangeNotification,
                                      aData);
}

nsresult
nsPermissionManager::Read()
{
  ENSURE_NOT_CHILD_PROCESS;

  nsresult rv;

  // delete expired permissions before we read in the db
  {
    // this deletion has its own scope so the write lock is released when done.
    nsCOMPtr<mozIStorageStatement> stmtDeleteExpired;
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
          "DELETE FROM moz_hosts WHERE expireType = ?1 AND expireTime <= ?2"),
          getter_AddRefs(stmtDeleteExpired));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmtDeleteExpired->BindInt32ByIndex(0, nsIPermissionManager::EXPIRE_TIME);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmtDeleteExpired->BindInt64ByIndex(1, PR_Now() / 1000);
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasResult;
    rv = stmtDeleteExpired->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, host, type, permission, expireType, expireTime, appId, isInBrowserElement "
    "FROM moz_hosts"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t id;
  nsAutoCString host, type;
  uint32_t permission;
  uint32_t expireType;
  int64_t expireTime;
  uint32_t appId;
  bool isInBrowserElement;
  bool hasResult;
  bool readError = false;

  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    // explicitly set our entry id counter for use in AddInternal(),
    // and keep track of the largest id so we know where to pick up.
    id = stmt->AsInt64(0);
    if (id > mLargestID)
      mLargestID = id;

    rv = stmt->GetUTF8String(1, host);
    if (NS_FAILED(rv)) {
      readError = true;
      continue;
    }

    rv = stmt->GetUTF8String(2, type);
    if (NS_FAILED(rv)) {
      readError = true;
      continue;
    }

    permission = stmt->AsInt32(3);
    expireType = stmt->AsInt32(4);

    // convert into int64_t value (milliseconds)
    expireTime = stmt->AsInt64(5);

    MOZ_ASSERT(stmt->AsInt64(6) >= 0);
    appId = static_cast<uint32_t>(stmt->AsInt64(6));
    isInBrowserElement = static_cast<bool>(stmt->AsInt32(7));

    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv = GetPrincipal(host, appId, isInBrowserElement, getter_AddRefs(principal));
    if (NS_FAILED(rv)) {
      readError = true;
      continue;
    }

    rv = AddInternal(principal, type, permission, id, expireType, expireTime,
                     eDontNotify, eNoDBOperation);
    if (NS_FAILED(rv)) {
      readError = true;
      continue;
    }
  }

  if (readError) {
    NS_ERROR("Error occured while reading the permissions database!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

static const char kMatchTypeHost[] = "host";

nsresult
nsPermissionManager::Import()
{
  ENSURE_NOT_CHILD_PROCESS;

  nsresult rv;

  nsCOMPtr<nsIFile> permissionsFile;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(permissionsFile));
  if (NS_FAILED(rv)) return rv;

  rv = permissionsFile->AppendNative(NS_LITERAL_CSTRING(kHostpermFileName));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream),
                                  permissionsFile);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(fileInputStream, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // start a transaction on the storage db, to optimize insertions.
  // transaction will automically commit on completion
  mozStorageTransaction transaction(mDBConn, true);

  /* format is:
   * matchtype \t type \t permission \t host
   * Only "host" is supported for matchtype
   * type is a string that identifies the type of permission (e.g. "cookie")
   * permission is an integer between 1 and 15
   */

  nsAutoCString buffer;
  bool isMore = true;
  while (isMore && NS_SUCCEEDED(lineInputStream->ReadLine(buffer, &isMore))) {
    if (buffer.IsEmpty() || buffer.First() == '#') {
      continue;
    }

    nsTArray<nsCString> lineArray;

    // Split the line at tabs
    ParseString(buffer, '\t', lineArray);

    if (lineArray[0].EqualsLiteral(kMatchTypeHost) &&
        lineArray.Length() == 4) {

      nsresult error;
      uint32_t permission = lineArray[2].ToInteger(&error);
      if (NS_FAILED(error))
        continue;

      // hosts might be encoded in UTF8; switch them to ACE to be consistent
      if (!IsASCII(lineArray[3])) {
        rv = NormalizeToACE(lineArray[3]);
        if (NS_FAILED(rv))
          continue;
      }

      nsCOMPtr<nsIPrincipal> principal;
      nsresult rv = GetPrincipal(lineArray[3], getter_AddRefs(principal));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = AddInternal(principal, lineArray[1], permission, 0,
                       nsIPermissionManager::EXPIRE_NEVER, 0, eDontNotify, eWriteToDB);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // we're done importing - delete the old file
  permissionsFile->Remove(false);

  return NS_OK;
}

nsresult
nsPermissionManager::NormalizeToACE(nsCString &aHost)
{
  // lazily init the IDN service
  if (!mIDNService) {
    nsresult rv;
    mIDNService = do_GetService(NS_IDNSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return mIDNService->ConvertUTF8toACE(aHost, aHost);
}

void
nsPermissionManager::UpdateDB(OperationType aOp,
                              mozIStorageAsyncStatement* aStmt,
                              int64_t aID,
                              const nsACString &aHost,
                              const nsACString &aType,
                              uint32_t aPermission,
                              uint32_t aExpireType,
                              int64_t aExpireTime,
                              uint32_t aAppId,
                              bool aIsInBrowserElement)
{
  ENSURE_NOT_CHILD_PROCESS_NORET;

  nsresult rv;

  // no statement is ok - just means we don't have a profile
  if (!aStmt)
    return;

  switch (aOp) {
  case eOperationAdding:
    {
      rv = aStmt->BindInt64ByIndex(0, aID);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindUTF8StringByIndex(1, aHost);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindUTF8StringByIndex(2, aType);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt32ByIndex(3, aPermission);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt32ByIndex(4, aExpireType);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt64ByIndex(5, aExpireTime);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt64ByIndex(6, aAppId);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt64ByIndex(7, aIsInBrowserElement);
      break;
    }

  case eOperationRemoving:
    {
      rv = aStmt->BindInt64ByIndex(0, aID);
      break;
    }

  case eOperationChanging:
    {
      rv = aStmt->BindInt64ByIndex(0, aID);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt32ByIndex(1, aPermission);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt32ByIndex(2, aExpireType);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt64ByIndex(3, aExpireTime);
      break;
    }

  default:
    {
      NS_NOTREACHED("need a valid operation in UpdateDB()!");
      rv = NS_ERROR_UNEXPECTED;
      break;
    }
  }

  if (NS_FAILED(rv)) {
    NS_WARNING("db change failed!");
    return;
  }

  nsCOMPtr<mozIStoragePendingStatement> pending;
  rv = aStmt->ExecuteAsync(nullptr, getter_AddRefs(pending));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

NS_IMETHODIMP
nsPermissionManager::AddrefAppId(uint32_t aAppId)
{
  if (aAppId == nsIScriptSecurityManager::NO_APP_ID) {
    return NS_OK;
  }

  bool found = false;
  for (uint32_t i = 0; i < mAppIdRefcounts.Length(); ++i) {
    if (mAppIdRefcounts[i].mAppId == aAppId) {
      ++mAppIdRefcounts[i].mCounter;
      found = true;
      break;
    }
  }

  if (!found) {
    ApplicationCounter app = { aAppId, 1 };
    mAppIdRefcounts.AppendElement(app);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::ReleaseAppId(uint32_t aAppId)
{
  // An app has been released, maybe we have to reset its session.

  if (aAppId == nsIScriptSecurityManager::NO_APP_ID) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < mAppIdRefcounts.Length(); ++i) {
    if (mAppIdRefcounts[i].mAppId == aAppId) {
      --mAppIdRefcounts[i].mCounter;

      if (!mAppIdRefcounts[i].mCounter) {
        mAppIdRefcounts.RemoveElementAt(i);
        return RemoveExpiredPermissionsForApp(aAppId);
      }

      break;
    }
  }

  return NS_OK;
}
