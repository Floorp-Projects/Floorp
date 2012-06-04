/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_filestream_h__
#define mozilla_dom_indexeddb_filestream_h__

#include "IndexedDatabase.h"

#include "nsIFileStreams.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsISeekableStream.h"
#include "nsIStandardFileStream.h"

class nsIFile;
struct quota_FILE;

BEGIN_INDEXEDDB_NAMESPACE

class FileStream : public nsISeekableStream,
                   public nsIInputStream,
                   public nsIOutputStream,
                   public nsIStandardFileStream,
                   public nsIFileMetadata
{
public:
  FileStream()
  : mFlags(0),
    mDeferredOpen(false),
    mQuotaFile(nsnull)
  { }

  virtual ~FileStream()
  {
    Close();
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSISTANDARDFILESTREAM
  NS_DECL_NSIFILEMETADATA

  // nsIInputStream
  NS_IMETHOD
  Close();

  NS_IMETHOD
  Available(PRUint32* _retval);

  NS_IMETHOD
  Read(char* aBuf, PRUint32 aCount, PRUint32* _retval);

  NS_IMETHOD
  ReadSegments(nsWriteSegmentFun aWriter, void* aClosure, PRUint32 aCount,
               PRUint32* _retval);

  NS_IMETHOD
  IsNonBlocking(bool* _retval);

  // nsIOutputStream

  // Close() already declared

  NS_IMETHOD
  Flush();

  NS_IMETHOD
  Write(const char* aBuf, PRUint32 aCount, PRUint32* _retval);

  NS_IMETHOD
  WriteFrom(nsIInputStream* aFromStream, PRUint32 aCount, PRUint32* _retval);

  NS_IMETHOD
  WriteSegments(nsReadSegmentFun aReader, void* aClosure, PRUint32 aCount,
                PRUint32* _retval);

  // IsNonBlocking() already declared

protected:
  /**
   * Cleans up data prepared in Init.
   */
  void
  CleanUpOpen()
  {
    mFilePath.Truncate();
    mDeferredOpen = false;
  }

  /**
   * Open the file. This is called either from Init
   * or from DoPendingOpen (if FLAGS_DEFER_OPEN is used when initializing this
   * stream). The default behavior of DoOpen is to open the file and save the
   * file descriptor.
   */
  virtual nsresult
  DoOpen();

  /**
   * If there is a pending open, do it now. It's important for this to be
   * inlined since we do it in almost every stream API call.
   */
  nsresult
  DoPendingOpen()
  {
    if (!mDeferredOpen) {
      return NS_OK;
    }

    return DoOpen();
  }

  /**
   * Data we need to do an open.
   */
  nsString mFilePath;
  nsString mMode;

  /**
   * Flags describing our behavior.  See the IDL file for possible values.
   */
  PRInt32 mFlags;

  /**
   * Whether we have a pending open (see FLAGS_DEFER_OPEN in the IDL file).
   */
  bool mDeferredOpen;

  /**
   * File descriptor for opened file.
   */
  quota_FILE* mQuotaFile;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_filestream_h__
