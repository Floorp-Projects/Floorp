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
#include "myspSuggestmgr.h"
#include "plstr.h"
#include "nsReadableUtils.h"
#include "nsMemory.h"
#include "nsUnicharUtils.h"

myspSuggestMgr::myspSuggestMgr()
{
}


myspSuggestMgr::~myspSuggestMgr()
{
  pAMgr = nsnull;
  maxSug = 0;
}

void 
myspSuggestMgr::setup(const nsAFlatString &tryme, int maxn, myspAffixMgr *aptr)
{
  // register affix manager and check in string of chars to 
  // try when building candidate suggestions
  pAMgr = aptr;
  ctry = tryme;
  maxSug = maxn;
}


// generate suggestions for a mispelled word
//    pass in address of array of char * pointers

nsresult myspSuggestMgr::suggest(PRUnichar ***slst,const nsAFlatString &word, PRUint32 *num)
{
  NS_ENSURE_ARG_POINTER(num);
  NS_ENSURE_ARG_POINTER(slst);

  nsresult res;
  PRUint32 nsug;
  PRUint32 i;
  PRUnichar **wlst;
  if(!(*slst)){
    nsug=0;
    wlst=(PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * maxSug);
    if(!wlst)
      return NS_ERROR_OUT_OF_MEMORY;
    memset(wlst, nsnull, sizeof(PRUnichar*) * maxSug); 
  }
  else{
    wlst=*slst;
    nsug=*num;
  }

  // perhaps we made a typical spelling error.
  res = replacechars(wlst, word, &nsug);

  // did we forget to add a char
  res = forgotchar(wlst, word, &nsug);
  if ((nsug < maxSug) && NS_SUCCEEDED(res)){
    res = forgotchar(wlst, word, &nsug);
  }
 
  // did we swap the order of chars by mistake
  if ((nsug < maxSug) && NS_SUCCEEDED(res)){
    res = swapchar(wlst, word, &nsug);
  }

  // did we add a char that should not be there
  if ((nsug < maxSug) && NS_SUCCEEDED(res)){
    res = extrachar(wlst, word, &nsug);
  }

    // did we just hit the wrong key in place of a good char
  if ((nsug < maxSug) && NS_SUCCEEDED(res)){
    res = badchar(wlst, word, &nsug);
  }

    // perhaps we forgot to hit space and two words ran together
  if ((nsug < maxSug) && NS_SUCCEEDED(res)){
    res = twowords(wlst, word, &nsug);
  }
  if(NS_FAILED(res)){
    for (i=0;i<maxSug; i++)
      if (wlst[i] != NULL) nsMemory::Free(wlst[i]);
    nsMemory::Free(wlst);
    *slst = 0;
    *num=0;
  }
  else{
    *slst=wlst;
    *num=nsug;
  }
  return res;
}


// suggestions for a typical spelling error that
// differs by more than 1 letter from the right spelling
nsresult myspSuggestMgr::replacechars(PRUnichar ** wlst,const nsAFlatString &word, PRUint32 *ns)
{
  nsAutoString candidate;
  PRBool cwrd;
  PRUint32 i,k;
  PRUint32 startOffset, findOffset;

  if (word.Length() < 2 || !pAMgr)
    return NS_OK;

  PRUint32 replaceTableLength = pAMgr->getReplaceTableLength();
  struct mozReplaceTable *replaceTable = pAMgr->getReplaceTable();

  if (replaceTable == nsnull)
    return NS_OK;

  for (i = 0; i < replaceTableLength; i++) {
    startOffset = 0;

    candidate.Assign(word);
    ToLowerCase(candidate);

    while ((findOffset = candidate.Find(replaceTable[i].pattern, startOffset)) != -1) {
      candidate.Assign(word);
      ToLowerCase(candidate);
      candidate.Replace(findOffset, replaceTable[i].pattern.Length(), replaceTable[i].replacement);

      cwrd = PR_TRUE;
      for (k = 0; k < *ns; k++) {
        if (candidate.Equals(wlst[k])){ 
          cwrd = PR_FALSE;
          break;
        }
      }

      if (cwrd && pAMgr->check(candidate)) {
        if (*ns < maxSug) {
          wlst[*ns] = ToNewUnicode(candidate);
          if (!wlst[*ns])
            return NS_ERROR_OUT_OF_MEMORY;
          (*ns)++;
        } else {
          return NS_OK;
        }
      }

      startOffset = findOffset + replaceTable[i].pattern.Length();
    }
  }

  return NS_OK;
}

// error is wrong char in place of correct one
nsresult myspSuggestMgr::badchar(PRUnichar ** wlst,const nsAFlatString &word, PRUint32 *ns)
{
  PRUnichar tmpc;
  nsAutoString candidate;
  PRBool cwrd;
  PRUint32 i,j,k;
  PRUint32 wl = word.Length();
  candidate.Assign(word);
  nsASingleFragmentString::char_iterator candIt;
  for (i=0,candidate.BeginWriting(candIt); i < wl; i++,candIt++) {
    tmpc = *candIt;
    for (j=0; j < ctry.Length(); j++) {
      if (ctry[j] == tmpc) continue;
      *candIt = ctry[j];
      cwrd = PR_TRUE;
      for(k=0;k < *ns;k++){
        if (candidate.Equals(wlst[k]) ){ 
          cwrd = PR_FALSE;
          break;
        }
      }
      if (cwrd && pAMgr->check(candidate)) {
        if (*ns < maxSug) {
          wlst[*ns] = ToNewUnicode(candidate);
          if(!wlst[*ns])
            return NS_ERROR_OUT_OF_MEMORY;
          (*ns)++;
        } else return NS_OK;
      }
      *candIt = tmpc;
    }
  }
  return NS_OK;
}


// error is word has an extra letter it does not need 
nsresult myspSuggestMgr::extrachar(PRUnichar ** wlst,const nsAFlatString &word, PRUint32 *ns)
{
  PRBool cwrd;
  nsString stCand;
  nsAutoString candidate;
  PRUint32 k;
  PRUint32 wl = word.Length();
  if (wl < 2) return 0;
  
  // try omitting one char of word at a time
  candidate.Assign(Substring(word,1,wl-1));
  nsASingleFragmentString::char_iterator r;
  nsASingleFragmentString::const_char_iterator p,end;
  word.EndReading(end);
  
  for (word.BeginReading(p),candidate.BeginWriting(r);  p != end;  ) {
    cwrd = PR_TRUE;
    for(k=0;k < *ns;k++){
      if (candidate.Equals(wlst[k])){ 
        cwrd = PR_FALSE;
        break;
      }
    }
    if (cwrd && pAMgr->check(candidate)) {
      if (*ns < maxSug) {
        wlst[*ns] = ToNewUnicode(candidate);
        if(!wlst[*ns])
          return NS_ERROR_OUT_OF_MEMORY;
        (*ns)++;
      } else return NS_OK;
    }
    *r++ = *p++;
  }
  return NS_OK;
}


// error is mising a letter it needs
nsresult myspSuggestMgr::forgotchar(PRUnichar **wlst,const nsAFlatString &word, PRUint32 *ns)
{
  PRBool cwrd;
  nsString stCand;
  nsAutoString candidate;
  PRUint32 i,k;
  candidate = NS_LITERAL_STRING(" ") + word;
  nsASingleFragmentString::char_iterator q;
  nsASingleFragmentString::const_char_iterator p,end;
  word.EndReading(end);

  // try inserting a tryme character before every letter
  for (word.BeginReading(p), candidate.BeginWriting(q);  p != end;  )  {
    for ( i = 0;  i < ctry.Length();  i++) {
      *q = ctry[i];
      cwrd = PR_TRUE;
      for(k=0;k < *ns;k++){
        if (candidate.Equals(wlst[k]) ){ 
          cwrd = PR_FALSE;
          break;
        }
      }
      if (cwrd && pAMgr->check(candidate)) {
        if (*ns < maxSug) {
          wlst[*ns] = ToNewUnicode(candidate);
          if(!wlst[*ns])
            return NS_ERROR_OUT_OF_MEMORY;
          (*ns)++;
        } else return NS_OK;
      }
    }
    *q++ = *p++;
  }
   
  // now try adding one to end */
  for ( i = 0;  i < ctry.Length();  i++) {
    *q = ctry[i];
    cwrd = PR_TRUE;
    for(k=0;k < *ns;k++){
      if (candidate.Equals(wlst[k])){ 
        cwrd = PR_FALSE;
        break;
      }
    }
    if (cwrd && pAMgr->check(candidate)) {
      if (*ns < maxSug) {
        wlst[*ns] = ToNewUnicode(candidate);
        if(!wlst[*ns])
          return NS_ERROR_OUT_OF_MEMORY;
        (*ns)++;
      } else return NS_OK;
    }
  }
  return NS_OK;
}


/* error is should have been two words */
nsresult myspSuggestMgr::twowords(PRUnichar ** wlst,const nsAFlatString &word, PRUint32 *ns)
{
  nsAutoString candidate;
  PRUint32 pos;
  PRUint32 wl=word.Length();
  if (wl < 3) return NS_OK;
  candidate.Assign(word);
  nsAutoString temp;

  // split the string into two pieces after every char
  // if both pieces are good words make them a suggestion
  for (pos =  1;  pos < wl;  pos++) {
    temp.Assign(Substring(candidate,0,pos));
    if (pAMgr->check(temp)) {
      temp.Assign(Substring(candidate,pos,wl-pos));
      if (pAMgr->check(temp)) {
        if (*ns < maxSug) {
          candidate.Insert(PRUnichar(' '),pos);
          wlst[*ns] = ToNewUnicode(candidate);
          if(!wlst[*ns])
            return NS_ERROR_OUT_OF_MEMORY;
          (*ns)++;
        } else return NS_OK;
      }
    }
  }
  return NS_OK;
}


// error is adjacent letter were swapped
nsresult myspSuggestMgr::swapchar(PRUnichar **wlst,const nsAFlatString &word, PRUint32 *ns)
{
  nsAutoString candidate;
  PRUnichar	tmpc;
  PRBool cwrd;
  PRUint32 k;
  candidate.Assign(word);
  nsASingleFragmentString::char_iterator p,q,end;
  candidate.EndWriting(end);

  for (candidate.BeginWriting(p),q=p, q++;  q != end;  p++,q++) {
    tmpc = *p;
    *p = *q;
    *q = tmpc;
    cwrd = PR_TRUE;
    for(k=0;k < *ns;k++){
      if (candidate.Equals(wlst[k])){ 
        cwrd = PR_FALSE;
        break;
      }
    }
    if (cwrd && pAMgr->check(candidate)) {
      if (*ns < maxSug) {
        wlst[*ns] = ToNewUnicode(candidate);
        if(!wlst[*ns])
          return NS_ERROR_OUT_OF_MEMORY;
        (*ns)++;
      } else return NS_OK;
    }
    tmpc = *p;
    *p = *q;
    *q = tmpc;
  }
  return NS_OK;
}

