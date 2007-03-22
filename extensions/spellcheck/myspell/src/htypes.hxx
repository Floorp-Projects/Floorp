#ifndef _HTYPES_HXX_
#define _HTYPES_HXX_

#define MAXDELEN    256

#define ROTATE_LEN   5

#define ROTATE(v,q) \
   (v) = ((v) << (q)) | (((v) >> (32 - q)) & ((1 << (q))-1));

struct hentry
{
  short    wlen;
  short    alen;
  char *   word;
  char *   astr;
  struct   hentry * next;
}; 

#endif
