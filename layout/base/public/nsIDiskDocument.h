/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIDiskDocument_h__
#define nsIDiskDocument_h__


#include "nsIDOMDocument.h"


#define NS_IDISKDOCUMENT_IID												\
{/* {daf96f80-0183-11d3-9ce4-a865f869f0bc}		*/			\
0xdaf96f80, 0x0183, 0x11d3,														\
{ 0x9c, 0xe4, 0xa8, 0x65, 0xf8, 0x69, 0xf0, 0xbc } }


/**
 * This interface is used to associate a document with a disk file,
 * and to save to that file.
 * 
 */

class nsIDOMDocument;
class nsFileSpec;


class nsIDiskDocument	: public nsISupports
{
public:

  static const nsIID& GetIID() { static nsIID iid = NS_IDISKDOCUMENT_IID; return iid; }

	//typedef enum {eFileDiskFile = 0, eFileRemote = 1 } ESaveFileLocation;

  /** Initialize the document output. This may be called on document
    * creation, or lazily before the first save. For a document read
    * in from disk, it should be called on document instantiation.
    *
    * @param aDoc						The document being observed.
    * @param aFileSpec			Filespec for the file, if a disk version
    *												of the file exists already. Otherwise nsnull.
    */
  NS_IMETHOD InitDiskDocument(nsFileSpec *aFileSpec)=0;

  /** Save the file to disk. This will be called after the caller has
    * displayed a put file dialog, which the user confirmed. The internal
    * fileSpec of the document is only updated with the given fileSpec if inSaveCopy == PR_FALSE.
    *    
    * @param inFileSpec					File to which to stream the document.
    * @param inReplaceExisting	true if replacing an existing file, otherwise false.
    *    												If false and aFileSpec exists, SaveFile returns an error.
    * @param inSaveCopy					True to save a copy of the file, without changing the file
    *														referenced internally.
    * @param inSaveFileType			Mime type to save (text/plain or text/html)
    * @param inSaveCharset			Charset to save the document in. If this is an empty
    *    												string, or "UCS2", then the doc will be saved in the document's default charset.
    * @param inSaveFlags        Flags (see nsIDocumentEncoder).  If unsure, use 0.
    * @param inWrapColumn       Wrap column, assuming that flags specify wrapping.
    */
  NS_IMETHOD SaveFile(			nsFileSpec*			inFileSpec,
  													PRBool 					inReplaceExisting,
  													PRBool					inSaveCopy,
  													const nsString&	inSaveFileType,
  													const nsString&	inSaveCharset,
                            PRUint32        inSaveFlags,
                            PRUint32        inWrapColumn)=0;
  
  /** Return a file spec for the file. If the file has not been saved yet,
    * and thus has no fileSpec, this will return NS_ERROR_NOT_INITIALIZED.
    */

  NS_IMETHOD GetFileSpec(nsFileSpec& aFileSpec)=0;

  /** Get the modification count for the document. A +ve
    * mod count indicates that the document is dirty,
    * and needs saving.
    */
  NS_IMETHOD GetModCount(PRInt32 *outModCount)=0;
 
  /** Reset the modification count for the document.
  	* this marks the documents as 'clean' and not
  	* in need of saving.
    */
  NS_IMETHOD ResetModCount()=0;

  /** Increment the modification count for the document by the given
    * amount (which may be -ve).
    */
  NS_IMETHOD IncrementModCount(PRInt32 aNumMods)=0;
	
};


#endif // nsIDiskDocument_h__


