/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *  Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsEmbedGlobalHistory.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsWeakReference.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsHashtable.h"
#include "nsInt64.h"
#include "prtypes.h"
#include "nsFixedSizeAllocator.h"
#include "nsVoidArray.h"
#include "nsIPrefService.h"

// Constants
static const PRInt32 kNewEntriesBetweenFlush = 10;

static const PRUint32 kDefaultExpirationIntervalDays = 7;

static const PRInt64 kMSecsPerDay = LL_INIT(0, 60 * 60 * 24 * 1000);
static const PRInt64 kOneThousand = LL_INIT(0, 1000);

#define PREF_BROWSER_HISTORY_EXPIRE_DAYS "browser.history_expire_days"

// Static Routine Prototypes
static nsresult readEntry(FILE *inStream, nsCString& url, HistoryEntry **entry);
static nsresult writeEntry(FILE *outStm, nsCStringKey *url, HistoryEntry *entry);

static PRIntn enumWriteEntry(nsHashKey *aKey, void *aData, void* closure);
static PRIntn enumWriteEntryIfUnwritten(nsHashKey *aKey, void *aData, void* closure);
static PRIntn enumDeleteEntry(nsHashKey *aKey, void *aData, void* closure);

//*****************************************************************************
// HistoryEntry
//*****************************************************************************   

class HistoryEntry {
public:
                HistoryEntry() :
                  mWritten(PR_FALSE) {}
  

  void          OnVisited()
                {
                  mLastVisitTime = PR_Now(); 
                  LL_DIV(mLastVisitTime, mLastVisitTime, kOneThousand);
                }
                
  PRInt64       GetLastVisitTime()
                { return mLastVisitTime; }
  void          SetLastVisitTime(const PRInt64& aTime)
                { mLastVisitTime = aTime; }

  PRBool        GetIsWritten()
                { return mWritten; }
  void          SetIsWritten(PRBool written = PR_TRUE)
                { mWritten = PR_TRUE; }
                  
  // Memory management stuff    
  static void*  operator new(size_t size) CPP_THROW_NEW;
  static void   operator delete(void *p, size_t size);
  
  // Must be called when done with all HistoryEntry objects
  static void ReleasePool();
 
 private:
  PRInt64       mLastVisitTime; // Millisecs
  PRPackedBool  mWritten; // TRUE if ever persisted

  static nsresult InitPool();
  static nsFixedSizeAllocator *sPool;
};

nsFixedSizeAllocator *HistoryEntry::sPool;

//*****************************************************************************   

void* HistoryEntry::operator new(size_t size) CPP_THROW_NEW
{
  if (size != sizeof(HistoryEntry))
    return ::operator new(size);
  if (!sPool && NS_FAILED(InitPool()))
    return nsnull;
    
  return sPool->Alloc(size);
}

void HistoryEntry::operator delete(void *p, size_t size)
{
  if (!p)
    return;
  if (size != sizeof(HistoryEntry))
    ::operator delete(p);
  if (!sPool) {
    NS_ERROR("HistoryEntry outlived its memory pool");
    return;
  }
  sPool->Free(p, size);
}

nsresult HistoryEntry::InitPool()
{
  if (!sPool) {
    sPool = new nsFixedSizeAllocator;
    if (!sPool)
      return NS_ERROR_OUT_OF_MEMORY;

    static const size_t kBucketSizes[] =
      { sizeof(HistoryEntry) };
    static const PRInt32 kInitialPoolSize =
      NS_SIZE_IN_HEAP(sizeof(HistoryEntry)) * 256;

    nsresult rv = sPool->Init("EmbedLite HistoryEntry Pool", kBucketSizes, 1, kInitialPoolSize);
    if (NS_FAILED(rv))
      return rv;
  }
  return NS_OK;
}

void HistoryEntry::ReleasePool()
{
  delete sPool;
  sPool = nsnull;
}

//*****************************************************************************
// nsEmbedGlobalHistory - Creation/Destruction
//*****************************************************************************   

NS_IMPL_ISUPPORTS3(nsEmbedGlobalHistory, nsIGlobalHistory, nsIObserver, nsISupportsWeakReference)

nsEmbedGlobalHistory::nsEmbedGlobalHistory() :
  mDataIsLoaded(PR_FALSE), mEntriesAddedSinceFlush(0),
  mURLTable(nsnull)
{  
  LL_I2L(mExpirationInterval, kDefaultExpirationIntervalDays);
  LL_MUL(mExpirationInterval, mExpirationInterval, kMSecsPerDay);
}

nsEmbedGlobalHistory::~nsEmbedGlobalHistory()
{
  FlushData();
  delete mURLTable;
  HistoryEntry::ReleasePool();
}

NS_IMETHODIMP nsEmbedGlobalHistory::Init()
{
  mURLTable = new nsHashtable;
  NS_ENSURE_TRUE(mURLTable, NS_ERROR_OUT_OF_MEMORY);
  
  // Get Pref and convert to millisecs
  nsCOMPtr<nsIPrefBranch> prefs(do_GetService("@mozilla.org/preferences-service;1"));
  if (prefs) {
    PRInt32 expireDays;
    prefs->GetIntPref(PREF_BROWSER_HISTORY_EXPIRE_DAYS, &expireDays);
    LL_I2L(mExpirationInterval, expireDays);
    LL_MUL(mExpirationInterval, mExpirationInterval, kMSecsPerDay);
  }
  
  // register to observe profile changes
  nsCOMPtr<nsIObserverService> observerService = 
       do_GetService("@mozilla.org/observer-service;1");
  NS_ASSERTION(observerService, "failed to get observer service");
  if (observerService)
    observerService->AddObserver(this, "profile-before-change", PR_TRUE);

  return NS_OK;
}

//*****************************************************************************
// nsEmbedGlobalHistory::nsIGlobalHistory
//*****************************************************************************   

NS_IMETHODIMP nsEmbedGlobalHistory::AddPage(const char *aURL)
{
  NS_ENSURE_ARG(aURL);

  nsresult rv = LoadData();
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCStringKey asKey(aURL);
  HistoryEntry *entry = NS_STATIC_CAST(HistoryEntry *, mURLTable->Get(&asKey));
  if (!entry) {
    
    if (++mEntriesAddedSinceFlush >= kNewEntriesBetweenFlush)
      FlushData(kFlushModeAppend);

    HistoryEntry *newEntry = new HistoryEntry;
    if (!newEntry)
      return NS_ERROR_FAILURE;
    (void)mURLTable->Put(&asKey, newEntry);
    entry = newEntry;
  }
  entry->OnVisited(); 
  
  return NS_OK;
}

NS_IMETHODIMP nsEmbedGlobalHistory::IsVisited(const char *aURL, PRBool *_retval)
{
  NS_ENSURE_ARG(aURL);
  NS_ENSURE_ARG_POINTER(_retval);
  
  nsresult rv = LoadData();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCStringKey asKey(aURL);
  
  *_retval = (mURLTable->Exists(&asKey));
  return NS_OK;
}

//*****************************************************************************
// nsEmbedGlobalHistory::nsIObserver
//*****************************************************************************   

NS_IMETHODIMP nsEmbedGlobalHistory::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  nsresult rv = NS_OK;
    
  if (strcmp(aTopic, "profile-before-change") == 0) {
    (void)FlushData();
    (void)ResetData();
  }
  return rv;
}

//*****************************************************************************
// nsEmbedGlobalHistory
//*****************************************************************************   

nsresult nsEmbedGlobalHistory::LoadData()
{
  if (!mDataIsLoaded) {
    
    nsresult rv;            
    PRBool exists;

    mDataIsLoaded = PR_TRUE;

    rv = GetHistoryFile();
    if (NS_FAILED(rv))
      return rv;
    rv = mHistoryFile->Exists(&exists);
    if (NS_FAILED(rv))
      return rv;
    if (!exists)
      return NS_OK;
    
    FILE *stdFile;
    rv = mHistoryFile->OpenANSIFileDesc("r", &stdFile);
    if (NS_FAILED(rv))
      return rv;
      
    nsCAutoString outString;
    HistoryEntry *newEntry;      
    while (NS_SUCCEEDED(readEntry(stdFile, outString, &newEntry))) {
      if (EntryHasExpired(newEntry)) {
        delete newEntry;
      }
      else {
        nsCStringKey asKey(outString);
        mURLTable->Put(&asKey, newEntry);
      }
    }
    
    fclose(stdFile);
  }
  return NS_OK;
}

nsresult nsEmbedGlobalHistory::FlushData(PRIntn mode)
{  
  if (mHistoryFile) {

    const char* openMode = (mode == kFlushModeAppend ? "a" : "w");
    FILE *stdFile;
    nsresult rv = mHistoryFile->OpenANSIFileDesc(openMode, &stdFile);
    if (NS_FAILED(rv)) return rv;

    // Before flushing either way, remove dead entries
    mURLTable->Enumerate(enumRemoveEntryIfExpired, this);  
    
    if (mode == kFlushModeAppend)
        mURLTable->Enumerate(enumWriteEntryIfUnwritten, stdFile);
    else
        mURLTable->Enumerate(enumWriteEntry, stdFile);
    
    mEntriesAddedSinceFlush = 0;
    fclose(stdFile);
  }
  return NS_OK; 
}

nsresult nsEmbedGlobalHistory::ResetData()
{
  mURLTable->Reset(enumDeleteEntry);
  mHistoryFile = 0;
  mDataIsLoaded = PR_FALSE;
  mEntriesAddedSinceFlush = 0;
  return NS_OK;
}

nsresult nsEmbedGlobalHistory::GetHistoryFile()
{
  nsresult rv;

  // Get the history file in our profile dir.
  // Notice we are not just getting NS_APP_HISTORY_50_FILE
  // because it is used by the "real" global history component.
  
  nsCOMPtr<nsIFile> aFile; 
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(aFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aFile->Append(NS_LITERAL_STRING("history.txt"));
  NS_ENSURE_SUCCESS(rv, rv);
  mHistoryFile = do_QueryInterface(aFile);
  return NS_OK;
}

PRBool nsEmbedGlobalHistory::EntryHasExpired(HistoryEntry *entry)
{
  // convert "now" from microsecs to millisecs
  PRInt64 nowInMilliSecs = PR_Now(); 
  LL_DIV(nowInMilliSecs, nowInMilliSecs, kOneThousand);

  // determine when the entry would have expired
  PRInt64 expirationIntervalAgo;
  LL_SUB(expirationIntervalAgo, nowInMilliSecs, mExpirationInterval);

  PRInt64 lastVisitTime = entry->GetLastVisitTime();
  return (LL_CMP(lastVisitTime, <, expirationIntervalAgo));
}

PRIntn nsEmbedGlobalHistory::enumRemoveEntryIfExpired(nsHashKey *aKey, void *aData, void* closure)
{
  HistoryEntry *entry = NS_STATIC_CAST(HistoryEntry*, aData);
  if (!entry)
    return PR_FALSE;
  nsEmbedGlobalHistory *history = NS_STATIC_CAST(nsEmbedGlobalHistory*, closure);
  if (!history)
    return PR_FALSE;
      
  if (history->EntryHasExpired(entry)) {
    // what do do here?
    //delete entry;
    return PR_TRUE;
  }
  return PR_TRUE;
}


//*****************************************************************************
// Static Functions
//*****************************************************************************   

static nsresult parsePRInt64(FILE *inStm, PRInt64& outValue)
{
  int c, charsRead = 0;
  nsInt64 value = 0;

  while (PR_TRUE) {
    c = fgetc(inStm);
    if (c == EOF || !isdigit(c))
      break;
      
    ++charsRead;
    PRInt32 digit = c - '0';
    value *= nsInt64(10);
    value += nsInt64(digit);
  }
  if (!charsRead)
    return NS_ERROR_FAILURE;
  outValue = value;
  return NS_OK;
}

nsresult readEntry(FILE *inStream, nsCString& outURL, HistoryEntry **outEntry)
{
  nsresult rv;

  // Get the last visted date
  PRInt64 value;
  rv = parsePRInt64(inStream, value);
  if (NS_FAILED(rv))
    return rv;
  
  // Get the URL
  int c;
  char buf[1024];
  char *next, *end = buf + sizeof(buf);
    
  outURL.Truncate(0);
  next = buf;
  
  while (PR_TRUE) {
    c = fgetc(inStream);
    
    if (c == EOF)
      break;
    else if (c == '\n')
      break;
    else if (c == '\r') {
      c = fgetc(inStream);
      if (c != '\n')
        ungetc(c, inStream);
      break;
    }
    else {
      *next++ = c;
      if (next >= end) {
        outURL.Append(buf, next - buf);
        next = buf;
      }
    }
  }
  if (next > buf)
    outURL.Append(buf, next - buf);
    
  if (!outURL.Length() && c == EOF)
    return NS_ERROR_FAILURE;
    
  *outEntry = new HistoryEntry;
  if (!*outEntry)
    return NS_ERROR_OUT_OF_MEMORY;
  (*outEntry)->SetLastVisitTime(value);
  (*outEntry)->SetIsWritten();
    
  return NS_OK;
}

static nsresult writePRInt64(FILE *outStm, const PRInt64& inValue)
{
  nsInt64 value(inValue);
  
  if (value == nsInt64(0)) {
    fputc('0', outStm);
    return NS_OK;
  }
  
  nsCAutoString tempString;

  while (value != nsInt64(0)) {
    PRInt32 ones = PRInt32(value % nsInt64(10));
    value /= nsInt64(10);
    tempString.Insert(char('0' + ones), 0);
  }
  int result = fputs(tempString.get(), outStm);
  return (result == EOF) ? NS_ERROR_FAILURE : NS_OK;
}

nsresult writeEntry(FILE *outStm, nsCStringKey *url, HistoryEntry *entry)
{
  writePRInt64(outStm, entry->GetLastVisitTime());
  fputc(':', outStm);

  fputs(url->GetString(), outStm);
  entry->SetIsWritten();

#if defined (XP_PC)
  fputc('\r', outStm);
  fputc('\n', outStm);
#elif defined(XP_UNIX)
  fputc('\n', outStm);
#else
  fputc('\r', outStm);
#endif

  return NS_OK;
}

PRBool enumWriteEntry(nsHashKey *aKey, void *aData, void* closure)
{
  FILE *outStm = NS_STATIC_CAST(FILE*, closure);
  if (!outStm)
    return PR_FALSE;
  nsCStringKey *stringKey = NS_STATIC_CAST(nsCStringKey*, aKey);
  if (!stringKey)
    return PR_FALSE;
  HistoryEntry *entry = NS_STATIC_CAST(HistoryEntry*, aData);
  if (!entry)
    return PR_FALSE;

  nsresult rv = writeEntry(outStm, stringKey, entry);
    
  return NS_SUCCEEDED(rv) ? PR_TRUE : PR_FALSE;
}

PRBool enumWriteEntryIfUnwritten(nsHashKey *aKey, void *aData, void* closure)
{
  FILE *outStm = NS_STATIC_CAST(FILE*, closure);
  if (!outStm)
    return PR_FALSE;
  nsCStringKey *stringKey = NS_STATIC_CAST(nsCStringKey*, aKey);
  if (!stringKey)
    return PR_FALSE;
  HistoryEntry *entry = NS_STATIC_CAST(HistoryEntry*, aData);
  if (!entry)
    return PR_FALSE;

  nsresult rv;
  if (!entry->GetIsWritten())
    rv = writeEntry(outStm, stringKey, entry);
    
  return NS_SUCCEEDED(rv) ? PR_TRUE : PR_FALSE;
}

PRIntn enumDeleteEntry(nsHashKey *aKey, void *aData, void* closure)
{
  HistoryEntry *entry = NS_STATIC_CAST(HistoryEntry*, aData);
  delete entry;
  
  return PR_TRUE;
}
