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


+ (id)ellipsisString
{
  static NSString* sEllipsisString = nil;
  if (!sEllipsisString)
  {
    unichar ellipsisChar = 0x2026;
    sEllipsisString = [[[NSString alloc] initWithCharacters:&ellipsisChar length:1] retain];
  }
  
  return sEllipsisString;
}

+ (id)stringWithPRUnichars:(const PRUnichar*)inString
{
  if (inString)
    return [self stringWithCharacters:inString length:nsCRT::strlen(inString)];
  else
    return [self string];
}

+ (id)stringWith_nsAString:(const nsAString&)inString
{
  nsPromiseFlatString flatString = PromiseFlatString(inString);
  return [self stringWithCharacters:flatString.get() length:flatString.Length()];
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

- (NSString*)stringByTruncatingTo:(int)maxCharacters at:(ETruncationType)truncationType
{
  NSString*	ellipsisString = [NSString ellipsisString];
  
  if ([self length] > maxCharacters)
  {
      NSMutableString *croppedString = [NSMutableString stringWithCapacity:maxCharacters + [ellipsisString length]];
      
      switch (truncationType)
      {
        case kTruncateAtStart:
          [croppedString appendString:ellipsisString];
          [croppedString appendString:[self substringWithRange:NSMakeRange([self length] - maxCharacters, maxCharacters)]];
          break;

        case kTruncateAtMiddle:
          {
            int len1 = maxCharacters / 2;
            int len2 = maxCharacters - len1;
            NSString *part1 = [self substringWithRange:NSMakeRange(0, len1)];
            NSString *part2 = [self substringWithRange:NSMakeRange([self length] - len2, len2)];
            [croppedString appendString:part1];
            [croppedString appendString:ellipsisString];
            [croppedString appendString:part2];
          }
          break;

        case kTruncateAtEnd:
          [croppedString appendString:[self substringWithRange:NSMakeRange(0, maxCharacters)]];
          [croppedString appendString:ellipsisString];
          break;

        default:
#if DEBUG
          NSLog(@"Unknown truncation type in stringByTruncatingTo::");
#endif          
          break;
      }
      
      return croppedString;
  }
  else
  {
    return [[self copy] autorelease];
  }
}


@end
