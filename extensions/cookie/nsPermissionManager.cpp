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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michiel van Leeuwen (mvl@exedo.nl)
 *   Daniel Witte (dwitte@stanford.edu)
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

#include "nsPermissionManager.h"
#include "nsPermission.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsCOMArray.h"
#include "nsArrayEnumerator.h"
#include "nsILineInputStream.h"
#include "nsIIDNService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "prprf.h"
#include "mozIStorageService.h"
#include "mozIStorageStatement.h"
#include "mozIStorageConnection.h"
#include "mozStorageHelper.h"
#include "mozStorageCID.h"

////////////////////////////////////////////////////////////////////////////////

#define PL_ARENA_CONST_ALIGN_MASK 3
#include "plarena.h"

static PLArenaPool *gHostArena = nsnull;

// making sHostArena 512b for nice allocation
// growing is quite cheap
#define HOST_ARENA_SIZE 512

// equivalent to strdup() - does no error checking,
// we're assuming we're only called with a valid pointer
static char *
ArenaStrDup(const char* str, PLArenaPool* aArena)
{
  void* mem;
  const PRUint32 size = strlen(str) + 1;
  PL_ARENA_ALLOCATE(mem, aArena, size);
  if (mem)
    memcpy(mem, str, size);
  return static_cast<char*>(mem);
}

nsHostEntry::nsHostEntry(const char* aHost)
{
  mHost = ArenaStrDup(aHost, gHostArena);
}

// XXX this can fail on OOM
nsHostEntry::nsHostEntry(const nsHostEntry& toCopy)
 : mHost(toCopy.mHost)
 , mPermissions(toCopy.mPermissions)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsPermissionManager Implementation

static const char kPermissionsFileName[] = "permissions.sqlite";
#define HOSTS_SCHEMA_VERSION 1

static const char kHostpermFileName[] = "hostperm.1";

static const char kPermissionChangeNotification[] = PERM_CHANGE_NOTIFICATION;

NS_IMPL_ISUPPORTS3(nsPermissionManager, nsIPermissionManager, nsIObserver, nsISupportsWeakReference)

nsPermissionManager::nsPermissionManager()
 : mLargestID(0)
{
}

nsPermissionManager::~nsPermissionManager()
{
  RemoveAllFromMemory();
}

nsresult
nsPermissionManager::Init()
{
  nsresult rv;

  if (!mHostTable.Init()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // ignore failure here, since it's non-fatal (we can run fine without
  // persistent storage - e.g. if there's no profile).
  // XXX should we tell the user about this?
  InitDB();

  mObserverService = do_GetService("@mozilla.org/observer-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    mObserverService->AddObserver(this, "profile-before-change", PR_TRUE);
    mObserverService->AddObserver(this, "profile-do-change", PR_TRUE);
  }

  return NS_OK;
}

nsresult
nsPermissionManager::InitDB()
{
  nsCOMPtr<nsIFile> permissionsFile;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(permissionsFile));
  if (!permissionsFile)
    return NS_ERROR_UNEXPECTED;

  nsresult rv = permissionsFile->AppendNative(NS_LITERAL_CSTRING(kPermissionsFileName));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageService> storage = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  if (!storage)
    return NS_ERROR_UNEXPECTED;

  // cache a connection to the hosts database
  rv = storage->OpenDatabase(permissionsFile, getter_AddRefs(mDBConn));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // delete and try again
    rv = permissionsFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = storage->OpenDatabase(permissionsFile, getter_AddRefs(mDBConn));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool tableExists = PR_FALSE;
  mDBConn->TableExists(NS_LITERAL_CSTRING("moz_hosts"), &tableExists);
  if (!tableExists) {
      rv = CreateTable();
      NS_ENSURE_SUCCESS(rv, rv);

  } else {
    // table already exists; check the schema version before reading
    PRInt32 dbSchemaVersion;
    rv = mDBConn->GetSchemaVersion(&dbSchemaVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    switch (dbSchemaVersion) {
    // upgrading.
    // every time you increment the database schema, you need to implement
    // the upgrading code from the previous version to the new one.
    // fall through to current version

    // current version.
    case HOSTS_SCHEMA_VERSION:
      break;

    case 0:
      {
        NS_WARNING("couldn't get schema version!");
          
        // the table may be usable; someone might've just clobbered the schema
        // version. we can treat this case like a downgrade using the codepath
        // below, by verifying the columns we care about are all there. for now,
        // re-set the schema version in the db, in case the checks succeed (if
        // they don't, we're dropping the table anyway).
        rv = mDBConn->SetSchemaVersion(HOSTS_SCHEMA_VERSION);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      // fall through to downgrade check

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
          "SELECT host, type, permission FROM moz_hosts"), getter_AddRefs(stmt));
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
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO moz_hosts "
    "(id, host, type, permission) "
    "VALUES (?1, ?2, ?3, ?4)"), getter_AddRefs(mStmtInsert));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_hosts "
    "WHERE id = ?1"), getter_AddRefs(mStmtDelete));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_hosts "
    "SET permission = ?2 WHERE id = ?1"), getter_AddRefs(mStmtUpdate));
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
  return mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE moz_hosts ("
      " id INTEGER PRIMARY KEY"
      ",host TEXT"
      ",type TEXT"
      ",permission INTEGER"
    ")"));
}

NS_IMETHODIMP
nsPermissionManager::Add(nsIURI     *aURI,
                         const char *aType,
                         PRUint32    aPermission)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aType);

  nsresult rv;

  nsCAutoString host;
  rv = GetHost(aURI, host);
  NS_ENSURE_SUCCESS(rv, rv);

  return AddInternal(host, nsDependentCString(aType), aPermission, 0, eNotify, eWriteToDB);
}

nsresult
nsPermissionManager::AddInternal(const nsAFlatCString &aHost,
                                 const nsAFlatCString &aType,
                                 PRUint32              aPermission,
                                 PRInt64               aID,
                                 NotifyOperationType   aNotifyOperation,
                                 DBOperationType       aDBOperation)
{
  if (!gHostArena) {
    gHostArena = new PLArenaPool;
    if (!gHostArena)
      return NS_ERROR_OUT_OF_MEMORY;    
    PL_INIT_ARENA_POOL(gHostArena, "PermissionHostArena", HOST_ARENA_SIZE);
  }

  // look up the type index
  PRInt32 typeIndex = GetTypeIndex(aType.get(), PR_TRUE);
  NS_ENSURE_TRUE(typeIndex != -1, NS_ERROR_OUT_OF_MEMORY);

  // When an entry already exists, PutEntry will return that, instead
  // of adding a new one
  nsHostEntry *entry = mHostTable.PutEntry(aHost.get());
  if (!entry) return NS_ERROR_FAILURE;
  if (!entry->GetKey()) {
    mHostTable.RawRemoveEntry(entry);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // figure out the transaction type, and get any existing permission value
  OperationType op;
  PRInt32 index = entry->GetPermissionIndex(typeIndex);
  PRUint32 oldPermission;
  if (index == -1) {
    if (aPermission == nsIPermissionManager::UNKNOWN_ACTION)
      op = eOperationNone;
    else
      op = eOperationAdding;

  } else {
    oldPermission = entry->GetPermissions()[index].mPermission;

    if (aPermission == oldPermission)
      op = eOperationNone;
    else if (aPermission == nsIPermissionManager::UNKNOWN_ACTION)
      op = eOperationRemoving;
    else
      op = eOperationChanging;
  }

  // do the work for adding, deleting, or changing a permission:
  // update the in-memory list, write to the db, and notify consumers.
  PRInt64 id;
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

      entry->GetPermissions().AppendElement(nsPermissionEntry(typeIndex, aPermission, id));

      if (aDBOperation == eWriteToDB)
        UpdateDB(op, mStmtInsert, id, aHost, aType, aPermission);

      if (aNotifyOperation == eNotify) {
        NotifyObserversWithPermission(aHost,
                                      mTypeArray[typeIndex],
                                      aPermission,
                                      NS_LITERAL_STRING("added").get());
      }

      break;
    }

  case eOperationRemoving:
    {
      id = entry->GetPermissions()[index].mID;
      entry->GetPermissions().RemoveElementAt(index);

      // If no more types are present, remove the entry
      if (entry->GetPermissions().IsEmpty())
        mHostTable.RawRemoveEntry(entry);

      if (aDBOperation == eWriteToDB)
        UpdateDB(op, mStmtDelete, id, EmptyCString(), EmptyCString(), 0);

      if (aNotifyOperation == eNotify) {
        NotifyObserversWithPermission(aHost,
                                      mTypeArray[typeIndex],
                                      oldPermission,
                                      NS_LITERAL_STRING("deleted").get());
      }

      break;
    }

  case eOperationChanging:
    {
      id = entry->GetPermissions()[index].mID;
      entry->GetPermissions()[index].mPermission = aPermission;

      if (aDBOperation == eWriteToDB)
        UpdateDB(op, mStmtUpdate, id, EmptyCString(), EmptyCString(), aPermission);

      if (aNotifyOperation == eNotify) {
        NotifyObserversWithPermission(aHost,
                                      mTypeArray[typeIndex],
                                      aPermission,
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
  NS_ENSURE_ARG_POINTER(aType);

  // AddInternal() handles removal, just let it do the work
  return AddInternal(PromiseFlatCString(aHost),
                     nsDependentCString(aType),
                     nsIPermissionManager::UNKNOWN_ACTION,
                     0,
                     eNotify,
                     eWriteToDB);
}

NS_IMETHODIMP
nsPermissionManager::RemoveAll()
{
  nsresult rv = RemoveAllInternal();
  NotifyObservers(nsnull, NS_LITERAL_STRING("cleared").get());
  return rv;
}

nsresult
nsPermissionManager::RemoveAllInternal()
{
  RemoveAllFromMemory();

  // clear the db
  if (mDBConn) {
    nsresult rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DELETE FROM moz_hosts"));
    if (NS_FAILED(rv)) {
      NS_WARNING("db delete failed");
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::TestExactPermission(nsIURI     *aURI,
                                         const char *aType,
                                         PRUint32   *aPermission)
{
  return CommonTestPermission(aURI, aType, aPermission, PR_TRUE);
}

NS_IMETHODIMP
nsPermissionManager::TestPermission(nsIURI     *aURI,
                                    const char *aType,
                                    PRUint32   *aPermission)
{
  return CommonTestPermission(aURI, aType, aPermission, PR_FALSE);
}

nsresult
nsPermissionManager::CommonTestPermission(nsIURI     *aURI,
                                          const char *aType,
                                          PRUint32   *aPermission,
                                          PRBool      aExactHostMatch)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aType);

  // set the default
  *aPermission = nsIPermissionManager::UNKNOWN_ACTION;

  nsCAutoString host;
  nsresult rv = GetHost(aURI, host);
  // no host doesn't mean an error. just return the default
  if (NS_FAILED(rv)) return NS_OK;
  
  PRInt32 typeIndex = GetTypeIndex(aType, PR_FALSE);
  // If type == -1, the type isn't known,
  // so just return NS_OK
  if (typeIndex == -1) return NS_OK;

  nsHostEntry *entry = GetHostEntry(host, typeIndex, aExactHostMatch);
  if (entry)
    *aPermission = entry->GetPermission(typeIndex);

  return NS_OK;
}

// Get hostentry for given host string and permission type.
// walk up the domain if needed.
// return null if nothing found.
nsHostEntry *
nsPermissionManager::GetHostEntry(const nsAFlatCString &aHost,
                                  PRUint32              aType,
                                  PRBool                aExactHostMatch)
{
  PRUint32 offset = 0;
  nsHostEntry *entry;
  do {
    entry = mHostTable.GetEntry(aHost.get() + offset);
    if (entry) {
      if (entry->GetPermission(aType) != nsIPermissionManager::UNKNOWN_ACTION)
        break;

      // reset entry, to be able to return null on failure
      entry = nsnull;
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

PR_STATIC_CALLBACK(PLDHashOperator)
AddPermissionsToList(nsHostEntry *entry, void *arg)
{
  nsGetEnumeratorData *data = static_cast<nsGetEnumeratorData *>(arg);

  for (PRUint32 i = 0; i < entry->GetPermissions().Length(); ++i) {
    nsPermissionEntry &permEntry = entry->GetPermissions()[i];

    nsPermission *perm = new nsPermission(entry->GetHost(), 
                                          data->types->ElementAt(permEntry.mType),
                                          permEntry.mPermission);

    data->array->AppendObject(perm);
  }

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP nsPermissionManager::GetEnumerator(nsISimpleEnumerator **aEnum)
{
  // roll an nsCOMArray of all our permissions, then hand out an enumerator
  nsCOMArray<nsIPermission> array;
  nsGetEnumeratorData data(&array, &mTypeArray);

  mHostTable.EnumerateEntries(AddPermissionsToList, &data);

  return NS_NewArrayEnumerator(aEnum, array);
}

NS_IMETHODIMP nsPermissionManager::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    // The profile is about to change,
    // or is going away because the application is shutting down.
    if (!nsCRT::strcmp(someData, NS_LITERAL_STRING("shutdown-cleanse").get())) {
      // clear the permissions file
      RemoveAllInternal();
    } else {
      RemoveAllFromMemory();
    }
  }  
  else if (!nsCRT::strcmp(aTopic, "profile-do-change")) {
    // the profile has already changed; init the db from the new location
    InitDB();
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
  mHostTable.Clear();
  if (gHostArena) {
    PL_FinishArenaPool(gHostArena);
    delete gHostArena;
  }
  gHostArena = nsnull;
  return NS_OK;
}

// Returns -1 on failure
PRInt32
nsPermissionManager::GetTypeIndex(const char *aType,
                                  PRBool      aAdd)
{
  for (PRUint32 i = 0; i < mTypeArray.Length(); ++i)
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

// wrapper function for mangling (host,type,perm) triplet into an nsIPermission.
void
nsPermissionManager::NotifyObserversWithPermission(const nsACString &aHost,
                                                   const nsCString  &aType,
                                                   PRUint32          aPermission,
                                                   const PRUnichar  *aData)
{
  nsCOMPtr<nsIPermission> permission =
    new nsPermission(aHost, aType, aPermission);
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
  nsresult rv;

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, host, type, permission "
    "FROM moz_hosts"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 id;
  nsCAutoString host, type;
  PRUint32 permission;
  PRBool hasResult;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    // explicitly set our entry id counter for use in AddInternal(),
    // and keep track of the largest id so we know where to pick up.
    id = stmt->AsInt64(0);
    if (id > mLargestID)
      mLargestID = id;

    rv = stmt->GetUTF8String(1, host);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->GetUTF8String(2, type);
    NS_ENSURE_SUCCESS(rv, rv);

    permission = stmt->AsInt32(3);

    rv = AddInternal(host, type, permission, id, eDontNotify, eNoDBOperation);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

static const char kMatchTypeHost[] = "host";

nsresult
nsPermissionManager::Import()
{
  nsresult rv;

  nsCOMPtr<nsIFile> permissionsFile;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(permissionsFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = permissionsFile->AppendNative(NS_LITERAL_CSTRING(kHostpermFileName));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream),
                                  permissionsFile);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(fileInputStream, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // start a transaction on the storage db, to optimize insertions.
  // transaction will automically commit on completion
  mozStorageTransaction transaction(mDBConn, PR_TRUE);

  /* format is:
   * matchtype \t type \t permission \t host
   * Only "host" is supported for matchtype
   * type is a string that identifies the type of permission (e.g. "cookie")
   * permission is an integer between 1 and 15
   */

  nsCAutoString buffer;
  PRBool isMore = PR_TRUE;
  while (isMore && NS_SUCCEEDED(lineInputStream->ReadLine(buffer, &isMore))) {
    if (buffer.IsEmpty() || buffer.First() == '#') {
      continue;
    }

    nsCStringArray lineArray;

    // Split the line at tabs
    lineArray.ParseString(buffer.get(), "\t");
    
    if (lineArray[0]->EqualsLiteral(kMatchTypeHost) &&
        lineArray.Count() == 4) {
      
      PRInt32 error;
      PRUint32 permission = lineArray[2]->ToInteger(&error);
      if (error)
        continue;

      // hosts might be encoded in UTF8; switch them to ACE to be consistent
      if (!IsASCII(*lineArray[3])) {
        rv = NormalizeToACE(*lineArray[3]);
        if (NS_FAILED(rv))
          continue;
      }

      rv = AddInternal(*lineArray[3], *lineArray[1], permission, 0, eDontNotify, eWriteToDB);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // we're done importing - delete the old file
  permissionsFile->Remove(PR_FALSE);

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

nsresult
nsPermissionManager::GetHost(nsIURI *aURI, nsACString &aResult)
{
  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(aURI);
  if (!innerURI) return NS_ERROR_FAILURE;

  nsresult rv = innerURI->GetAsciiHost(aResult);

  if (NS_FAILED(rv) || aResult.IsEmpty())
    return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

void
nsPermissionManager::UpdateDB(OperationType         aOp,
                              mozIStorageStatement* aStmt,
                              PRInt64               aID,
                              const nsACString     &aHost,
                              const nsACString     &aType,
                              PRUint32              aPermission)
{
  nsresult rv;

  // no statement is ok - just means we don't have a profile
  if (!aStmt)
    return;

  switch (aOp) {
  case eOperationAdding:
    {
      rv = aStmt->BindInt64Parameter(0, aID);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindUTF8StringParameter(1, aHost);
      if (NS_FAILED(rv)) break;
      
      rv = aStmt->BindUTF8StringParameter(2, aType);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt32Parameter(3, aPermission);
      break;
    }

  case eOperationRemoving:
    {
      rv = aStmt->BindInt64Parameter(0, aID);
      break;
    }

  case eOperationChanging:
    {
      rv = aStmt->BindInt64Parameter(0, aID);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt32Parameter(1, aPermission);
      break;
    }

  default:
    {
      NS_NOTREACHED("need a valid operation in UpdateDB()!");
      rv = NS_ERROR_UNEXPECTED;
      break;
    }
  }

  if (NS_SUCCEEDED(rv)) {
    PRBool hasResult;
    rv = aStmt->ExecuteStep(&hasResult);
    aStmt->Reset();
  }

  if (NS_FAILED(rv))
    NS_WARNING("db change failed!");
}

