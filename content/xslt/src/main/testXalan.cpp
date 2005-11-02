/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Axel Hecht <axel@pike.org>
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

#include "txStandaloneXSLTProcessor.h"
#include "nsXPCOM.h"
#include <fstream.h>
#include "nsDoubleHashtable.h"
#include "nsIComponentManager.h"
#include "nsILocalFile.h"
#include "nsISimpleEnumerator.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "prenv.h"
#include "prsystem.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"

#ifdef NS_TRACE_MALLOC
#include "nsTraceMalloc.h"
#endif
#ifdef MOZ_JPROF
#include "jprof.h"
#endif

/**
 * Prints the command line help screen to the console
 */
void printHelp()
{
  cerr << "testXalan [-o output-file] [category]*" << endl << endl;
  cerr << "Options:";
  cerr << endl << endl;
  cerr << "\t-o  specify output file (default: write to stdout)";
  cerr << endl << endl;
  cerr << "\t Specify XALAN_DIR in your environement." << endl;
  cerr << endl;
}

/**
 * Helper class to report success and failure to RDF
 */
class txRDFOut
{
public:
    explicit txRDFOut(ostream* aOut)
        : mOut(aOut), mSuccess(0), mFail(0), mParent(nsnull)
    {
    }
    explicit txRDFOut(const nsACString& aName, txRDFOut* aParent)
        : mName(aName), mOut(aParent->mOut), mSuccess(0), mFail(0),
          mParent(aParent)
    {
    }
    ~txRDFOut()
    {
        *mOut << "  <RDF:Description about=\"urn:x-buster:conf" <<
            mName.get() <<
            "\">\n" <<
            "    <NC:orig_succCount NC:parseType=\"Integer\">" <<
            mSuccess <<
            "</NC:orig_succCount>\n" <<
            "    <NC:orig_failCount NC:parseType=\"Integer\">" <<
            mFail <<
            "</NC:orig_failCount>\n" <<
            "  </RDF:Description>" << endl;
    }

    void feed(const nsACString& aTest, PRBool aSuccess)
    {
        *mOut << "  <RDF:Description about=\"urn:x-buster:" <<
            PromiseFlatCString(aTest).get() <<
            "\"\n                   NC:orig_succ=\"";
        if (aSuccess) {
            *mOut << "yes";
            succeeded();
        }
        else {
            *mOut << "no";
            failed();
        }
        *mOut << "\" />\n";
    }

    void succeeded()
    {
        if (mParent)
            mParent->succeeded();
        ++mSuccess;
    }
    void failed()
    {
        if (mParent)
            mParent->failed();
        ++mFail;
    }
private:
    nsCAutoString mName;
    ostream* mOut;
    PRUint32 mSuccess, mFail;
    txRDFOut* mParent;
};

static void
readToString(istream& aIstream, nsACString& aString)
{
    static char buffer[1024];
    int read = 0;
    do {
        aIstream.read(buffer, 1024);
        read = aIstream.gcount();
        aString.Append(Substring(buffer, buffer + read));
    } while (!aIstream.eof());
}

/**
 * Get the XALAN_DIR environment variable and return a nsIFile for
 * the conf and the conf-gold subdirectory. Create a TEMP file, too.
 * Return an error if either does not exist.
 */
static nsresult
setupXalan(const char* aPath, nsIFile** aConf, nsIFile** aConfGold,
           nsIFile** aTemp)
{
    nsresult rv;
    nsCOMPtr<nsILocalFile> conf(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID,
                                                  &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr <nsIFile> tmpFile;
    rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmpFile));
    NS_ENSURE_SUCCESS(rv, rv);
    tmpFile->Append(NS_LITERAL_STRING("xalan.out"));
    rv = tmpFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    rv = conf->InitWithNativePath(nsDependentCString(aPath));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIFile> gold;
    rv = conf->Clone(getter_AddRefs(gold));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = conf->Append(NS_LITERAL_STRING("conf"));
    NS_ENSURE_SUCCESS(rv, rv);
    PRBool isDir;
    rv = conf->IsDirectory(&isDir);
    if (NS_FAILED(rv) || !isDir) {
        return NS_ERROR_FILE_NOT_DIRECTORY;
    }
    rv = gold->Append(NS_LITERAL_STRING("conf-gold"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = gold->IsDirectory(&isDir);
    if (NS_FAILED(rv) || !isDir || !conf || !gold) {
        return NS_ERROR_FILE_NOT_DIRECTORY;
    }
    // got conf and conf-gold subdirectories
    *aConf = conf;
    NS_ADDREF(*aConf);
    *aConfGold = gold;
    NS_ADDREF(*aConfGold);
    *aTemp = tmpFile;
    NS_ADDREF(*aTemp);
    return NS_OK;
}

/**
 * Run a category of Xalan tests
 */
void runCategory(nsIFile* aConfCat, nsIFile* aGoldCat, nsIFile* aRefTmp,
                 txRDFOut* aOut)
{
    nsresult rv;
    //clone the nsIFiles, so that we can return easily
    nsCOMPtr<nsIFile> conf, gold;
    aConfCat->Clone(getter_AddRefs(conf));
    aGoldCat->Clone(getter_AddRefs(gold));
    nsCAutoString catName, refTmp;
    conf->GetNativeLeafName(catName);
    aRefTmp->GetNativePath(refTmp);
    txRDFOut results(catName, aOut);
    nsCOMPtr<nsISimpleEnumerator> tests;
    rv = conf->GetDirectoryEntries(getter_AddRefs(tests));
    if (NS_FAILED(rv))
        return;
    PRBool hasMore, isFile;
    nsCAutoString leaf;
    NS_NAMED_LITERAL_CSTRING(xsl, ".xsl");
    while (NS_SUCCEEDED(tests->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr<nsILocalFile> test;
        tests->GetNext(getter_AddRefs(test));
        test->GetNativeLeafName(leaf);
        if (xsl.Equals(Substring(leaf, leaf.Length()-4, 4))) {
            // we have a stylesheet, let's look for source and reference
            nsAFlatCString::char_iterator start, ext;
            leaf.BeginWriting(start);
            leaf.EndWriting(ext);
            ext -= 2;
            // overwrite extension with .xml
            *ext = 'm'; // this one was easy
            nsCOMPtr<nsIFile> source;
            conf->Clone(getter_AddRefs(source));
            rv = source->AppendNative(leaf);
            if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(source->IsFile(&isFile)) &&
                isFile) {
                nsCOMPtr<nsIFile> reference;
                gold->Clone(getter_AddRefs(reference));
                // overwrite extension with .out
                --ext;
                nsCharTraits<char>::copy(ext, "out", 3);
                rv = reference->AppendNative(leaf);
                if (NS_SUCCEEDED(rv) &&
                    NS_SUCCEEDED(reference->IsFile(&isFile)) &&
                    isFile) {
                    nsCAutoString src, style, refPath;
                    test->GetNativePath(style);
                    source->GetNativePath(src);
                    reference->GetNativePath(refPath);
                    if (PR_GetDirectorySeparator() =='\\') {
                        src.ReplaceChar('\\','/');
                        style.ReplaceChar('\\','/');
                        refPath.ReplaceChar('\\','/');
                    }
                    SimpleErrorObserver obs;
                    txStandaloneXSLTProcessor proc;
                    fstream result(refTmp.get(),
                                   ios::in | ios::out | ios::trunc);
                    rv = proc.transform(src, style, result, obs);
                    PRBool success = PR_FALSE;
                    if (NS_SUCCEEDED(rv)) {
                        result.flush();
                        PRInt64 resultSize, refSize;
                        aRefTmp->GetFileSize(&resultSize);
                        reference->GetFileSize(&refSize);
                        result.seekg(0);
                        int toread = (int)resultSize;
                        nsCString resContent, refContent;
                        resContent.SetCapacity(toread);
                        readToString(result, resContent);
                        result.close();
                        ifstream refStream(refPath.get());
                        toread = (int)refSize;
                        refContent.SetCapacity(toread);
                        readToString(refStream, refContent);
                        refStream.close();
                        success = resContent.Equals(refContent);
                    }
                    ext--;
                    results.feed(Substring(start, ext), success);
                }
            }
        }
    }
}
/**
 * The Xalan testcases app
 */
int main(int argc, char** argv)
{
#ifdef NS_TRACE_MALLOC
    NS_TraceMallocStartupArgs(argc, argv);
#endif
#ifdef MOZ_JPROF
    setupProfilingStuff();
#endif
    char* xalan = PR_GetEnv("XALAN_DIR");
    if (!xalan) {
        printHelp();
        return 1;
    }
    nsresult rv;
    rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> conf, gold, resFile;
    rv = setupXalan(xalan, getter_AddRefs(conf), getter_AddRefs(gold),
                    getter_AddRefs(resFile));
    if (NS_FAILED(rv)) {
        NS_ShutdownXPCOM(nsnull);
        printHelp();
        return -1;
    }

    //-- handle output stream
    ostream* resultOutput = &cout;
    ofstream resultFileStream;

    int argn = 1;
    // skip -- gnu style options
    while (argn < argc) {
        nsDependentCString opt(argv[argn]);
        if (!Substring(opt, 0, 2).EqualsLiteral("--")) {
            break;
        }
        ++argn;
    }
    if (argn < argc) {
        nsDependentCString opt(argv[argn]);
        if (Substring(opt, 0, 2).EqualsLiteral("-o")) {
            if (opt.Length() > 2) {
                const nsAFlatCString& fname = 
                    PromiseFlatCString(Substring(opt, 2, opt.Length()-2));
                resultFileStream.open(fname.get(), ios::out);
            }
            else {
                ++argn;
                if (argn < argc) {
                    resultFileStream.open(argv[argn], ios::out);
                }
            }
            if (!resultFileStream) {
                cerr << "error opening output file" << endl;
                PRBool exists;
                if (NS_SUCCEEDED(resFile->Exists(&exists)) && exists)
                    resFile->Remove(PR_FALSE);
                NS_ShutdownXPCOM(nsnull);
                return -1;
            }
            ++argn;
            resultOutput = &resultFileStream;
        }
    }

    if (!txXSLTProcessor::init()) {
        PRBool exists;
        if (NS_SUCCEEDED(resFile->Exists(&exists)) && exists)
            resFile->Remove(PR_FALSE);
        NS_ShutdownXPCOM(nsnull);
        return 1;
    }

    *resultOutput << "<?xml version=\"1.0\"?>\n" << 
        "<RDF:RDF xmlns:NC=\"http://home.netscape.com/NC-rdf#\"\n" <<
        "         xmlns:RDF=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">" << endl;

    txRDFOut* rdfOut = new txRDFOut(resultOutput);
    nsCOMPtr<nsIFile> tempFile;
    if (argn < argc) {
        // categories are specified
        while (argn < argc) {
            nsDependentCString cat(argv[argn++]);
            rv = conf->AppendNative(cat);
            if (NS_SUCCEEDED(rv)) {
                rv = gold->AppendNative(cat);
                if (NS_SUCCEEDED(rv)) {
                    runCategory(conf, gold, resFile, rdfOut);
                    rv = gold->GetParent(getter_AddRefs(tempFile));
                    NS_ASSERTION(NS_SUCCEEDED(rv), "can't go back?");
                    gold = tempFile;
                }
                rv = conf->GetParent(getter_AddRefs(tempFile));
                NS_ASSERTION(NS_SUCCEEDED(rv), "can't go back?");
                conf = tempFile;
            }
        }
    }
    else {
        // no category specified, do everything
        nsCOMPtr<nsISimpleEnumerator> cats;
        rv = conf->GetDirectoryEntries(getter_AddRefs(cats));
        PRBool hasMore, isDir;
        nsCAutoString leaf;
        while (NS_SUCCEEDED(cats->HasMoreElements(&hasMore)) && hasMore) {
            nsCOMPtr<nsILocalFile> cat;
            cats->GetNext(getter_AddRefs(cat));
            rv = cat->IsDirectory(&isDir);
            if (NS_SUCCEEDED(rv) && isDir) {
                rv = cat->GetNativeLeafName(leaf);
                if (NS_SUCCEEDED(rv) && 
                    !leaf.EqualsLiteral("CVS")) {
                    rv = gold->AppendNative(leaf);
                    if (NS_SUCCEEDED(rv)) {
                        runCategory(cat, gold, resFile, rdfOut);
                        rv = gold->GetParent(getter_AddRefs(tempFile));
                        gold = tempFile;
                    }
                }
            }
        }
    }
    delete rdfOut;
    rdfOut = nsnull;
    *resultOutput << "</RDF:RDF>" << endl;
    PRBool exists;
    if (NS_SUCCEEDED(resFile->Exists(&exists)) && exists)
        resFile->Remove(PR_FALSE);
    resultFileStream.close();
    txXSLTProcessor::shutdown();
    rv = NS_ShutdownXPCOM(nsnull);
#ifdef NS_TRACE_MALLOC
    NS_TraceMallocShutdown();
#endif
    NS_ENSURE_SUCCESS(rv, rv);
    return 0;
}
