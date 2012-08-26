/******* BEGIN LICENSE BLOCK *******
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
 * The Initial Developers of the Original Code are Kevin Hendricks (MySpell)
 * and László Németh (Hunspell). Portions created by the Initial Developers
 * are Copyright (C) 2002-2005 the Initial Developers. All Rights Reserved.
 * 
 * Contributor(s): Kevin Hendricks (kevin.hendricks@sympatico.ca)
 *                 David Einstein (deinst@world.std.com)
 *                 László Németh (nemethl@gyorsposta.hu)
 *                 Caolan McNamara (caolanm@redhat.com)
 *                 Davide Prina
 *                 Giuseppe Modugno
 *                 Gianluca Turconi
 *                 Simon Brouwer
 *                 Noll Janos
 *                 Biro Arpad
 *                 Goldman Eleonora
 *                 Sarlos Tamas
 *                 Bencsath Boldizsar
 *                 Halacsy Peter
 *                 Dvornik Laszlo
 *                 Gefferth Andras
 *                 Nagy Viktor
 *                 Varga Daniel
 *                 Chris Halls
 *                 Rene Engelhard
 *                 Bram Moolenaar
 *                 Dafydd Jones
 *                 Harri Pitkanen
 *                 Andras Timar
 *                 Tor Lillqvist
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
 ******* END LICENSE BLOCK *******/

#ifndef _AFFIX_HXX_
#define _AFFIX_HXX_

#include "hunvisapi.h"

#include "atypes.hxx"
#include "baseaffix.hxx"
#include "affixmgr.hxx"

/* A Prefix Entry  */

class LIBHUNSPELL_DLL_EXPORTED PfxEntry : protected AffEntry
{
       AffixMgr*    pmyMgr;

       PfxEntry * next;
       PfxEntry * nexteq;
       PfxEntry * nextne;
       PfxEntry * flgnxt;

public:

  PfxEntry(AffixMgr* pmgr, affentry* dp );
  ~PfxEntry();

  inline bool          allowCross() { return ((opts & aeXPRODUCT) != 0); }
  struct hentry *      checkword(const char * word, int len, char in_compound, 
                            const FLAG needflag = FLAG_NULL);

  struct hentry *      check_twosfx(const char * word, int len, char in_compound, const FLAG needflag = FLAG_NULL);

  char *      check_morph(const char * word, int len, char in_compound,
                            const FLAG needflag = FLAG_NULL);

  char *      check_twosfx_morph(const char * word, int len,
                  char in_compound, const FLAG needflag = FLAG_NULL);

  inline FLAG getFlag()   { return aflag;   }
  inline const char *  getKey()    { return appnd;  } 
  char *               add(const char * word, int len);

  inline short getKeyLen() { return appndl; } 

  inline const char *  getMorph()    { return morphcode;  } 

  inline const unsigned short * getCont()    { return contclass;  } 
  inline short           getContLen()    { return contclasslen;  } 

  inline PfxEntry *    getNext()   { return next;   }
  inline PfxEntry *    getNextNE() { return nextne; }
  inline PfxEntry *    getNextEQ() { return nexteq; }
  inline PfxEntry *    getFlgNxt() { return flgnxt; }

  inline void   setNext(PfxEntry * ptr)   { next = ptr;   }
  inline void   setNextNE(PfxEntry * ptr) { nextne = ptr; }
  inline void   setNextEQ(PfxEntry * ptr) { nexteq = ptr; }
  inline void   setFlgNxt(PfxEntry * ptr) { flgnxt = ptr; }
  
  inline char * nextchar(char * p);
  inline int    test_condition(const char * st);
};




/* A Suffix Entry */

class LIBHUNSPELL_DLL_EXPORTED SfxEntry : protected AffEntry
{
       AffixMgr*    pmyMgr;
       char *       rappnd;

       SfxEntry *   next;
       SfxEntry *   nexteq;
       SfxEntry *   nextne;
       SfxEntry *   flgnxt;
           
       SfxEntry *   l_morph;
       SfxEntry *   r_morph;
       SfxEntry *   eq_morph;

public:

  SfxEntry(AffixMgr* pmgr, affentry* dp );
  ~SfxEntry();

  inline bool          allowCross() { return ((opts & aeXPRODUCT) != 0); }
  struct hentry *   checkword(const char * word, int len, int optflags, 
                    PfxEntry* ppfx, char ** wlst, int maxSug, int * ns,
//                    const FLAG cclass = FLAG_NULL, const FLAG needflag = FLAG_NULL, char in_compound=IN_CPD_NOT);
                    const FLAG cclass = FLAG_NULL, const FLAG needflag = FLAG_NULL, const FLAG badflag = 0);

  struct hentry *   check_twosfx(const char * word, int len, int optflags, PfxEntry* ppfx, const FLAG needflag = FLAG_NULL);

  char *      check_twosfx_morph(const char * word, int len, int optflags,
                 PfxEntry* ppfx, const FLAG needflag = FLAG_NULL);
  struct hentry * get_next_homonym(struct hentry * he);
  struct hentry * get_next_homonym(struct hentry * word, int optflags, PfxEntry* ppfx, 
    const FLAG cclass, const FLAG needflag);


  inline FLAG getFlag()   { return aflag;   }
  inline const char *  getKey()    { return rappnd; } 
  char *               add(const char * word, int len);


  inline const char *  getMorph()    { return morphcode;  } 

  inline const unsigned short * getCont()    { return contclass;  } 
  inline short           getContLen()    { return contclasslen;  } 
  inline const char *  getAffix()    { return appnd; } 

  inline short getKeyLen() { return appndl; } 

  inline SfxEntry *    getNext()   { return next;   }
  inline SfxEntry *    getNextNE() { return nextne; }
  inline SfxEntry *    getNextEQ() { return nexteq; }

  inline SfxEntry *    getLM() { return l_morph; }
  inline SfxEntry *    getRM() { return r_morph; }
  inline SfxEntry *    getEQM() { return eq_morph; }
  inline SfxEntry *    getFlgNxt() { return flgnxt; }

  inline void   setNext(SfxEntry * ptr)   { next = ptr;   }
  inline void   setNextNE(SfxEntry * ptr) { nextne = ptr; }
  inline void   setNextEQ(SfxEntry * ptr) { nexteq = ptr; }
  inline void   setFlgNxt(SfxEntry * ptr) { flgnxt = ptr; }

  inline char * nextchar(char * p);
  inline int    test_condition(const char * st, const char * begin);

};

#endif


