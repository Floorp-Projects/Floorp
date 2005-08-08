/*
********************************************************************************
    
    $Log: PDEUtilities.c,v $
    Revision 1.3  2005/08/08 03:08:43  smfr%smfr.org
    Fix bug 202014: add UI to the Mac Print Dialog Extension (PDE) that allows the user to select which headers and footers to print. Patch by Conrad Carlen, r=pinkerton, sr=me, a=asa.

    Revision 1.2  2003/04/03 19:20:05  ccarlen%netscape.com
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

#include "PDECore.h"
#include "PDECustom.h"
#include "PDEUtilities.h"

// callback function to handle the 'help' event
static OSStatus MyHandleHelpEvent (EventHandlerCallRef, EventRef, void *userData);


/*
--------------------------------------------------------------------------------
    MyDebugMessage
--------------------------------------------------------------------------------
*/

extern void MyDebugMessage (char *msg, SInt32 value)
{
    char *debug = getenv ("PDEDebug");
    if (debug != NULL)
    {
        fprintf (stdout, "%s (%d)\n", msg, (int) value);
        fflush (stdout);
    }
}

/*
--------------------------------------------------------------------------------
    MyCFAssign
--------------------------------------------------------------------------------
*/

extern CFTypeRef MyCFAssign(CFTypeRef srcRef, CFTypeRef dstRef)
{
    if (srcRef)
        CFRetain(srcRef);
    if (dstRef)
        CFRelease(dstRef);
    dstRef = srcRef;
    return dstRef;
}

/*
--------------------------------------------------------------------------------
    _MyGetBundle
--------------------------------------------------------------------------------
*/

static CFBundleRef _MyGetBundle (Boolean stillNeeded)
{
    static CFBundleRef sBundle = NULL;
    
    if (stillNeeded)
    {
        if (sBundle == NULL)
        {
            sBundle = CFBundleGetBundleWithIdentifier (kMyBundleIdentifier);
            CFRetain (sBundle);
        }
    }
    else
    {
        if (sBundle != NULL)
        {
            CFRelease (sBundle);
            sBundle = NULL;
        }
    }

    return sBundle;
}


/*
--------------------------------------------------------------------------------
    MyGetBundle
--------------------------------------------------------------------------------
*/

extern CFBundleRef MyGetBundle()
{
    return _MyGetBundle (TRUE);
}


/*
--------------------------------------------------------------------------------
    MyFreeBundle
--------------------------------------------------------------------------------
*/

extern void MyFreeBundle()
{
    _MyGetBundle (FALSE);
}


#pragma mark -

/*
--------------------------------------------------------------------------------
    MyGetTitle
--------------------------------------------------------------------------------
*/

extern CFStringRef MyGetTitle()
{
    return MyGetCustomTitle (TRUE);
}


/*
--------------------------------------------------------------------------------
    MyFreeTitle
--------------------------------------------------------------------------------
*/

extern void MyFreeTitle()
{
    MyGetCustomTitle (FALSE);
}


#pragma mark -

/*
--------------------------------------------------------------------------------
    MyGetTicket
--------------------------------------------------------------------------------
*/

extern OSStatus MyGetTicket (
    PMPrintSession  session,
    CFStringRef     ticketID,
    PMTicketRef*    ticketPtr
)

{
    OSStatus result = noErr;
    CFTypeRef type = NULL;
    PMTicketRef ticket = NULL;
    
    *ticketPtr = NULL;

    result = PMSessionGetDataFromSession (session, ticketID, &type);

    if (result == noErr)
    {    
        if (CFNumberGetValue (
            (CFNumberRef) type, kCFNumberSInt32Type, (void*) &ticket))
        {
            *ticketPtr = ticket;
        }
        else {
            result = kPMInvalidValue;
        }
    }

    return result;
}


/*
--------------------------------------------------------------------------------
    MyEmbedControl
--------------------------------------------------------------------------------
*/

extern OSStatus MyEmbedControl (
    WindowRef nibWindow,
    ControlRef userPane,
    const ControlID *controlID,
    ControlRef* outControl
)

{
    ControlRef control = NULL;
    OSStatus result = noErr;

    *outControl = NULL;

    result = GetControlByID (nibWindow, controlID, &control);
    if (result == noErr)
    {
        SInt16 dh, dv;
        Rect nibFrame, controlFrame, paneFrame;

        (void) GetWindowBounds (nibWindow, kWindowContentRgn, &nibFrame);
        (void) GetControlBounds (userPane, &paneFrame);
        (void) GetControlBounds (control, &controlFrame);
        
        // find vertical and horizontal deltas needed to position the control
        // such that the nib-based interface is centered inside the dialog pane

        dh = ((paneFrame.right - paneFrame.left) - 
                (nibFrame.right - nibFrame.left))/2;

        if (dh < 0) dh = 0;

        dv = ((paneFrame.bottom - paneFrame.top) - 
                (nibFrame.bottom - nibFrame.top))/2;

        if (dv < 0) dv = 0;
                
        OffsetRect (
            &controlFrame, 
            paneFrame.left + dh, 
            paneFrame.top + dv
        );
 
        (void) SetControlBounds (control, &controlFrame);

        // make visible
        result = SetControlVisibility (control, TRUE, FALSE);

        if (result == noErr) 
        {
            result = EmbedControl (control, userPane);
            if (result == noErr)
            {
                // return the control only if everything worked
                *outControl = control;
            }
        }
    }

    return result;
}


#pragma mark -

/*
--------------------------------------------------------------------------------
    MyReleaseContext
--------------------------------------------------------------------------------
*/

extern void MyReleaseContext (MyContext context)
{
    if (context != NULL)
    {
        if (context->customContext != NULL) {
            MyReleaseCustomContext (context->customContext);
        }

        free (context);
    }
}


#pragma mark -

/*
--------------------------------------------------------------------------------
    MyInstallHelpEventHandler
--------------------------------------------------------------------------------
*/

#define kMyNumberOfEventTypes   1

extern OSStatus MyInstallHelpEventHandler (
    WindowRef inWindow, 
    EventHandlerRef *outHandler,
    EventHandlerUPP *outHandlerUPP
)

{
    static const EventTypeSpec sEventTypes [kMyNumberOfEventTypes] =
    {
        { kEventClassCommand, kEventCommandProcess }
    };

    OSStatus result = noErr;
    EventHandlerRef handler = NULL;
    EventHandlerUPP handlerUPP = NewEventHandlerUPP (MyHandleHelpEvent);

    result = InstallWindowEventHandler (
        inWindow,
        handlerUPP,
        kMyNumberOfEventTypes,
        sEventTypes,
        NULL, 
        &handler
    );

    *outHandler = handler;
    *outHandlerUPP = handlerUPP;
    
    MyDebugMessage("InstallEventHandler", result);
    return result;
}


/*
--------------------------------------------------------------------------------
    MyRemoveHelpEventHandler
--------------------------------------------------------------------------------
*/

extern OSStatus MyRemoveHelpEventHandler (
    EventHandlerRef *helpHandlerP, 
    EventHandlerUPP *helpHandlerUPP
)

{
    OSStatus result = noErr;
    
    // we remove the help handler if there is still one present
    if (*helpHandlerP != NULL)
    {
        MyDebugMessage("Removing event handler", result);
        result = RemoveEventHandler (*helpHandlerP);
        *helpHandlerP = NULL;
    }

    if (*helpHandlerUPP != NULL)
    {
        DisposeEventHandlerUPP (*helpHandlerUPP);
        *helpHandlerUPP = NULL;
    }
    return result;
}


/*
--------------------------------------------------------------------------------
    MyHandleHelpEvent
--------------------------------------------------------------------------------
*/

static OSStatus MyHandleHelpEvent
(
    EventHandlerCallRef call,
    EventRef event, 
    void *userData
)

{
    HICommand   commandStruct;
    OSStatus    result = eventNotHandledErr;

    GetEventParameter (
        event, kEventParamDirectObject,
        typeHICommand, NULL, sizeof(HICommand), 
        NULL, &commandStruct
    );

    if (commandStruct.commandID == 'help')
    {
        // result = MyDisplayHelp();
        // MyDebugMessage("handled help event", result);
    }

    return result;
}


// END OF SOURCE
