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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Aas (josh@mozilla.com)
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

/*
 We index digits from right to left, such that the rightmost digit is digit 0.
 
 We take digits 0-3 as suffix. We take digits 4-6 as prefix. We take digits 7-9
 as area code. If there are only 11 digits and digit 10 is a 1, then throw it away
 and stop. If there are more than 10 digits and its not just a single 1 after the
 digit at index 9, we put all digits after index 9 into remainderString.
 
 Fail if we don't have 7 or more than 9 digits.
*/

#import "PhoneNumber.h"

@implementation PhoneNumber

// init will never fail - if it gets a bad phoneString, all member variables will be nil
-(id)initWithPhoneNumberString:(NSString*)phoneString 
{
  if ((self = [super init])) {
    remainderString = nil;
    areaCodeString = nil;
    prefixString = nil;
    suffixString = nil;
    if (phoneString) {
      // get phone number into just a string of digits
      NSCharacterSet *digitCharSet = [NSCharacterSet decimalDigitCharacterSet];
      NSMutableString *tmpString = [NSMutableString stringWithCapacity:[phoneString length]];
      [tmpString appendString:phoneString];
      // remove non-digit characters
      for (int i = [tmpString length] - 1; i >= 0; i--) {
        if (![digitCharSet characterIsMember:[tmpString characterAtIndex:i]])
          [tmpString deleteCharactersInRange:NSMakeRange(i, 1)];
      }
      
      // only fill in some values if we have an acceptable number of digits in total
      int digitCount = [tmpString length];
      if ((digitCount == 7) || (digitCount > 9)) {
        // if we got to this point, we must have prefix and suffix strings
        prefixString = [[tmpString substringWithRange:NSMakeRange(digitCount - 7, 3)] retain];
        suffixString = [[tmpString substringWithRange:NSMakeRange(digitCount - 4, 4)] retain];
        
        // now fill in area code and remainder (if they exist)
        if (digitCount > 9) {
          // grab the area code
          areaCodeString = [[tmpString substringWithRange:NSMakeRange(digitCount - 10, 3)] retain];
          // ignore everything else if there is only one more digit and its a 1
          if (!((digitCount == 10) && ([[tmpString substringWithRange:NSMakeRange(9,1)] isEqualToString:@"1"]))) {
            remainderString = [[tmpString substringWithRange:NSMakeRange(0, digitCount - 9)] retain];
          }
        }
      }
    }
    return self;
  }
  return nil;
}

-(void)dealloc 
{
  [remainderString release];
  [areaCodeString release];
  [prefixString release];
  [suffixString release];
  [super dealloc];
}

-(NSString*)remainderString 
{
  return remainderString;
}

-(NSString*)areaCodeString 
{
  return areaCodeString;
}

-(NSString*)prefixString 
{
  return prefixString;
}

-(NSString*)suffixString
{
  return suffixString;
}

@end
