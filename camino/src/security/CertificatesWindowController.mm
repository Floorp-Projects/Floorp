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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Simon Fraser <smfr@smfr.org>
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

#import "nsCOMPtr.h"
#import "nsString.h"

#import "nsSupportsArray.h"

#include "nsIX509Cert.h"
#include "nsIX509CertValidity.h"
#include "nsIX509CertDB.h"

#import "nsICertTree.h"
#import "nsINSSCertCache.h"

#import "nsComponentManagerUtils.h"
#import "nsServiceManagerUtils.h"
#import "nsILocalFile.h"

#import "ExtendedSplitView.h"
#import "ExtendedOutlineView.h"
#import "CertificateItem.h"
#import "CertificateView.h"
#import "ViewCertificateDialogController.h"
#import "CertificatesWindowController.h"

// C++ class that holds owning refs to XPCOM interfaces for the window controller
class CertDataOwner
{
public:

  CertDataOwner()
  {
    mCertCache = do_CreateInstance("@mozilla.org/security/nsscertcache;1");
    mCertCache->CacheAllCerts();
  }

  nsCOMPtr<nsINSSCertCache>   mCertCache;
};

#pragma mark -

@interface CertificatesWindowController(Private)

- (void)reloadCertData;
- (void)clearCertData;

- (void)setupCertsData;
- (void)categorySelectionChanged;
- (nsINSSCertCache*)certCache;    // returns non-addreffed pointer
- (NSString*)detailsColumnKey;
- (NSArray*)selectedCertificates;
- (NSArray*)allCertificatesOfCurrentType;
- (PRUint32)selectedCertType;

- (void)backupCertificates:(NSArray*)inCertificates;

- (void)deleteCertSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo;
- (void)backupPanelDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;

- (void)importPanelDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (void)restorePanelDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;

- (void)deleteCertificates:(NSArray*)inCertItems;
- (void)performBackup:(NSDictionary*)backupParams;
- (void)performRestore:(NSString*)inFilePath;
- (void)performImport:(NSString*)inFilePath;

- (void)certificateChanged:(NSNotification*)inNotification;

@end

#pragma mark -

// C++ class per data source, that manages the list of certificates for this collection
class CertificateListData
{
public:
  CertificateListData(PRUint32 inCertType)
  : mCertType(inCertType)
  {
  }

  void        LoadCertificates(nsINSSCertCache* inCache);
  PRUint32    GetNumberOfCertificates();
  PRBool      GetCertificateAt(PRUint32 inIndex, nsIX509Cert** outCert);
  
protected:

  PRUint32              mCertType;
  nsSupportsArray       mCertArray;
};


void CertificateListData::LoadCertificates(nsINSSCertCache* inCache)
{
  mCertArray.Clear();

  // there is no way to get a list of all the nsIX509Certs via the APIs,
  // other than to go via nsICertTree; see bug 306500. 
  nsCOMPtr<nsICertTree> certTree = do_CreateInstance("@mozilla.org/security/nsCertTree;1");
  if (!certTree) return;

  nsresult rv = certTree->LoadCertsFromCache(inCache, mCertType);
  if (NS_FAILED(rv)) return;
  
  // now copy them into our array. certTree builds an outline-view like
  // data strucure, but it's row-based and not suited to NSOutlineView
  // (ugly mixing of view with model). So we'll just do the tree building
  // ourselves.
  PRInt32 numRows = 0;
  certTree->GetRowCount(&numRows);
  for (PRInt32 i = 0; i < numRows; i ++)
  {
    nsCOMPtr<nsIX509Cert> thisCert;
    certTree->GetCert(i, getter_AddRefs(thisCert));
    if (thisCert)
      mCertArray.AppendElement(thisCert);
  }
}


PRUint32
CertificateListData::GetNumberOfCertificates()
{
  PRUint32 numCerts;
  mCertArray.Count(&numCerts);
  return numCerts;
}

PRBool
CertificateListData::GetCertificateAt(PRUint32 inIndex, nsIX509Cert** outCert)
{
  nsCOMPtr<nsIX509Cert> thisCert = do_QueryElementAt(&mCertArray, inIndex);
  *outCert = thisCert;
  NS_IF_ADDREF(*outCert);
  return (*outCert != NULL);
}


#pragma mark -

@interface DummyDataSource : NSObject
{
}

@end

@implementation DummyDataSource

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
  return nil;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
  return NO;
}

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
  return 0;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
  return nil;
}

@end

#pragma mark -

@interface CertificatesDataSource : NSObject
{
  PRUint32          mCertType;
  NSArray*          mOrganizationsArray;    // list of root objects; each is a dict
                                            // with a name, and an array of children

  NSArray*          mCertsArray;            // array of CertificateItem*

  CertificateListData*    mCertListData;
}

- (id)initWithCertType:(PRUint32)inType;
- (void)ensureCertsLoaded:(nsINSSCertCache*)inCertCache;
- (void)rebuildCertsTree;
- (NSArray*)certificates;

- (PRUint32)certType;

@end

@implementation CertificatesDataSource

- (id)initWithCertType:(PRUint32)inType
{
  if ((self = [super init]))
  {
    mCertType = inType;
    mCertListData = new CertificateListData(inType);
  }
  return self;
}

- (void)dealloc
{
  [mCertsArray release];
  [mOrganizationsArray release];
  
  delete mCertListData;
  [super dealloc];
}

- (void)ensureCertsLoaded:(nsINSSCertCache*)inCertCache
{
  if (mCertListData->GetNumberOfCertificates() == 0)
  {
    mCertListData->LoadCertificates(inCertCache);
    [self rebuildCertsTree];
  }
}

static int CompareCertOrganizationSortProc(id inObject1, id inObject2, void* inContext)
{
  NSString* org1String = [inObject1 organization];
  NSString* org2String = [inObject2 organization];
  
  return [org1String compare:org2String options:NSCaseInsensitiveSearch];
}

- (void)rebuildCertsTree
{
  [mCertsArray release];
  mCertsArray = nil;
  
  PRUint32 numCerts = mCertListData->GetNumberOfCertificates();
  NSMutableArray* certsArray = [NSMutableArray arrayWithCapacity:mCertListData->GetNumberOfCertificates()];
  for (PRUint32 i = 0; i < numCerts; i ++)
  {
    nsCOMPtr<nsIX509Cert> thisCert;
    if (mCertListData->GetCertificateAt(i, getter_AddRefs(thisCert)))
    {
      CertificateItem* thisCertItem = [[CertificateItem alloc] initWithCert:thisCert];
      [certsArray addObject:thisCertItem];
      [thisCertItem release];
    }
  }
  
  mCertsArray = [certsArray retain];
  
  // now sort by Organization
  [certsArray sortUsingFunction:CompareCertOrganizationSortProc context:NULL];
  
  // now create the groupings
  [mOrganizationsArray release];
  mOrganizationsArray = nil;
  
  NSMutableArray* orgsArray = [NSMutableArray arrayWithCapacity:10];
  
  NSString*       curOrganization = nil;
  NSMutableArray* certsInCurrentOrg = nil;
  
  NSEnumerator* certsEnum = [certsArray objectEnumerator];
  CertificateItem* curCertItem;
  while ((curCertItem = [certsEnum nextObject]))
  {
    NSString* thisCertOrg = [curCertItem organization];

    if (!curOrganization || ![curOrganization isEqualToStringIgnoringCase:thisCertOrg])
    {
      certsInCurrentOrg = [NSMutableArray array];
      curOrganization   = thisCertOrg;

      NSDictionary* categoryDict = [NSDictionary dictionaryWithObjectsAndKeys:
                                        thisCertOrg, @"organization",
                                  certsInCurrentOrg, @"certs",
                                                     nil];
      [orgsArray addObject:categoryDict];
    }

    // sort these at some point...
    [certsInCurrentOrg addObject:curCertItem];
  }
  
  mOrganizationsArray = [orgsArray retain];
}

- (PRUint32)certType
{
  return mCertType;
}

- (NSArray*)certificates
{
  return mCertsArray;
}

#pragma mark -

// outline view data source methods

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
  if (!item)
    return [mOrganizationsArray objectAtIndex:index];

  if ([item isKindOfClass:[NSDictionary class]])
    return [[item objectForKey:@"certs"] objectAtIndex:index];

  return nil;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
  if (!item)
    return YES;

  if ([item isKindOfClass:[NSDictionary class]])
    return YES;

  return NO;
}

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
  if (!item)
    return [mOrganizationsArray count];

  if ([item isKindOfClass:[NSDictionary class]])
    return [[item objectForKey:@"certs"] count];

  return 0;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
  NSString* tableColumnID = [tableColumn identifier];
  
  if ([item isKindOfClass:[NSDictionary class]])
  {
    if ([tableColumnID isEqualToString:@"displayName"])
      return [item objectForKey:@"organization"];
    else
      return nil;
  }
  
  if ([item isKindOfClass:[CertificateItem class]])
    if ([tableColumnID isEqualToString:@"details"])
      return [item valueForKey:[[outlineView delegate] detailsColumnKey]];
    else  
      return [item valueForKey:tableColumnID];

  return nil;
}

@end


#pragma mark -

@implementation CertificatesWindowController

static CertificatesWindowController* gCertificatesWindowController;

+ (CertificatesWindowController*)sharedCertificatesWindowController
{
  if (!gCertificatesWindowController)
  {
  // this -init ref is balanced by the -autorelease in -windowWillClose
    gCertificatesWindowController = [[CertificatesWindowController alloc] initWithWindowNibName:@"CertificatesWindow"];
  }
  return gCertificatesWindowController;
}

- (void)dealloc
{
  if (self == gCertificatesWindowController)
    gCertificatesWindowController = nil;

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [mCertificatesData release];
  [mDetailsColumnKey release];
  delete mDataOwner;
  [super dealloc];
}

- (void)awakeFromNib
{
  [self setShouldCascadeWindows:NO];
  [[self window] setFrameAutosaveName:@"CertificatesWindow"];

  [mCertsOutlineView setAutoresizesOutlineColumn:NO];

  [mCertsOutlineView setAutosaveName:@"CertificatesOutlineView"];
  [mCertsOutlineView setAutosaveTableColumns:YES];
}

- (void)windowDidLoad
{
  // we need to listen for changes in our parent chain, to update cached trust
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(certificateChanged:)
                                               name:CertificateChangedNotificationName
                                             object:nil];
  
  [mSplitter setAutosaveSplitterPosition:YES];
  [mSplitter setAutosaveName:@"CategoriesSplitter"];

  [mCertsOutlineView setDoubleAction:@selector(viewSelectedCertificates:)];
  [mCertsOutlineView setDeleteAction:@selector(deleteSelectedCertificates:)];
  [mCertsOutlineView setTarget:self];
  
  NSTableColumn* validityColumn = [mCertsOutlineView tableColumnWithIdentifier:@"attributedShortValidityString"];
  [[validityColumn dataCell] setWraps:NO];

  [self reloadCertData];
}

- (void)windowWillClose:(NSNotification *)aNotification
{
  // balance the init from when the window was shown
  [self autorelease];
}

#pragma mark -

- (IBAction)viewSelectedCertificates:(id)sender
{
  unsigned int certTypeForTrust = [self selectedCertType];
  if (certTypeForTrust == nsIX509Cert::SERVER_CERT)
    certTypeForTrust = nsIX509Cert::CA_CERT;    // let the user edit the trust settings

  NSEnumerator* selectedItemsEnum = [[self selectedCertificates] objectEnumerator];
  CertificateItem* certItem;
  while ((certItem = [selectedItemsEnum nextObject]))
  {
    [ViewCertificateDialogController showCertificateWindowWithCertificateItem:certItem
                                                     certTypeForTrustSettings:certTypeForTrust];
  }
}

- (IBAction)deleteSelectedCertificates:(id)sender
{
  NSArray* selectedCerts = [self selectedCertificates];
  if ([selectedCerts count] == 0) return;

  NSString* titleFormat = nil;
  NSString* messageStr = nil;

  switch ([self selectedCertType])
  {
    default:
    case nsIX509Cert::USER_CERT:
      titleFormat = NSLocalizedStringFromTable(@"DeleteUserCertTitleFormat", @"CertificateDialogs", @"");
      messageStr  = NSLocalizedStringFromTable(@"DeleteUserCertMsg", @"CertificateDialogs", @"");
      break;
    case nsIX509Cert::EMAIL_CERT:
      titleFormat = NSLocalizedStringFromTable(@"DeleteEmailCertTitleFormat", @"CertificateDialogs", @"");
      messageStr  = NSLocalizedStringFromTable(@"DeleteEmailCertMsg", @"CertificateDialogs", @"");
      break;
    case nsIX509Cert::SERVER_CERT:
      titleFormat = NSLocalizedStringFromTable(@"DeleteWebSiteCertTitleFormat", @"CertificateDialogs", @"");
      messageStr  = NSLocalizedStringFromTable(@"DeleteWebSiteCertMsg", @"CertificateDialogs", @"");
      break;
    case nsIX509Cert::CA_CERT:
      titleFormat = NSLocalizedStringFromTable(@"DeleteCACertTitleFormat", @"CertificateDialogs", @"");
      messageStr  = NSLocalizedStringFromTable(@"DeleteCACertMsg", @"CertificateDialogs", @"");
      break;
  }
  
  // warning dialog...
  NSString* deleteButton  = NSLocalizedStringFromTable(@"DeleteCertButtonTitle", @"CertificateDialogs", @"");
  NSString* cancelButton  = NSLocalizedStringFromTable(@"CancelButtonTitle", @"CertificateDialogs", @"");

#warning fix for plurals
  NSString* title = [NSString stringWithFormat:titleFormat, [[selectedCerts objectAtIndex:0] displayName]];
  
  NSBeginAlertSheet(title,
          deleteButton,
          cancelButton,
          nil,    // other button
          [self window],
          self,
          @selector(deleteCertSheetDidEnd:returnCode:contextInfo:),
          nil,
          (void*)[selectedCerts retain], 
          messageStr);
}

- (IBAction)backupSelectedCertificates:(id)sender
{
  NSArray* selectedCerts = [self selectedCertificates];
  if ([selectedCerts count] == 0) return;

  [self backupCertificates:selectedCerts];
}

- (IBAction)backupAllCertificates:(id)sender
{
  NSArray* selectedCerts = [self allCertificatesOfCurrentType];
  if ([selectedCerts count] == 0) return;

  [self backupCertificates:selectedCerts];
}

- (IBAction)restoreCertificates:(id)sender
{
  NSOpenPanel* theOpenPanel = [NSOpenPanel openPanel];
  
  [theOpenPanel beginSheetForDirectory:nil
                                  file:nil
                                 types:[NSArray arrayWithObjects:@"p12", @"pfx", nil]
                        modalForWindow:[self window]
                         modalDelegate:self
                        didEndSelector:@selector(restorePanelDidEnd:returnCode:contextInfo:)
                           contextInfo:NULL];

}


- (IBAction)importCertificates:(id)sender
{
  NSOpenPanel* theOpenPanel = [NSOpenPanel openPanel];
  
  [theOpenPanel beginSheetForDirectory:nil
                                  file:nil
                                 types:[NSArray arrayWithObjects:@"crt", @"cert", @"cer", @"pem", @"der", nil]
                        modalForWindow:[self window]
                         modalDelegate:self
                        didEndSelector:@selector(importPanelDidEnd:returnCode:contextInfo:)
                           contextInfo:NULL];


}

// testing
#include "nsITokenPasswordDialogs.h"

- (IBAction)changePassword:(id)sender
{
  nsCOMPtr<nsITokenPasswordDialogs> tokenDlgs = do_GetService(NS_TOKENPASSWORDSDIALOG_CONTRACTID);
  PRBool cancelled;
  NS_NAMED_LITERAL_STRING(deviceName, "Software Security Device");
  tokenDlgs->SetPassword(NULL, deviceName.get(), &cancelled);
}

#pragma mark -

- (void)reloadCertData
{
  int curSelectedRow = [mCategoriesTable selectedRow];
  if (curSelectedRow == -1)
    curSelectedRow = 0;

  delete mDataOwner;
  mDataOwner = new CertDataOwner;

  [self setupCertsData];
  [mCategoriesTable reloadData];

  [mCategoriesTable selectRow:curSelectedRow byExtendingSelection:NO];
  [self categorySelectionChanged];  
}

- (void)setupCertsData
{
  [mCertificatesData release];
  mCertificatesData = nil;

  NSMutableArray* certificatesData = [NSMutableArray arrayWithCapacity:4];

  CertificatesDataSource* myCertsDataSource = [[[CertificatesDataSource alloc] initWithCertType:nsIX509Cert::USER_CERT] autorelease];
  NSDictionary* myCertsDict = [NSDictionary dictionaryWithObjectsAndKeys:
             NSLocalizedStringFromTable(@"MyCertsCategoryLabel", @"CertificateDialogs", @""), @"category_name",
                                                                  [NSNumber numberWithInt:1], @"category_type",
                                                                           myCertsDataSource, @"data_source",
                                                                  @"SerialNumberColumnLabel", @"details_column_label",
                                                                             @"serialNumber", @"details_column_identifier",
                                                                                              nil];
  [certificatesData addObject:myCertsDict];

  CertificatesDataSource* othersCertsDataSource = [[[CertificatesDataSource alloc] initWithCertType:nsIX509Cert::EMAIL_CERT] autorelease];
  NSDictionary* othersCertsDict = [NSDictionary dictionaryWithObjectsAndKeys:
         NSLocalizedStringFromTable(@"OthersCertsCategoryLabel", @"CertificateDialogs", @""), @"category_name",
                                                                  [NSNumber numberWithInt:2], @"category_type",
                                                                       othersCertsDataSource, @"data_source",
                                                                  @"EmailAddressColumnLabel", @"details_column_label",
                                                                             @"emailAddress", @"details_column_identifier",
                                                                                              nil];
  [certificatesData addObject:othersCertsDict];

  CertificatesDataSource* webSitesCertsDataSource = [[[CertificatesDataSource alloc] initWithCertType:nsIX509Cert::SERVER_CERT] autorelease];
  NSDictionary* webSitesCertsDict = [NSDictionary dictionaryWithObjectsAndKeys:
       NSLocalizedStringFromTable(@"WebSitesCertsCategoryLabel", @"CertificateDialogs", @""), @"category_name",
                                                                  [NSNumber numberWithInt:3], @"category_type",
                                                                     webSitesCertsDataSource, @"data_source",
                                                                       @"OrgUnitColumnLabel", @"details_column_label", // lame to show org unit. What else can we show?
                                                                       @"organizationalUnit", @"details_column_identifier",
                                                                                              nil];
  [certificatesData addObject:webSitesCertsDict];


  CertificatesDataSource* authoritiesCertsDataSource = [[[CertificatesDataSource alloc] initWithCertType:nsIX509Cert::CA_CERT] autorelease];
  NSDictionary* authoritiesCertsDict = [NSDictionary dictionaryWithObjectsAndKeys:
    NSLocalizedStringFromTable(@"AuthoritiesCertsCategoryLabel", @"CertificateDialogs", @""), @"category_name",
                                                                  [NSNumber numberWithInt:3], @"category_type",
                                                                  authoritiesCertsDataSource, @"data_source",
                                                                       @"OrgUnitColumnLabel", @"details_column_label", // lame to show org unit. What else can we show?
                                                                       @"organizationalUnit", @"details_column_identifier",
                                                                                              nil];
  [certificatesData addObject:authoritiesCertsDict];
  mCertificatesData = [certificatesData retain];
}

- (void)clearCertData
{
  // for reasons known only to NSOutlineView, it won't release all the refs it has
  // to objects in the old data source if we just set the data source to nil
  // and tell it to reload. So we give it a dummy data source that is just empty.
  [mCertsOutlineView setDataSource:[[[DummyDataSource alloc] init] autorelease]];
  [mCertsOutlineView reloadData];

  [mCertificatesData release];
  mCertificatesData = nil;
}

// we can't change the table column identifier for the details column, because
// it breaks the auto-saving of table columns. so we use this instead.
- (NSString*)detailsColumnKey
{
  return mDetailsColumnKey;
}

- (NSArray*)selectedCertificates
{
  NSEnumerator* selectedItemsEnum = [[mCertsOutlineView selectedItems] objectEnumerator];
  NSMutableArray* selectedCerts = [NSMutableArray array];
 
  id curItem;
  while ((curItem = [selectedItemsEnum nextObject]))
  {
    if ([curItem isKindOfClass:[CertificateItem class]])
      [selectedCerts addObject:curItem];
  }
  
  return selectedCerts;
}

- (NSArray*)allCertificatesOfCurrentType
{
  id dataSource = [mCertsOutlineView dataSource];
  if ([dataSource isKindOfClass:[CertificatesDataSource class]])
    return [dataSource certificates];

  return nil;
}

- (PRUint32)selectedCertType
{
  return [[mCertsOutlineView dataSource] certType];
}

- (void)categorySelectionChanged
{
  int rowIndex = [mCategoriesTable selectedRow];
  NSDictionary* rowDict = nil;
  if (rowIndex != -1 && rowIndex < [mCertificatesData count])
  {
    rowDict = [mCertificatesData objectAtIndex:rowIndex];
    CertificatesDataSource* certsDataSource = [rowDict objectForKey:@"data_source"];
    [certsDataSource ensureCertsLoaded:mDataOwner->mCertCache.get()];
    [mCertsOutlineView setDataSource:certsDataSource];
  }
  else
  {
    [mCertsOutlineView setDataSource:nil];
  }

  // rejigger the columns
  if (rowDict)
  {
    NSString* columnTitle      = NSLocalizedStringFromTable([rowDict objectForKey:@"details_column_label"], @"CertificateDialogs", @"");
    NSString* columnIdentifier = [rowDict objectForKey:@"details_column_identifier"];
    
    [[mDetailsColumn headerCell] setTitle:columnTitle];
    [mDetailsColumnKey autorelease];
    mDetailsColumnKey = [columnIdentifier retain];
  }
  
  [mCertsOutlineView deselectAll:nil];
  [mCertsOutlineView reloadData];
  
  [mCertsOutlineView expandAllItems];
}

- (void)deleteCertificates:(NSArray*)inCertItems
{
  nsCOMPtr<nsIX509CertDB> certDB = do_GetService("@mozilla.org/security/x509certdb;1");
  if (!certDB) return;

  NSEnumerator* certsEnum = [inCertItems objectEnumerator];

  CertificateItem* curCertItem;
  while ((curCertItem = [certsEnum nextObject]))
  {
    certDB->DeleteCertificate([curCertItem cert]);
  }
  
  // the actual deletes don't happen until all refs to the cert have gone away,
  // and since there are autoreleased CertificateItems floating around, we have
  // to delay the reload.
  [self clearCertData];   // drop refs
  [self performSelector:@selector(reloadCertData) withObject:nil afterDelay:0];
}

-(BOOL)validateMenuItem:(NSMenuItem*)inMenuItem
{
  SEL action = [inMenuItem action];
  if ((action == @selector(viewSelectedCertificates:)) ||
      (action == @selector(deleteSelectedCertificates:)))
  {
    return ([[self selectedCertificates] count] > 0);
  }

  // backup only applies to user certs
  if (action == @selector(backupSelectedCertificates:))
  {
    return ([self selectedCertType] == nsIX509Cert::USER_CERT) &&
           ([[self selectedCertificates] count] > 0);
  }

  // only enable backup all if there are some
  if (action == @selector(backupAllCertificates:))
  {
    return ([self selectedCertType] == nsIX509Cert::USER_CERT) &&
           ([[self allCertificatesOfCurrentType] count] > 0);
  }

  return YES;
}

- (void)backupCertificates:(NSArray*)inCertificates
{
  NSSavePanel* theSavePanel = [NSSavePanel savePanel];
  [theSavePanel setRequiredFileType:@"p12"];
  [theSavePanel setCanSelectHiddenExtension:YES];

  [theSavePanel beginSheetForDirectory:nil
                                  file:nil
                        modalForWindow:[self window]
                         modalDelegate:self
                        didEndSelector:@selector(backupPanelDidEnd:returnCode:contextInfo:)
                           contextInfo:(void*)[inCertificates retain]];
}

- (void)deleteCertSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  NSArray* selectedCerts = (NSArray*)contextInfo;
  if (returnCode == NSAlertDefaultReturn)
  {
    // drop out to the main event loop
    [self performSelector:@selector(deleteCertificates:) withObject:selectedCerts afterDelay:0];
  }
  [selectedCerts release];
}

- (void)backupPanelDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  NSArray* selectedCerts = (NSArray*)contextInfo;
  if (returnCode == NSAlertDefaultReturn)
  {
    // pop out to the main event loop, so that the various password dialogs
    // show after the save sheet has gone away
    NSDictionary* backupParams = [NSDictionary dictionaryWithObjectsAndKeys:
                                            [sheet filename], @"file_path",
                                               selectedCerts, @"certs",
                                                              nil];
    [self performSelector:@selector(performBackup:) withObject:backupParams afterDelay:0];
  }
  [selectedCerts release];
}

- (void)restorePanelDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  if (returnCode == NSAlertDefaultReturn)
  {
    // pop out to the main event loop, so that the various password dialogs
    // show after the save sheet has gone away
    [self performSelector:@selector(performRestore:) withObject:[sheet filename] afterDelay:0];
  }
}

- (void)importPanelDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  if (returnCode == NSAlertDefaultReturn)
  {
    // pop out to the main event loop, so that the various password dialogs
    // show after the save sheet has gone away
    [self performSelector:@selector(performImport:) withObject:[sheet filename] afterDelay:0];
  }
}

- (void)performBackup:(NSDictionary*)backupParams
{
  NSString* filePath    = [backupParams objectForKey:@"file_path"];
  NSArray*  certsToSave = [backupParams objectForKey:@"certs"];

  PRUint32 numCerts = [certsToSave count];
  
  nsIX509Cert** certList = new nsIX509Cert*[numCerts];
  for (PRUint32 i = 0; i < numCerts; i ++)
    certList[i] = [[certsToSave objectAtIndex:i] cert];
  
  nsCOMPtr<nsILocalFile> destFile;
  NS_NewNativeLocalFile(nsDependentCString([filePath fileSystemRepresentation]), PR_TRUE, getter_AddRefs(destFile));
  if (!destFile) return;
  
  nsCOMPtr<nsIX509CertDB> certDB = do_GetService("@mozilla.org/security/x509certdb;1");
  if (certDB)
    certDB->ExportPKCS12File(nsnull /* any token */, destFile, numCerts, certList);

  delete [] certList;
}


- (void)performRestore:(NSString*)inFilePath
{
  nsCOMPtr<nsILocalFile> destFile;
  NS_NewNativeLocalFile(nsDependentCString([inFilePath fileSystemRepresentation]), PR_TRUE, getter_AddRefs(destFile));
  if (!destFile) return;
  
  nsCOMPtr<nsIX509CertDB> certDB = do_GetService("@mozilla.org/security/x509certdb;1");
  if (certDB)
  {
    nsresult rv = certDB->ImportPKCS12File(nsnull /* any token */, destFile);
    NSLog(@"ImportPKCS12File returned %X", rv);
  }
  
  [self reloadCertData];
}

- (void)performImport:(NSString*)inFilePath
{
  nsCOMPtr<nsILocalFile> destFile;
  NS_NewNativeLocalFile(nsDependentCString([inFilePath fileSystemRepresentation]), PR_TRUE, getter_AddRefs(destFile));
  if (!destFile) return;
  
  nsCOMPtr<nsIX509CertDB> certDB = do_GetService("@mozilla.org/security/x509certdb;1");
  if (certDB)
  {
    // import certs that match the viewed type (why not import them all?)
    nsresult rv = certDB->ImportCertsFromFile(nsnull /* any token */, destFile, [self selectedCertType]);
    NSLog(@"ImportCertsFromFile returned %X", rv);
  }
  
  [self reloadCertData];
}

- (void)certificateChanged:(NSNotification*)inNotification
{
  CertificateItem* certItem = [inNotification object];

  // Not sure if NSOutlineView calls -isEqual to figure out if
  // a row is displaying an item. If so, this is OK, otherwise
  // it's not strictly correct because CertificateItems are not
  // necessarily singletons.
  [mCertsOutlineView reloadItem:certItem reloadChildren:NO];
}

#pragma mark -

// NSTableView data source methods

- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
  return [mCertificatesData count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(int)row
{
  NSDictionary* rowDict = [mCertificatesData objectAtIndex:row];
  return [rowDict objectForKey:@"category_name"];
}

// NSTableView delegate methods
- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
  [self categorySelectionChanged];  
}

#pragma mark -

- (NSMenu *)outlineView:(NSOutlineView *)outlineView contextMenuForItems:(NSArray*)items
{
  if ([self selectedCertType] == nsIX509Cert::USER_CERT)
    return mUserCertsContextMenu;

  return mOtherCertsContextMenu;
}

#pragma mark -

- (void)splitViewDidResizeSubviews:(NSNotification *)aNotification
{
}

- (float)splitView:(NSSplitView *)sender constrainSplitPosition:(float)proposedPosition ofSubviewAt:(int)offset
{
  const float kMinCategoriesColumnWidth = 80.0f;

  if (offset == 0)
  {
    if (proposedPosition < kMinCategoriesColumnWidth)
      return kMinCategoriesColumnWidth;
  }
  return proposedPosition;

}

@end



