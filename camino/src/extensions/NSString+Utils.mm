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
 *   David Haas   <haasd@cae.wisc.edu>
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

#import <AppKit/AppKit.h>		// for NSStringDrawing.h

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
    sEllipsisString = [[NSString alloc] initWithCharacters:&ellipsisChar length:1];
  }
  
  return sEllipsisString;
}

+ (id)escapedURLString:(NSString *)unescapedString
{
  NSString *escapedString = (NSString *) CFURLCreateStringByAddingPercentEscapes(NULL, (CFStringRef)unescapedString, NULL, NULL, kCFStringEncodingUTF8);
  return [escapedString autorelease];
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

#define ASSIGN_STACK_BUFFER_CHARACTERS  256

- (void)assignTo_nsAString:(nsAString&)ioString
{
  PRUnichar     stackBuffer[ASSIGN_STACK_BUFFER_CHARACTERS];
  PRUnichar*    buffer = stackBuffer;
  
  // XXX maybe fix this to use SetLength(0), SetLength(len), and a writing iterator.
  unsigned int len = [self length];
  
  if (len + 1 > ASSIGN_STACK_BUFFER_CHARACTERS)
  {
    buffer = (PRUnichar *)malloc(sizeof(PRUnichar) * (len + 1));
    if (!buffer)
      return;
  }

  [self getCharacters: buffer];   // does not null terminate
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

- (NSString*)stringByTruncatingTo:(unsigned in)maxCharacters at:(ETruncationType)truncationType
{
  if ([self length] > maxCharacters)
  {
    NSMutableString *mutableCopy = [self mutableCopy];
    [mutableCopy truncateTo:maxCharacters at:truncationType];
    return [mutableCopy autorelease];
  }

  return [[self copy] autorelease];
}

- (NSString *)stringByTrimmingWhitespace
{
  return [self stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
}


- (PRUnichar*)createNewUnicodeBuffer
{
  PRUint32 length = [self length];
  PRUnichar* retStr = (PRUnichar*)nsMemory::Alloc((length + 1) * sizeof(PRUnichar));
  [self getCharacters:retStr];
  retStr[length] = PRUnichar(0);
  return retStr;
}

-(NSString *)stringByRemovingAmpEscapes
{
  NSMutableString* dirtyStringMutant = [NSMutableString stringWithString:self];
  [dirtyStringMutant replaceOccurrencesOfString:@"&amp;"withString:@"&" options:NSLiteralSearch range:NSMakeRange(0,[dirtyStringMutant length])];
  [dirtyStringMutant replaceOccurrencesOfString:@"&quot;"withString:@"\"" options:NSLiteralSearch range:NSMakeRange(0,[dirtyStringMutant length])];
  [dirtyStringMutant replaceOccurrencesOfString:@"&lt;"withString:@"<" options:NSLiteralSearch range:NSMakeRange(0,[dirtyStringMutant length])];
  [dirtyStringMutant replaceOccurrencesOfString:@"&gt;"withString:@">" options:NSLiteralSearch range:NSMakeRange(0,[dirtyStringMutant length])];
  [dirtyStringMutant replaceOccurrencesOfString:@"&mdash;"withString:@"-" options:NSLiteralSearch range:NSMakeRange(0,[dirtyStringMutant length])];
  return [dirtyStringMutant stringByRemovingCharactersInSet:[NSCharacterSet controlCharacterSet]];
}

-(NSString *)stringByAddingAmpEscapes
{
  NSMutableString* dirtyStringMutant = [NSMutableString stringWithString:self];
  [dirtyStringMutant replaceOccurrencesOfString:@"&"withString:@"&amp;" options:NSLiteralSearch range:NSMakeRange(0,[dirtyStringMutant length])];
  [dirtyStringMutant replaceOccurrencesOfString:@"\""withString:@"&quot;" options:NSLiteralSearch range:NSMakeRange(0,[dirtyStringMutant length])];
  [dirtyStringMutant replaceOccurrencesOfString:@"<"withString:@"&lt;" options:NSLiteralSearch range:NSMakeRange(0,[dirtyStringMutant length])];
  [dirtyStringMutant replaceOccurrencesOfString:@">"withString:@"&gt;" options:NSLiteralSearch range:NSMakeRange(0,[dirtyStringMutant length])];
  return [NSString stringWithString:dirtyStringMutant];
}

-(NSString *)stripWWW
{
  if ([self hasPrefix:@"www."] && ([self length]>4))
    return [self substringFromIndex:4];
  return self;
}

// Windows buttons have shortcut keys specified by ampersands in the
// title string. This function removes them from such strings.
-(NSString*)stringByRemovingWindowsShortcutAmpersand
{
  NSMutableString* dirtyStringMutant = [NSMutableString stringWithString:self];
  // we loop through removing all single ampersands and reducing double ampersands to singles
  unsigned int searchLocation = 0;
  while (searchLocation < [dirtyStringMutant length]) {
    searchLocation = [dirtyStringMutant rangeOfString:@"&" options:nil
                                                range:NSMakeRange(searchLocation, [dirtyStringMutant length] - searchLocation)].location;
    if (searchLocation == NSNotFound) {
      break;
    } else {
      [dirtyStringMutant deleteCharactersInRange:NSMakeRange(searchLocation, 1)];
      // ampersand or not, we leave the next character alone
      searchLocation++;
    }
  }
  return [NSString stringWithString:dirtyStringMutant];
}

@end


@implementation NSMutableString (ChimeraMutableStringUtils)

- (void)truncateTo:(unsigned)maxCharacters at:(ETruncationType)truncationType
{
  if ([self length] > maxCharacters)
  {
  NSRange replaceRange;
  replaceRange.length = [self length] - maxCharacters;

  switch (truncationType)
  {
    case kTruncateAtStart:
      replaceRange.location = 0;
      break;

    case kTruncateAtMiddle:
      replaceRange.location = maxCharacters / 2;
      break;

    case kTruncateAtEnd:
      replaceRange.location = maxCharacters;
      break;

    default:
#if DEBUG
      NSLog(@"Unknown truncation type in stringByTruncatingTo::");
#endif          
      replaceRange.location = maxCharacters;
      break;
  }
  
  [self replaceCharactersInRange:replaceRange withString:[NSString ellipsisString]];
  }
}


- (void)truncateToWidth:(float)maxWidth at:(ETruncationType)truncationType withAttributes:(NSDictionary *)attributes
{
  // First check if we have to truncate at all.
  if ([self sizeWithAttributes:attributes].width > maxWidth)
  {
    // Essentially, we perform a binary search on the string length
    // which fits best into maxWidth.

    float width;
    int lo = 0;
    int hi = [self length];
    int mid;

    // Make a backup copy of the string so that we can restore it if we fail low.
    NSMutableString *backup = [self mutableCopy];
    
    while (hi >= lo)
    {
      mid = (hi + lo) / 2;
      
      // Cut to mid chars and calculate the resulting width
      [self truncateTo:mid at:truncationType];
      width = [self sizeWithAttributes:attributes].width;
      
      if (width > maxWidth) {
        // Fail high - string is still to wide. For the next cut, we can simply
        // work on the already cut string, so we don't restore using the backup. 
        hi = mid - 1;
      } else if (width == maxWidth) {
        // Perfect match, abort the search.
        break;
      } else {
        // Fail low - we cut off to much. Restore the string before cutting again.
        lo = mid + 1;
        [self setString:backup];
      }
    }
    // Perform the final cut (unless this was already a perfect match).
    if (width != maxWidth)
      [self truncateTo:hi at:truncationType];
    [backup release];
  }
}

@end
