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
 * The Original Code is Camino code.
 *
 * The Initial Developer of the Original Code is
 * Bruce Davidson <Bruce.Davidson@iplbath.com>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bruce Davidson
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

#import "ABAddressBook+Utils.h"

#import <AppKit/NSWorkspace.h>

@interface ABAddressBook(CaminoPRIVATE)
- (void)openAddressBookAtRecord:(NSString*)uniqueId editFlag:(BOOL)withEdit;
@end

@implementation ABAddressBook (CaminoExtensions)

//
// Returns the record containing the specified e-mail, or nil if
// there is no such record. (If more than one record does!) we
// simply return the first.
//
- (ABPerson*)recordFromEmail:(NSString*)emailAddress
{
  ABSearchElement* search = [ABPerson searchElementForProperty:kABEmailProperty label:nil key:nil value:emailAddress comparison:kABEqualCaseInsensitive];
  
  NSArray* matches = [self recordsMatchingSearchElement:search];
  
  if ( [matches count] > 0 )
	  return [matches objectAtIndex:0];
	
	return nil;
}

//
// Determine if a record containing the given e-mail address as an e-mail address
// property exists in the address book.
//
- (BOOL)emailAddressExistsInAddressBook:(NSString*)emailAddress
{
  return ( [self recordFromEmail:emailAddress] != nil );
}

//
// Return the real name of the person in the address book with the
// specified e-mail address. Returns nil if the e-mail address does not
// occur in the address book.
//
- (NSString*)getRealNameForEmailAddress:(NSString*)emailAddress
{
  // These constants are taken from the 10.3 SDK. We declare them here because they are not
	// supported on 10.2, and we still need to build on 10.2.
	const int ABConstsShowAsMask    = 0x07;  // kABShowAsMask
	const int ABConstsShowAsPerson  = 0x00;  // kABShowAsPerson
	
  NSString* realName = nil;

  ABPerson* person = [self recordFromEmail:emailAddress];
  if ( person != nil ) {
	  
    NSNumber* flags = [person valueForProperty:@"ABPersonFlags"];
		
		int displayMode = ABConstsShowAsPerson; // Default incase we're on 10.2 and property not set
		if ( flags != nil ) 
		  displayMode = [flags intValue] & ABConstsShowAsMask;

		if ( displayMode == ABConstsShowAsPerson) {
      NSString* firstName = [person valueForProperty:kABFirstNameProperty];
      NSString* lastName  = [person valueForProperty:kABLastNameProperty];
    
      if ( firstName != nil && lastName != nil )
			  realName = [[firstName stringByAppendingString:@" "] stringByAppendingString:lastName];
			else if ( lastName != nil )
			  realName = lastName;
			else if ( firstName != nil )
			  realName = firstName;
			else
			  realName = emailAddress;

		} else {
		  realName = [person valueForProperty:kABOrganizationProperty];
		}
  }

  return realName;
}

//
// Add a new ABPerson record to the address book with the given e-mail address
// Then open the new record for edit so the user can fill in the rest of
// the details.
//
- (void)addNewPersonFromEmail:(NSString*)emailAddress
{
  ABPerson* newPerson = [[[ABPerson alloc] init] autorelease];
    
  ABMutableMultiValue* emailCollection = [[[ABMutableMultiValue alloc] init] autorelease];
  [emailCollection addValue:emailAddress withLabel:kABEmailWorkLabel];
  [newPerson setValue:emailCollection forProperty:kABEmailProperty];
    
  if ([self addRecord:newPerson]) {
    [self save];
    [self openAddressBookAtRecord:[newPerson uniqueId] editFlag:YES];
  }
}

//
// Opens the Address Book application at the record containing the given email address
//
- (void)openAddressBookForRecordWithEmail:(NSString*)emailAddress
{
  ABPerson* person = [self recordFromEmail:emailAddress];
	if ( person != nil ) {
	  [self openAddressBookAtRecord:[person uniqueId] editFlag:NO];
	}
}

//
// Opens the Address Book application at the specified record
// optionally sets up the record for editing, e.g. to complete remaining values
//
- (void)openAddressBookAtRecord:(NSString*)uniqueId editFlag:(BOOL)withEdit
{
  NSString *urlString = [NSString stringWithFormat:@"addressbook://%@", uniqueId];
	if ( withEdit )
	  urlString = [urlString stringByAppendingString:@"?edit"];
		
  [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:urlString]];
}

@end
