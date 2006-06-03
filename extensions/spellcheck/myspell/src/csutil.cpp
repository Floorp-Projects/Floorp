#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "csutil.hxx"

#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "nsICaseConversion.h"
#include "nsICharsetConverterManager.h"
#include "nsUnicharUtilCIID.h"
#include "nsUnicharUtils.h"

#ifdef __SUNPRO_CC // for SunONE Studio compiler
using namespace std;
#endif

// strip strings into token based on single char delimiter
// acts like strsep() but only uses a delim char and not 
// a delim string

char * mystrsep(char ** stringp, const char delim)
{
  char * rv = NULL;
  char * mp = *stringp;
  int n = strlen(mp);
  if (n > 0) {
     char * dp = (char *)memchr(mp,(int)((unsigned char)delim),n);
     if (dp) {
        *stringp = dp+1;
        int nc = (int)((unsigned long)dp - (unsigned long)mp); 
        rv = (char *) malloc(nc+1);
        memcpy(rv,mp,nc);
        *(rv+nc) = '\0';
        return rv;
     } else {
       rv = (char *) malloc(n+1);
       memcpy(rv, mp, n);
       *(rv+n) = '\0';
       *stringp = mp + n;
       return rv;
     }
  }
  return NULL;
}


// replaces strdup with ansi version
char * mystrdup(const char * s)
{
  char * d = NULL;
  if (s) {
     int sl = strlen(s);
     d = (char *) malloc(((sl+1) * sizeof(char)));
     if (d) memcpy(d,s,((sl+1)*sizeof(char)));
  }
  return d;
}


// remove cross-platform text line end characters
void mychomp(char * s)
{
  int k = strlen(s);
  if ((k > 0) && ((*(s+k-1)=='\r') || (*(s+k-1)=='\n'))) *(s+k-1) = '\0';
  if ((k > 1) && (*(s+k-2) == '\r')) *(s+k-2) = '\0';
}


//  does an ansi strdup of the reverse of a string
char * myrevstrdup(const char * s)
{
    char * d = NULL;
    if (s) {
       int sl = strlen(s);
       d = (char *) malloc((sl+1) * sizeof(char));
       if (d) {
	 const char * p = s + sl - 1;
         char * q = d;
         while (p >= s) *q++ = *p--;
         *q = '\0';
       }
    }
    return d; 
}

#if 0
// return 1 if s1 is a leading subset of s2
int isSubset(const char * s1, const char * s2)
{
  int l1 = strlen(s1);
  int l2 = strlen(s2);
  if (l1 > l2) return 0;
  if (strncmp(s2,s1,l1) == 0) return 1;
  return 0;
}
#endif


// return 1 if s1 is a leading subset of s2
int isSubset(const char * s1, const char * s2)
{
  while( *s1 && (*s1 == *s2) ) {
    s1++;
    s2++;
  }
  return (*s1 == '\0');
}


// return 1 if s1 (reversed) is a leading subset of end of s2
int isRevSubset(const char * s1, const char * end_of_s2, int len)
{
  while( (len > 0) && *s1 && (*s1 == *end_of_s2) ) {
    s1++;
    end_of_s2--;
    len --;
  }
  return (*s1 == '\0');
}


#if 0
// Not needed in mozilla
// convert null terminated string to all caps using encoding 
void enmkallcap(char * d, const char * p, const char * encoding)
{
  struct cs_info * csconv = get_current_cs(encoding);
  while (*p != '\0') { 
    *d++ = csconv[((unsigned char) *p)].cupper;
    p++;
  }
  *d = '\0';
}


// convert null terminated string to all little using encoding
void enmkallsmall(char * d, const char * p, const char * encoding)
{
  struct cs_info * csconv = get_current_cs(encoding);
  while (*p != '\0') { 
    *d++ = csconv[((unsigned char) *p)].clower;
    p++;
  }
  *d = '\0';
}


// convert null terminated string to have intial capital using encoding
void enmkinitcap(char * d, const char * p, const char * encoding)
{
  struct cs_info * csconv = get_current_cs(encoding);
  memcpy(d,p,(strlen(p)+1));
  if (*p != '\0') *d= csconv[((unsigned char)*p)].cupper;
}
#endif

// convert null terminated string to all caps 
void mkallcap(char * p, const struct cs_info * csconv)
{
  while (*p != '\0') { 
    *p = csconv[((unsigned char) *p)].cupper;
    p++;
  }
}


// convert null terminated string to all little
void mkallsmall(char * p, const struct cs_info * csconv)
{
  while (*p != '\0') { 
    *p = csconv[((unsigned char) *p)].clower;
    p++;
  }
}


// convert null terminated string to have intial capital
void mkinitcap(char * p, const struct cs_info * csconv)
{
  if (*p != '\0') *p = csconv[((unsigned char)*p)].cupper;
}


// XXX This function was rewritten for mozilla. Instead of storing the
// conversion tables static in this file, create them when needed
// with help the mozilla backend.
struct cs_info * get_current_cs(const char * es) {
  struct cs_info *ccs;

  nsCOMPtr<nsIUnicodeEncoder> encoder; 
  nsCOMPtr<nsIUnicodeDecoder> decoder; 
  nsCOMPtr<nsICaseConversion> caseConv;

  nsresult rv;
  nsCOMPtr<nsICharsetConverterManager> ccm = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return nsnull;

  rv = ccm->GetUnicodeEncoder(es, getter_AddRefs(encoder));
  if (encoder && NS_SUCCEEDED(rv))
    encoder->SetOutputErrorBehavior(encoder->kOnError_Replace, nsnull, '?');
  if (NS_FAILED(rv))
    return nsnull;
  rv = ccm->GetUnicodeDecoder(es, getter_AddRefs(decoder));

  caseConv = do_GetService(NS_UNICHARUTIL_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return nsnull;

  ccs = (struct cs_info *) malloc(256 * sizeof(cs_info));

  PRInt32 charLength = 256;
  PRInt32 uniLength = 512;
  char *source = (char *)malloc(charLength * sizeof(char));
  PRUnichar *uni = (PRUnichar *)malloc(uniLength * sizeof(PRUnichar));
  char *lower = (char *)malloc(charLength * sizeof(char));
  char *upper = (char *)malloc(charLength * sizeof(char));

  // Create a long string of all chars.
  unsigned int i;
  for (i = 0x00; i <= 0xff ; ++i) {
    source[i] = i;
  }

  // Convert this long string to unicode
  rv = decoder->Convert(source, &charLength, uni, &uniLength);

  // Do case conversion stuff, and convert back.
  caseConv->ToUpper(uni, uni, uniLength);
  encoder->Convert(uni, &uniLength, upper, &charLength);

  uniLength = 512;
  charLength = 256;
  rv = decoder->Convert(source, &charLength, uni, &uniLength);
  caseConv->ToLower(uni, uni, uniLength);
  encoder->Convert(uni, &uniLength, lower, &charLength);

  // Store
  for (i = 0x00; i <= 0xff ; ++i) {
    ccs[i].cupper = upper[i];
    ccs[i].clower = lower[i];
    
    if (ccs[i].clower != (unsigned char)i)
      ccs[i].ccase = true;
    else
      ccs[i].ccase = false;
      
  }

  free(source);
  free(uni);
  free(lower);
  free(upper);

  return ccs;
}


struct lang_map lang2enc[] = {
  {"ca","ISO8859-1"},
  {"cs","ISO8859-2"},
  {"da","ISO8859-1"},
  {"de","ISO8859-1"},
  {"el","ISO8859-7"},
  {"en","ISO8859-1"},
  {"es","ISO8859-1"},
  {"fr","ISO8859-1"},
  {"hr","ISO8859-2"},
  {"hu","ISO8859-2"},
  {"it","ISO8859-1"},
  {"la","ISO8859-1"},
  {"lv","ISO8859-13"},
  {"nl","ISO8859-1"},
  {"pl","ISO8859-2"},
  {"pt","ISO8859-1"},
  {"sv","ISO8859-1"},
  {"ru","KOI8-R"},
  {"bg","microsoft-cp1251"},
};


const char * get_default_enc(const char * lang) {
  int n = sizeof(lang2enc) / sizeof(lang2enc[0]);
  for (int i = 0; i < n; i++) {
    if (strcmp(lang,lang2enc[i].lang) == 0) {
      return lang2enc[i].def_enc;
    }
  }
  return NULL;
}
