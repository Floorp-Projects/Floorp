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
 * The Original Code is Mozilla Spellchecker Component.
 *
 * The Initial Developer of the Original Code is
 * David Einstein.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): David Einstein <Deinst@world.std.com>
 *                 Kevin Hendricks <kevin.hendricks@sympatico.ca>
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
 *  This spellchecker is based on the MySpell spellchecker made for Open Office
 *  by Kevin Hendricks.  Although the algorithms and code, have changed 
 *  slightly, the architecture is still the same. The Mozilla implementation
 *  is designed to be compatible with the Open Office dictionaries.
 *  Please do not make changes to the affix or dictionary file formats 
 *  without attempting to coordinate with Kevin.  For more information 
 *  on the original MySpell see 
 *  http://whiteboard.openoffice.org/source/browse/whiteboard/lingucomponent/source/spellcheck/myspell/
 *
 *  A special thanks and credit goes to Geoff Kuenning
 * the creator of ispell.  MySpell's affix algorithms were
 * based on those of ispell which should be noted is
 * copyright Geoff Kuenning et.al. and now available
 * under a BSD style license. For more information on ispell
 * and affix compression in general, please see:
 * http://www.cs.ucla.edu/ficus-members/geoff/ispell.html
 * (the home page for ispell)
 *
 * ***** END LICENSE BLOCK ***** */
#include "myspAffixmgr.h"
#include "nsIFile.h"
#include "nsReadLine.h"
#include "nsReadableUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "plstr.h"
#include "nsNetUtil.h"
#include "nsICharsetConverterManager.h"
#include "nsUnicharUtilCIID.h"
#include "nsUnicharUtils.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);

static PRInt32 SplitString(nsACString &in,nsCString out[],PRInt32 size);
static void doubleReverseHack(nsACString &s);

myspAffixMgr::myspAffixMgr() :
  mReplaceTable(nsnull)
{
}


myspAffixMgr::~myspAffixMgr() 
{
  mPersonalDictionary = nsnull;
  delete[] mReplaceTable;
}

nsresult myspAffixMgr::GetPersonalDictionary(mozIPersonalDictionary * *aPersonalDictionary)
{
  *aPersonalDictionary = mPersonalDictionary;
  NS_IF_ADDREF(*aPersonalDictionary);
  return NS_OK;
}

nsresult myspAffixMgr::SetPersonalDictionary(mozIPersonalDictionary * aPersonalDictionary)
{
  mPersonalDictionary = aPersonalDictionary;
  return NS_OK;
}

nsresult 
myspAffixMgr::Load(const nsString& aDictionary)
{
  nsresult res=NS_OK;
  nsCOMPtr<nsIFile> dicFile;
  nsCOMPtr<nsIFile> affFile;
  PRBool fileExists;

  //get the directory
  res = NS_GetSpecialDirectory(NS_XPCOM_COMPONENT_DIR, getter_AddRefs(dicFile));
  if(NS_FAILED(res)) return res;
  if(!dicFile)return NS_ERROR_FAILURE;
  res = dicFile->Append(NS_LITERAL_STRING("myspell"));
  if(NS_FAILED(res)) return res;
  res = dicFile->Exists(&fileExists);
  if(NS_FAILED(res)) return res;
  if(!fileExists) return NS_ERROR_FAILURE; 
  res = dicFile->Clone(getter_AddRefs(affFile));
  if(NS_FAILED(res)) return res;
  if(!dicFile)return NS_ERROR_FAILURE;

  //get the affix file
  nsString affName=aDictionary;
  affName.AppendLiteral(".aff");
  res=affFile->Append(affName);
  if(NS_FAILED(res)) return res; 
  res = affFile->Exists(&fileExists);
  if(NS_FAILED(res)) return res;
  if(!fileExists) return NS_ERROR_FAILURE; 

  //get the dictionary file
  nsString dicName=aDictionary;
  dicName.AppendLiteral(".dic");
  res=dicFile->Append(dicName);
  if(NS_FAILED(res)) return res; 
  res = dicFile->Exists(&fileExists);
  if(NS_FAILED(res)) return res;
  if(!fileExists) return NS_ERROR_FAILURE; 
 
  // load the affixFile
  nsCOMPtr<nsIInputStream> affStream;
  res = NS_NewLocalFileInputStream(getter_AddRefs(affStream), affFile);
  if(NS_FAILED(res)) return res;
  if(!affStream)return NS_ERROR_FAILURE;
  res = parse_file(affStream);

  PRInt32 pos=aDictionary.FindChar('-');
  if(pos<1) pos = 2;  // FIXME should be min of 2 and aDictionary.Length()
  mLanguage = Substring(aDictionary,0,pos);

  // load the dictionary
  nsCOMPtr<nsIInputStream> dicStream;
  res = NS_NewLocalFileInputStream(getter_AddRefs(dicStream), dicFile);
  if(NS_FAILED(res)) return res;
  if(!dicStream)return NS_ERROR_FAILURE;
  res = LoadDictionary(dicStream);

  return res;
}


// read in aff file and build up prefix and suffix data structures
nsresult  myspAffixMgr::parse_file(nsIInputStream *strm)
{
  PRInt32 j;
  PRInt32 numents;
  nsLineBuffer *lineBuffer;
  nsresult rv = NS_InitLineBuffer(&lineBuffer);
  PRBool moreData=PR_TRUE;
  PRInt32 pos;
  nsCString cmds[5];
  mozAffixMod newMod;

  prefixes.clear();
  suffixes.clear();
  
  nsCOMPtr<nsICharsetConverterManager> ccm = do_GetService(kCharsetConverterManagerCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  numents = 0;      // number of affentry structures to parse
  char flag='\0';   // affix char identifier
  {  
    PRInt16 ff=0;
    char ft;


    // read in each line ignoring any that do not
    // start with PFX or SFX

    nsCAutoString line;
    while (moreData) {
      NS_ReadLine(strm,lineBuffer,line,&moreData);
      /* parse in the try string */
      if (Substring(line,0,3).Equals("TRY")) {
        pos = line.FindChar(' ');
        if(pos != -1){
          trystring = Substring(line,pos+1,line.Length()-pos-1);
        } 
      }

      /* parse in the name of the character set used by the .dict and .aff */
      if (Substring(line,0,3).Equals("SET")) {
        pos = line.FindChar(' ');
        if(pos != -1){
          mEncoding.Assign(Substring(line,pos+1,line.Length()-pos-1));
          mEncoding.CompressWhitespace(PR_TRUE,PR_TRUE);

          rv = ccm->GetUnicodeDecoder(mEncoding.get(), getter_AddRefs(mDecoder));
          NS_ENSURE_SUCCESS(rv, rv);
          rv = ccm->GetUnicodeEncoder(mEncoding.get(), getter_AddRefs(mEncoder));
          if (mEncoder && NS_SUCCEEDED(rv)) {
            mEncoder->SetOutputErrorBehavior(mEncoder->kOnError_Signal, nsnull, '?');
          }
          NS_ENSURE_SUCCESS(rv, rv);
        } 
      }

      /* parse in the typical fault correcting table */
      if (Substring(line,0,3).Equals("REP")) {
        PRInt32 numFields = SplitString(line, cmds, 3);

        if (numFields == 2)
          numents = atoi(cmds[1].get());

        mReplaceTable = new mozReplaceTable[numents];
        mReplaceTableLength = numents;

        PRInt32 i = 0;
        nsAutoString pattern, replacement;

        for (j = 0; (j < numents) && moreData; j++) {
          NS_ReadLine(strm,lineBuffer,line,&moreData);

          numFields = SplitString(line, cmds, 3);

          if(!cmds[0].Equals("REP")) { //consistency check
            NS_WARNING("REP line from .aff file is inconsitent");
            continue;
          }

          rv = DecodeString(cmds[1], pattern);
          NS_ENSURE_SUCCESS(rv, rv);
          rv = DecodeString(cmds[2], replacement);
          NS_ENSURE_SUCCESS(rv, rv);
          // Make sure the replacements are lower case.
          // We don't want to convert them for every lookup.
          ToLowerCase(pattern);
          ToLowerCase(replacement);
          mReplaceTable[i].pattern = pattern.get();
          mReplaceTable[i].replacement = replacement.get();
          
          i++;
        } 
      }

      // get the type of this affix: P - prefix, S - suffix
      ft = ' ';
      if (Substring(line,0,3).Equals("PFX")) ft = 'P';
      if (Substring(line,0,3).Equals("SFX")) ft = 'S';
      if (ft != ' ') {
        numents = 0;
        ff=0;
        // split line into pieces
        PRInt32 numFields=SplitString(line, cmds, 5);
        if(numFields > 1)flag=cmds[1].First();
        if((numFields > 2)&&(cmds[2].First()=='Y'))ff=XPRODUCT;
        if(numFields >3)numents = atoi(cmds[3].get());

        // now parse numents affentries for this affix
        for (j=0; (j < numents)&&moreData; j++) {
          NS_ReadLine(strm,lineBuffer,line,&moreData);
          PRInt32 numFields=SplitString(line,cmds,5);
          nsString tempStr;

          if((numFields < 5)||(cmds[1].First()!=flag)){ //consistency check
            NS_WARNING("PFX/SFX line from .aff file is inconsitent");
            continue;
          }
          if(cmds[3].Equals("0")){
            cmds[3].Truncate();
          }
          newMod.flags = ff;
          newMod.mID = flag;
          newMod.mTruncateLength=cmds[3].Length();

          if(ft == 'P'){
            if(cmds[2].Equals("0")){
              newMod.mAppend.Assign("");
              if(!cmds[4].Equals(".")){
                cmds[3].Append(cmds[4]);
              }
            }
            else{ // cmds[2] != 0
              newMod.mAppend.Assign( cmds[2]);
              if((cmds[2].Length()>cmds[4].Length())||!cmds[2].Equals(Substring(cmds[4],0,cmds[2].Length()))){
                NS_WARNING("PFX/SFX line from .aff file is inconsitent");
                continue;
              }
              cmds[3].Append(Substring(cmds[4],cmds[2].Length(),cmds[4].Length()-cmds[2].Length()));
            }
            prefixes.addMod(cmds[3].get(),&newMod);
          }
          else{ // suffix
            nsCString suffixTest;
            if(cmds[2].Equals("0")){
              newMod.mAppend.Assign("");
              if(!cmds[4].Equals(".")){
                suffixTest.Assign(cmds[4]);
                suffixTest.Append(cmds[3]);
              }
              else{
                suffixTest.Assign( cmds[3]);
              }
            }
            else{ // cmds[2] != 0
              newMod.mAppend.Assign( cmds[2]);
              if((cmds[2].Length()>cmds[4].Length())||
                 !cmds[2].Equals(Substring(cmds[4],cmds[4].Length()-cmds[2].Length(),cmds[2].Length()))){
                NS_WARNING("PFX/SFX line from .aff file is inconsitent");
                continue;
              }
              suffixTest=Substring(cmds[4],0,cmds[4].Length()-cmds[2].Length());
              suffixTest.Append(cmds[3]);
            }
            if(suffixTest.Length() != 0)doubleReverseHack(suffixTest);
            suffixes.addMod(suffixTest.get(),&newMod);
          }
        }        
      }
    }
  }

  return NS_OK;
}


nsresult
myspAffixMgr::LoadDictionary(nsIInputStream *strm)
{
  nsLineBuffer *lineBuffer;
  nsresult res;
  res= NS_InitLineBuffer(&lineBuffer);
  nsCAutoString line;
  PRBool moreData;
  PRInt32 pos;
  nsACString::const_iterator begin1,end1,begin2,end2;
  
  // first read the first line of file to get hash table size */
  mHashTable.Reset();

  res = NS_ReadLine(strm,lineBuffer,line,&moreData);
  
  // loop through all words on much list and add to hash
  // table and create word and affix strings

  while (moreData) {
    res = NS_ReadLine(strm,lineBuffer,line,&moreData);
    
    // split each line into word and affix char strings
    pos = line.FindChar('/');
    if(pos==-1){
      line.BeginReading(begin1);
      line.EndReading(end1);
      begin2=end2=begin1;
    }
    else{
      line.BeginReading(begin1);
      begin2=end1=begin1;
      end1.advance(pos);
      begin2.advance(pos+1);
      line.EndReading(end2);
    }


    // add the word and its index
    mHashTable.Put(PromiseFlatCString(Substring(begin1,end1)).get(),PromiseFlatCString(Substring(begin2,end2)).get());

  }

  return NS_OK;
}

// return the preferred try string for suggestions
void myspAffixMgr::get_try_string(nsAString &aTryString)
{
  PRInt32 outLength;
  PRInt32 inLength = trystring.Length();
  nsresult rv = mDecoder->GetMaxLength(trystring.get(), inLength, &outLength);

  if (NS_SUCCEEDED(rv)) {
    PRUnichar *tmpPtr = (PRUnichar *) malloc(sizeof(PRUnichar) * (outLength + 1));
    if (tmpPtr) {
      rv = mDecoder->Convert(trystring.get(), &inLength, tmpPtr, &outLength);
      if (NS_SUCCEEDED(rv)) {
        tmpPtr[outLength] = 0;
        aTryString = tmpPtr;
      }
      free(tmpPtr);
    }
  }
}

mozReplaceTable *myspAffixMgr::getReplaceTable()
{
  return mReplaceTable;
}

PRUint32 myspAffixMgr::getReplaceTableLength()
{
  return mReplaceTableLength;
}

PRBool 
myspAffixMgr::prefixCheck(const nsAFlatCString &word)
{
  nsACString::const_iterator end,curr;
  nsCString tempWord;
  mozAffixState *currState= &prefixes;
  const char * he = NULL;
  PRUint32 wLength=word.Length();

  word.BeginReading(curr);
  word.EndReading(end);
  while((curr!=end)&&(currState!=nsnull)){
    // check the current mods
    mozAffixMod *currMod=currState->getMod();
    while(currMod != nsnull){
      tempWord.Assign(currMod->mAppend);
      tempWord.Append(Substring(word,currMod->mTruncateLength,wLength - currMod->mTruncateLength));
      he = mHashTable.Get(tempWord.get());;
      if((he != nsnull)&&(PL_strchr(he, currMod->mID))) return PR_TRUE;
      if(((currMod->flags)&XPRODUCT)&&suffixCheck(tempWord,PR_TRUE,currMod->mID)) return PR_TRUE;
      currMod = currMod->next;
    }
    currState=currState->nextState(*curr);
    curr++;
  }
  if(currState != nsnull){
     mozAffixMod *currMod=currState->getMod();
    while(currMod != nsnull){
      tempWord.Assign(currMod->mAppend);
      tempWord.Append(Substring(word,currMod->mTruncateLength,wLength - currMod->mTruncateLength));
      he=mHashTable.Get(tempWord.get());
      if((he != nsnull)&&(PL_strchr(he, currMod->mID))) return PR_TRUE;
      // no need to check cross product, we reached the end of the word.
      currMod = currMod->next;
     }
  }
  return PR_FALSE;
}

PRBool myspAffixMgr::suffixCheck(const nsAFlatCString &word,PRBool cross,char crossID)
{
  nsACString::const_iterator start,curr;
  nsCString tempWord;
  mozAffixState *currState= &suffixes;
  const char * he = NULL;
  PRUint32 wLength=word.Length();

  word.EndReading(curr);
  word.BeginReading(start);
  while((curr!=start)&&(currState!=nsnull)){
    // check the current mods
    mozAffixMod *currMod=currState->getMod();
    while(currMod != nsnull){
      tempWord=Substring(word,0,wLength - currMod->mTruncateLength);
      tempWord.Append(currMod->mAppend);
      he = mHashTable.Get(tempWord.get());;
      if((he != nsnull)&&PL_strchr(he, currMod->mID)&&((!cross)||PL_strchr(he, crossID))) return PR_TRUE;
      currMod = currMod->next;
    }
    curr--;
    currState=currState->nextState(*curr);
  }
  //Ok, we've read the last character of the word, but who knows, 
  //we could still get a match en-US "her" for example. Mozdev bug 895
  if(currState != nsnull){
    mozAffixMod *currMod=currState->getMod();
    while(currMod != nsnull){
      tempWord=Substring(word,0,wLength - currMod->mTruncateLength);
      tempWord.Append(currMod->mAppend);
      he = mHashTable.Get(tempWord.get());;
      if((he != nsnull)&&PL_strchr(he, currMod->mID)&&((!cross)||PL_strchr(he, crossID))) return PR_TRUE;
      currMod = currMod->next;
     }
  }
  return PR_FALSE;
}

PRBool myspAffixMgr::check(const nsAFlatString &word)
{
  const char *he = nsnull;

  PRInt32 inLength = word.Length();
  PRInt32 outLength;
  nsresult rv = mEncoder->GetMaxLength(word.get(), inLength, &outLength);
  // NS_ERROR_UENC_NOMAPPING is a NS_SUCCESS, no error.
  if (NS_FAILED(rv) || rv == NS_ERROR_UENC_NOMAPPING) {
    // not a word in the current charset, so likely not
    // a word in the current language
    return PR_FALSE;
  }
  char *charsetWord = (char *) nsMemory::Alloc(sizeof(char) * (outLength+1));
  rv = mEncoder->Convert(word.get(), &inLength, charsetWord, &outLength);
  charsetWord[outLength] = '\0';

  he = mHashTable.Get(charsetWord);

  if(he != nsnull) return PR_TRUE;
  if(prefixCheck(nsDependentCString(charsetWord)))
    return PR_TRUE;
  if(suffixCheck(nsDependentCString(charsetWord)))
    return PR_TRUE;

  PRBool good=PR_FALSE;
  if (mPersonalDictionary) {
    rv = mPersonalDictionary->Check(word.get(), mLanguage.get(), &good);
    if (NS_FAILED(rv))
      return PR_FALSE;
  }
  return good;
}

nsresult
myspAffixMgr::DecodeString(const nsAFlatCString &aSource, nsAString &aDest)
{
  if (!mDecoder) {
    aDest.Truncate();
    return NS_OK;
  }
  PRInt32 inLength = aSource.Length();
  PRInt32 outLength;
  nsresult rv = mDecoder->GetMaxLength(aSource.get(), inLength, &outLength);
  NS_ENSURE_SUCCESS(rv, rv);
  PRUnichar *dest = (PRUnichar *)malloc(sizeof(PRUnichar) * (outLength + 1));
  if (!dest)
    return NS_ERROR_OUT_OF_MEMORY;
  rv = mDecoder->Convert(aSource.get(), &inLength, dest, &outLength);
  dest[outLength] = 0;
  aDest = dest;
  free(dest);
  return rv;
}


static PRInt32 
SplitString(nsACString &in,nsCString out[],PRInt32 size)
{
  nsACString::const_iterator startWord;
  nsACString::const_iterator endWord;
  nsACString::const_iterator endLine;
  PRInt32 pos=0;
  in.BeginReading(startWord);
  in.EndReading(endLine);
  while((pos < size)&&(startWord!=endLine)){
    while((startWord!=endLine)&&(*startWord == ' '))startWord++;
    endWord=startWord;
    while((endWord!=endLine)&&(*endWord != ' '))endWord++;
    if(startWord != endWord){
      out[pos++] = Substring(startWord,endWord);
    }
    startWord=endWord;
  }
  return pos;
} 

/*
  reverse the suffix search string so that we put it into the tree in reverse.
  we need to reverse the blocks so that the ^ in negated blocks occurs first.
 */
static void doubleReverseHack(nsACString &s)
{
  nsACString::iterator start,end,curr;
  char temp;

  s.BeginWriting(start);
  s.EndWriting(end);
  curr=start;
  while(start!=end){
    if(*start=='['){
      curr=start;
      while((curr!=end)&&(*curr != ']')) curr++;
      while(start != curr){
        temp=*curr;
        *curr=*start;
        *start=temp;
        start++;
        if(start==curr)break;
        curr--;
      }
      while((start != end)&&(*start != '[')) start++;
      if(*start != '[')start++;
    }
    start++;
  }
  s.BeginWriting(start);
  end--;
  while(start != end){
    temp = *start;
    *start = *end;
    *end=temp;
    start++;
    if(start == end)break;
    end--;
  }
}
