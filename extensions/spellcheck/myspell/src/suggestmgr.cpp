#include "license.readme"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include "suggestmgr.hxx"

// using namespace std;

extern char * mystrdup(const char *);


SuggestMgr::SuggestMgr(const char * tryme, int maxn, 
                       AffixMgr * aptr)
{

  // register affix manager and check in string of chars to 
  // try when building candidate suggestions
  pAMgr = aptr;
  ctry = mystrdup(tryme);
  ctryl = 0;
  if (ctry)
    ctryl = strlen(ctry);
  maxSug = maxn;
  nosplitsugs=(0==1);
  if (pAMgr) pAMgr->get_nosplitsugs();
}


SuggestMgr::~SuggestMgr()
{
  pAMgr = NULL;
  if (ctry) free(ctry);
  ctry = NULL;
  ctryl = 0;
  maxSug = 0;
}



// generate suggestions for a mispelled word
//    pass in address of array of char * pointers

int SuggestMgr::suggest(char** wlst, int ns, const char * word)
{
    
    int nsug = ns;

    // did we swap the order of chars by mistake
    if ((nsug < maxSug) && (nsug > -1))
      nsug = swapchar(wlst, word, nsug);

    // perhaps we made chose the wrong char from a related set
    if ((nsug < maxSug) && (nsug > -1))
      nsug = mapchars(wlst, word, nsug);

    // perhaps we made a typical fault of spelling
    if ((nsug < maxSug) && (nsug > -1))
      nsug = replchars(wlst, word, nsug);

    // did we forget to add a char
    if ((nsug < maxSug) && (nsug > -1))
      nsug = forgotchar(wlst, word, nsug);

    // did we add a char that should not be there
    if ((nsug < maxSug) && (nsug > -1))
      nsug = extrachar(wlst, word, nsug);
   
    // did we just hit the wrong key in place of a good char
    if ((nsug < maxSug) && (nsug > -1))
      nsug = badchar(wlst, word, nsug);

    // perhaps we forgot to hit space and two words ran together
    if (!nosplitsugs) {
        if ((nsug < maxSug) && (nsug > -1))
           nsug = twowords(wlst, word, nsug);
    }
    return nsug;
}



// suggestions for when chose the wrong char out of a related set
int SuggestMgr::mapchars(char** wlst, const char * word, int ns)
{
  int wl = strlen(word);
  if (wl < 2 || ! pAMgr) return ns;

  int nummap = pAMgr->get_nummap();
  struct mapentry* maptable = pAMgr->get_maptable();
  if (maptable==NULL) return ns;
  ns = map_related(word, 0, wlst, ns, maptable, nummap);
  return ns;
}


int SuggestMgr::map_related(const char * word, int i, char** wlst, int ns, const mapentry* maptable, int nummap) 
{
  char c = *(word + i);
  if (c == 0) {
      int cwrd = 1;
      for (int m=0; m < ns; m++)
	  if (strcmp(word,wlst[m]) == 0) cwrd = 0;
      if ((cwrd) && check(word,strlen(word))) {
	  if (ns < maxSug) {
	      wlst[ns] = mystrdup(word);
              // fprintf(stderr,"map_related %d adding %s\n",ns, wlst[ns]); fflush(stderr);
	      if (wlst[ns] == NULL) return -1;
	      ns++;
	  }
      }
      return ns;
  } 
  int in_map = 0;
  for (int j = 0; j < nummap; j++) {
    if (strchr(maptable[j].set,c) != 0) {
      in_map = 1;
#ifdef __SUNPRO_CC // for SunONE Studio compiler
      char * newword = mystrdup(word);
#else
      char * newword = strdup(word);
#endif
      for (int k = 0; k < maptable[j].len; k++) {
	*(newword + i) = *(maptable[j].set + k);
	ns = map_related(newword, (i+1), wlst, ns, maptable, nummap);
      }
      free(newword);
    }
  }
  if (!in_map) {
     i++;
     ns = map_related(word, i, wlst, ns, maptable, nummap);
  }
  return ns;
}



// suggestions for a typical fault of spelling, that
// differs with more, than 1 letter from the right form.
int SuggestMgr::replchars(char** wlst, const char * word, int ns)
{
  char candidate[MAXSWL];
  const char * r;
  int lenr, lenp;
  int cwrd;

  int wl = strlen(word);
  if (wl < 2 || ! pAMgr) return ns;

  int numrep = pAMgr->get_numrep();
  struct replentry* reptable = pAMgr->get_reptable();
  if (reptable==NULL) return ns;

  for (int i=0; i < numrep; i++ ) {
      r = word;
      lenr = strlen(reptable[i].replacement);
      lenp = strlen(reptable[i].pattern);
      // search every occurence of the pattern in the word
      while ((r=strstr(r, reptable[i].pattern)) != NULL) {
	  strcpy(candidate, word);
	  if (r-word + lenr + strlen(r+lenp) >= MAXSWL) break;
	  strcpy(candidate+(r-word),reptable[i].replacement);
	  strcpy(candidate+(r-word)+lenr, r+lenp);
          cwrd = 1;
          for (int k=0; k < ns; k++)
	      if (strcmp(candidate,wlst[k]) == 0) cwrd = 0;
          if ((cwrd) && check(candidate,strlen(candidate))) {
	      if (ns < maxSug) {
		  wlst[ns] = mystrdup(candidate);
                  // fprintf(stderr,"replchars %d adding %s\n",ns,wlst[ns]); fflush(stderr);
		  if (wlst[ns] == NULL) return -1;
		  ns++;
	      } else return ns;
	  }
          r++; // search for the next letter
      }
   }
   return ns;
}


// error is wrong char in place of correct one
int SuggestMgr::badchar(char ** wlst, const char * word, int ns)
{
  char	tmpc;
  char	candidate[MAXSWL];

  int wl = strlen(word);
  int cwrd;
  strcpy (candidate, word);

  // swap out each char one by one and try all the tryme
  // chars in its place to see if that makes a good word
  for (int i=0; i < wl; i++) {
    tmpc = candidate[i];
    for (int j=0; j < ctryl; j++) {
       if (ctry[j] == tmpc) continue;
       candidate[i] = ctry[j];
       cwrd = 1;
       for (int k=0; k < ns; k++)
	 if (strcmp(candidate,wlst[k]) == 0) cwrd = 0;
       if ((cwrd) && check(candidate,wl)) {
	 if (ns < maxSug) {
            wlst[ns] = mystrdup(candidate);
            // fprintf(stderr,"bad_char %d adding %s\n",ns, wlst[ns]); fflush(stderr);
            if (wlst[ns] == NULL) return -1;
            ns++;
         } else return ns;
       }
       candidate[i] = tmpc;
    }
  }
  return ns;
}


// error is word has an extra letter it does not need 
int SuggestMgr::extrachar(char** wlst, const char * word, int ns)
{
   char	   candidate[MAXSWL];
   const char *  p;
   char *  r;
   int cwrd;

   int wl = strlen(word);
   if (wl < 2) return ns;

   // try omitting one char of word at a time
   strcpy (candidate, word + 1);
   for (p = word, r = candidate;  *p != 0;  ) {
       cwrd = 1;
       for (int k=0; k < ns; k++)
	 if (strcmp(candidate,wlst[k]) == 0) cwrd = 0;
       if ((cwrd) && check(candidate,wl-1)) {
	 if (ns < maxSug) {
            wlst[ns] = mystrdup(candidate);
            // fprintf(stderr,"extra_char %d adding %s\n",ns,wlst[ns]); fflush(stderr);
            if (wlst[ns] == NULL) return -1;
            ns++;
         } else return ns; 
       }
       *r++ = *p++;
   }
   return ns;
}


// error is mising a letter it needs
int SuggestMgr::forgotchar(char ** wlst, const char * word, int ns)
{
   char	candidate[MAXSWL];
   const char *	p;
   char *	q;
   int cwrd;

   int wl = strlen(word);

   // try inserting a tryme character before every letter
   strcpy(candidate + 1, word);
   for (p = word, q = candidate;  *p != 0;  )  {
      for (int i = 0;  i < ctryl;  i++) {
	 *q = ctry[i];
         cwrd = 1;
         for (int k=0; k < ns; k++)
	   if (strcmp(candidate,wlst[k]) == 0) cwrd = 0;
         if ((cwrd) && check(candidate,wl+1)) {
	    if (ns < maxSug) {
                wlst[ns] = mystrdup(candidate);
                // fprintf(stderr,"forgotchar %d adding %s\n",ns,wlst[ns]); fflush(stderr);
                if (wlst[ns] == NULL) return -1;
                ns++;
            } else return ns; 
         }
      }
      *q++ = *p++;
   }

   // now try adding one to end */
   for (int i = 0;  i < ctryl;  i++) {
      *q = ctry[i];
      cwrd = 1;
      for (int k=0; k < ns; k++)
	if (strcmp(candidate,wlst[k]) == 0) cwrd = 0;
      if ((cwrd) && check(candidate,wl+1)) {
	 if (ns < maxSug) {
             wlst[ns] = mystrdup(candidate);
	     // fprintf(stderr,"forgot_char %d adding %s\n",ns,wlst[ns]); fflush(stderr);
	     if (wlst[ns] == NULL) return -1;
             ns++;
         } else return ns;
      }
   }
   return ns;
}


/* error is should have been two words */
int SuggestMgr::twowords(char ** wlst, const char * word, int ns)
{
    char candidate[MAXSWL];
    char * p;

    int wl=strlen(word);
    if (wl < 3) return ns;
    strcpy(candidate + 1, word);

    // split the string into two pieces after every char
    // if both pieces are good words make them a suggestion
    for (p = candidate + 1;  p[1] != '\0';  p++) {
       p[-1] = *p;
       *p = '\0';
       if (check(candidate,strlen(candidate))) {
	 if (check((p+1),strlen(p+1))) {
	    *p = ' ';
	    if (ns < maxSug) {
                wlst[ns] = mystrdup(candidate);
	        // fprintf(stderr,"two_words %d adding %s\n",ns,wlst[ns]); fflush(stderr);
                if (wlst[ns] == NULL) return -1;
                ns++;
            } else return ns;
         }
       }
    }
    return ns;
}


// error is adjacent letter were swapped
int SuggestMgr::swapchar(char ** wlst, const char * word, int ns)
{
   char	candidate[MAXSWL];
   char * p;
   char	tmpc;
   int cwrd;

   int wl = strlen(word);

   // try swapping adjacent chars one by one
   strcpy(candidate, word);
   for (p = candidate;  p[1] != 0;  p++) {
      tmpc = *p;
      *p = p[1];
      p[1] = tmpc;
      cwrd = 1;
      for (int k=0; k < ns; k++)
	if (strcmp(candidate,wlst[k]) == 0) cwrd = 0;
      if ((cwrd) && check(candidate,wl)) {
	 if (ns < maxSug) {
             wlst[ns] = mystrdup(candidate);
	     // fprintf(stderr,"swap_char %d adding %s\n",ns,wlst[ns]); fflush(stderr);
             if (wlst[ns] == NULL) return -1;
             ns++;
         } else return ns;
      }
      tmpc = *p;
      *p = p[1];
      p[1] = tmpc;
   }
   return ns;
}


// generate a set of suggestions for very poorly spelled words
int SuggestMgr::ngsuggest(char** wlst, char * word, HashMgr* pHMgr)
{

  int i, j;
  int lval;
  int sc;
  int lp;

  if (! pHMgr) return 0;

  // exhaustively search through all root words
  // keeping track of the MAX_ROOTS most similar root words
  struct hentry * roots[MAX_ROOTS];
  int scores[MAX_ROOTS];
  for (i = 0; i < MAX_ROOTS; i++) {
    roots[i] = NULL;
    scores[i] = -100 * i;
  }
  lp = MAX_ROOTS - 1;

  int n = strlen(word);

  struct hentry* hp = NULL;
  int col = -1;
  while ((hp = pHMgr->walk_hashtable(col, hp))) {
    sc = ngram(3, word, hp->word, NGRAM_LONGER_WORSE);
    if (sc > scores[lp]) {
      scores[lp] = sc;
      roots[lp] = hp;
      int lval = sc;
      for (j=0; j < MAX_ROOTS; j++)
	if (scores[j] < lval) {
	  lp = j;
          lval = scores[j];
	}
    }  
  }

  // find minimum threshhold for a passable suggestion
  // mangle original word three differnt ways
  // and score them to generate a minimum acceptable score
  int thresh = 0;
  char * mw = NULL;
  for (int sp = 1; sp < 4; sp++) {
#ifdef __SUNPRO_CC // for SunONE Studio compiler
     mw = mystrdup(word);
#else
     mw = strdup(word);
#endif
     for (int k=sp; k < n; k+=4) *(mw + k) = '*';
     thresh = thresh + ngram(n, word, mw, NGRAM_ANY_MISMATCH);
     free(mw);
  }
  mw = NULL;
  thresh = thresh / 3;
  thresh--;

  // now expand affixes on each of these root words and
  // and use length adjusted ngram scores to select
  // possible suggestions
  char * guess[MAX_GUESS];
  int gscore[MAX_GUESS];
  for(i=0;i<MAX_GUESS;i++) {
     guess[i] = NULL;
     gscore[i] = -100 * i;
  }

  lp = MAX_GUESS - 1;

  struct guessword * glst;
  glst = (struct guessword *) calloc(MAX_WORDS,sizeof(struct guessword));
  if (! glst) return 0;

  for (i = 0; i < MAX_ROOTS; i++) {

      if (roots[i]) {
        struct hentry * rp = roots[i];
	int nw = pAMgr->expand_rootword(glst, MAX_WORDS, rp->word, rp->wlen,
                                        rp->astr, rp->alen);
        for (int k = 0; k < nw; k++) {
           sc = ngram(n, word, glst[k].word, NGRAM_ANY_MISMATCH);
	   if (sc > thresh)
	   {
		if (sc > gscore[lp])
		{
			if (guess[lp]) free(guess[lp]);
			gscore[lp] = sc;
			guess[lp] = glst[k].word;
			glst[k].word = NULL;
			lval = sc;
			for (j=0; j < MAX_GUESS; j++)
			{
				if (gscore[j] < lval)
				{
					lp = j;
					lval = gscore[j];
				}
			}
		}
	   }
	   free (glst[k].word);
	   glst[k].word = NULL;
	   glst[k].allow = 0;
	}
      }
  }
  if (glst) free(glst);

  // now we are done generating guesses
  // sort in order of decreasing score and copy over
  
  bubblesort(&guess[0], &gscore[0], MAX_GUESS);
  int ns = 0;
  for (i=0; i < MAX_GUESS; i++) {
    if (guess[i]) {
      int unique = 1;
      for (j=i+1; j < MAX_GUESS; j++)
	if (guess[j]) 
	    if (!strcmp(guess[i], guess[j])) unique = 0;
      if (unique) {
         wlst[ns++] = guess[i];
      } else {
	 free(guess[i]);
      }
    }
  }
  return ns;
}




// see if a candidate suggestion is spelled correctly
// needs to check both root words and words with affixes
int SuggestMgr::check(const char * word, int len)
{
  struct hentry * rv=NULL;
  if (pAMgr) { 
    rv = pAMgr->lookup(word);
    if (rv == NULL) rv = pAMgr->affix_check(word,len);
  }
  if (rv) return 1;
  return 0;
}



// generate an n-gram score comparing s1 and s2
int SuggestMgr::ngram(int n, char * s1, const char * s2, int uselen)
{
  int nscore = 0;
  int l1 = strlen(s1);
  int l2 = strlen(s2);
  int ns;
  for (int j=1;j<=n;j++) {
    ns = 0;
    for (int i=0;i<=(l1-j);i++) {
      char c = *(s1 + i + j);
      *(s1 + i + j) = '\0';
      if (strstr(s2,(s1+i))) ns++;
      *(s1 + i + j ) = c;
    }
    nscore = nscore + ns;
    if (ns < 2) break;
  }
  ns = 0;
  if (uselen == NGRAM_LONGER_WORSE) ns = (l2-l1)-2;
  if (uselen == NGRAM_ANY_MISMATCH) ns = abs(l2-l1)-2;
  return (nscore - ((ns > 0) ? ns : 0));
}


// sort in decreasing order of score
void SuggestMgr::bubblesort(char** rword, int* rsc, int n )
{
      int m = 1;
      while (m < n) {
	  int j = m;
	  while (j > 0) {
	    if (rsc[j-1] < rsc[j]) {
	        int sctmp = rsc[j-1];
                char * wdtmp = rword[j-1];
	        rsc[j-1] = rsc[j];
                rword[j-1] = rword[j];
                rsc[j] = sctmp;
                rword[j] = wdtmp;
	        j--;
	    } else break;
	  }
          m++;
      }
      return;
}

