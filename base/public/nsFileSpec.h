/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

//    First checked in on 98/11/20 by John R. McMullen in the wrong directory.
//    Checked in again 98/12/04.
//    Polished version 98/12/08.

//========================================================================================
//
//  Classes defined:
//
//      nsFilePath, nsFileURL, nsFileSpec, nsPersistentFileDescriptor
//      nsDirectoryIterator. Oh, and a convenience class nsAutoCString.
//
//  Q.  How should I represent files at run time?
//  A.  Use nsFileSpec.  Using char* will lose information on some platforms.
//
//  Q.  Then what are nsFilePath and nsFileURL for?
//  A.  Only when you need a char* parameter for legacy code.
//
//  Q.  How should I represent files in a persistent way (eg, in a disk file)?
//  A.  Use nsPersistentFileDescriptor.  Convert to and from nsFileSpec at run time.
//
//  This suite provides the following services:
//
//      1.  Encapsulates all platform-specific file details, so that files can be
//          described correctly without any platform #ifdefs
//
//      2.  Type safety.  This will fix the problems that used to occur because people
//          confused file paths.  They used to use const char*, which could mean three
//          or four different things.  Bugs were introduced as people coded, right up
//          to the moment Communicator 4.5 shipped.
//
//      3.  Used in conjunction with nsFileStream.h (q.v.), this supports all the power
//          and readability of the ansi stream syntax.
//
//          Basic example:
//
//              nsFilePath myPath("/Development/iotest.txt");
//
//              nsOutputFileStream testStream(myPath);
//              testStream << "Hello World" << nsEndl;
//
//      4.  Handy methods for manipulating file specifiers safely, e.g. MakeUnique(),
//          SetLeafName(), Exists().
//
//      5.  Easy cross-conversion.
//
//          Examples:
//
//              Initialize a URL from a string without suffix
//
//                  nsFileURL fileURL("file:///Development/MPW/MPW%20Shell");
//
//              Initialize a Unix path from a URL
//
//                  nsFilePath filePath(fileURL);
//
//              Initialize a native file spec from a URL
//
//                  nsFileSpec fileSpec(fileURL);
//
//              Make the spec unique (this one has no suffix).
//
//                  fileSpec.MakeUnique();
//
//              Assign the spec to a URL
//
//                  fileURL = fileSpec;
//
//              Assign a unix path using a string with a suffix.
//
//                  filePath = "/Development/MPW/SysErrs.err";
//
//              Assign to a file spec using a unix path.
//
//                  fileSpec = filePath;
//
//              Make this unique (this one has a suffix).
//
//                  fileSpec.MakeUnique();
//
//      6.  Fixes a bug that have been there for a long time, and
//          is inevitable if you use NSPR alone, where files are described as paths.
//
//          The problem affects platforms (Macintosh) in which a path does not fully
//          specify a file, because two volumes can have the same name.  This
//          is solved by holding a "private" native file spec inside the
//          nsFilePath and nsFileURL classes, which is used when appropriate.
//
//      Not yet done:
//
//          Equality operators... much more.
//
//========================================================================================

#ifndef _FILESPEC_H_
#define _FILESPEC_H_

#include "nscore.h"
#include "nsError.h"
#include "nsString.h"

//========================================================================================
//                          Compiler-specific macros, as needed
//========================================================================================
#if !defined(NS_USING_NAMESPACE) && (defined(__MWERKS__) || defined(XP_PC))
#define NS_USING_NAMESPACE
#endif

#ifdef NS_USING_NAMESPACE

#define NS_NAMESPACE_PROTOTYPE
#define NS_NAMESPACE namespace
#define NS_NAMESPACE_END
#define NS_EXPLICIT explicit
#else

#define NS_NAMESPACE_PROTOTYPE static
#define NS_NAMESPACE struct
#define NS_NAMESPACE_END ;
#define NS_EXPLICIT

#endif
//=========================== End Compiler-specific macros ===============================

#ifdef XP_MAC
#include <Files.h>
#elif defined(XP_UNIX) || defined (XP_OS2)
#include <dirent.h>
#elif defined(XP_PC)
#include "prio.h"
#endif

//========================================================================================
// Here are the allowable ways to describe a file.
//========================================================================================

class nsFileSpec;             // Preferred.  For i/o use nsInputFileStream, nsOutputFileStream
class nsFilePath;             // This can be passed to NSPR file I/O routines, if you must.
class nsFileURL;
class nsPersistentFileDescriptor; // Used for storage across program launches.

#define kFileURLPrefix "file://"
#define kFileURLPrefixLength (7)

class nsOutputStream;
class nsInputStream;
class nsIOutputStream;
class nsIInputStream;
class nsOutputFileStream;
class nsInputFileStream;
class nsOutputConsoleStream;
class nsString;

//========================================================================================
// Conversion of native file errors to nsresult values. These are really only for use
// in the file module, clients of this interface shouldn't really need them.
// Error results returned from this interface have, in the low-order 16 bits,
// native errors that are masked to 16 bits.  Assumption: a native error of 0 is success
// on all platforms. Note the way we define this using an inline function.  This
// avoids multiple evaluation if people go NS_FILE_RESULT(function_call()).
#define NS_FILE_RESULT(x) ns_file_convert_result((PRInt32)x)
nsresult ns_file_convert_result(PRInt32 nativeErr);
#define NS_FILE_FAILURE NS_FILE_RESULT(-1)

//========================================================================================
class NS_BASE nsAutoCString
//
// This should be in nsString.h, but the owner would not reply to my proposal.  After four
// weeks, I decided to put it in here.
//
// This is a quiet little class that acts as a sort of autoptr for
// a const char*.  If you used to call nsString::ToNewCString(), just
// to pass the result a parameter list, it was a nuisance having to
// call delete [] on the result after the call.  Now you can say
//     nsString myStr;
//     ...
//     f(nsAutoCString(myStr));
// where f is declared as void f(const char*);  This call will
// make a temporary char* pointer on the stack and delete[] it
// when the function returns.
//========================================================================================
{
public:
    NS_EXPLICIT                  nsAutoCString(const nsString& other) : mCString(other.ToNewCString()) {}
    virtual                      ~nsAutoCString();    
                                 operator const char*() const { return mCString; }

                                 // operator const char*() { return mCString; }
                                 //  don't need this, since |operator const char*() const| can
                                 //  serve for both |const| and non-|const| callers
protected:
                                 const char* mCString;
}; // class nsAutoCString

//========================================================================================
class NS_BASE nsSimpleCharString
//  An envelope for char*: reference counted. Used internally by all the nsFileSpec
//  classes below.
//========================================================================================
{
public:
                                 nsSimpleCharString();
                                 nsSimpleCharString(const char*);
                                 nsSimpleCharString(const nsString&);
                                 nsSimpleCharString(const nsSimpleCharString&);
                                 nsSimpleCharString(const char* inData, PRUint32 inLength);
                                 
                                 ~nsSimpleCharString();
                                 
    void                         operator = (const char*);
    void                         operator = (const nsString&);
    void                         operator = (const nsSimpleCharString&);
                                 
                                 operator const char*() const { return mData ? mData->mString : 0; }
                                 operator char* ()
                                 {
                                     ReallocData(Length()); // requires detaching if shared...
                                     return mData ? mData->mString : 0;
                                 }
    PRBool                       operator == (const char*);
    PRBool                       operator == (const nsString&);
    PRBool                       operator == (const nsSimpleCharString&);

    void                         operator += (const char* inString);
    nsSimpleCharString           operator + (const char* inString) const;
    
    char                         operator [](int i) const { return mData ? mData->mString[i] : 0; }
    char&                        operator [](int i)
                                 {
                                     if (i >= (int)Length())
                                         ReallocData((PRUint32)i + 1);
                                     return mData->mString[i]; // caveat appelator
                                 }
    char&                        operator [](unsigned int i) { return (*this)[(int)i]; }
    
    void                         Catenate(const char* inString1, const char* inString2);
   
    void                         SetToEmpty(); 
    PRBool                       IsEmpty() const { return Length() == 0; }
    
    PRUint32                     Length() const { return mData ? mData->mLength : 0; }
    void                         SetLength(PRUint32 inLength) { ReallocData(inLength); }
    void                         CopyFrom(const char* inData, PRUint32 inLength);
    void                         LeafReplace(char inSeparator, const char* inLeafName);
    char*                        GetLeaf(char inSeparator) const; // use PR_Free()
    void                         Unescape();

protected:

    void                         AddRefData();
    void                         ReleaseData();
    void                         ReallocData(PRUint32 inLength);

// DATA
protected:
    struct Data {
        int         mRefCount;
        PRUint32    mLength;
        char        mString[1];
        };
    Data*                        mData;
}; // class nsSimpleCharString

//========================================================================================
class NS_BASE nsFileSpec
//    This is whatever each platform really prefers to describe files as.  Declared first
//  because the other two types have an embedded nsFileSpec object.
//========================================================================================
{
    public:
                                nsFileSpec();
                                
                                // These two meathods take *native* file paths.
        NS_EXPLICIT             nsFileSpec(const char* inString, PRBool inCreateDirs = PR_FALSE);
        NS_EXPLICIT             nsFileSpec(const nsString& inString, PRBool inCreateDirs = PR_FALSE);
                                
        
        NS_EXPLICIT             nsFileSpec(const nsFilePath& inPath);
        NS_EXPLICIT             nsFileSpec(const nsFileURL& inURL);
        NS_EXPLICIT             nsFileSpec(const nsPersistentFileDescriptor& inURL);
                                nsFileSpec(const nsFileSpec& inPath);
        virtual                 ~nsFileSpec();

                                // These two operands take *native* file paths.
        void                    operator = (const char* inPath);
        void                    operator = (const nsString& inPath)
                                {
                                    const nsAutoCString path(inPath);
                                    *this = path;
                                }

        void                    operator = (const nsFilePath& inPath);
        void                    operator = (const nsFileURL& inURL);
        void                    operator = (const nsFileSpec& inOther);
        void                    operator = (const nsPersistentFileDescriptor& inOther);

        PRBool                  operator ==(const nsFileSpec& inOther) const;
        PRBool                  operator !=(const nsFileSpec& inOther) const;

                                operator const char* () const { return GetCString(); }
                                    // Same as GetCString (please read the comments).
       const char*              GetCString() const;
                                    // This is the only conversion to const char*
                                    // that is provided, and it allows the
                                    // path to be "passed" to legacy code.  This practice
                                    // is VERY EVIL and should only be used to support legacy
                                    // code.  Using it guarantees bugs on Macintosh.
                                    // The path is cached and freed by the nsFileSpec destructor
                                    // so do not delete (or free) it.

#ifdef XP_MAC
        // For Macintosh people, this is meant to be useful in its own right as a C++ version
        // of the FSSpec struct.        
                                nsFileSpec(
                                    short vRefNum,
                                    long parID,
                                    ConstStr255Param name);
                                nsFileSpec(const FSSpec& inSpec)
                                    : mSpec(inSpec), mError(NS_OK) {}
        void                    operator = (const FSSpec& inSpec)
                                    { mSpec = inSpec; mError = NS_OK; }

                                operator FSSpec* () { return &mSpec; }
                                operator const FSSpec* const () { return &mSpec; }
                                operator  FSSpec& () { return mSpec; }
                                operator const FSSpec& () const { return mSpec; }
                                
        const FSSpec&           GetFSSpec() const { return mSpec; }
        FSSpec&                 GetFSSpec() { return mSpec; }
        ConstFSSpecPtr          GetFSSpecPtr() const { return &mSpec; }
        FSSpecPtr               GetFSSpecPtr() { return &mSpec; }
        void                    MakeAliasSafe();
                                    // Called for the spec of an alias.  Copies the alias to
                                    // a secret temp directory and modifies the spec to point
                                    // to it.  Sets mError.
        void                    ResolveAlias(PRBool& wasAliased);
                                    // Called for the spec of an alias.  Modifies the spec to
                                    // point to the original.  Sets mError.
        void                    MakeUnique(ConstStr255Param inSuggestedLeafName);
        StringPtr               GetLeafPName() { return mSpec.name; }
        ConstStr255Param        GetLeafPName() const { return mSpec.name; }

        OSErr                   GetCatInfo(CInfoPBRec& outInfo) const;

#endif // end of Macintosh utility methods.

        PRBool                  Valid() const { return NS_SUCCEEDED(Error()); }
        nsresult                Error() const
                                {
                                    #ifndef XP_MAC 
                                    if (mPath.IsEmpty() && NS_SUCCEEDED(mError)) 
                                        ((nsFileSpec*)this)->mError = NS_FILE_FAILURE; 
                                    #endif 
                                    return mError;
                                }
        PRBool                  Failed() const { return (PRBool)NS_FAILED(Error()); }


        friend                  NS_BASE nsOutputStream& operator << (
                                    nsOutputStream& s,
                                    const nsFileSpec& spec); // THIS IS FOR DEBUGGING ONLY.
                                        // see PersistentFileDescriptor for the real deal.


        //--------------------------------------------------
        // Queries and path algebra.  These do not modify the disk.
        //--------------------------------------------------

        char*                   GetLeafName() const; // Allocated.  Use delete [].
        void                    SetLeafName(const char* inLeafName);
                                    // inLeafName can be a relative path, so this allows
                                    // one kind of concatenation of "paths".
        void                    SetLeafName(const nsString& inLeafName)
                                {
                                    const nsAutoCString leafName(inLeafName);
                                    SetLeafName(leafName);
                                }
        void                    GetParent(nsFileSpec& outSpec) const;
                                    // Return the filespec of the parent directory. Used
                                    // in conjunction with GetLeafName(), this lets you
                                    // parse a path into a list of node names.  Beware,
                                    // however, that the top node is still not a name,
                                    // but a spec.  Volumes on Macintosh can have identical
                                    // names.  Perhaps could be used for an operator --() ?

        typedef PRUint32        TimeStamp; // ie nsFileSpec::TimeStamp.  This is 32 bits now,
                                           // but might change, eg, to a 64-bit class.  So use the
                                           // typedef, and use a streaming operator to convert
                                           // to a string, so that your code won't break.  It's
                                           // none of your business what the number means.  Don't
                                           // rely on the implementation.
        void                    GetModDate(TimeStamp& outStamp) const;
                                           // This will return different values on different
                                           // platforms, even for the same file (eg, on a server).
                                           // But if the platform is constant, it will increase after
                                           // every file modification.
        PRBool                  ModDateChanged(const TimeStamp& oldStamp) const
                                {
                                    TimeStamp newStamp;
                                    GetModDate(newStamp);
                                    return newStamp != oldStamp;
                                }
        
        PRUint32                GetFileSize() const;
        PRUint32                GetDiskSpaceAvailable() const;
        
        nsFileSpec              operator + (const char* inRelativePath) const;
        nsFileSpec              operator + (const nsString& inRelativePath) const
                                {
                                    const nsAutoCString 
                                      relativePath(inRelativePath);
                                    return *this + relativePath;
                                }
        void                    operator += (const char* inRelativePath);
                                    // Concatenate the relative path to this directory.
                                    // Used for constructing the filespec of a descendant.
                                    // This must be a directory for this to work.  This differs
                                    // from SetLeafName(), since the latter will work
                                    // starting with a sibling of the directory and throws
                                    // away its leaf information, whereas this one assumes
                                    // this is a directory, and the relative path starts
                                    // "below" this.
        void                    operator += (const nsString& inRelativePath)
                                {
                                    const nsAutoCString relativePath(inRelativePath);
                                    *this += relativePath;
                                }

        void                    MakeUnique();
        void                    MakeUnique(const char* inSuggestedLeafName);
        void                    MakeUnique(const nsString& inSuggestedLeafName)
                                {
                                    const nsAutoCString suggestedLeafName(inSuggestedLeafName);
                                    MakeUnique(suggestedLeafName);
                                }
    
        PRBool                  IsDirectory() const;
                                    // More stringent than Exists()
        PRBool                  IsFile() const;
                                    // More stringent than Exists()
        PRBool                  Exists() const;

        //--------------------------------------------------
        // Creation and deletion of objects.  These can modify the disk.
        //--------------------------------------------------

        void                    CreateDirectory(int mode = 0700 /* for unix */);
        void                    CreateDir(int mode = 0700) { CreateDirectory(mode); }
                                   // workaround for yet another VC++ bug with long identifiers.
        void                    Delete(PRBool inRecursive) const;
        
        nsresult                Rename(const char* inNewName); // not const: gets updated
        nsresult                Rename(const nsString& inNewName)
                                {
                                    const nsAutoCString newName(inNewName);
                                    return Rename(newName);
                                }
        nsresult                Copy(const nsFileSpec& inNewParentDirectory) const;
        nsresult                Move(const nsFileSpec& inNewParentDirectory);
        nsresult                Execute(const char* args) const;
        nsresult                Execute(const nsString& args) const
                                {
                                    const nsAutoCString argsString(args);
                                    return Execute(argsString);
                                }
        //--------------------------------------------------
        // Data
        //--------------------------------------------------

    protected:
                                friend class nsFilePath;
                                friend class nsFileURL;
                                friend class nsDirectoryIterator;
#ifdef XP_MAC
        FSSpec                  mSpec;
#endif
        nsSimpleCharString      mPath;
        nsresult                mError;
}; // class nsFileSpec

// FOR HISTORICAL REASONS:

typedef nsFileSpec nsNativeFileSpec;

//========================================================================================
class NS_BASE nsFileURL
//    This is an escaped string that looks like "file:///foo/bar/mumble%20fish".  Since URLs
//    are the standard way of doing things in mozilla, this allows a string constructor,
//    which just stashes the string with no conversion.
//========================================================================================
{
    public:
                                nsFileURL(const nsFileURL& inURL);
        NS_EXPLICIT             nsFileURL(const char* inString, PRBool inCreateDirs = PR_FALSE);
        NS_EXPLICIT             nsFileURL(const nsString& inString, PRBool inCreateDirs = PR_FALSE);
        NS_EXPLICIT             nsFileURL(const nsFilePath& inPath);
        NS_EXPLICIT             nsFileURL(const nsFileSpec& inPath);
        virtual                 ~nsFileURL();

//        nsString             GetString() const { return mPath; }
                                    // may be needed for implementation reasons,
                                    // but should not provide a conversion constructor.

        void                    operator = (const nsFileURL& inURL);
        void                    operator = (const char* inString);
        void                    operator = (const nsString& inString)
                                {
                                    const nsAutoCString string(inString);
                                    *this = string;
                                }
        void                    operator = (const nsFilePath& inOther);
        void                    operator = (const nsFileSpec& inOther);

        void                    operator +=(const char* inRelativeUnixPath);
        nsFileURL               operator +(const char* inRelativeUnixPath) const;
                                operator const char* () const { return (const char*)mURL; } // deprecated.
        const char*             GetAsString() const { return (const char*)mURL; }

        friend                  NS_BASE nsOutputStream& operator << (
                                     nsOutputStream& s, const nsFileURL& spec);

#ifdef XP_MAC
                                // Accessor to allow quick assignment to a mFileSpec
        const nsFileSpec&       GetFileSpec() const { return mFileSpec; }
#endif

    protected:
                                friend class nsFilePath; // to allow construction of nsFilePath
        nsSimpleCharString      mURL;

#ifdef XP_MAC
        // Since the path on the macintosh does not uniquely specify a file (volumes
        // can have the same name), stash the secret nsFileSpec, too.
        nsFileSpec              mFileSpec;
#endif
}; // class nsFileURL

//========================================================================================
class NS_BASE nsFilePath
//    This is a string that looks like "/foo/bar/mumble fish".  Same as nsFileURL, but
//    without the "file:// prefix", and NOT %20 ENCODED! Strings passed in must be
//    valid unix-style paths in this format.
//========================================================================================
{
    public:
                                nsFilePath(const nsFilePath& inPath);
        NS_EXPLICIT             nsFilePath(const char* inString, PRBool inCreateDirs = PR_FALSE);
        NS_EXPLICIT             nsFilePath(const nsString& inString, PRBool inCreateDirs = PR_FALSE);
        NS_EXPLICIT             nsFilePath(const nsFileURL& inURL);
        NS_EXPLICIT             nsFilePath(const nsFileSpec& inPath);
        virtual                 ~nsFilePath();

                                
                                operator const char* () const { return mPath; }
                                    // This will return a UNIX string.  If you
                                    // need a string that can be passed into
                                    // NSPR, take a look at the nsprPath class.

        void                    operator = (const nsFilePath& inPath);
        void                    operator = (const char* inString);
        void                    operator = (const nsString& inString)
                                {
                                    const nsAutoCString string(inString);
                                    *this = string;
                                }
        void                    operator = (const nsFileURL& inURL);
        void                    operator = (const nsFileSpec& inOther);

        void                    operator +=(const char* inRelativeUnixPath);
        nsFilePath              operator +(const char* inRelativeUnixPath) const;

#ifdef XP_MAC
    public:
                                // Accessor to allow quick assignment to a mFileSpec
        const nsFileSpec&       GetFileSpec() const { return mFileSpec; }
#endif

    private:

        nsSimpleCharString       mPath;
#ifdef XP_MAC
        // Since the path on the macintosh does not uniquely specify a file (volumes
        // can have the same name), stash the secret nsFileSpec, too.
        nsFileSpec               mFileSpec;
#endif
}; // class nsFilePath

//========================================================================================
class NS_BASE nsPersistentFileDescriptor
// To save information about a file's location in another file, initialize
// one of these from your nsFileSpec, and then write this out to your output stream.
// To retrieve the info, create one of these, read its value from an input stream.
// and then make an nsFileSpec from it.
//========================================================================================
{
    public:
                                nsPersistentFileDescriptor() {}
                                    // For use prior to reading in from a stream
                                nsPersistentFileDescriptor(const nsPersistentFileDescriptor& inPath);
        virtual                 ~nsPersistentFileDescriptor();
        void					operator = (const nsPersistentFileDescriptor& inPath);
        
        // Conversions
        NS_EXPLICIT             nsPersistentFileDescriptor(const nsFileSpec& inPath);
        void					operator = (const nsFileSpec& inPath);
        
    	nsresult                Read(nsIInputStream* aStream);
    	nsresult                Write(nsIOutputStream* aStream);
    	    // writes the data to a file
    	friend NS_BASE nsInputStream& operator >> (nsInputStream&, nsPersistentFileDescriptor&);
    		// reads the data from a file
    	friend NS_BASE nsOutputStream& operator << (nsOutputStream&, const nsPersistentFileDescriptor&);
    	    // writes the data to a file
        friend class nsFileSpec;

    private:

        void                    GetData(nsSimpleCharString& outData, PRInt32& outSize) const;
        void                    SetData(const nsSimpleCharString& inData, PRInt32 inSize);

    protected:

        nsSimpleCharString      mDescriptorString;

}; // class nsPersistentFileDescriptor

//========================================================================================
class NS_BASE nsDirectoryIterator
//  Example:
//
//       nsFileSpec parentDir(...); // directory over whose children we shall iterate
//       for (nsDirectoryIterator i(parentDir); i.Exists(); i++)
//       {
//              // do something with i.Spec()
//       }
//
//  or:
//
//       for (nsDirectoryIterator i(parentDir, PR_FALSE); i.Exists(); i--)
//       {
//              // do something with i.Spec()
//       }
//
//  Currently, the only platform on which backwards iteration actually goes backwards
//  is Macintosh.  On other platforms, both styles will work, but will go forwards.
//========================================================================================
{
	public:
	                            nsDirectoryIterator(
	                            	const nsFileSpec& parent,
	                            	int iterateDirection = +1);
#ifndef XP_MAC
	// Macintosh currently doesn't allocate, so needn't clean up.
	    virtual                 ~nsDirectoryIterator();
#endif
	    PRBool                  Exists() const { return mExists; }
	    nsDirectoryIterator&    operator ++(); // moves to the next item, if any.
	    nsDirectoryIterator&    operator ++(int) { return ++(*this); } // post-increment.
	    nsDirectoryIterator&    operator --(); // moves to the previous item, if any.
	    nsDirectoryIterator&    operator --(int) { return --(*this); } // post-decrement.
	                            operator nsFileSpec&() { return mCurrent; }
	    
	    nsFileSpec&             Spec() { return mCurrent; }
	     
	private:
	    nsFileSpec              mCurrent;
	    PRBool                  mExists;
	      
#if defined(XP_UNIX)
        DIR*                    mDir;
#elif defined(XP_PC)
        PRDir*                  mDir; // XXX why not use PRDir for Unix too?
#elif defined(XP_MAC)
        OSErr                   SetToIndex();
	    short                   mIndex;
	    short                   mMaxIndex;
#endif
}; // class nsDirectoryIterator

//========================================================================================
class NS_BASE nsprPath
//  This class will allow you to pass anyone of the nsFile* classes directly into NSPR
//  without the need to worry about whether you have the right kind of filepath or not.
//  It will also take care of cleaning up any allocated memory. 
//========================================================================================
{
public:
    NS_EXPLICIT                  nsprPath(const nsFileSpec& other)  : mFilePath(other) { modifiedNSPRPath = nsnull; }
    NS_EXPLICIT                  nsprPath(const nsFileURL&  other)  : mFilePath(other) { modifiedNSPRPath = nsnull; }
    NS_EXPLICIT                  nsprPath(const nsFilePath& other)  : mFilePath(other) { modifiedNSPRPath = nsnull; }
    
    virtual                      ~nsprPath();    
 
                                 operator const char*();

private:
                                 nsFilePath mFilePath;
                                 char* modifiedNSPRPath;
                             
                                 
}; // class nsprPath

#endif //  _FILESPEC_H_
