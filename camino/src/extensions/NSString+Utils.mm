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
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <sfraser@netscape.com>
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
 * ***** END LICENSE BLOCK ***** */

#import "NSString+Utils.h"

#include "nsString.h"
#include "nsPromiseFlatString.h"
#include "nsCRT.h"

@implementation NSString (ChimeraStringUtils)


+ (id)stringWithPRUnichars:(const PRUnichar*)inString
{
  if (inString)
    return [NSString stringWithCharacters:inString length:nsCRT::strlen(inString)];
  else
    return [NSString string];
}

+ (id)stringWith_nsAString:(const nsAString&)inString
{
  nsPromiseFlatString flatString = PromiseFlatString(inString);
  return [NSString stringWithCharacters:flatString.get() length:flatString.Length()];
}

#define ASSIGN_STACK_BUFFER_CHARACTERS	256

- (void)assignTo_nsAString:(nsAString&)ioString
{
  PRUnichar			stackBuffer[ASSIGN_STACK_BUFFER_CHARACTERS];
  PRUnichar*		buffer = stackBuffer;
  
  // XXX maybe fix this to use SetLength(0), SetLength(len), and a writing iterator.
  unsigned int len = [self length];
  
  if (len + 1 > ASSIGN_STACK_BUFFER_CHARACTERS)
  {
    buffer = (PRUnichar *)malloc(sizeof(PRUnichar) * (len + 1));
    if (!buffer)
      return;
  }

  [self getCharacters: buffer];		// does not null terminate
  ioString.Assign(buffer, len);
  
  if (buffer != stackBuffer)
    free(buffer);
}

- (NSString *)stringByRemovingCharactersInSet:(NSCharacterSet*)characterSet
{
  NSScanner*       cleanerScanner = [NSScanner scannerWithString:self];
  NSMutableString* cleanString    = [NSMutableString stringWithCapacity:[self length]];
  
  while (![cleanerScanner isAtEnd])
  {
    NSString* stringFragment;
    if ([cleanerScanner scanUpToCharactersFromSet:characterSet intoString:&stringFragment])
      [cleanString appendString:stringFragment];

    [cleanerScanner scanCharactersFromSet:characterSet intoString:nil];
  }

  return cleanString;
}

- (NSString *)stringByReplacingCharactersInSet:(NSCharacterSet*)characterSet withString:(NSString*)string
{
  NSScanner*       cleanerScanner = [NSScanner scannerWithString:self];
  NSMutableString* cleanString    = [NSMutableString stringWithCapacity:[self length]];
  
  while (![cleanerScanner isAtEnd])
  {
    NSString* stringFragment;
    if ([cleanerScanner scanUpToCharactersFromSet:characterSet intoString:&stringFragment])
      [cleanString appendString:stringFragment];

    if ([cleanerScanner scanCharactersFromSet:characterSet intoString:nil])
      [cleanString appendString:string];
  }

  return cleanString;
}

@end
