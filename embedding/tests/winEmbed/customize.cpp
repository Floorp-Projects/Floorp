/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *  Doug Turner <dougt@netscape.com> 
 *  Adam Lock <adamlock@netscape.com>
 *  J.Betak <jbetak@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include "dcck.h"

typedef unsigned int PRUint32;
typedef PRUint32 nsresult;
typedef int PRIntn;
typedef PRIntn PRBool;

#define PR_TRUE             1
#define PR_FALSE            0
#define PR_fprintf          printf
#define NS_FAILED(_nsresult) ((_nsresult) & 0x80000000)
#define NS_SUCCEEDED(_nsresult) (!((_nsresult) & 0x80000000))
#define HELP_SPACER_1   "\t"
#define HELP_SPACER_2   "\t\t"
#define HELP_SPACER_4   "\t\t\t\t"
#define NS_OK                              0

static const unsigned char uc[] =
{
    '\000', '\001', '\002', '\003', '\004', '\005', '\006', '\007',
    '\010', '\011', '\012', '\013', '\014', '\015', '\016', '\017',
    '\020', '\021', '\022', '\023', '\024', '\025', '\026', '\027',
    '\030', '\031', '\032', '\033', '\034', '\035', '\036', '\037',
    ' ',    '!',    '"',    '#',    '$',    '%',    '&',    '\'',
    '(',    ')',    '*',    '+',    ',',    '-',    '.',    '/',
    '0',    '1',    '2',    '3',    '4',    '5',    '6',    '7',
    '8',    '9',    ':',    ';',    '<',    '=',    '>',    '?',
    '@',    'A',    'B',    'C',    'D',    'E',    'F',    'G',
    'H',    'I',    'J',    'K',    'L',    'M',    'N',    'O',
    'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W',
    'X',    'Y',    'Z',    '[',    '\\',   ']',    '^',    '_',
    '`',    'A',    'B',    'C',    'D',    'E',    'F',    'G',
    'H',    'I',    'J',    'K',    'L',    'M',    'N',    'O',
    'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W',
    'X',    'Y',    'Z',    '{',    '|',    '}',    '~',    '\177',
    '\200', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
    '\210', '\211', '\212', '\213', '\214', '\215', '\216', '\217',
    '\220', '\221', '\222', '\223', '\224', '\225', '\226', '\227',
    '\230', '\231', '\232', '\233', '\234', '\235', '\236', '\237',
    '\240', '\241', '\242', '\243', '\244', '\245', '\246', '\247',
    '\250', '\251', '\252', '\253', '\254', '\255', '\256', '\257',
    '\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
    '\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',
    '\300', '\301', '\302', '\303', '\304', '\305', '\306', '\307',
    '\310', '\311', '\312', '\313', '\314', '\315', '\316', '\317',
    '\320', '\321', '\322', '\323', '\324', '\325', '\326', '\327',
    '\330', '\331', '\332', '\333', '\334', '\335', '\336', '\337',
    '\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
    '\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
    '\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
    '\370', '\371', '\372', '\373', '\374', '\375', '\376', '\377'
};


PRIntn
PL_strcasecmp(const char *a, const char *b);


PRIntn
PL_strcasecmp(const char *a, const char *b)
{
    const unsigned char *ua = (const unsigned char *)a;
    const unsigned char *ub = (const unsigned char *)b;

    if( ((const char *)0 == a) || (const char *)0 == b ) 
        return (PRIntn)(a-b);

    while( (uc[*ua] == uc[*ub]) && ('\0' != *a) )
    {
        a++;
        ua++;
        ub++;
    }

    return (PRIntn)(uc[*ua] - uc[*ub]);
}

static void DumpHelp(char *appname)
{
  printf("\n");
  printf("Usage: %s [ options ... ]\n", appname);
  printf("       where options include:\n");
  printf("\n");

  printf("-h or -help        %sPrint this message.\n",HELP_SPACER_1);
  printf("\n");
  printf("-appDir            %sreturn absolute application installation path\n",HELP_SPACER_1);
  printf("-Profile <Profile> %sreturn absolute path to the given user profile\n",HELP_SPACER_1);
  printf("-currentProfile    %sreturn absolute path to the current user profile\n",HELP_SPACER_1);
  printf("-defaultPref       %sappend \"isp.js\" to application defaults in \"all-ns\".\n",HELP_SPACER_1);
  printf("-userPref <Profile>%sappend \"isp.js\" to user defaults in \"prefs.js\"\n",HELP_SPACER_1);


}


int main(int argc, char *argv[])
{
  for (int i=1; i<argc; i++) {

    if ((PL_strcasecmp(argv[i], "-h") == 0)
        || (PL_strcasecmp(argv[i], "-help") == 0)
        || (PL_strcasecmp(argv[i], "--help") == 0)
        || (PL_strcasecmp(argv[i], "/h") == 0)
        || (PL_strcasecmp(argv[i], "/help") == 0)
        || (PL_strcasecmp(argv[i], "/?") == 0)
      ) {
      DumpHelp(argv[0]);
      return 0;
    }

    if ((PL_strcasecmp(argv[i], "-appDir") == 0)
        || (PL_strcasecmp(argv[i], "--appDir") == 0)
        || (PL_strcasecmp(argv[i], "/appDir") == 0)
      ) {
      char* retBuf;
      retBuf = GetNSInstallDir();
      printf("\n%s\n", retBuf);
      return 0;
    }

    if ((PL_strcasecmp(argv[i], "-Profile") == 0)
        || (PL_strcasecmp(argv[i], "--Profile") == 0)
        || (PL_strcasecmp(argv[i], "/Profile") == 0)
      ) {
      char* retBuf;
      if ((i+1)<argc) { 
        retBuf = GetNSUserProfile(argv[i+1]);
        printf("\n%s\n", retBuf);
        return 0;
      } else {
        return 1;
      }
    }

    if ((PL_strcasecmp(argv[i], "-currentProfile") == 0)
        || (PL_strcasecmp(argv[i], "--currentProfile") == 0)
        || (PL_strcasecmp(argv[i], "/currentProfile") == 0)
      ) {
      char* retBuf;
      retBuf = GetNSCurrentProfile();
      printf("\n%s\n", retBuf);
      return 0;
    }

    if ((PL_strcasecmp(argv[i], "-defaultPref") == 0)
        || (PL_strcasecmp(argv[i], "--defaultPref") == 0)
        || (PL_strcasecmp(argv[i], "/defaultPref") == 0)
      ) {
      if NS_SUCCEEDED(AppendDefaultPref(GetPrefTemplateFilePath(), 
                       GetNSPrefDefaultsFilePath())) {
        printf("Netscape account defaults have been modified.\n");
      } else {
        printf("Error.\n");
      }
      return 0;
    }
   
    if ((PL_strcasecmp(argv[i], "-userPrefJS") == 0)
        || (PL_strcasecmp(argv[i], "--userPrefJS") == 0)
        || (PL_strcasecmp(argv[i], "/userPrefJS") == 0)
      ) {
      if ((i+1)<argc) { 
        char* retBuf;
        retBuf = GetNSUserProfileJS(argv[i+1]);
        printf("\n%s\n", retBuf);
        return 0;
      } else {
        return 1;
      }
    }

    if ((PL_strcasecmp(argv[i], "-userPref") == 0)
      || (PL_strcasecmp(argv[i], "--userPref") == 0)
      || (PL_strcasecmp(argv[i], "/userPref") == 0)
    ) {
      if ((i+1)<argc) { 
        if NS_SUCCEEDED(AppendUserPref(GetPrefTemplateFilePath(), 
                         argv[i+1])) {
          printf("Netscape account defaults have been modified.\n");
        } else {
          printf("Error.\n");
        }
        return 0;
      } else {
        return 1;
      }
    }
  }

	int rv = 0;
  return rv;
}

