#ifndef _ATYPES_HXX_
#define _ATYPES_HXX_

#define SETSIZE         256
#define MAXAFFIXES      256
#define MAXWORDLEN      100
#define XPRODUCT        (1 << 0)

#define MAXLNLEN        1024

#define TESTAFF( a , b , c ) memchr((void *)(a), (int)(b), (size_t)(c) )

struct affentry
{
   char * strip;
   char * appnd;
   short  stripl;
   short  appndl;
   short  numconds;
   short  xpflg;
   char   achar;
   char   conds[SETSIZE];
};

struct replentry {
  char * pattern;
  char * replacement;
};

struct mapentry {
  char * set;
  int len;
};

struct guessword {
  char * word;
  bool allow;
};

#endif





