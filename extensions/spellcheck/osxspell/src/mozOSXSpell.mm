/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Spellchecker Component.
 *
 * The Initial Developer of the Original Code is Mike Pinkerton.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Mike Pinkerton <mikepinkerton@mac.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * This spellchecker is based on the built-in spellchecker on Mac OS X. It
 * does not use any MySpell technology or rely on their dictionaries. It's just
 * a thin wrapper around the Cocoa NSSpellChecker API.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozOSXSpell.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

#import <Cocoa/Cocoa.h>

// utility category we need for PRUnichar<->NSString conversion (taken from Camino)
@interface NSString(PRUnicharUtils)
+ (id)stringWithPRUnichars:(const PRUnichar*)inString;
- (PRUnichar*)createNewUnicodeBuffer;
@end


NS_IMPL_ISUPPORTS1(mozOSXSpell, mozISpellCheckingEngine)

mozOSXSpell::mozOSXSpell()
{
}

mozOSXSpell::~mozOSXSpell()
{
}

//
// GetDictionary
//
// Nothing to do here, we don't have a dictionary on disk, so this is really
// just a no-op. The caller is responsible for disposing of |aDictionary|.
//
NS_IMETHODIMP mozOSXSpell::GetDictionary(PRUnichar **aDictionary)
{
  NS_ENSURE_ARG_POINTER(aDictionary);

  *aDictionary = [@"" createNewUnicodeBuffer];
  return NS_OK;
}

//
// SetDictionary
//
// Another no-op as there's nothing to load or initialize.
//
NS_IMETHODIMP mozOSXSpell::SetDictionary(const PRUnichar *aDictionary)
{
  return NS_OK;
}

//
// GetLanguage
//
// Returns the language of the current dictionary, which should be the l10n
// the user is running.  The caller is responsible for disposing of |aLanguage|.
//
NS_IMETHODIMP mozOSXSpell::GetLanguage(PRUnichar **aLanguage)
{
  NS_ENSURE_ARG_POINTER(aLanguage);

  if (!mLanguage.Length()) {
    NSString* lang = [[NSSpellChecker sharedSpellChecker] language];
    *aLanguage = [lang createNewUnicodeBuffer];
    mLanguage.Assign(*aLanguage);
  }
  else
    *aLanguage = ToNewUnicode(mLanguage);

  return *aLanguage ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

//
// GetProvidesPersonalDictionary
//
// We let Gecko handle the personal dictionary, even though NSSpellChecker can
// handle ignoring words itself. 
//
NS_IMETHODIMP mozOSXSpell::GetProvidesPersonalDictionary(PRBool *aProvidesPersonalDictionary)
{
  NS_ENSURE_ARG_POINTER(aProvidesPersonalDictionary);

  *aProvidesPersonalDictionary = PR_FALSE;
  return NS_OK;
}

//
// GetProvidesWordUtils
//
// I have no idea what this is, so we don't provide it.
//
NS_IMETHODIMP mozOSXSpell::GetProvidesWordUtils(PRBool *aProvidesWordUtils)
{
  NS_ENSURE_ARG_POINTER(aProvidesWordUtils);

  *aProvidesWordUtils = PR_FALSE;
  return NS_OK;
}

//
// GetName
//
// Name not supported (nor is it in MySpell impl)
//
NS_IMETHODIMP mozOSXSpell::GetName(PRUnichar * *aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//
// GetCopyright
//
// Copyright not supported (nor is it in MySpell impl)
//
NS_IMETHODIMP mozOSXSpell::GetCopyright(PRUnichar * *aCopyright)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//
// GetPersonalDictionary
//
// Return the personal dictionary we've been given with Set.
//
NS_IMETHODIMP mozOSXSpell::GetPersonalDictionary(mozIPersonalDictionary** aPersonalDictionary)
{
  *aPersonalDictionary = mPersonalDictionary;
  NS_IF_ADDREF(*aPersonalDictionary);
  return NS_OK;
}

//
// SetPersonalDictionary
//
// Hold onto the personal dictionary we're given.
//
NS_IMETHODIMP mozOSXSpell::SetPersonalDictionary(mozIPersonalDictionary* aPersonalDictionary)
{
  mPersonalDictionary = aPersonalDictionary;
  return NS_OK;
}

//
// GetDictionaryList
//
// We only support the OS dictionary from NSSpellChecker so there will only ever
// be one. The caller is responsible for disposing of |aDictionaries|.
//
NS_IMETHODIMP mozOSXSpell::GetDictionaryList(PRUnichar ***aDictionaries, PRUint32 *aCount)
{
  NS_ENSURE_ARG_POINTER(aDictionaries);
  NS_ENSURE_ARG_POINTER(aCount);

  *aCount = 1;
  *aDictionaries = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *)); // only one entry
  GetLanguage(*aDictionaries);
	
	return NS_OK;
}

//
// Check
//
// Check if the given word is spelled correctly. If the main dictionary says
// it's not, check again against the peronal dictionary we were given.
//
NS_IMETHODIMP mozOSXSpell::Check(const PRUnichar *aWord, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aWord);
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = PR_FALSE;

  NSString* wordStr = [NSString stringWithPRUnichars:aWord];
  NSRange misspelledRange = [[NSSpellChecker sharedSpellChecker] checkSpellingOfString:wordStr startingAt:0];
  if (misspelledRange.location != NSNotFound && mPersonalDictionary)
    mPersonalDictionary->Check(aWord, mLanguage.get(), aResult);
  else
    *aResult = PR_TRUE;

  return NS_OK;
}

//
// Suggest
//
// Provide a list of suggestions for the incrorectly spelled word |aWord| by 
// converting the list we get back from NSSpellChecker into an array of PRUnichar
// strings. If |aWord| is spelled correctly, |aSuggestions| will be NULL. The caller
// is responsible for disposing of |aSuggestions|.
//
NS_IMETHODIMP mozOSXSpell::Suggest(const PRUnichar *aWord, PRUnichar ***aSuggestions, PRUint32 *aSuggestionCount)
{
  NS_ENSURE_ARG_POINTER(aSuggestions);
  NS_ENSURE_ARG_POINTER(aSuggestionCount);
  *aSuggestions = NULL;

  // check the word against the NSSpellChecker
  NSString* wordStr = [NSString stringWithPRUnichars:aWord];
  NSArray* guesses = [[NSSpellChecker sharedSpellChecker] guessesForWord:wordStr];
  *aSuggestionCount = [guesses count];

  // convert results from NSArray to array of PRUnichar's
  if (*aSuggestionCount) {
    *aSuggestions = (PRUnichar **)nsMemory::Alloc(*aSuggestionCount * sizeof(PRUnichar *));    
    PRUint32 i = 0;
    NSEnumerator* e = [guesses objectEnumerator];
    NSString* guess = nil;
    while ((guess = [e nextObject])) {
      (*aSuggestions)[i] = [guess createNewUnicodeBuffer];
      ++i;
    }
  }

  return NS_OK;
}

#pragma mark -

//
// String utilities taken from Camino
//

@implementation NSString(PRUnicharUtils)

- (PRUnichar*)createNewUnicodeBuffer
{
  PRUint32 length = [self length];
  PRUnichar* retStr = (PRUnichar*)nsMemory::Alloc((length + 1) * sizeof(PRUnichar));
  [self getCharacters:retStr];
  retStr[length] = PRUnichar(0);
  return retStr;
}

+ (id)stringWithPRUnichars:(const PRUnichar*)inString
{
  if (inString)
    return [self stringWithCharacters:inString length:nsCRT::strlen(inString)];
  else
    return [self string];
}

@end

//
// Factory Methods
//

#include "nsIGenericFactory.h"

#include "mozOSXSpell.h"

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//

NS_GENERIC_FACTORY_CONSTRUCTOR(mozOSXSpell)

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static nsModuleComponentInfo components[] = {
  { "OSX Spell check service", MOZ_OSXSPELL_CID, MOZ_OSXSPELL_CONTRACTID, mozOSXSpellConstructor }
};

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
NS_IMPL_NSGETMODULE(mozOSXSpellModule, components)
