/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <Carbon/Carbon.h>
#include <Print/PMPrintingDialogExtensions.h>
#include <PrintCore/PMTicket.h>


#include "PrintDialogPDE.h"
#include "nsPDECommon.h"

/*
 * The text and labels for the PDE are localized using a strings file.
 * You can use the command genstrings to create a strings file from this source. Use:
 *  genstrings -s CopyLocalizedStringFromPlugin PrintDialogPDE.cp
 */
#define CopyLocalizedStringFromPlugin(key, pluginBundleRef) \
    CFBundleCopyLocalizedString((pluginBundleRef), (key), (key), NULL)

// each application needs to customize these CFSTRs to identify their application 
#define kPMPrintingManager          CFSTR("org.mozilla.printingmanager")
#define kSampleAppUserOptionKindID  CFSTR("org.mozilla.print.pde.PrintDialogPDEOnly")
#define kPrintDialogPDEBundleID     CFSTR("org.mozilla.print.pde.PrintDialogPDE")

// Generated from GUIDGEN
#define kPrintDialogPDEIntfFactoryIDStr CFSTR("F48965F3-45C2-11D6-A87C-00105A183419")

#define kMAXH     150     // max size of our drawing area
#define kMAXV     130     // should be calculated based on our needs

enum {
  kSyncDirectionSetTickets = false,       // Set Ticket(s) based on UI
  kSyncDirectionSetUserInterface = true   // Set UI to reflect Ticket(s)
};

const ResType kPlugInCreator = kPDE_Creator;    // should be set to an appropriate creator

/*----------------------------------------------------------------------------
  Type definitions
------------------------------------------------------------------------------*/


typedef struct {
  const          IUnknownVTbl*  vtable; // Pointer to the vtable:    
  IUnknownVTbl   vtableStorage;         // Our vtable storage:
  CFUUIDRef      factoryID;             // Factory ID this instance is for:
  ULONG          refCount;              // Reference counter:
} IUnknownInstance;


typedef struct{
  PlugInIntfVTable* vtable;
  UInt32            refCount;     // Reference counter
} PrintSelOnlyPlugInInterface;

typedef struct{
  SInt16      theResFile;         // Resource File of this PDE
  CFBundleRef theBundleRef;       // Our bundle reference
  CFStringRef titleStringRef;     // Our PDE's title string ref
  ControlRef  thePrintSelectionOnly;
  ControlRef  thePrintFrameAsIs;
  ControlRef  thePrintSelectedFrame;
  ControlRef  thePrintFramesSeparately;
  ControlRef  theShrinkToFit;
  ControlRef  theRadioGroup;
} PrintDialogPDEOnlyContext, *PrintDialogPDEOnlyContextPtr;


static OSStatus GetTicketRef(PMPrintSession printSession, CFStringRef aTicket, 
                                PMTicketRef* aPrintSettingsPtr);

static OSStatus InitContext ( PrintDialogPDEOnlyContextPtr* aContext);

#pragma export on
#if __cplusplus
extern "C" {
#endif

// The following define prototypes for the PDE API...
static OSStatus MozPDEPrologue( PMPDEContext        *aContext,
                                OSType              *aCreator,
                                CFStringRef         *aUserOptionKind,
                                CFStringRef         *aTtitle, 
                                UInt32              *aMaxH, 
                                UInt32              *aMaxV);

static OSStatus MozPDEInitialize( PMPDEContext      aContext, 
                                  PMPDEFlags        *aFlags,
                                  PMPDERef          aRef,
                                  ControlRef        aParentUserPane,
                                  PMPrintSession    aPrintSession);

static OSStatus MozPDEGetSummaryText( PMPDEContext  aContext, 
                                      CFArrayRef    *aTitleArray,
                                      CFArrayRef    *aSummaryArray);

static OSStatus MozPDESync( PMPDEContext    aContext,
                            PMPrintSession  aPrintSession,
                            Boolean         aReinitializePlugin);

static OSStatus MozPDEOpen(PMPDEContext aContext);

static OSStatus MozPDEClose(PMPDEContext aContext);

static OSStatus MozPDETerminate(PMPDEContext aContext,OSStatus aStatus);

#if __cplusplus
}
#endif
#pragma export off
          

// Factory ID and routine.
// Plugin factory names must not be mangled by C++ compiler.

#if __cplusplus
extern "C" {
#endif
    // Factory function:
    void* PrintDialogPDEPluginFactory( CFAllocatorRef aAllocator, CFUUIDRef aTypeID );
#if __cplusplus
}
#endif

// More Prototypes
static OSStatus MozCreatePlugInInterface( PlugInIntf** aObjPtr );
static ULONG IUnknownAddRef( void* aObj );
static ULONG IUnknownRelease( void* aObj );


//========================================================================================

static ULONG 
IUnknownAddRef(void* aObj)
{   
  IUnknownInstance* instance = (IUnknownInstance*) aObj;
  ULONG refCount = 0; // We can't do much with errors here since we can only
  // update reference count value.   
  if (instance != NULL) {
    // Get updated refCount value (should be under mutex):
    refCount = ++instance->refCount;
  } else {
    refCount = 0;
  }
  return refCount;
}

//========================================================================================

static ULONG 
IUnknownRelease(void* aObj)
{
  IUnknownInstance* instance = (IUnknownInstance*) aObj;
  ULONG refCount = 0;
    
  // We can't do much with errors here since we can only return
  // updated reference count value.
  if (instance != NULL) {
    // Get updated refCount value (should be under mutex):
    // Make sure refCount is non-zero:
    if (0 == instance->refCount) {
      instance = NULL;
      return(refCount);
    }

    refCount = --instance->refCount;
    
    // Is it time to self-destruct?
    if (0 == refCount) {     
      // Unregister 'instance for factory' with CoreFoundation:
      CFPlugInRemoveInstanceForFactory(instance->factoryID);                            
      // Release used factoryID:        
      CFRelease(instance->factoryID);
      instance->factoryID = NULL;

      // Deallocate object's memory block:
      free((void*) instance);
      instance = NULL;
    }
  }
    
  return refCount;    
}

//========================================================================================

static OSStatus 
MozPMRetain(PMPlugInHeaderInterface* aObj)
{
  if (aObj != NULL) {
    PrintSelOnlyPlugInInterface* plugin = (PrintSelOnlyPlugInInterface*) aObj;

    // Increment reference count:
    plugin->refCount++;    
  }
  return noErr;
}

//========================================================================================

static OSStatus 
MozPMRelease(PMPlugInHeaderInterface** aObjPtr)
{
  if (*aObjPtr != NULL) {
    PrintSelOnlyPlugInInterface* plugin = (PrintSelOnlyPlugInInterface*) *aObjPtr;

    // Clear caller's variable:
    *aObjPtr = NULL;

    // Decrement reference count:
    plugin->refCount--;

    // When reference count is zero it's time self-destruct:
    if (0 == plugin->refCount) {
      // Delete object's vtable:
      free((char *)plugin->vtable);
      // Delete object's memory block:
      free((char *)plugin);
    }
  }
  return noErr;
}

//========================================================================================

static OSStatus  
MozPMGetAPIVersion(PMPlugInHeaderInterface* aObj,PMPlugInAPIVersion* aVersionPtr)
{
  // Return versioning info:
  aVersionPtr->buildVersionMajor = kPDEBuildVersionMajor;
  aVersionPtr->buildVersionMinor = kPDEBuildVersionMinor;
  aVersionPtr->baseVersionMajor  = kPDEBaseVersionMajor;
  aVersionPtr->baseVersionMinor  = kPDEBaseVersionMinor;

  return noErr;
}

//========================================================================================

static OSStatus 
MozCreatePlugInInterface(PlugInIntf** aObjPtr )
{
  PrintSelOnlyPlugInInterface*  intf = NULL;
  PlugInIntfVTable*             vtable = NULL;

  // Allocate object and clear it:
  intf = (PrintSelOnlyPlugInInterface*) calloc(1, sizeof( PrintSelOnlyPlugInInterface ));
  
  if (intf != NULL) {
    // Assign all plugin data members:
    intf->refCount = 1;

    // Allocate object's vtable and clear it:
    vtable = (PlugInIntfVTable*) calloc(1, sizeof( PlugInIntfVTable ));
    if (vtable != NULL) {
      intf->vtable = vtable;
    
      // Assign all plugin header methods:
      vtable->plugInHeader.Retain = MozPMRetain;
      vtable->plugInHeader.Release  = MozPMRelease;
      vtable->plugInHeader.GetAPIVersion  = MozPMGetAPIVersion;

      // Assign all plugin methods:
      vtable->Prologue = MozPDEPrologue;
      vtable->Initialize = MozPDEInitialize;
      vtable->Sync = MozPDESync;
      vtable->GetSummaryText = MozPDEGetSummaryText;
      vtable->Open = MozPDEOpen;
      vtable->Close = MozPDEClose;
      vtable->Terminate = MozPDETerminate;
    }
  }
  
  // Return results:
  *aObjPtr = (PlugInIntf*) intf;

  return noErr;
}

//========================================================================================

static HRESULT 
IUnknownQueryInterface(void* aObj,REFIID aIID,LPVOID* aIntfPtr )
{
  IUnknownInstance* instance = (IUnknownInstance*) aObj;
  CFUUIDRef   theIntfID = NULL, reqIntfID = NULL;
  HRESULT     err = E_UNEXPECTED;
  PlugInIntf* interface;

  // Get IDs for requested and PDE interfaces:
  reqIntfID = CFUUIDCreateFromUUIDBytes( kCFAllocatorDefault, aIID );
  theIntfID = CFUUIDCreateFromString( kCFAllocatorDefault, kDialogExtensionIntfIDStr );
  if (reqIntfID && theIntfID) {
    // If we are asked to return the interface for 
    // the IUnknown vtable, which the system already has access to,
    // just increment the refcount value
    if (CFEqual( reqIntfID, IUnknownUUID ) ) {
      instance->vtable->AddRef( (void*) instance );
      *aIntfPtr = (LPVOID) instance;
      err = S_OK;
    } else if (CFEqual(reqIntfID, theIntfID)) {  
      err = MozCreatePlugInInterface( &interface );
      if ( noErr == err ) {
        *aIntfPtr = (LPVOID) interface;
        err = S_OK;
      } else { 
        *aIntfPtr = NULL;
        err = E_NOINTERFACE;
      }
    } else {
      *aIntfPtr = NULL;
      err = E_NOINTERFACE;
    }
  } else {  // we will return the err = E_NOINTERFACE and a  *aIntfPtr of NULL;
    *aIntfPtr = NULL;
    err = E_NOINTERFACE;
  }
  // Clean up and return status:
  if (reqIntfID) {
    CFRelease( reqIntfID );
  }
  if (theIntfID) {
    CFRelease( theIntfID );
  }

  return( err );
}
 
//========================================================================================

void* 
PrintDialogPDEPluginFactory(CFAllocatorRef aAllocator,CFUUIDRef aReqTypeID )
{
  CFUUIDRef         theInstID;
  IUnknownInstance* instance = NULL;

  // There is not much we can do with errors - just return NULL.
  theInstID = CFUUIDCreateFromString(kCFAllocatorDefault, kAppPrintDialogTypeIDStr);
  // If the requested type matches our plugin type (it should!)
  // have a plugin instance created which will query us for
  // interfaces:
  if (theInstID && CFEqual(aReqTypeID, theInstID)) {
    CFUUIDRef theFactoryID = CFUUIDCreateFromString(kCFAllocatorDefault, kPrintDialogPDEIntfFactoryIDStr);
    if (theFactoryID) {
      // allocate and clear our instance structure
      instance = (IUnknownInstance*) calloc(1, sizeof(IUnknownInstance));
      if (instance != NULL) {
        // Assign all members:
        instance->vtable = &instance->vtableStorage;

        instance->vtableStorage.QueryInterface = IUnknownQueryInterface;
        instance->vtableStorage.AddRef = IUnknownAddRef;
        instance->vtableStorage.Release = IUnknownRelease;

        instance->factoryID = theFactoryID;
        instance->refCount = 1;

        // Register the newly created instance
        CFPlugInAddInstanceForFactory(theFactoryID);
      }
    }
  }
  if (theInstID) {
    CFRelease(theInstID);
  }
    
  return ((void*) instance);
}

//========================================================================================

static OSStatus 
MozPDEPrologue(PMPDEContext *aContext,OSType *aCreator,CFStringRef *aUserOptionKind,CFStringRef *aTitle,UInt32 *aMaxH,UInt32 *aMaxV)
{
  OSStatus err = noErr;
  PrintDialogPDEOnlyContextPtr theContext = NULL;  // Pointer to our context data.
    
  err = InitContext(&theContext);
    
  if (noErr == err) {
    *aContext = (PMPDEContext) theContext;

    // calculate the maximum amount of screen real estate that this plugin needs.
    *aMaxH = kMAXH;
    *aMaxV = kMAXV;

   /* The semantics of the CFStrings represented by *aTitle and *aUserOptionKind
      are 'Get' semantics: the caller will retain what it needs to retain.
      This means that we need to release this title string sometime after
      this routine returns. We put our reference to the string into our context
      data so we can release that string when we dispose of the context data.
   */
    theContext->titleStringRef = CopyLocalizedStringFromPlugin(    
        CFSTR("Mozilla Printing Extensions"),
        theContext->theBundleRef);

    if (theContext->titleStringRef != NULL) {
      *aTitle = theContext->titleStringRef;
    }

    *aUserOptionKind = kSampleAppUserOptionKindID;
    *aCreator = kPlugInCreator;    
  } else {
    err = kPMInvalidPDEContext;           // return an error
  }

  return (err);
}

//========================================================================================

static OSStatus 
MozPDEInitialize( PMPDEContext    aContext, 
                  PMPDEFlags*     aFlags,
                  PMPDERef        aRef,
                  ControlRef      aParentUserPane,
                  PMPrintSession  aPrintSession)
{
  OSStatus err = noErr;
  PrintDialogPDEOnlyContextPtr theContext = NULL;  // Pointer to our context data.

  theContext = (PrintDialogPDEOnlyContextPtr) aContext;

  if ((theContext != NULL) && (aPrintSession != NULL)) {   
    WindowRef theWindow = NULL;
    short savedResFile = CurResFile();
    UseResFile(theContext->theResFile);
    theWindow = GetControlOwner(aParentUserPane);  // get the windowref from the user pane

    // get controls
    theContext->thePrintSelectionOnly = GetNewControl(ePrintSelectionControlID, theWindow);
    theContext->thePrintFrameAsIs = GetNewControl(ePrintFrameAsIsControlID, theWindow);
    theContext->thePrintSelectedFrame = GetNewControl(ePrintSelectedFrameControlID, theWindow);
    theContext->thePrintFramesSeparately = GetNewControl(ePrintFramesSeparatelyControlID, theWindow);
    theContext->theShrinkToFit = GetNewControl(eShrinkToFiControlID, theWindow);
    theContext->theRadioGroup = GetNewControl(eRadioGroupControlID, theWindow);

    // embed controls
    EmbedControl(theContext->thePrintSelectionOnly, aParentUserPane);
    EmbedControl(theContext->theShrinkToFit, aParentUserPane);
    EmbedControl(theContext->thePrintFrameAsIs, theContext->theRadioGroup);
    EmbedControl(theContext->thePrintSelectedFrame, theContext->theRadioGroup);
    EmbedControl(theContext->thePrintFramesSeparately, theContext->theRadioGroup);
    EmbedControl(theContext->theRadioGroup, aParentUserPane);    

    // set controls as visible
    SetControlVisibility(theContext->theRadioGroup, true, false);
    SetControlVisibility(theContext->thePrintSelectionOnly, true, false);
    SetControlVisibility(theContext->thePrintFrameAsIs, true, false);
    SetControlVisibility(theContext->thePrintSelectedFrame, true, false);
    SetControlVisibility(theContext->thePrintFramesSeparately, true, false);
    SetControlVisibility(theContext->theShrinkToFit, true, false);

    // Set default value
    SetControlValue(theContext->thePrintSelectionOnly, 0);
    SetControlValue(theContext->thePrintFrameAsIs, 0);
    SetControlValue(theContext->thePrintSelectedFrame, 0);
    SetControlValue(theContext->thePrintFramesSeparately, 0);
    SetControlValue(theContext->theShrinkToFit, 0);

    // Set flags
    *aFlags = kPMPDENoFlags;

    // Initialize this plugins controls based on the information in the 
    // PageSetup or PrintSettings ticket.
    err = MozPDESync(aContext, aPrintSession, kSyncDirectionSetUserInterface);
    if (err == kPMKeyNotFound)
    err = noErr;

    UseResFile(savedResFile);
  } else {
    err = kPMInvalidPDEContext;
  }

  return (err);
}

//========================================================================================

static OSStatus 
MozPDEGetSummaryText(PMPDEContext aContext, CFArrayRef *aTitleArray,CFArrayRef *aSummaryArray)
{
  OSStatus err = noErr;
  CFMutableArrayRef theTitleArray = NULL;       // Init CF strings
  CFMutableArrayRef theSummaryArray = NULL;
  CFStringRef titleStringRef = NULL;
  CFStringRef summaryStringRef = NULL;
  PrintDialogPDEOnlyContextPtr theContext = NULL;  // Pointer to our context data.
    
  theContext = (PrintDialogPDEOnlyContextPtr) aContext;
  *aTitleArray = NULL;
  *aSummaryArray = NULL;

  if (theContext != NULL) {
    //  NOTE: if the second parameter to CFArrayCreateMutable 
    //      is not 0 then the array is a FIXED size
    theTitleArray = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    theSummaryArray = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    
    if ((theTitleArray != NULL) && (theSummaryArray != NULL)) {
      SInt16  theControlValue = -1;
      titleStringRef = CopyLocalizedStringFromPlugin(CFSTR(" Print Selected Text Only"),theContext->theBundleRef);
    
      theControlValue = GetControlValue(theContext->thePrintSelectionOnly);
      switch (theControlValue) {
        case 0:
          summaryStringRef = CopyLocalizedStringFromPlugin(CFSTR(" No, Print All Text"),theContext->theBundleRef);
          break;
        case 1:
          summaryStringRef = CopyLocalizedStringFromPlugin(CFSTR(" Yes"),theContext->theBundleRef);
          break;
      }

      if (titleStringRef && summaryStringRef) {
        CFArrayAppendValue(theTitleArray, titleStringRef);
        CFArrayAppendValue(theSummaryArray, summaryStringRef);
      } else {
       err = memFullErr;
      }
    }else{
      err = memFullErr;
    }
  } else {
    err = kPMInvalidPDEContext;
  }

  // we release these because we've added them already to the title and summary array
  // or we don't need them because there was an error
  if (titleStringRef) {
    CFRelease(titleStringRef);
  }
  if (summaryStringRef) {
    CFRelease(summaryStringRef);
  }
        
  // update the data passed in.
  if (!err) {
    *aTitleArray = theTitleArray;
    *aSummaryArray = theSummaryArray;
  } else {
    if (theTitleArray) {
      CFRelease(theTitleArray);
    }
    if (theSummaryArray) {
      CFRelease(theSummaryArray);
    }
  }

  return (err);
}

//========================================================================================

static OSStatus 
MozPDESync(PMPDEContext aContext,PMPrintSession  aPrintSession,Boolean aSyncDirection)
{
  OSStatus err = noErr;
  PrintDialogPDEOnlyContextPtr theContext = NULL;    // Pointer to our context data.
  CFDataRef ourPDEPrintSettingsDataRef = NULL;

  theContext = (PrintDialogPDEOnlyContextPtr) aContext;
    
  if ((theContext != NULL) && (aPrintSession != NULL)) {
    PMTicketRef printSettingsContainer = NULL;
    err = GetTicketRef(aPrintSession, kPDE_PMPrintSettingsRef, &printSettingsContainer);

    if (noErr == err) {
      nsPrintExtensions printSettings;
      SInt16 theControlValue = -1;

      printSettings.mHaveSelection = false;
      printSettings.mHaveFrames = false;
      printSettings.mHaveFrameSelected = false;
      printSettings.mPrintSelection = false;
      printSettings.mPrintFrameAsIs = false;
      printSettings.mPrintSelectedFrame = false;
      printSettings.mPrintFramesSeparately = false;
      printSettings.mShrinkToFit = false;
               
      switch (aSyncDirection){
        case kSyncDirectionSetUserInterface:
          err = PMTicketGetCFData(printSettingsContainer, kPMTopLevel, kPMTopLevel, kAppPrintDialogPDEOnlyKey, &ourPDEPrintSettingsDataRef);
          if (!err) {
            if (CFDataGetLength(ourPDEPrintSettingsDataRef) == sizeof(printSettings) ) {
              printSettings = *(nsPrintExtensions *)CFDataGetBytePtr(ourPDEPrintSettingsDataRef);
            }
        
            if (printSettings.mHaveSelection == true) {
              EnableControl((ControlHandle)theContext->thePrintSelectionOnly);
              theControlValue = (printSettings.mPrintSelection) ? 1 : 0;
              SetControlValue(theContext->thePrintSelectionOnly, theControlValue); 
            } else {
              DisableControl((ControlHandle)theContext->thePrintSelectionOnly);
              SetControlValue(theContext->thePrintSelectionOnly, 0);
            }
        
            if (printSettings.mHaveFrames == true) {
              EnableControl((ControlHandle)theContext->thePrintFrameAsIs);
              SetControlValue(theContext->thePrintFrameAsIs, 1);
          
            // control for printing selected frame
              if (printSettings.mHaveFrameSelected == true) {
                EnableControl((ControlHandle)theContext->thePrintSelectedFrame);
                theControlValue = ( printSettings.mPrintSelectedFrame) ? 1 : 0;
                SetControlValue(theContext->thePrintSelectedFrame, theControlValue);
              } else {
                DisableControl((ControlHandle)theContext->thePrintSelectedFrame);
                SetControlValue(theContext->thePrintSelectedFrame, 0);
              }
            
              // control to print all frames separately
              EnableControl((ControlHandle)theContext->thePrintFramesSeparately);
              theControlValue = (printSettings.mPrintFramesSeparately) ? 1 : 0;
              SetControlValue(theContext->thePrintFramesSeparately, theControlValue);
            } else {
              DisableControl((ControlHandle)theContext->thePrintFrameAsIs);
              SetControlValue(theContext->thePrintFrameAsIs, 0);
              DisableControl((ControlHandle)theContext->thePrintSelectedFrame);
              SetControlValue(theContext->thePrintSelectedFrame, 0);
              DisableControl((ControlHandle)theContext->thePrintFramesSeparately);
              SetControlValue(theContext->thePrintFramesSeparately, 0);
            }
          
            // shrink to fit
            theControlValue = (printSettings.mShrinkToFit) ? 1 : 0;
            SetControlValue(theContext->theShrinkToFit, theControlValue);
          }
          break;

         case kSyncDirectionSetTickets:
           theControlValue = GetControlValue(theContext->thePrintSelectionOnly);
           printSettings.mPrintSelection = theControlValue != 0;
           theControlValue = GetControlValue(theContext->thePrintFrameAsIs);
           printSettings.mPrintFrameAsIs = theControlValue != 0;
           theControlValue = GetControlValue(theContext->thePrintSelectedFrame);
           printSettings.mPrintSelectedFrame = theControlValue != 0;
           theControlValue = GetControlValue(theContext->thePrintFramesSeparately);
           printSettings.mPrintFramesSeparately = theControlValue != 0;
           theControlValue = GetControlValue(theContext->theShrinkToFit);
           printSettings.mShrinkToFit = theControlValue != 0;
         
           ourPDEPrintSettingsDataRef = CFDataCreate(kCFAllocatorDefault, (UInt8*)&printSettings, sizeof(printSettings));
           if (ourPDEPrintSettingsDataRef) {
             err = PMTicketSetCFData(printSettingsContainer, kPMPrintingManager, kAppPrintDialogPDEOnlyKey, ourPDEPrintSettingsDataRef,kPMUnlocked);
             CFRelease(ourPDEPrintSettingsDataRef);
           } else {
             err = memFullErr;
           }
           break;
         }
      }
    } else {
      err = kPMInvalidPDEContext;
    }
  
  return (err);
}

//========================================================================================

static OSStatus 
MozPDEOpen(PMPDEContext aContext )
{
  OSStatus err = noErr;
  PrintDialogPDEOnlyContextPtr theContext = NULL;  // Pointer to our context data.
  
  theContext = (PrintDialogPDEOnlyContextPtr) aContext;
  if (theContext != NULL) {
    // make sure you make yourself the current resource file if you need
    // access to your resources
  
    //short savedResFile = CurResFile();
    //UseResFile(theContext->theResFile);

    //  Do something useful here
    //UseResFile(savedResFile);
  } else {
    err = kPMInvalidPDEContext;
  }

  return err;
}
//========================================================================================

static OSStatus 
MozPDEClose(PMPDEContext aContext )
{
  OSStatus err = noErr;
  PrintDialogPDEOnlyContextPtr theContext = NULL;  // Pointer to our context data.

  theContext = (PrintDialogPDEOnlyContextPtr) aContext;
  if (theContext != NULL) {
    //  Do something useful here
  } else {
    err = kPMInvalidPDEContext;
  }
  return ( err );
}

//========================================================================================

static OSStatus 
MozPDETerminate(PMPDEContext aContext,OSStatus aStatus)
{
  OSStatus err = noErr;
  PrintDialogPDEOnlyContextPtr theContext = NULL;  // Pointer to our context data.

  theContext = (PrintDialogPDEOnlyContextPtr) aContext;

  if (theContext != NULL) {   
    if (theContext->theResFile != -1) {  // Close the resource fork
      CFBundleCloseBundleResourceMap(theContext->theBundleRef, theContext->theResFile);
      theContext->theResFile = -1;
    }

    if (theContext->titleStringRef) {
     CFRelease(theContext->titleStringRef);
    }

    // Free the global aContext.
    free((char*) theContext);
    theContext = NULL;
  } else {
    err = kPMInvalidPDEContext;
  }
  return ( err );
}

//========================================================================================

static OSStatus 
InitContext(PrintDialogPDEOnlyContextPtr* aContext)
{ 
  OSStatus  err = noErr;

  *aContext =  (PrintDialogPDEOnlyContextPtr) calloc(1, sizeof(PrintDialogPDEOnlyContext));

  if (NULL != *aContext) {
    CFBundleRef theBundleRef = NULL;

    (*aContext)->theResFile = -1;

    //  Open the resource fork
    theBundleRef = CFBundleGetBundleWithIdentifier(kPrintDialogPDEBundleID);
    if (theBundleRef) {
      (*aContext)->theResFile =  CFBundleOpenBundleResourceMap(theBundleRef);
      if ((*aContext)->theResFile == -1) {
        err = kPMGeneralError;
      }

      (*aContext)->theBundleRef = theBundleRef;
    } else {
      err = kPMInvalidPDEContext;     // this really needs a better error code
    }
  } else {
    err = kPMInvalidPDEContext;
  }
  return (err);  
}

//========================================================================================

static OSStatus 
GetTicketRef(PMPrintSession aPrintSession,CFStringRef aTicket,PMTicketRef* aPrintSettingsPtr)
{
  OSStatus err = noErr;
  CFTypeRef cfTicketRef = NULL;

  err = PMSessionGetDataFromSession(aPrintSession, aTicket, &cfTicketRef);

  if ((noErr == err) && (cfTicketRef)) {
    // This returns a Boolean (lossy data) that we don't care about
    Boolean lossy;
    lossy = CFNumberGetValue((CFNumberRef)cfTicketRef, kCFNumberSInt32Type, (void*)aPrintSettingsPtr);

    if (NULL == *aPrintSettingsPtr) {
      err = kPMInvalidValue;
    }
  } else {
    err = kPMInvalidTicket;
  }
  return err;
}
