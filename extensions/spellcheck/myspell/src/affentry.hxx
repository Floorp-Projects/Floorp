#ifndef _AFFIX_HXX_
#define _AFFIX_HXX_

#include "atypes.hxx"
#include "baseaffix.hxx"
#include "affixmgr.hxx"


/* A Prefix Entry  */

class PfxEntry : public AffEntry
{
       AffixMgr*    pmyMgr;

       PfxEntry * next;
       PfxEntry * nexteq;
       PfxEntry * nextne;
       PfxEntry * flgnxt;

public:

  PfxEntry(AffixMgr* pmgr, affentry* dp );
  ~PfxEntry();

  struct hentry *      check(const char * word, int len);

  inline bool          allowCross() { return ((xpflg & XPRODUCT) != 0); }
  inline unsigned char getFlag()   { return achar;   }
  inline const char *  getKey()    { return appnd;  } 
  char *               add(const char * word, int len);

  inline PfxEntry *    getNext()   { return next;   }
  inline PfxEntry *    getNextNE() { return nextne; }
  inline PfxEntry *    getNextEQ() { return nexteq; }
  inline PfxEntry *    getFlgNxt() { return flgnxt; }

  inline void   setNext(PfxEntry * ptr)   { next = ptr;   }
  inline void   setNextNE(PfxEntry * ptr) { nextne = ptr; }
  inline void   setNextEQ(PfxEntry * ptr) { nexteq = ptr; }
  inline void   setFlgNxt(PfxEntry * ptr) { flgnxt = ptr; }
};




/* A Suffix Entry */

class SfxEntry : public AffEntry
{
       AffixMgr*    pmyMgr;
       char *       rappnd;

       SfxEntry *   next;
       SfxEntry *   nexteq;
       SfxEntry *   nextne;
       SfxEntry *   flgnxt;

public:

  SfxEntry(AffixMgr* pmgr, affentry* dp );
  ~SfxEntry();

  struct hentry *   check(const char * word, int len, int optflags, 
                                                       AffEntry* ppfx);

  inline bool          allowCross() { return ((xpflg & XPRODUCT) != 0); }
  inline unsigned char getFlag()   { return achar;   }
  inline const char *  getKey()    { return rappnd; } 
  char *               add(const char * word, int len);

  inline SfxEntry *    getNext()   { return next;   }
  inline SfxEntry *    getNextNE() { return nextne; }
  inline SfxEntry *    getNextEQ() { return nexteq; }
  inline SfxEntry *    getFlgNxt() { return flgnxt; }

  inline void   setNext(SfxEntry * ptr)   { next = ptr;   }
  inline void   setNextNE(SfxEntry * ptr) { nextne = ptr; }
  inline void   setNextEQ(SfxEntry * ptr) { nexteq = ptr; }
  inline void   setFlgNxt(SfxEntry * ptr) { flgnxt = ptr; }

};


#endif


