/* unitest.c: Test driver for Unicode strings module unistring.c */

/* public declarations */
#include "unistring.h"

#include <assert.h>

int main(int argc, char *argv[])
{
  UNICHAR dest[6], dest2[6];

  printf("\nTesting ucscopy and ucslen ...\n");

  ucscopy(dest, "abcdef", 0);
  printf("ucscopy(dest, \"abcdef\",    0)\n");

  ucscopy(dest, "abcdef", 6);
  printf("ucscopy(dest, \"abcdef\",    6): dest=");
  ucsprint(stdout, dest, 6);
  printf(" (abcdef)\n");

  ucscopy(dest, "",       6);
  printf("ucscopy(dest, \"\",          6): dest=");
  ucsprint(stdout, dest, 6);
  printf(" (^@^@^@^@^@^@)\n");

  printf("ucslen(dest) = %d (0)\n", ucslen(dest));

  ucscopy(dest, "babc",    6);
  printf("ucscopy(dest, \"babc\",       6): dest=");
  ucsprint(stdout, dest, 6);
  printf(" (babc^@^@)\n");

  printf("ucslen(dest) = %d (4)\n", ucslen(dest));

  printf("\nTesting ucschr ...\n");

  printf("*ucschr(\"babc\", U_NUL) = '%c',%d ('',4)\n",
         (char) *ucschr(dest, U_NUL), ucschr(dest, U_NUL)-dest);
  
  printf("*ucschr(\"babc\", U_b_CHAR) = '%c',%d ('b',0)\n",
         (char) *ucschr(dest, U_b_CHAR), ucschr(dest, U_b_CHAR)-dest);
  
  printf("*ucschr(\"babc\", U_a_CHAR) = '%c',%d ('a',1)\n",
         (char) *ucschr(dest, U_a_CHAR), ucschr(dest, U_a_CHAR)-dest);
  
  printf("*ucschr(\"babc\", U_c_CHAR) = '%c',%d ('c',3)\n",
         (char) *ucschr(dest, U_c_CHAR), ucschr(dest, U_c_CHAR)-dest);

  printf("ucschr(\"babc\", U_d_CHAR) = 0x%x (0x0)\n", ucschr(dest, U_d_CHAR));
  
  printf("\nTesting ucsrchr ...\n");

  printf("*ucsrchr(\"babc\", U_NUL) = '%c',%d ('',4)\n",
         (char) *ucsrchr(dest, U_NUL), ucsrchr(dest, U_NUL)-dest);

  printf("*ucsrchr(\"babc\", U_b_CHAR) = '%c',%d ('b',2)\n",
         (char) *ucsrchr(dest, U_b_CHAR), ucsrchr(dest, U_b_CHAR)-dest);

  printf("*ucsrchr(\"babc\", U_a_CHAR) = '%c', %d ('a',1)\n",
         (char) *ucsrchr(dest, U_a_CHAR), ucsrchr(dest, U_a_CHAR)-dest);

  printf("*ucschr(\"babc\", U_c_CHAR) = '%c',%d ('c',3)\n",
         (char) *ucschr(dest, U_c_CHAR), ucschr(dest, U_c_CHAR)-dest);

  printf("ucsrchr(\"babc\", U_d_CHAR) = 0x%x (0x0)\n", ucsrchr(dest, U_d_CHAR));

  printf("\nTesting ucscmp ...\n");
  ucscopy(dest,  "", 6);
  ucscopy(dest2, "", 6);
  printf("ucscmp(\"\",\"\") = %d (0)\n", ucscmp(dest,dest));

  ucscopy(dest,  "abc", 6);
  printf("ucscmp(\"abc\",\"\") = %d (97)\n", ucscmp(dest,dest2));
  printf("ucscmp(\"\",\"abc\") = %d (-97)\n", ucscmp(dest2,dest));

  ucscopy(dest2, "abe", 6);
  printf("ucscmp(\"abc\",\"abc\") = %d (0)\n", ucscmp(dest,dest));
  printf("ucscmp(\"abc\",\"abe\") = %d (-2)\n", ucscmp(dest,dest2));
  printf("ucscmp(\"abe\",\"abc\") = %d (2)\n", ucscmp(dest2,dest));

  ucscopy(dest2, "abcd", 6);
  printf("ucscmp(\"abc\",\"abcd\") = %d (-100)\n", ucscmp(dest,dest2));
  printf("ucscmp(\"abcd\",\"abc\") = %d (100)\n", ucscmp(dest2,dest));

  printf("\nTesting ucsncmp ...\n");
  ucscopy(dest,  "", 6);
  ucscopy(dest2, "", 6);
  printf("ucsncmp(\"\",\"\",6) = %d (0)\n", ucsncmp(dest,dest,6));

  ucscopy(dest,  "abc", 6);
  printf("ucsncmp(\"abc\",\"\",6) = %d (97)\n", ucsncmp(dest,dest2,6));
  printf("ucsncmp(\"\",\"abc\",6) = %d (-97)\n", ucsncmp(dest2,dest,6));

  ucscopy(dest2, "abe", 6);
  printf("ucsncmp(\"abc\",\"abc\",4) = %d (0)\n", ucsncmp(dest,dest,4));
  printf("ucsncmp(\"abc\",\"abc\",3) = %d (0)\n", ucsncmp(dest,dest,3));

  printf("ucsncmp(\"abc\",\"abe\",5) = %d (-2)\n", ucsncmp(dest,dest2,5));
  printf("ucsncmp(\"abc\",\"abe\",4) = %d (-2)\n", ucsncmp(dest,dest2,4));
  printf("ucsncmp(\"abc\",\"abe\",3) = %d (-2)\n", ucsncmp(dest,dest2,3));
  printf("ucsncmp(\"abc\",\"abe\",2) = %d (0)\n", ucsncmp(dest,dest2,2));
  printf("ucsncmp(\"abc\",\"abe\",1) = %d (0)\n", ucsncmp(dest,dest2,1));
  printf("ucsncmp(\"abc\",\"abe\",0) = %d (0)\n", ucsncmp(dest,dest2,0));

  printf("ucsncmp(\"abe\",\"abc\",4) = %d (2)\n", ucsncmp(dest2,dest,4));
  printf("ucsncmp(\"abe\",\"abc\",3) = %d (2)\n", ucsncmp(dest2,dest,3));

  ucscopy(dest,  "abcde", 6);
  printf("ucsncmp(\"abcde\",\"abe\",2) = %d (0)\n", ucsncmp(dest,dest2,2));
  printf("ucsncmp(\"abcde\",\"abe\",3) = %d (-2)\n", ucsncmp(dest,dest2,3));
  printf("ucsncmp(\"abcde\",\"abe\",4) = %d (-2)\n", ucsncmp(dest,dest2,4));
  printf("ucsncmp(\"abe\",\"abcde\",2) = %d (0)\n", ucsncmp(dest2,dest,2));
  printf("ucsncmp(\"abe\",\"abcde\",6) = %d (2)\n", ucsncmp(dest2,dest,6));

  printf("\nTesting ucsncpy ...\n");
  ucscopy(dest2, "abcde", 5);

  ucsncpy(dest, dest2, 0);
  printf("ucsncpy(dest, \"abcde\",    0)\n");

  ucsncpy(dest, dest2, 5);
  printf("ucsncpy(dest, \"abcde\",    5): dest=");
  ucsprint(stdout, dest, 5);
  printf(" (abcde)\n");

  ucscopy(dest2, "", 6);
  ucsncpy(dest, dest2,       5);
  printf("ucsncpy(dest, \"\",         5): dest=");
  ucsprint(stdout, dest, 5);
  printf(" (^@^@^@^@^@)\n");

  ucscopy(dest2, "bab", 6);
  ucsncpy(dest, dest2,    5);
  printf("ucsncpy(dest, \"bab\",      5): dest=");
  ucsprint(stdout, dest, 5);
  printf(" (bab^@^@)\n");

  printf("\nTesting ucsstr ...\n");
  ucscopy(dest, "abc", 6);
  ucscopy(dest2, "", 6);
  printf("ucsstr(\"abc\",\"\") = %d (0)\n", ucsstr(dest, dest2)-dest);
  ucscopy(dest2, "a", 6);
  printf("ucsstr(\"abc\",\"a\") = %d (0)\n", ucsstr(dest, dest2)-dest);
  ucscopy(dest2, "ab", 6);
  printf("ucsstr(\"abc\",\"ab\") = %d (0)\n", ucsstr(dest, dest2)-dest);
  ucscopy(dest2, "abc", 6);
  printf("ucsstr(\"abc\",\"abc\") = %d (0)\n", ucsstr(dest, dest2)-dest);
  ucscopy(dest2, "bc", 6);
  printf("ucsstr(\"abc\",\"bc\") = %d (1)\n", ucsstr(dest, dest2)-dest);
  ucscopy(dest2, "c", 6);
  printf("ucsstr(\"abc\",\"c\") = %d (2)\n", ucsstr(dest, dest2)-dest);
  ucscopy(dest2, "d", 6);
  printf("ucsstr(\"abc\",\"d\") = 0x%x (0x0)\n", ucsstr(dest, dest2));
  ucscopy(dest2, "ac", 6);
  printf("ucsstr(\"abc\",\"ac\") = 0x%x (0x0)\n", ucsstr(dest, dest2));
  ucscopy(dest2, "acb", 6);
  printf("ucsstr(\"abc\",\"acb\") = 0x%x (0x0)\n", ucsstr(dest, dest2));
  ucscopy(dest2, "abcd", 6);
  printf("ucsstr(\"abc\",\"abcd\") = 0x%x (0x0)\n", ucsstr(dest, dest2));

  printf("\nTesting ucsspn ...\n");
  ucscopy(dest, "babc", 6);
  ucscopy(dest2, "", 6);
  printf("ucsspn(\"babc\",\"\") = %d (0)\n", ucsspn(dest, dest2));
  ucscopy(dest2, "b", 6);
  printf("ucsspn(\"babc\",\"b\") = %d (1)\n", ucsspn(dest, dest2));
  ucscopy(dest2, "ab", 6);
  printf("ucsspn(\"babc\",\"ab\") = %d (3)\n", ucsspn(dest, dest2));
  ucscopy(dest2, "abc", 6);
  printf("ucsspn(\"babc\",\"abc\") = %d (4)\n", ucsspn(dest, dest2));
  ucscopy(dest2, "cba", 6);
  printf("ucsspn(\"babc\",\"cba\") = %d (4)\n", ucsspn(dest, dest2));
  ucscopy(dest2, "abcd", 6);
  printf("ucsspn(\"babc\",\"abcd\") = %d (4)\n", ucsspn(dest, dest2));
  ucscopy(dest2, "abcde", 6);
  printf("ucsspn(\"babc\",\"abcde\") = %d (4)\n", ucsspn(dest, dest2));
  ucscopy(dest2, "c", 6);
  printf("ucsspn(\"babc\",\"c\") = %d (0)\n", ucsspn(dest, dest2));
  ucscopy(dest2, "d", 6);
  printf("ucsspn(\"babc\",\"d\") = %d (0)\n", ucsspn(dest, dest2));
  ucscopy(dest2, "cd", 6);
  printf("ucsspn(\"babc\",\"cd\") = %d (0)\n", ucsspn(dest, dest2));

  printf("\nTesting ucscspn ...\n");
  ucscopy(dest, "babc", 6);
  ucscopy(dest2, "", 6);
  printf("ucscspn(\"babc\",\"\") = %d (4)\n", ucscspn(dest, dest2));
  ucscopy(dest2, "b", 6);
  printf("ucscspn(\"babc\",\"b\") = %d (0)\n", ucscspn(dest, dest2));
  ucscopy(dest2, "ab", 6);
  printf("ucscspn(\"babc\",\"ab\") = %d (0)\n", ucscspn(dest, dest2));
  ucscopy(dest2, "abc", 6);
  printf("ucscspn(\"babc\",\"abc\") = %d (0)\n", ucscspn(dest, dest2));
  ucscopy(dest2, "cba", 6);
  printf("ucscspn(\"babc\",\"cba\") = %d (0)\n", ucscspn(dest, dest2));
  ucscopy(dest2, "abcd", 6);
  printf("ucscspn(\"babc\",\"abcd\") = %d (0)\n", ucscspn(dest, dest2));
  ucscopy(dest2, "abcde", 6);
  printf("ucscspn(\"babc\",\"abcde\") = %d (0)\n", ucscspn(dest, dest2));
  ucscopy(dest2, "a", 6);
  printf("ucscspn(\"babc\",\"a\") = %d (1)\n", ucscspn(dest, dest2));
  ucscopy(dest2, "c", 6);
  printf("ucscspn(\"babc\",\"c\") = %d (3)\n", ucscspn(dest, dest2));
  ucscopy(dest2, "cd", 6);
  printf("ucscspn(\"babc\",\"cd\") = %d (3)\n", ucscspn(dest, dest2));
  ucscopy(dest2, "d", 6);
  printf("ucscspn(\"babc\",\"d\") = %d (4)\n", ucscspn(dest, dest2));

  return;
}
