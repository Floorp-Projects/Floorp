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
 *   Simon Fraser <smfr@smfr.org>
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


#import "NSFileManager+Utils.h"

@implementation NSFileManager(CaminoFileManagerUtils)

- (BOOL)createDirectoriesInPath:(NSString *)path attributes:(NSDictionary *)attributes
{
  NSArray*        dirs = [[path stringByStandardizingPath] pathComponents];
  NSString* curPath = nil;
  
  unsigned i, numDirs = [dirs count];
  for (i = 0; i < numDirs; i ++)
  {
    if (curPath)
      curPath = [curPath stringByAppendingPathComponent:[dirs objectAtIndex:i]];
    else
      curPath = [dirs objectAtIndex:i];

    BOOL isDir;
    if ([self fileExistsAtPath:curPath isDirectory:&isDir])
    {
      if (!isDir)
        return NO;    // can't replace file with directory

      continue;
    }
    
    BOOL created = [self createDirectoryAtPath:curPath attributes:attributes];
    if (!created)
      return NO;
  }
  
  return YES;
}

- (long long)sizeOfFileAtPath:(NSString*)inPath traverseLink:(BOOL)inTraverseLink
{
  NSDictionary* fileAttribs = [self fileAttributesAtPath:inPath traverseLink:inTraverseLink];
  if (!fileAttribs) return -1LL;
  
  NSNumber* fileSize = [fileAttribs objectForKey:NSFileSize];
  if (!fileSize) return -1LL;
  
  return [fileSize longLongValue];
}


@end
