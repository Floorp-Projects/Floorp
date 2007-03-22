#include "license.readme"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "myspell.hxx"

// using namespace std;


MySpell::MySpell(const char * affpath, const char * dpath)
{
    encoding = NULL;
    csconv = NULL;

    /* first set up the hash manager */
    pHMgr = new HashMgr(dpath);

    /* next set up the affix manager */
    /* it needs access to the hash manager lookup methods */
    pAMgr = new AffixMgr(affpath,pHMgr);

    /* get the preferred try string and the dictionary */
    /* encoding from the Affix Manager for that dictionary */
    char * try_string = pAMgr->get_try_string();
    encoding = pAMgr->get_encoding();
    csconv = get_current_cs(encoding);

    /* and finally set up the suggestion manager */
    maxSug = 25;
    pSMgr = new SuggestMgr(try_string, maxSug, pAMgr);
    if (try_string) free(try_string);
}


MySpell::~MySpell()
{
    delete pSMgr;
    delete pAMgr;
    delete pHMgr;
    
    csconv= NULL;
    if (encoding) 
        free(encoding);
}


// make a copy of src at destination while removing all leading
// blanks and removing any trailing periods after recording
// their presence with the abbreviation flag
// also since already going through character by character, 
// set the capitalization type
// return the length of the "cleaned" word

int MySpell::cleanword(char * dest, const char * src, int * pcaptype, int * pabbrev)
{ 

  // with the new breakiterator code this should not be needed anymore
   const char * special_chars = "._#$%&()* +,-/:;<=>[]\\^`{|}~\t \x0a\x0d\x01\'\"";

   unsigned char * p = (unsigned char *) dest;
   const unsigned char * q = (const unsigned char * ) src;

   // first skip over any leading special characters
   while ((*q != '\0') && (strchr(special_chars,(int)(*q)))) q++;
   
   // now strip off any trailing special characters 
   // if a period comes after a normal char record its presence
   *pabbrev = 0;
   int nl = strlen((const char *)q);
   while ((nl > 0) && (strchr(special_chars,(int)(*(q+nl-1))))) {
       nl--;
   }
   if ( *(q+nl) == '.' ) *pabbrev = 1;
   
   // if no characters are left it can't be an abbreviation and can't be capitalized
   if (nl <= 0) { 
       *pcaptype = NOCAP;
       *pabbrev = 0;
       *p = '\0';
       return 0;
   }

   // now determine the capitalization type of the first nl letters
   int ncap = 0;
   int nneutral = 0;
   int nc = 0;
   while (nl > 0) {
       nc++;
       if (csconv[(*q)].ccase) ncap++;
       if (csconv[(*q)].cupper == csconv[(*q)].clower) nneutral++;
       *p++ = *q++;
       nl--;
   }
   // remember to terminate the destination string
   *p = '\0';

   // now finally set the captype
   if (ncap == 0) {
        *pcaptype = NOCAP;
   } else if ((ncap == 1) && csconv[(unsigned char)(*dest)].ccase) {
        *pcaptype = INITCAP;
  } else if ((ncap == nc) || ((ncap + nneutral) == nc)){
        *pcaptype = ALLCAP;
  } else {
        *pcaptype = HUHCAP;
  }
  return nc;
} 
       

int MySpell::spell(const char * word)
{
  char * rv=NULL;
  char cw[MAXWORDLEN+1];
  char wspace[MAXWORDLEN+1];

  int wl = strlen(word);
  if (wl > (MAXWORDLEN - 1)) return 0;
  int captype = 0;
  int abbv = 0;
  wl = cleanword(cw, word, &captype, &abbv);
  if (wl == 0) return 1;

  switch(captype) {
     case HUHCAP:
     case NOCAP:   { 
                     rv = check(cw); 
                     if ((abbv) && !(rv)) {
		         memcpy(wspace,cw,wl);
                         *(wspace+wl) = '.';
                         *(wspace+wl+1) = '\0';
                         rv = check(wspace);
                     }
                     break;
                   }

     case ALLCAP:  {
                     memcpy(wspace,cw,(wl+1));
                     mkallsmall(wspace, csconv);
                     rv = check(wspace);
                     if (!rv) {
                        mkinitcap(wspace, csconv);
                        rv = check(wspace);
                     }
                     if (!rv) rv = check(cw);
                     if ((abbv) && !(rv)) {
		         memcpy(wspace,cw,wl);
                         *(wspace+wl) = '.';
                         *(wspace+wl+1) = '\0';
                         rv = check(wspace);
                     }
                     break; 
                   }
     case INITCAP: { 
                     memcpy(wspace,cw,(wl+1));
                     mkallsmall(wspace, csconv);
                     rv = check(wspace);
                     if (!rv) rv = check(cw);
                     if ((abbv) && !(rv)) {
		         memcpy(wspace,cw,wl);
                         *(wspace+wl) = '.';
                         *(wspace+wl+1) = '\0';
                         rv = check(wspace);
                     }
                     break; 
                   }
  }
  if (rv) return 1;
  return 0;
}


char * MySpell::check(const char * word)
{
  struct hentry * he = NULL;
  if (pHMgr)
     he = pHMgr->lookup (word);

  if ((he == NULL) && (pAMgr)) {
     // try stripping off affixes */
     he = pAMgr->affix_check(word, strlen(word));

     // try check compound word
     if ((he == NULL) && (pAMgr->get_compound())) {
          he = pAMgr->compound_check(word, strlen(word), (pAMgr->get_compound())[0]);
     }

  }

  if (he) return he->word;
  return NULL;
}



int MySpell::suggest(char*** slst, const char * word)
{
  char cw[MAXWORDLEN+1];
  char wspace[MAXWORDLEN+1];
  if (! pSMgr) return 0;
  int wl = strlen(word);
  if (wl > (MAXWORDLEN-1)) return 0;
  int captype = 0;
  int abbv = 0;
  wl = cleanword(cw, word, &captype, &abbv);
  if (wl == 0) return 0;

  int ns = 0;
  char ** wlst = (char **) calloc(maxSug, sizeof(char *));
  if (wlst == NULL) return 0;

  switch(captype) {
     case NOCAP:   { 
                     ns = pSMgr->suggest(wlst, ns, cw); 
                     break;
                   }

     case INITCAP: { 

                     ns = pSMgr->suggest(wlst,ns,cw); 
                     if (ns != -1) {
                        memcpy(wspace,cw,(wl+1));
                        mkallsmall(wspace, csconv);
                        if (ns) {
                           ns = pSMgr->suggest(wlst, ns, wspace);
                        } else {
                           int ns2 = pSMgr->suggest(wlst, ns, wspace);
                           for (int j=ns; j < ns2; j++)
                              mkinitcap(wlst[j], csconv);
                           ns = ns2;
			}    
                     }
                     break;
                   }

     case HUHCAP: { 
                     ns = pSMgr->suggest(wlst, ns, cw);
                     if (ns != -1) {
                       memcpy(wspace,cw,(wl+1));
                       mkallsmall(wspace, csconv);
                       ns = pSMgr->suggest(wlst, ns, wspace);
                     } 
                     break;
                   }

     case ALLCAP: { 
                     memcpy(wspace,cw,(wl+1));
                     mkallsmall(wspace, csconv);
                     ns = pSMgr->suggest(wlst, ns, wspace);
                     if (ns > 0) {
                       for (int j=0; j < ns; j++)
                         mkallcap(wlst[j], csconv);
                     } 
                     if (ns != -1) 
                         ns = pSMgr->suggest(wlst, ns , cw);
                     break;
                   }
  }
  if (ns > 0) {
       *slst = wlst;
       return ns;
  }
  // try ngram approach since found nothing
  if (ns == 0) { 
     ns = pSMgr->ngsuggest(wlst, cw, pHMgr);
     if (ns) {
         switch(captype) {
	    case NOCAP:  break;
            case HUHCAP: break; 
            case INITCAP: { 
                            for (int j=0; j < ns; j++)
                              mkinitcap(wlst[j], csconv);
                          }
                          break;

            case ALLCAP: { 
                            for (int j=0; j < ns; j++)
                              mkallcap(wlst[j], csconv);
                         } 
                         break;
	 }
         *slst = wlst;
         return ns;
     }
  }
  if (ns < 0) {
     // we ran out of memory - we should free up as much as possible
     for (int i=0;i<maxSug; i++)
	 if (wlst[i] != NULL) free(wlst[i]);
  }
  if (wlst) free(wlst);
  *slst = NULL;
  return 0;
}


char * MySpell::get_dic_encoding()
{
  return encoding;
}

