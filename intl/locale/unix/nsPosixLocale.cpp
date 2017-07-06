/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "nsString.h"
#include "nsPosixLocale.h"
#include "mozilla/Sprintf.h"
#include "plstr.h"
#include "nsReadableUtils.h"

static bool
ParseLocaleString(const char* locale_string, char* language, char* country, char* extra, char separator);

nsresult
nsPosixLocale::GetPlatformLocale(const nsAString& locale, nsACString& posixLocale)
{
  char  country_code[MAX_COUNTRY_CODE_LEN+1];
  char  lang_code[MAX_LANGUAGE_CODE_LEN+1];
  char  extra[MAX_EXTRA_LEN+1];
  char  posix_locale[MAX_LOCALE_LEN+1];
  NS_LossyConvertUTF16toASCII xp_locale(locale);

  if (!xp_locale.IsEmpty()) {
    if (!ParseLocaleString(xp_locale.get(),lang_code,country_code,extra,'-')) {
//      strncpy(posixLocale,"C",length);
      posixLocale = xp_locale;  // use xp locale if parse failed
      return NS_OK;
    }

    if (*country_code) {
      if (*extra) {
        SprintfLiteral(posix_locale,"%s_%s.%s",lang_code,country_code,extra);
      }
      else {
        SprintfLiteral(posix_locale,"%s_%s",lang_code,country_code);
      }
    }
    else {
      if (*extra) {
        SprintfLiteral(posix_locale,"%s.%s",lang_code,extra);
      }
      else {
        SprintfLiteral(posix_locale,"%s",lang_code);
      }
    }

    posixLocale = posix_locale;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsPosixLocale::GetXPLocale(const char* posixLocale, nsAString& locale)
{
  char  country_code[MAX_COUNTRY_CODE_LEN+1];
  char  lang_code[MAX_LANGUAGE_CODE_LEN+1];
  char  extra[MAX_EXTRA_LEN+1];
  char  posix_locale[MAX_LOCALE_LEN+1];

  if (posixLocale!=nullptr) {
    if (strcmp(posixLocale,"C")==0 || strcmp(posixLocale,"POSIX")==0) {
      locale.AssignLiteral("en-US");
      return NS_OK;
    }
    if (strcmp(posixLocale,"C.UTF-8")==0) {
      locale.AssignLiteral("en-US.UTF-8");
      return NS_OK;
    }
    if (!ParseLocaleString(posixLocale,lang_code,country_code,extra,'_')) {
//      * locale = "x-user-defined";
      // use posix if parse failed
      CopyASCIItoUTF16(nsDependentCString(posixLocale), locale);
      return NS_OK;
    }

    // Special case: substitute "nb" (Norwegian Bokmal) for macrolanguage
    // code "no" (Norwegian)
    if (nsDependentCString(lang_code).LowerCaseEqualsLiteral("no")) {
      lang_code[1] = 'b';
    }

    if (*country_code) {
      SprintfLiteral(posix_locale,"%s-%s",lang_code,country_code);
    }
    else {
      SprintfLiteral(posix_locale,"%s",lang_code);
    }

    CopyASCIItoUTF16(nsDependentCString(posix_locale), locale);
    return NS_OK;

  }

    return NS_ERROR_FAILURE;

}

//
// returns false/true depending on if it was of the form LL-CC.Extra
static bool
ParseLocaleString(const char* locale_string, char* language, char* country, char* extra, char separator)
{
  const char *src = locale_string;
  char modifier[MAX_EXTRA_LEN+1];
  char *dest;
  int dest_space, len;

  *language = '\0';
  *country = '\0';
  *extra = '\0';
  if (strlen(locale_string) < 2) {
    return(false);
  }

  //
  // parse the language part
  //
  dest = language;
  dest_space = MAX_LANGUAGE_CODE_LEN;
  while ((*src) && (isalpha(*src)) && (dest_space--)) {
    *dest++ = tolower(*src++);
  }
  *dest = '\0';
  len = dest - language;
  if ((len != 2) && (len != 3)) {
    NS_ASSERTION((len == 2) || (len == 3), "language code too short");
    NS_ASSERTION(len < 3, "reminder: verify we can handle 3+ character language code in all parts of the system; eg: language packs");
    *language = '\0';
    return(false);
  }

  // check if all done
  if (*src == '\0') {
    return(true);
  }

  if ((*src != '_') && (*src != '-') && (*src != '.') && (*src != '@')) {
    NS_ASSERTION(isalpha(*src), "language code too long");
    NS_ASSERTION(!isalpha(*src), "unexpected language/country separator");
    *language = '\0';
    return(false);
  }

  //
  // parse the country part
  //
  if ((*src == '_') || (*src == '-')) {
    src++;
    dest = country;
    dest_space = MAX_COUNTRY_CODE_LEN;
    while ((*src) && (isalpha(*src)) && (dest_space--)) {
      *dest++ = toupper(*src++);
    }
    *dest = '\0';
    len = dest - country;
    if (len != 2) {
      NS_ASSERTION(len == 2, "unexpected country code length");
      *language = '\0';
      *country = '\0';
      return(false);
    }
  }

  // check if all done
  if (*src == '\0') {
    return(true);
  }

  if ((*src != '.') && (*src != '@')) {
    NS_ASSERTION(isalpha(*src), "country code too long");
    NS_ASSERTION(!isalpha(*src), "unexpected country/extra separator");
    *language = '\0';
    *country = '\0';
    return(false);
  }

  //
  // handle the extra part
  //
  if (*src == '.') {
    src++;  // move past the extra part separator
    dest = extra;
    dest_space = MAX_EXTRA_LEN;
    while ((*src) && (*src != '@') && (dest_space--)) {
      *dest++ = *src++;
    }
    *dest = '\0';
    len = dest - extra;
    if (len < 1) {
      NS_ASSERTION(len > 0, "found country/extra separator but no extra code");
      *language = '\0';
      *country = '\0';
      *extra = '\0';
      return(false);
    }
  }

  // check if all done
  if (*src == '\0') {
    return(true);
  }

  //
  // handle the modifier part
  //

  if (*src == '@') {
    src++;  // move past the modifier separator
    NS_ASSERTION(strcmp("euro",src) == 0, "found non euro modifier");
    dest = modifier;
    dest_space = MAX_EXTRA_LEN;
    while ((*src) && (dest_space--)) {
      *dest++ = *src++;
    }
    *dest = '\0';
    len = dest - modifier;
    if (len < 1) {
      NS_ASSERTION(len > 0, "found modifier separator but no modifier code");
      *language = '\0';
      *country = '\0';
      *extra = '\0';
      *modifier = '\0';
      return(false);
    }
  }

  // check if all done
  if (*src == '\0') {
    return(true);
  }

  NS_ASSERTION(*src == '\0', "extra/modifier code too long");
  *language = '\0';
  *country = '\0';
  *extra = '\0';

  return(false);
}

