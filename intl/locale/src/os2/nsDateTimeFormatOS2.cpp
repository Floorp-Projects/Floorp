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
#include "ulsitem.h"

#include "nsDateTimeFormatOS2.h"
#include "nsILocaleOS2.h"
#include "nsCOMPtr.h"

static NS_DEFINE_IID(kILocaleOS2IID, NS_ILOCALEOS2_IID);
static NS_DEFINE_IID(kIDateTimeFormatIID, NS_IDATETIMEFORMAT_IID);

// nsISupports implementation
NS_IMPL_ISUPPORTS(nsDateTimeFormatOS2,kIDateTimeFormatIID)

// ctor/dtor
nsDateTimeFormatOS2::nsDateTimeFormatOS2()
{
   NS_INIT_REFCNT();
}

nsDateTimeFormatOS2::~nsDateTimeFormatOS2()
{
}

// nsIDateTimeFormat

nsresult nsDateTimeFormatOS2::FormatTime( nsILocale       *aLocale, 
                               const nsDateFormatSelector  aDateFormatSelector, 
                               const nsTimeFormatSelector  aTimeFormatSelector, 
                               const time_t                aTime,
                               nsString                   &aStringOut)
{
   struct tm *now = 0;
   now = localtime( &aTime); // XXX dodgy (this whole thing ought to be using
                             //     PRTimes etc., anyway)
   return FormatTMTime( aLocale, aDateFormatSelector, aTimeFormatSelector,
                        now, aStringOut);
}

// performs a locale sensitive date formatting operation on the struct tm parameter
nsresult nsDateTimeFormatOS2::FormatTMTime( nsILocale     *aLocale, 
                               const nsDateFormatSelector  aDateFormatSelector, 
                               const nsTimeFormatSelector  aTimeFormatSelector, 
                               const struct tm            *aTime, 
                               nsString                   &aStringOut)
{
   if( !aLocale || !aTime)
      return NS_ERROR_NULL_POINTER;

   nsCOMPtr <nsILocaleOS2> os2locale;
   nsresult      rc = NS_ERROR_FAILURE;

   // find a sane locale object

   if( NS_SUCCEEDED(aLocale->QueryInterface( kILocaleOS2IID,
                                             getter_AddRefs(os2locale))))
   {
      LocaleObject locale_object = 0;
      os2locale->GetLocaleObject( &locale_object);

      // Build up a strftime-style format string - date first and then
      // time.
      //
      // XXX Usage of %c for "long date" copied from unix, even though
      //     it's documented as producing time as well.  Change it if
      //     weird things happen.
      //
      // XXX Not sure when if ever we should look at PM_National in the
      //     ini and override locale-defaults.  Maybe only if this locale
      //     is the same as the system's LC_TIME locale?
      //
      #define FORMAT_BUFF_LEN 20 // update this if things get complex below

      UniChar format[ FORMAT_BUFF_LEN] = { 0 };

      UniChar *pString = nsnull;

      // date
      switch( aDateFormatSelector)
      {
         case kDateFormatNone:
            break; 
         case kDateFormatLong:
            UniStrcat( format, (UniChar*)L"%c");
            break;
         case kDateFormatShort:
            UniStrcat( format, (UniChar*)L"%x");
            break;
         case kDateFormatYearMonth:
            UniQueryLocaleItem( locale_object, DATESEP, &pString);
            UniStrcat( format, (UniChar*)L"%y");
            UniStrcat( format, pString);
            UniStrcat( format, (UniChar*)L"%m");
            UniFreeMem( pString);
            break;
         case kDateFormatWeekday:
            UniStrcat( format, (UniChar*)L"%a");
            break;
         default: 
            printf( "Unknown date format (%d)\n", aDateFormatSelector);
            break;
      }

      // space between date & time
      if( aDateFormatSelector != kDateFormatNone &&
          aTimeFormatSelector != kTimeFormatNone)
          UniStrcat( format, (UniChar*)L" ");

      // time
      switch( aTimeFormatSelector)
      {
         case kTimeFormatNone:
            break;
         case kTimeFormatSeconds:
            UniStrcat( format, (UniChar*)L"%r");
            break;
         case kTimeFormatNoSeconds:
            UniQueryLocaleItem( locale_object, TIMESEP, &pString);
            UniStrcat( format, (UniChar*)L"%I");
            UniStrcat( format, pString);
            UniStrcat( format, (UniChar*)L"%M %p");
            UniFreeMem( pString);
            break;
         case kTimeFormatSecondsForce24Hour:
            UniStrcat( format, (UniChar*)L"%T");
            break;
         case kTimeFormatNoSecondsForce24Hour:
            UniStrcat( format, (UniChar*)L"%R");
            break;
         default:
            printf( "Unknown time format (%d)\n", aTimeFormatSelector);
            break;
      }

      // now produce the string (dodgy buffer - iwbn if the function
      // had a handy `pass-me-null-to-find-the-length' mode).
      UniChar buffer[ 100]; // surely ample?

      size_t ret = UniStrftime( locale_object, buffer, 100, format, aTime);

      if( ret == 0)
      {
         printf( "UniStrftime needs a bigger buffer, rethink\n");
      }
      else
      {
         aStringOut.SetString( (const PRUnichar*) buffer);
         rc = NS_OK;
      }
   }

   return rc;
}
