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

/* based on MySpell (c) 2001 by Kevin Hendicks */

#include "mozAffixMod.h"

mozAffixState::mozAffixState()
{
  mTrans=nsnull;
  mMods=nsnull;
  mDefault=nsnull;
}

mozAffixState::~mozAffixState()
{
  clear();
}

void
mozAffixState::clear()
{
  // clean out any mods
  mozAffixMod * nextmod=mMods;
  while(nextmod != nsnull){
    mozAffixMod *temp=nextmod->next;
    delete nextmod;
    nextmod = temp;
  }
  mMods=nsnull;

  //clean out transitions
  mozAffixStateTrans * nexttrans=mTrans;
  while(nexttrans != nsnull){
    mozAffixStateTrans *temp=nexttrans->nextTrans;
    delete nexttrans->nextState;
    delete nexttrans;
    nexttrans=temp;
  }
  mTrans=nsnull;

  if(mDefault != nsnull){
    delete mDefault;
  }
  mDefault=nsnull;
}

mozAffixState *
mozAffixState::nextState(char c)
{
  mozAffixStateTrans * nexttrans=mTrans;
  while(nexttrans != nsnull){
    if(c==nexttrans->mRule) return nexttrans->nextState;
    nexttrans = nexttrans->nextTrans;
  }
  return mDefault;
}

void 
mozAffixState::addMod(const char *affix, mozAffixMod *mod)
{
  mozAffixStateTrans * nexttrans=mTrans;
  // figure out what kind of character we have and act accordingly
  if(*affix == '['){
    char *endblock=(char *)affix+1;
    char *startblock=(char *)affix+1; 
    while((*endblock != ']')&&(*endblock != '\0')) endblock++;
    if(*startblock == '^'){
      char *currblock = startblock+1;
      //OK, let us start with the complicated case.
      //Here we sacrifice efficiency for simplicity. we are only running this at startup, 
      // and the lists are not going to be large anyway.  First we modify all of the states 
      // not in the block, then we add unmodified clones for the states in the block that do 
      // not occur in the list already. 
      
      //first loop -- go through current states modifying if not found;
      while(nexttrans != nsnull){
        currblock=startblock+1;
        PRBool found=PR_FALSE;
        while(currblock < endblock){
          if(*currblock == nexttrans->mRule){
            found = PR_TRUE;
            break;
          }
          currblock++;
        }
        if(!found){
          nexttrans->nextState->addMod(endblock+1,mod);
        }
        nexttrans=nexttrans->nextTrans;
      }

      //second loop add new states if necessary (if they don't already exist)
      currblock = startblock+1;
      while(currblock < endblock){
        //just add each block one at a time
        PRBool found=PR_FALSE;
        nexttrans=mTrans;
        while(nexttrans!=nsnull){
          if(nexttrans->mRule == *currblock){
            found = PR_TRUE;
            break;
          }
          nexttrans=nexttrans->nextTrans;
        }
        if(!found){
          mozAffixState *newState=clone(mDefault);
          mozAffixStateTrans *newTrans=new mozAffixStateTrans;
          newTrans->mRule=*currblock;
          newTrans->nextState=newState;
          newTrans->nextTrans=mTrans;
          mTrans=newTrans;
        }
        currblock++;
      }
      if(mDefault==nsnull) mDefault=new mozAffixState;
      mDefault->addMod(endblock+1,mod);
    }
    else{  // a block of included characters
      while(startblock < endblock){
        //just add each block one at a time
        PRBool found=PR_FALSE;
        nexttrans=mTrans;
        while(nexttrans!=nsnull){
          if(nexttrans->mRule == *startblock){
            nexttrans->nextState->addMod(endblock+1,mod);
            found = PR_TRUE;
            break;
          }
          nexttrans=nexttrans->nextTrans;
        }
        if(!found){
          mozAffixState *newState=clone(mDefault);
          mozAffixStateTrans *newTrans=new mozAffixStateTrans;
          newTrans->mRule=*startblock;
          newTrans->nextState=newState;
          newTrans->nextTrans=mTrans;
          mTrans=newTrans;
          newState->addMod(endblock+1,mod);
        }
        startblock++;
      }
    }
  }
  else if(*affix == '\0'){
    // all we've got to do is insert the mod;
    mozAffixMod * temp= new mozAffixMod;
    temp->mID=mod->mID;
    temp->flags= mod->flags;
    temp->mAppend.Assign( mod->mAppend);
    temp->mTruncateLength = mod->mTruncateLength;
    temp->next= mMods;
    mMods=temp;
  }
  else{
    // If the single character is a "." fill everything.
    if (*affix == '.'){
      while((nexttrans!=nsnull)){
        nexttrans->nextState->addMod(affix+1,mod);
        nexttrans=nexttrans->nextTrans;
      }
      if(mDefault==nsnull) mDefault=new mozAffixState;
      mDefault->addMod(affix+1,mod);
    }
    else {
      PRBool found=PR_FALSE;
      while((nexttrans!=nsnull)){
        if(nexttrans->mRule == *affix){
          nexttrans->nextState->addMod(affix+1,mod);
          found = PR_TRUE;
          break;
        }
        nexttrans=nexttrans->nextTrans;
      }
      if(!found){
        mozAffixState *newState=clone(mDefault);
        mozAffixStateTrans *newTrans=new mozAffixStateTrans;
        newTrans->mRule=*affix;
        newTrans->nextState=newState;
        newTrans->nextTrans=mTrans;
        mTrans=newTrans;
        newState->addMod(affix+1,mod);
      }
    }
  }
}

mozAffixState *
mozAffixState::clone(mozAffixState * old)
{
  mozAffixState *newState = new mozAffixState;
  if(old != nsnull){
    if(old->mDefault != nsnull){
      mDefault=clone(old->mDefault);
    }
    mozAffixStateTrans *nexttrans=old->mTrans;
    while(nexttrans != nsnull){
      mozAffixStateTrans *temp = new mozAffixStateTrans;
      temp->mRule = nexttrans->mRule;
      temp->nextState = clone(nexttrans->nextState);
      temp->nextTrans=mTrans;
      mTrans=temp;
      nexttrans = nexttrans->nextTrans;
    }
    mozAffixMod *nextMod=old->mMods;
    while(nextMod!=nsnull){
      mozAffixMod * temp= new mozAffixMod;
      temp->mID=nextMod->mID;
      temp->flags = nextMod->flags;
      temp->mAppend.Assign( nextMod->mAppend);
      temp->mTruncateLength = nextMod->mTruncateLength;
      temp->next=mMods;
      mMods=temp;
      nextMod = nextMod->next;
    }
  }
  return newState;
}
