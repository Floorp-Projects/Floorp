/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#define INCL_DOS
#include <os2.h>
#include "unidef.h"

#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include "nsILocale.h"
#include "nsLocaleOS2.h"
#include "nsLocaleCID.h"

static NS_DEFINE_IID(kILocaleIID, NS_ILOCALE_IID);
static NS_DEFINE_IID(kIOS2LocaleIID, NS_IOS2LOCALE_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

// for now
#ifndef DEBUG
 #define DEBUG
#endif

// nsISupports implementation
NS_IMPL_ADDREF(nsOS2Locale)
NS_IMPL_RELEASE(nsOS2Locale)

nsresult nsOS2Locale::QueryInterface( const nsIID &aIID, void **aInstancePtr)
{
   if( !aInstancePtr)
      return NS_ERROR_NULL_POINTER;

   *aInstancePtr = nsnull;

   if( aIID.Equals( kIOS2LocaleIID))
      *aInstancePtr = (void*) ((nsIOS2Locale*)this);
   else if( aIID.Equals( kILocaleIID))
      *aInstancePtr = (void*) ((nsILocale*)this);
   else if( aIID.Equals( kISupportsIID))
      *aInstancePtr = (void*) ((nsISupports*)(nsILocale*)(this));

   if( !*aInstancePtr)
      return NS_NOINTERFACE;

   NS_ADDREF_THIS();
   return NS_OK;
}

// ctor-dtor
nsOS2Locale::nsOS2Locale() : mLocaleObject(nsnull)
{
   NS_INIT_REFCNT();
}

nsOS2Locale::~nsOS2Locale()
{
   if( mLocaleObject)
      UniFreeLocaleObject( mLocaleObject);
}

// convert from a 'cool' netscape-style locale category to a sensible index
#define NUM_LOCALE_CATEGORIES 6

static int GetLocaleCategory( const nsString *aCat)
{
   int category = -1;

   if( aCat->EqualsWithConversion( "NSILOCALE_TIME"))          category = LC_TIME;
   else if( aCat->EqualsWithConversion( "NSILOCALE_COLLATE"))  category = LC_COLLATE;
   else if( aCat->EqualsWithConversion( "NSILOCALE_CTYPE"))    category = LC_CTYPE;
   else if( aCat->EqualsWithConversion( "NSILOCALE_MONETARY")) category = LC_MONETARY;
   else if( aCat->EqualsWithConversion( "NSILOCALE_MESSAGES")) category = LC_MESSAGES;
   else if( aCat->EqualsWithConversion( "NSILOCALE_NUMERIC"))  category = LC_NUMERIC;

   return category;
}

// nsIOS2Locale

// Init a complex locale - categories should be magic nsLocale words
nsresult nsOS2Locale::Init( nsString **aCatList,
                            nsString **aValList,
                            PRUint8    aLength)
{
   char    categories[NUM_LOCALE_CATEGORIES][12];
   PRUint8 i = 0;

   if( aLength < NUM_LOCALE_CATEGORIES)
   {
      // query default locale to fill in unspecified gaps in supplied info
      LocaleObject def_locale;
      UniCreateLocaleObject( UNI_MBS_STRING_POINTER,
                             (const void*) "", &def_locale);
      char *pszLCALL, *pszCategory;
      UniQueryLocaleObject( def_locale, LC_ALL, UNI_MBS_STRING_POINTER,
                            (void **)&pszLCALL);
   
      // init categories array from it
      pszCategory = strtok( pszLCALL, " ");
      while( pszCategory)
      {
         strcpy( categories[i], pszCategory);
         i++;
         pszCategory = strtok( 0, " ");
      }

      // fill in anything that didn't work (hmm)
      for( ; i < NUM_LOCALE_CATEGORIES; i++)
         strcpy( categories[i], "en_US");
   
      // free up locale stuff
      UniFreeMem( pszLCALL);
      UniFreeLocaleObject( def_locale);
   }

   // now fill in categories passed in
   for( i = 0; i < aLength; i++)
   {
      int category = GetLocaleCategory( aCatList[i]);
      if( category != -1)
         aValList[i]->ToCString( categories[i], 12);
#ifdef DEBUG
      else
      {
         char buff[12];
         aValList[i]->ToCString( buff, 12);
         printf( "Bad locale magic %s\n", buff);
      }
#endif
   }

   // build string of the various locales & create an object.
   nsresult rc = NS_OK;
   char localebuff[80];
   sprintf( localebuff, "%s %s %s %s %s %s",
            categories[0], categories[1], categories[2],
            categories[3], categories[4], categories[5]);
   int ret = UniCreateLocaleObject( UNI_MBS_STRING_POINTER,
                                    (const void*) localebuff,
                                    &mLocaleObject);

   if( ret)
   {
#ifdef DEBUG
      printf( "Locale init string `%s' gave rc %d\n", localebuff, ret);
#endif
      // didn't work.  Try something else...
      rc = Init( "");
   }

   return rc;
}

// Init a locale object from a xx-XX style name
nsresult nsOS2Locale::Init( const nsString &aLocaleName)
{
   char szLocale[7] = { 0 };

   aLocaleName.ToCString( szLocale, 6);
   nsresult rc = NS_ERROR_FAILURE;

   if( strlen( szLocale) == 5)
   {
      // implementations are not consistent: some use _ & some - as separator.
      szLocale[2] = '_';
      rc = Init( szLocale);

      if( NS_FAILED(rc))
      {
#ifdef DEBUG
         printf( "Can't find locale %s, using fallback\n", szLocale);
#endif
         rc = Init( ""); // XXX maybe this should be 0 -> UNIV ?
      }
   }
#ifdef DEBUG
   else printf( "Malformed locale string %s\n", szLocale);
#endif

   return rc;
}

nsresult nsOS2Locale::Init( char *pszLocale)
{
   nsresult nr = NS_ERROR_FAILURE;

   int ret = UniCreateLocaleObject( UNI_MBS_STRING_POINTER,
                                    (const void *) pszLocale,
                                    &mLocaleObject);
   if( !ret)
      nr = NS_OK;

   return nr;
}

// Get the OS/2 locale object
nsresult nsOS2Locale::GetLocaleObject( LocaleObject *aLocaleObject)
{
   if( !aLocaleObject)
      return NS_ERROR_NULL_POINTER;

   *aLocaleObject = mLocaleObject;
   return NS_OK;
}

// nsILocale
nsresult nsOS2Locale::GetCategory( const nsString *aCat, nsString *aLocale)
{
   if( !aCat || !aLocale)
      return NS_ERROR_NULL_POINTER;

   nsresult rc = NS_OK;

   int category = 0;

   category = GetLocaleCategory( aCat);
   if( -1 == category)
      rc = NS_ERROR_FAILURE;

   if( NS_SUCCEEDED(rc))
   {
      char *pszLocale = 0;
      UniQueryLocaleObject( mLocaleObject, category,
                            UNI_MBS_STRING_POINTER,
                            (void**) &pszLocale);
      aLocale->AssignWithConversion( pszLocale);
      UniFreeMem( pszLocale);
   }

   return rc;
}

// System locale
nsSystemLocale::nsSystemLocale()
{
   Init( ""); // create locale based on value of LANG and friends
}

// XXXX STUBS
NS_IMETHODIMP 
nsOS2Locale::GetPlatformLocale(const nsString* locale,char* os2Locale, size_t length)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOS2Locale::GetXPLocale(const char* os2Locale, nsString* locale)
{
  return NS_OK;
}
