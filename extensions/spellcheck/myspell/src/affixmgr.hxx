#ifndef _AFFIXMGR_HXX_
#define _AFFIXMGR_HXX_

#include "atypes.hxx"
#include "baseaffix.hxx"
#include "hashmgr.hxx"
#include <cstdio>

class AffixMgr
{

  AffEntry *          pStart[SETSIZE];
  AffEntry *          sStart[SETSIZE];
  AffEntry *          pFlag[SETSIZE];
  AffEntry *          sFlag[SETSIZE];
  HashMgr *           pHMgr;
  char *              trystring;
  char *              encoding;
  char *              compound;
  int                 cpdmin;
  int                 numrep;
  replentry *         reptable;
  int                 nummap;
  mapentry *          maptable;
  bool                nosplitsugs;


public:
 
  AffixMgr(const char * affpath, HashMgr * ptr);
  ~AffixMgr();
  struct hentry *     affix_check(const char * word, int len);
  struct hentry *     prefix_check(const char * word, int len);
  struct hentry *     suffix_check(const char * word, int len, int sfxopts, AffEntry* ppfx);
  int                 expand_rootword(struct guessword * wlst, int maxn, 
                             const char * ts, int wl, const char * ap, int al);
  struct hentry *     compound_check(const char * word, int len, char compound_flag);
  struct hentry *     lookup(const char * word);
  int                 get_numrep();
  struct replentry *  get_reptable();
  int                 get_nummap();
  struct mapentry *   get_maptable();
  char *              get_encoding();
  char *              get_try_string();
  char *              get_compound();
  bool                get_nosplitsugs();
             
private:
  int  parse_file(const char * affpath);
  int  parse_try(char * line);
  int  parse_set(char * line);
  int  parse_cpdflag(char * line);
  int  parse_cpdmin(char * line);
  int  parse_reptable(char * line, FILE * af);
  int  parse_maptable(char * line, FILE * af);
  int  parse_affix(char * line, const char at, FILE * af);

  void encodeit(struct affentry * ptr, char * cs);
  int build_pfxtree(AffEntry* pfxptr);
  int build_sfxtree(AffEntry* sfxptr);
  AffEntry* process_sfx_in_order(AffEntry* ptr, AffEntry* nptr);
  AffEntry* process_pfx_in_order(AffEntry* ptr, AffEntry* nptr);
  int process_pfx_tree_to_list();
  int process_sfx_tree_to_list();
  int process_pfx_order();
  int process_sfx_order();
};

#endif

