#include "license.readme"

#include <ctype.h>
#include <string.h>
#include <stdlib.h> 
#include <stdio.h> 

#include "affentry.hxx"

// using namespace std;

extern char * mystrdup(const char * s);
extern char *  myrevstrdup(const char * s);

PfxEntry::PfxEntry(AffixMgr* pmgr, affentry* dp)
{
  // register affix manager
  pmyMgr = pmgr;

  // set up its intial values
  achar = dp->achar;         // char flag 
  strip = dp->strip;         // string to strip
  appnd = dp->appnd;         // string to append
  stripl = dp->stripl;       // length of strip string
  appndl = dp->appndl;       // length of append string
  numconds = dp->numconds;   // number of conditions to match
  xpflg = dp->xpflg;         // cross product flag
  // then copy over all of the conditions
  memcpy(&conds[0],&dp->conds[0],SETSIZE*sizeof(conds[0]));
  next = NULL;
  nextne = NULL;
  nexteq = NULL;
}


PfxEntry::~PfxEntry()
{
    achar = '\0';
    if (appnd) free(appnd);
    if (strip)free(strip);
    pmyMgr = NULL;
    appnd = NULL;
    strip = NULL;    
}



// add prefix to this word assuming conditions hold
char * PfxEntry::add(const char * word, int len)
{
    int			cond;
    char	        tword[MAXWORDLEN+1];

     /* make sure all conditions match */
     if ((len > stripl) && (len >= numconds)) {
            unsigned char * cp = (unsigned char *) word;
            for (cond = 0;  cond < numconds;  cond++) {
	       if ((conds[*cp++] & (1 << cond)) == 0)
	          break;
            }
            if (cond >= numconds) {
	      /* we have a match so add prefix */
              int tlen = 0;
              if (appndl) {
	          strcpy(tword,appnd);
                  tlen += appndl;
               } 
               char * pp = tword + tlen;
               strcpy(pp, (word + stripl));
               return mystrdup(tword);
	    }
     }
     return NULL;    
}




// check if this prefix entry matches 
struct hentry * PfxEntry::check(const char * word, int len)
{
    int			cond;	// condition number being examined
    int	                tmpl;   // length of tmpword
    struct hentry *     he;     // hash entry of root word or NULL
    unsigned char *	cp;		
    char	        tmpword[MAXWORDLEN+1];


    // on entry prefix is 0 length or already matches the beginning of the word.
    // So if the remaining root word has positive length
    // and if there are enough chars in root word and added back strip chars
    // to meet the number of characters conditions, then test it

     tmpl = len - appndl;

     if ((tmpl > 0) &&  (tmpl + stripl >= numconds)) {

	    // generate new root word by removing prefix and adding
	    // back any characters that would have been stripped

	    if (stripl) strcpy (tmpword, strip);
	    strcpy ((tmpword + stripl), (word + appndl));

            // now make sure all of the conditions on characters
            // are met.  Please see the appendix at the end of
            // this file for more info on exactly what is being
            // tested

	    cp = (unsigned char *)tmpword;
	    for (cond = 0;  cond < numconds;  cond++) {
		if ((conds[*cp++] & (1 << cond)) == 0) break;
	    }

            // if all conditions are met then check if resulting
            // root word in the dictionary

	    if (cond >= numconds) {
		tmpl += stripl;
		if ((he = pmyMgr->lookup(tmpword)) != NULL) {
		   if (TESTAFF(he->astr, achar, he->alen)) return he;
		}

		// prefix matched but no root word was found 
                // if XPRODUCT is allowed, try again but now 
                // ross checked combined with a suffix

		if (xpflg & XPRODUCT) {
		   he = pmyMgr->suffix_check(tmpword, tmpl, XPRODUCT, (AffEntry *)this);
                   if (he) return he;
		}
	    }
     }
    return NULL;
}



SfxEntry::SfxEntry(AffixMgr * pmgr, affentry* dp)
{
  // register affix manager
  pmyMgr = pmgr;

  // set up its intial values
  achar = dp->achar;         // char flag 
  strip = dp->strip;         // string to strip
  appnd = dp->appnd;         // string to append
  stripl = dp->stripl;       // length of strip string
  appndl = dp->appndl;       // length of append string
  numconds = dp->numconds;   // number of conditions to match
  xpflg = dp->xpflg;         // cross product flag

  // then copy over all of the conditions
  memcpy(&conds[0],&dp->conds[0],SETSIZE*sizeof(conds[0]));

  rappnd = myrevstrdup(appnd);
}


SfxEntry::~SfxEntry()
{
    achar = '\0';
    if (appnd) free(appnd);
    if (rappnd) free(rappnd);
    if (strip) free(strip);
    pmyMgr = NULL;
    appnd = NULL;
    strip = NULL;    
}



// add suffix to this word assuming conditions hold
char * SfxEntry::add(const char * word, int len)
{
    int			cond;
    char	        tword[MAXWORDLEN+1];

     /* make sure all conditions match */
     if ((len > stripl) && (len >= numconds)) {
            unsigned char * cp = (unsigned char *) (word + len);
            for (cond = numconds; --cond >=0; ) {
	       if ((conds[*--cp] & (1 << cond)) == 0)
	          break;
            }
            if (cond < 0) {
	      /* we have a match so add suffix */
              strcpy(tword,word);
              int tlen = len;
              if (stripl) {
		 tlen -= stripl;
              }
              char * pp = (tword + tlen);
              if (appndl) {
	          strcpy(pp,appnd);
                  tlen += appndl;
	      } else *pp = '\0';
               return mystrdup(tword);
	    }
     }
     return NULL;
}



// see if this suffix is present in the word 
struct hentry * SfxEntry::check(const char * word, int len, int optflags, AffEntry* ppfx)
{
    int	                tmpl;		 // length of tmpword 
    int			cond;		 // condition beng examined
    struct hentry *     he;              // hash entry pointer
    unsigned char *	cp;
    char	        tmpword[MAXWORDLEN+1];
    PfxEntry* ep = (PfxEntry *) ppfx;


    // if this suffix is being cross checked with a prefix
    // but it does not support cross products skip it

    if ((optflags & XPRODUCT) != 0 &&  (xpflg & XPRODUCT) == 0)
        return NULL;

    // upon entry suffix is 0 length or already matches the end of the word.
    // So if the remaining root word has positive length
    // and if there are enough chars in root word and added back strip chars
    // to meet the number of characters conditions, then test it

    tmpl = len - appndl;

    if ((tmpl > 0)  &&  (tmpl + stripl >= numconds)) {

	    // generate new root word by removing suffix and adding
	    // back any characters that would have been stripped or
	    // or null terminating the shorter string

	    strcpy (tmpword, word);
	    cp = (unsigned char *)(tmpword + tmpl);
	    if (stripl) {
		strcpy ((char *)cp, strip);
		tmpl += stripl;
		cp = (unsigned char *)(tmpword + tmpl);
	    } else *cp = '\0';

            // now make sure all of the conditions on characters
            // are met.  Please see the appendix at the end of
            // this file for more info on exactly what is being
            // tested

	    for (cond = numconds;  --cond >= 0; ) {
		if ((conds[*--cp] & (1 << cond)) == 0) break;
	    }

            // if all conditions are met then check if resulting
            // root word in the dictionary

	    if (cond < 0) {
	        if ((he = pmyMgr->lookup(tmpword)) != NULL) {
                     if (TESTAFF(he->astr, achar , he->alen) && 
                           ((optflags & XPRODUCT) == 0 || 
                           TESTAFF(he->astr, ep->getFlag(), he->alen))) return he;
	        }  
	    }
    }
    return NULL;
}




#if 0

Appendix:  Understanding Affix Code


An affix is either a  prefix or a suffix attached to root words to make 
other words.

Basically a Prefix or a Suffix is set of AffEntry objects
which store information about the prefix or suffix along 
with supporting routines to check if a word has a particular 
prefix or suffix or a combination.

The structure affentry is defined as follows:

struct affentry
{
   unsigned char achar;   // char used to represent the affix
   char * strip;          // string to strip before adding affix
   char * appnd;          // the affix string to add
   short  stripl;         // length of the strip string
   short  appndl;         // length of the affix string
   short  numconds;       // the number of conditions that must be met
   short  xpflg;          // flag: XPRODUCT- combine both prefix and suffix 
   char   conds[SETSIZE]; // array which encodes the conditions to be met
};


Here is a suffix borrowed from the en_US.aff file.  This file 
is whitespace delimited.

SFX D Y 4 
SFX D   0     e          d
SFX D   y     ied        [^aeiou]y
SFX D   0     ed         [^ey]
SFX D   0     ed         [aeiou]y

This information can be interpreted as follows:

In the first line has 4 fields

Field
-----
1     SFX - indicates this is a suffix
2     D   - is the name of the character flag which represents this suffix
3     Y   - indicates it can be combined with prefixes (cross product)
4     4   - indicates that sequence of 4 affentry structures are needed to
               properly store the affix information

The remaining lines describe the unique information for the 4 SfxEntry 
objects that make up this affix.  Each line can be interpreted
as follows: (note fields 1 and 2 are as a check against line 1 info)

Field
-----
1     SFX         - indicates this is a suffix
2     D           - is the name of the character flag for this affix
3     y           - the string of chars to strip off before adding affix
                         (a 0 here indicates the NULL string)
4     ied         - the string of affix characters to add
5     [^aeiou]y   - the conditions which must be met before the affix
                    can be applied

Field 5 is interesting.  Since this is a suffix, field 5 tells us that
there are 2 conditions that must be met.  The first condition is that 
the next to the last character in the word must *NOT* be any of the 
following "a", "e", "i", "o" or "u".  The second condition is that
the last character of the word must end in "y".

So how can we encode this information concisely and be able to 
test for both conditions in a fast manner?  The answer is found
but studying the wonderful ispell code of Geoff Kuenning, et.al. 
(now available under a normal BSD license).

If we set up a conds array of 256 bytes indexed (0 to 255) and access it
using a character (cast to an unsigned char) of a string, we have 8 bits
of information we can store about that character.  Specifically we
could use each bit to say if that character is allowed in any of the 
last (or first for prefixes) 8 characters of the word.

Basically, each character at one end of the word (up to the number 
of conditions) is used to index into the conds array and the resulting 
value found there says whether the that character is valid for a 
specific character position in the word.  

For prefixes, it does this by setting bit 0 if that char is valid 
in the first position, bit 1 if valid in the second position, and so on. 

If a bit is not set, then that char is not valid for that postion in the
word.

If working with suffixes bit 0 is used for the character closest 
to the front, bit 1 for the next character towards the end, ..., 
with bit numconds-1 representing the last char at the end of the string. 

Note: since entries in the conds[] are 8 bits, only 8 conditions 
(read that only 8 character positions) can be examined at one
end of a word (the beginning for prefixes and the end for suffixes.

So to make this clearer, lets encode the conds array values for the 
first two affentries for the suffix D described earlier.


  For the first affentry:    
     numconds = 1             (only examine the last character)

     conds['e'] =  (1 << 0)   (the word must end in an E)
     all others are all 0

  For the second affentry:
     numconds = 2             (only examine the last two characters)     

     conds[X] = conds[X] | (1 << 0)     (aeiou are not allowed)
         where X is all characters *but* a, e, i, o, or u
         

     conds['y'] = (1 << 1)     (the last char must be a y)
     all other bits for all other entries in the conds array are zero


#endif

