/*
********************************************************************************

    $Log: PDECore.c,v $
    Revision 1.2  2003/04/03 19:20:04  ccarlen%netscape.com
    Bug 188508 - Upgrade print dialog PDE. r=pinkerton/sr=sfraser



    (c) Copyright 2002 Apple Computer, Inc.  All rights reserved.
    
    IMPORTANT: This Apple software is supplied to you by Apple Computer,
    Inc. ("Apple") in consideration of your agreement to the following
    terms, and your use, installation, modification or redistribution of
    this Apple software constitutes acceptance of these terms.  If you do
    not agree with these terms, please do not use, install, modify or
    redistribute this Apple software.
    
    In consideration of your agreement to abide by the following terms, and
    subject to these terms, Apple grants you a personal, non-exclusive
    license, under Apple's copyrights in this original Apple software (the
    "Apple Software"), to use, reproduce, modify and redistribute the Apple
    Software, with or without modifications, in source and/or binary forms;
    provided that if you redistribute the Apple Software in its entirety and
    without modifications, you must retain this notice and the following
    text and disclaimers in all such redistributions of the Apple Software.
    Neither the name, trademarks, service marks or logos of Apple Computer,
    Inc. may be used to endorse or promote products derived from the Apple
    Software without specific prior written permission from Apple.  Except
    as expressly stated in this notice, no other rights or licenses, express
    or implied, are granted by Apple herein, including but not limited to
    any patent rights that may be infringed by your derivative works or by
    other works in which the Apple Software may be incorporated.
    
    The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES
    NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
    OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
    
    IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
    OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
    MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
    AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
    STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
    
********************************************************************************
*/

#include <Carbon/Carbon.h>
#include <Print/PMPrintingDialogExtensions.h>

#include "PDECustom.h"
#include "PDEUtilities.h"


enum SyncDirection {
    kSyncTicketFromPane = FALSE,
    kSyncPaneFromTicket = TRUE
};


/*
--------------------------------------------------------------------------------
    Prototypes
--------------------------------------------------------------------------------
*/

// callbacks

static HRESULT   MyQueryInterface   (void*, REFIID, LPVOID*);
static ULONG     MyIUnknownRetain   (void*);
static ULONG     MyIUnknownRelease  (void*);

static OSStatus  MyPMRetain         (PMPlugInHeaderInterface*);
static OSStatus  MyPMRelease        (PMPlugInHeaderInterface**);
static OSStatus  MyPMGetAPIVersion  (PMPlugInHeaderInterface*, PMPlugInAPIVersion*);

static OSStatus  MyPrologue   (PMPDEContext*, OSType*, CFStringRef*, CFStringRef*, UInt32*, UInt32*);
static OSStatus  MyInitialize (PMPDEContext, PMPDEFlags*, PMPDERef, ControlRef, PMPrintSession);
static OSStatus  MySync       (PMPDEContext, PMPrintSession, Boolean);
static OSStatus  MyGetSummary (PMPDEContext, CFArrayRef*, CFArrayRef*);
static OSStatus  MyOpen       (PMPDEContext);
static OSStatus  MyClose      (PMPDEContext);
static OSStatus  MyTerminate  (PMPDEContext, OSStatus);


/*
--------------------------------------------------------------------------------
    instance types for the two interfaces we support
--------------------------------------------------------------------------------
*/

typedef struct
{
    const IUnknownVTbl *vtable;
    SInt32 refCount;
    CFUUIDRef factoryID;

} MyIUnknownInstance;

typedef struct
{
    const PlugInIntfVTable *vtable;
    SInt32 refCount;

} MyPDEInstance;


#pragma mark -

/*
--------------------------------------------------------------------------------
**  MyCFPlugInFactory
**  
**  Creates an instance of the IUnknown interface (see the COM
**  specification for more information about IUnknown). The name of this
**  factory function needs to be associated with the factory UUID in the
**  CFPlugInFactories property list entry, so it can be loaded by the
**  printing system for use in the dialog.
**  
--------------------------------------------------------------------------------
*/

extern void* MyCFPlugInFactory (
    CFAllocatorRef allocator, 
    CFUUIDRef typeUUID
)

{
    // our IUnknown interface function table
    static const IUnknownVTbl sMyIUnknownVTable =
    {
        NULL, // required padding for COM
        MyQueryInterface,
        MyIUnknownRetain,
        MyIUnknownRelease
    };
    
    CFBundleRef         myBundle    = NULL;
    CFDictionaryRef     myTypes     = NULL;
    CFStringRef         requestType = NULL;
    CFArrayRef          factories   = NULL;
    CFStringRef         factory     = NULL;
    CFUUIDRef           factoryID   = NULL;
    MyIUnknownInstance  *instance   = NULL;

    myBundle = MyGetBundle();

    if (myBundle != NULL)
    {
        myTypes = CFBundleGetValueForInfoDictionaryKey (
            myBundle, CFSTR("CFPlugInTypes"));
    
        if (myTypes != NULL) 
        {
            // get a reference to the requested type 
            // verify that the requested type matches my type (it should!)
            requestType = CFUUIDCreateString (allocator, typeUUID);
            if (requestType != NULL)
            {
                factories = CFDictionaryGetValue (myTypes, requestType);
                CFRelease (requestType);
                if (factories != NULL) 
                {   // assume the factory we want is entry [0]
                    factory = CFArrayGetValueAtIndex (factories, 0);
                    if (factory != NULL) 
                    {
                       // get a reference to my factory ID
                        factoryID = CFUUIDCreateFromString (
                            allocator, factory);
                        if (factoryID != NULL)
                        {
                            // construct an instance of the IUnknown interface
                            instance = malloc (sizeof(MyIUnknownInstance));
                            if (instance != NULL)
                            {
                                instance->vtable = &sMyIUnknownVTable;
                                instance->refCount = 1;
                                instance->factoryID = factoryID;
                                CFPlugInAddInstanceForFactory (factoryID);
                            }
                            else {
                                CFRelease (factoryID);
                            }
                        }
                    }
                }
            }
        }
    }

    MyDebugMessage ("Factory", (SInt32) instance);
    return instance;
}


#pragma mark -

/*
--------------------------------------------------------------------------------
**  MyQueryInterface
**  
**  Finds the requested interface and returns an instance to the caller.
**  If the request is for the "base" IUnknown interface, we just bump
**  the refcount.
**  
--------------------------------------------------------------------------------
*/
 
static HRESULT MyQueryInterface (
    void *this, 
    REFIID iID, 
    LPVOID *ppv
)

{

    // PDE interface function table

    static const PlugInIntfVTable sMyPDEVTable = 
    { 
        {
            MyPMRetain,
            MyPMRelease, 
            MyPMGetAPIVersion 
        },
        MyPrologue,
        MyInitialize,
        MySync,
        MyGetSummary,
        MyOpen,
        MyClose,
        MyTerminate
    };


    CFUUIDRef requestID = NULL;
    CFUUIDRef actualID  = NULL;
    HRESULT   result = E_UNEXPECTED;

    
    // get a reference to the UUID for the requested interface
    requestID = CFUUIDCreateFromUUIDBytes (kCFAllocatorDefault, iID);
    if (requestID != NULL)
    {
        // get a reference to the UUID for all PDE interfaces
        actualID = CFUUIDCreateFromString (kCFAllocatorDefault, kDialogExtensionIntfIDStr);
        if (actualID != NULL)
        {
            if (CFEqual (requestID, actualID))
            {
                // caller wants an instance of my PDE interface
        
                MyPDEInstance *instance = malloc (sizeof(MyPDEInstance));
        
                if (instance != NULL)
                {
                    instance->vtable = &sMyPDEVTable;
                    instance->refCount = 1;
                    *ppv = instance;
                    result = S_OK;
                }
            }
            else
            {
                if (CFEqual (requestID, IUnknownUUID))
                {
                    // caller wants an instance of my IUnknown interface
                    MyIUnknownRetain (this);
                    *ppv = this;
                    result = S_OK;
                }
                else
                {
                    *ppv = NULL;
                    result = E_NOINTERFACE;
                }
            }
            CFRelease (actualID);
        }
        CFRelease (requestID);
    }

    MyDebugMessage("MyQueryInterface", result);
    return result;
}


/*
--------------------------------------------------------------------------------
**  MyIUnknownRetain
**  
**  Increments the reference count for the calling interface on an
**  object. It should be called for every new copy of a pointer to an
**  interface on a given object.
**  
**  Returns an integer from 1 to n, the value of the new reference
**  count. This information is meant to be used for diagnostic/testing
**  purposes only, because, in certain situations, the value might be
**  unstable.
**  
--------------------------------------------------------------------------------
*/

static ULONG MyIUnknownRetain (void* this)
{   
    MyIUnknownInstance* instance = (MyIUnknownInstance*) this;
    ULONG refCount = 1;
        
    if (instance != NULL) {
        refCount = ++instance->refCount;
    }

    MyDebugMessage("MyIUnknownRetain", refCount);
    return refCount;
}


/*
--------------------------------------------------------------------------------
**  MyIUnknownRelease
**  
**  Decrements the reference count for the calling interface on an
**  object. If the reference count on the object reaches zero, the
**  object is freed from memory.
**  
**  Returns the resulting value of the reference count, which is for 
**  diagnostic/testing purposes only.
**  
--------------------------------------------------------------------------------
*/

static ULONG MyIUnknownRelease (void* this)
{
    MyIUnknownInstance* instance = (MyIUnknownInstance*) this;
    ULONG refCount = 0;

    if (instance != NULL)
    {
        refCount = --instance->refCount;
        if (refCount == 0)
        {
            CFPlugInRemoveInstanceForFactory (instance->factoryID);
            CFRelease (instance->factoryID);
            free (instance);
        }
    }

    MyDebugMessage("MyIUnknownRelease", refCount);
    return refCount;      
}


/*
--------------------------------------------------------------------------------
    MyPMRetain
--------------------------------------------------------------------------------
*/

static OSStatus MyPMRetain (PMPlugInHeaderInterface* this)
{
    MyPDEInstance* instance = (MyPDEInstance*) this;
    ULONG refCount = 1;
    OSStatus result = noErr;
    
    if (instance != NULL) {
        refCount = ++instance->refCount;
    }

    MyDebugMessage("MyPMRetain", refCount);
    return result;
}


/*
--------------------------------------------------------------------------------
    MyPMRelease
--------------------------------------------------------------------------------
*/

static OSStatus MyPMRelease (
    PMPlugInHeaderInterface** this
)

{
    MyPDEInstance* instance = (MyPDEInstance*) *this;
    ULONG refCount = 0;
    OSStatus result = noErr;

    // clear caller's instance variable (don't ask)
    *this = NULL;

    if(instance != NULL)
    {
        // decrement instance reference count, and free if zero
        refCount = --instance->refCount;
    
        if (refCount == 0) 
        {
            free (instance);
            MyFreeTitle();
            MyFreeBundle();
        }
    }

    MyDebugMessage("MyPMRelease", refCount);
    return result;
}


/*
--------------------------------------------------------------------------------
    MyPMGetAPIVersion
--------------------------------------------------------------------------------
*/

static OSStatus MyPMGetAPIVersion (
    PMPlugInHeaderInterface* this, 
    PMPlugInAPIVersion* versionPtr
)

{
    OSStatus result = noErr;

    // constants defined in PMPrintingDialogExtensions.h
    versionPtr->buildVersionMajor = kPDEBuildVersionMajor;
    versionPtr->buildVersionMinor = kPDEBuildVersionMinor;
    versionPtr->baseVersionMajor = kPDEBaseVersionMajor;
    versionPtr->baseVersionMinor = kPDEBaseVersionMinor;
    
    MyDebugMessage("MyPMGetAPIVersion", result);
    return result;
}


#pragma mark -

/*
--------------------------------------------------------------------------------
    MyPrologue
--------------------------------------------------------------------------------
*/

/*
    When the printing system displays a printing dialog, it calls the
    prologue function in each registered dialog extension. If the user
    chooses a different printer while the dialog is still open, each
    prologue function is called again.

    If a prologue function returns a non-zero result code, the printing
    system will call the plug-in's terminate function.
*/

static OSStatus MyPrologue (
    PMPDEContext    *outContext,    // session-specific global data
    OSType          *creator,       // not used
    CFStringRef     *paneKind,      // kind ID string for this PDE
    CFStringRef     *title,         // localized title string
    UInt32          *maxH,          // maximum horizontal extent
    UInt32          *maxV           // maximum vertical extent
)

{
    MyContext context = NULL;
    OSStatus result = kPMInvalidPDEContext;

    context = malloc (sizeof (MyContextBlock));

    if (context != NULL)
    {
        context->customContext = MyCreateCustomContext();
        context->initialized = FALSE;
        context->userPane = NULL;
        context->helpHandler = NULL;
        context->helpHandlerUPP = NULL;

        // assign output parameters
        *outContext = (PMPDEContext) context;
        *creator    = kMozPDECreatorCode;
        *paneKind   = kMyPaneKindID;
        *title      = MyGetTitle();
        *maxH       = kMyMaxH;
        *maxV       = kMyMaxV;

        result = noErr;
    }

    MyDebugMessage("MyPrologue", result);
    return result;
}


/*
--------------------------------------------------------------------------------
    MyInitialize
--------------------------------------------------------------------------------
*/

static OSStatus MyInitialize (   
    PMPDEContext inContext,
    PMPDEFlags* flags,
    PMPDERef ref,
    ControlRef userPane,
    PMPrintSession session
)

{
    MyContext context = (MyContext) inContext;
    OSStatus result = noErr;

    *flags = kPMPDENoFlags;
    context->userPane = userPane;

    result = MySync (
        inContext, session, kSyncPaneFromTicket);

    MyDebugMessage("MyInitialize", result);
    return result;
}


/*
--------------------------------------------------------------------------------
    MySync
--------------------------------------------------------------------------------
*/

static OSStatus MySync (
    PMPDEContext inContext,
    PMPrintSession session,
    Boolean syncDirection
)

{
    MyContext context = (MyContext) inContext;
    OSStatus result = noErr;

    if (syncDirection == kSyncPaneFromTicket)
    {
        result = MySyncPaneFromTicket (context->customContext, session);
    }
    else
    {
        result = MySyncTicketFromPane (context->customContext, session);
    }

    return result;
}


/*
--------------------------------------------------------------------------------
    MyGetSummary
--------------------------------------------------------------------------------
*/

/*
    For each control, gets a localized description of the title & current value
*/

static OSStatus MyGetSummary (
    PMPDEContext inContext,
    CFArrayRef *titles,
    CFArrayRef *values
)

{
    MyContext context = (MyContext) inContext;
    CFMutableArrayRef titleArray = NULL;
    CFMutableArrayRef valueArray = NULL;

    // assume the worst
    OSStatus result = kPMInvalidPDEContext;

    // when the second argument to CFArrayCreateMutable is 0, 
    // the array size is not fixed

    titleArray = CFArrayCreateMutable (
        kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    if (titleArray != NULL)
    {
        valueArray = CFArrayCreateMutable (
            kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

        if (valueArray != NULL)
        {
            result = MyGetSummaryText (
                context->customContext, 
                titleArray, 
                valueArray
            );
        }
    }

    if (result != noErr)
    {
        if (titleArray != NULL)
        {
            CFRelease (titleArray);
            titleArray = NULL;
        }
        if (valueArray != NULL)
        {
            CFRelease (valueArray);
            valueArray = NULL;
        }
    }

    *titles = titleArray;
    *values = valueArray;

    MyDebugMessage("MyGetSummary", result);
    return result;
}


/*
--------------------------------------------------------------------------------
    MyOpen
--------------------------------------------------------------------------------
*/

static OSStatus MyOpen (PMPDEContext inContext)
{
    MyContext context = (MyContext) inContext;
    OSStatus result = noErr;

    if (!context->initialized)
    {
        // initialize pane
    
        IBNibRef nib = NULL;
    
        result = CreateNibReferenceWithCFBundle (
            MyGetBundle(), 
            kMyNibFile,
            &nib
        );

        if (result == noErr)
        {
            WindowRef nibWindow = NULL;
 
            result = CreateWindowFromNib (
                nib, 
                kMyNibWindow, 
                &nibWindow
            );

            if (result == noErr)
            {
                result = MyEmbedCustomControls (
                    context->customContext, 
                    nibWindow, 
                    context->userPane
                );

                if (result == noErr)
                {
                    context->initialized = TRUE;
                }

                DisposeWindow (nibWindow);
            }

            DisposeNibReference (nib);
        }
    }

    if (context->initialized)
    {
        result = MyInstallHelpEventHandler (
            GetControlOwner (context->userPane), 
            &(context->helpHandler),
            &(context->helpHandlerUPP)
        );
    }
    
    MyDebugMessage("MyOpen", result);
    return result;
}


/*
--------------------------------------------------------------------------------
    MyClose
--------------------------------------------------------------------------------
*/

static OSStatus MyClose (PMPDEContext inContext)
{
    MyContext context = (MyContext) inContext;
    OSStatus result = noErr;

    result = MyRemoveHelpEventHandler (
        &(context->helpHandler),
        &(context->helpHandlerUPP)
    );

    MyDebugMessage("MyClose", result);
    return result;
}


/*
--------------------------------------------------------------------------------
    MyTerminate
--------------------------------------------------------------------------------
*/

static OSStatus MyTerminate (
    PMPDEContext inContext, 
    OSStatus inStatus
)

{
    MyContext context = (MyContext) inContext;
    OSStatus result = noErr;

    if (context != NULL)
    {
        result = MyRemoveHelpEventHandler (
            &(context->helpHandler),
            &(context->helpHandlerUPP)
        );

        if (context->customContext != NULL) {
            MyReleaseCustomContext (context->customContext);
        }

        free (context);
    }

    MyDebugMessage("MyTerminate", result);
    return result;
}


// END OF SOURCE
