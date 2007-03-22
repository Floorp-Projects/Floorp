#ifndef _MYSPELLMGR_HXX_
#define _MYSPELLMGR_HXX_

#include "hashmgr.hxx"
#include "affixmgr.hxx"
#include "suggestmgr.hxx"
#include "csutil.hxx"

#define NOCAP   0
#define INITCAP 1
#define ALLCAP  2
#define HUHCAP  3

class MySpell
{
  AffixMgr*       pAMgr;
  HashMgr*        pHMgr;
  SuggestMgr*     pSMgr;
  char *          encoding;
  struct cs_info * csconv;
  int             maxSug;

public:
  MySpell(const char * affpath, const char * dpath);
  ~MySpell();

  int suggest(char*** slst, const char * word);
  int spell(const char *);
  char * get_dic_encoding();

private:
   int    cleanword(char *, const char *, int *, int *);
   char * check(const char *);
};

#endif
