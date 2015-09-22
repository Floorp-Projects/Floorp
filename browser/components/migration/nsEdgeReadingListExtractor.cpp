/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEdgeReadingListExtractor.h"

#include <windows.h>

#include "nsCOMPtr.h"
#include "nsIConsoleService.h"
#include "nsIMutableArray.h"
#include "nsIWritablePropertyBag2.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsWindowsMigrationUtils.h"

#define NS_HANDLE_JET_ERROR(err) { \
  if (err < JET_errSuccess) {	\
    rv = ConvertJETError(err); \
    goto CloseDB; \
  } \
}

#define MAX_URL_LENGTH 4168
#define MAX_TITLE_LENGTH 1024

NS_IMPL_ISUPPORTS(nsEdgeReadingListExtractor, nsIEdgeReadingListExtractor)

NS_IMETHODIMP
nsEdgeReadingListExtractor::Extract(const nsAString& aDBPath, nsIArray** aItems)
{
  nsresult rv = NS_OK;
  *aItems = nullptr;

  if (!aDBPath.Length()) {
    return NS_ERROR_FAILURE;
  }

  JET_ERR err;
  JET_INSTANCE instance;
  JET_SESID sesid;
  JET_DBID dbid;
  JET_TABLEID tableid;
  JET_COLUMNDEF urlColumnInfo = { 0 };
  JET_COLUMNDEF titleColumnInfo = { 0 };
  JET_COLUMNDEF addedDateColumnInfo = { 0 };
  FILETIME addedDate;
  wchar_t urlBuffer[MAX_URL_LENGTH] = { 0 };
  wchar_t titleBuffer[MAX_TITLE_LENGTH] = { 0 };

  // Need to ensure this happens before we skip ahead to CloseDB,
  // otherwise the compiler complains.
  nsCOMPtr<nsIMutableArray> items = do_CreateInstance(NS_ARRAY_CONTRACTID);

  // JET does not throw exceptions, and so error handling and ensuring we close
  // the DB is a bit finnicky. Keep track of how far we got so we guarantee closing
  // the right things
  bool instanceCreated, sessionCreated, dbOpened, tableOpened;

  // Check for the right page size and initialize with that
  unsigned long pageSize;
  err = JetGetDatabaseFileInfoW(static_cast<char16ptr_t>(aDBPath.BeginReading()),
                                &pageSize, sizeof(pageSize), JET_DbInfoPageSize);
  NS_HANDLE_JET_ERROR(err)
  err = JetSetSystemParameter(&instance, 0, JET_paramDatabasePageSize, pageSize, NULL);
  NS_HANDLE_JET_ERROR(err)

  // Turn off recovery, because otherwise we will create log files in either the cwd or
  // overwrite Edge's own logfiles, which is useless at best and at worst might mess with
  // Edge actually using the DB
  err = JetSetSystemParameter(&instance, 0, JET_paramRecovery, 0, "Off");
  NS_HANDLE_JET_ERROR(err)

  // Start our session:
  err = JetCreateInstance(&instance, "edge_readinglist_migration");
  NS_HANDLE_JET_ERROR(err)
  instanceCreated = true;

  err = JetInit(&instance);
  NS_HANDLE_JET_ERROR(err)
  err = JetBeginSession(instance, &sesid, 0, 0);
  NS_HANDLE_JET_ERROR(err)
  sessionCreated = true;

  // Actually open the DB, and make sure to do so readonly:
  err = JetAttachDatabaseW(sesid, static_cast<char16ptr_t>(aDBPath.BeginReading()),
                           JET_bitDbReadOnly);
  NS_HANDLE_JET_ERROR(err)
  dbOpened = true;
  err = JetOpenDatabaseW(sesid, static_cast<char16ptr_t>(aDBPath.BeginReading()),
                         NULL, &dbid, JET_bitDbReadOnly);
  NS_HANDLE_JET_ERROR(err)

  // Open the readinglist table and get information on the columns we are interested in:
  err = JetOpenTable(sesid, dbid, "ReadingList", NULL, 0, JET_bitTableReadOnly, &tableid);
  NS_HANDLE_JET_ERROR(err)
  tableOpened = true;
  err = JetGetColumnInfo(sesid, dbid, "ReadingList", "URL", &urlColumnInfo,
                         sizeof(urlColumnInfo), JET_ColInfo);
  NS_HANDLE_JET_ERROR(err)
  if (urlColumnInfo.cbMax > MAX_URL_LENGTH) {
    nsCOMPtr<nsIConsoleService> consoleService = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    if (consoleService) {
      consoleService->LogStringMessage(NS_LITERAL_STRING("Edge migration: URL column size increased").get());
    }
  }
  err = JetGetColumnInfo(sesid, dbid, "ReadingList", "Title", &titleColumnInfo,
                         sizeof(titleColumnInfo), JET_ColInfo);
  NS_HANDLE_JET_ERROR(err)
  if (titleColumnInfo.cbMax > MAX_TITLE_LENGTH) {
    nsCOMPtr<nsIConsoleService> consoleService = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    if (consoleService) {
      consoleService->LogStringMessage(NS_LITERAL_STRING("Edge migration: Title column size increased").get());
    }
  }
  err = JetGetColumnInfo(sesid, dbid, "ReadingList", "AddedDate", &addedDateColumnInfo,
                         sizeof(addedDateColumnInfo), JET_ColInfo);
  NS_HANDLE_JET_ERROR(err)

  // verify the column types are what we expect:
  if (urlColumnInfo.coltyp != JET_coltypLongText ||
      titleColumnInfo.coltyp != JET_coltypLongText ||
      addedDateColumnInfo.coltyp != JET_coltypLongLong) {
    rv = NS_ERROR_NOT_IMPLEMENTED;
    goto CloseDB;
  }

  // If we got here, we've found our table and column information

  err = JetMove(sesid, tableid, JET_MoveFirst, 0);
  // It's possible there are 0 items in this table, in which case we want to
  // not fail:
  if (err == JET_errNoCurrentRecord) {
    items.forget(aItems);
    goto CloseDB;
  }
  // Check for any other errors
  NS_HANDLE_JET_ERROR(err)

  do {
    err = JetRetrieveColumn(sesid, tableid, urlColumnInfo.columnid, &urlBuffer,
                            sizeof(urlBuffer), NULL, 0, NULL);
    NS_HANDLE_JET_ERROR(err)
    err = JetRetrieveColumn(sesid, tableid, titleColumnInfo.columnid, &titleBuffer,
                            sizeof(titleBuffer), NULL, 0, NULL);
    NS_HANDLE_JET_ERROR(err)
    err = JetRetrieveColumn(sesid, tableid, addedDateColumnInfo.columnid, &addedDate,
                            sizeof(addedDate), NULL, 0, NULL);
    NS_HANDLE_JET_ERROR(err)
    nsCOMPtr<nsIWritablePropertyBag2> pbag = do_CreateInstance("@mozilla.org/hash-property-bag;1");
    bool dateIsValid;
    PRTime prAddedDate = WinMigrationFileTimeToPRTime(&addedDate, &dateIsValid);
    nsDependentString url(urlBuffer);
    nsDependentString title(titleBuffer);
    pbag->SetPropertyAsAString(NS_LITERAL_STRING("uri"), url);
    pbag->SetPropertyAsAString(NS_LITERAL_STRING("title"), title);
    if (dateIsValid) {
      pbag->SetPropertyAsInt64(NS_LITERAL_STRING("time"), prAddedDate);
    }
    items->AppendElement(pbag, false);
    memset(urlBuffer, 0, sizeof(urlBuffer));
    memset(titleBuffer, 0, sizeof(titleBuffer));
  } while (JET_errSuccess == JetMove(sesid, tableid, JET_MoveNext, 0));

  items.forget(aItems);

CloseDB:
  // Terminate ESENT. This performs a clean shutdown.
  // Ignore errors while closing:
  if (tableOpened)
    JetCloseTable(sesid, tableid);
  if (dbOpened)
    JetCloseDatabase(sesid, dbid, 0);
  if (sessionCreated)
    JetEndSession(sesid, 0);
  if (instanceCreated)
    JetTerm(instance);

  return rv;
}

nsresult
nsEdgeReadingListExtractor::ConvertJETError(const JET_ERR &aError)
{
  switch (aError) {
    case JET_errPageSizeMismatch:
    case JET_errInvalidName:
    case JET_errColumnNotFound:
      // The DB format has changed and we haven't updated this migration code:
      return NS_ERROR_NOT_IMPLEMENTED;
    case JET_errDatabaseLocked:
      return NS_ERROR_FILE_IS_LOCKED;
    case JET_errPermissionDenied:
    case JET_errAccessDenied:
      return NS_ERROR_FILE_ACCESS_DENIED;
    case JET_errInvalidFilename:
      return NS_ERROR_FILE_INVALID_PATH;
    case JET_errFileNotFound:
      return NS_ERROR_FILE_NOT_FOUND;
    default:
      return NS_ERROR_FAILURE;
  }
}

