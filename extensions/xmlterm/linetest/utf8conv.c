/* utf8conv.c: Test driver for UTF8 conversions in unistring.c */

/* public declarations */
#include "unistring.h"

#include <assert.h>

int main(int argc, char *argv[])
{
  int argNo, ucs16_opt, utf8_opt, j;
  int result, remaining, encoded, decoded;

  if (argc < 2) {
    printf("Usage: %s (-ucs16 <unicode-hex-string>|-utf8 <utf8-hex-string>|<ascii-chars>)\n",
           argv[0]);
    exit(1);
  }

  ucs16_opt = 0;
  utf8_opt = 0;
  argNo = 1;

  if (argc > 1) {
    argNo = 2;
    if (strcmp(argv[1],"-ucs16") == 0) {
      ucs16_opt = 1;
    } else if (strcmp(argv[1],"-utf8") == 0) {
      utf8_opt = 1;
    }
  }

  if (ucs16_opt) {
    char* tems = argv[argNo];
    int nus = strlen(argv[argNo])/4;
    int ns = 3*nus;
    UNICHAR* us = (UNICHAR*) malloc((nus+1)*sizeof(UNICHAR));
    unsigned char* s = (unsigned char*) malloc(ns+1);
    char temus[5] = {0,0,0,0,0};

    for (j=0; j<nus; j++) {
      temus[0] = tems[4*j];
      temus[1] = tems[4*j+1];
      temus[2] = tems[4*j+2];
      temus[3] = tems[4*j+3];
      sscanf(temus, "%x", &us[j]);
    }
    us[nus] = U_NUL;

    ucsprint(stderr, us, nus);
    fprintf(stderr, "\n");

    ucstoutf8(us, nus, s, ns, &remaining, &encoded);
    assert(encoded <= ns);
    ns = encoded;
    s[ns] = '\0';

    printf("UTF8(%d)=0x", ns);
    for (j=0; j<ns; j++) {
      printf("%02x",s[j]);
    }
    printf(", remaining=%d\n", remaining);

    result = utf8toucs(s, ns, us, nus, 0, &remaining, &decoded);
    assert(decoded <= nus);
    us[decoded] = U_NUL;

    printf("UCS(%d)=0x", decoded);

    for (j=0; j<decoded; j++) {
      printf("%04x",us[j]);
    }
    printf(", remaining=%d, result=%d\n", remaining, result);

    free(us);
    free(s);

  } else {
    char* tems = argv[argNo];
    int ns, nus;
    char* s;
    UNICHAR* us;

    ns = (utf8_opt) ? strlen(argv[argNo])/2 : strlen(argv[argNo]);
    s = (char*) malloc(ns+1);
    nus = ns;
    us = (UNICHAR*) malloc((nus+1)*sizeof(UNICHAR));

    if (utf8_opt) {
      char temstr[3] = {0,0,0};
      int ival;
      for (j=0; j<ns; j++) {
        temstr[0] = tems[2*j];
        temstr[1] = tems[2*j+1];
        sscanf(temstr, "%x", &ival);
        s[j] = (unsigned char) ival;
      }
    } else {
      for (j=0; j<ns; j++)
        s[j] = tems[j];
    }
    s[ns] = '\0';

    result = utf8toucs(s, ns, us, nus, 0, &remaining, &decoded);
    assert(decoded <= nus);
    nus = decoded;
    us[nus] = U_NUL;

    printf("UCS(%d)=0x", nus);

    for (j=0; j<nus; j++) {
      printf("%04x",us[j]);
    }

    printf(", remaining=%d, result=%d\n", remaining, result);

    ucstoutf8(us, nus, s, ns, &remaining, &encoded);

    assert(encoded <= ns);
    s[encoded] = '\0';

    if (utf8_opt) {
      printf("UTF8(%d)=0x", encoded);

      for (j=0; j<encoded; j++)
        printf("%02x", s[j]);

      printf(", remaining=%d\n", remaining);

    } else {
      printf("UTF8(%d)='%s', remaining=%d\n", encoded, s, remaining);
    }

    free(us);
    free(s);
  }

  return 0;
}
